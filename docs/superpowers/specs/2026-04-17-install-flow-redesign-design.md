# Install Flow Redesign — download-triggered, confirmation-gated

**Status**: design, awaiting approval
**Branch**: `feature/extension-ux-and-devtools`
**Target release**: v1.2

## Context

v1.1 ships `ExtensionInstallNavigationThrottle` (see
`chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.{cc,h}`)
which silently intercepts any main-frame navigation whose URL ends in `.crx`
/ `.user.js`, or matches a Chrome Web Store detail page, or has response
`Content-Type: application/x-chrome-extension`. The throttle cancels the
navigation and spawns a self-owned `CrxDownloadInstaller` that runs a
`SimpleURLLoader::DownloadToFile` to the cache dir and then calls
`DesktopAndroidExtensionInstaller::InstallFromFile`.

User feedback (verbatim, translated): "the install flow is terrible. The
install flow should fire only on DOWNLOAD, and with a user dialog — like
'install extension {name}, yes/no', with fine print explaining risks."

Two concrete asks:

1. Trigger on **download**, not navigation. The browser should recognise a
   `.crx` only once it has decided the response is a downloadable file.
2. Show a **confirm dialog** with extension name + risk fine-print + Yes/No.

## Non-goals

- Full upstream permission-warning engine. Chrome maps raw manifest
  permissions into human-readable warnings via `PermissionMessageProvider`
  + `ChromePermissionMessageProvider`; that lives under
  `extensions/common/permissions` and is deliberately not compiled into
  desktop-android. We will show the *raw* `manifest.permissions` array
  (`"tabs"`, `"storage"`, `"<all_urls>"`, etc.) as-is.
- CRX3 signature verification. Still matches Kiwi's stance; file is trusted
  once the user confirms.
- Allow-listing / publisher verification.
- Update dialogs. `autoUpdate` is still a no-op.
- Integrating with the stock Android download notification UI. The CRX
  never reaches `Downloads/`; it goes straight to our cache dir.
- `webstorePrivate` integration. Store-detail URL matching stays.

## Trigger point — recommendation: `ShouldInterceptDownload`

### Decision

Replace the v1.1 `WillStartRequest` / `WillRedirectRequest` URL-matching
path with:

- `ContentBrowserClient::ShouldInterceptDownload` — vetoes the download
  before it writes to `Downloads/` and lets us route the bytes to our own
  install pipeline. Implemented in
  `chrome/browser/chrome_content_browser_client.cc` (already contains our
  `ENABLE_DESKTOP_ANDROID_EXTENSIONS` block at lines 701 / 5474).

Keep for store-detail URLs only (no download involved):

- `ExtensionInstallNavigationThrottle`, narrowed to match only
  `chromewebstore.google.com/detail/...` and `chrome.google.com/webstore/detail/...`.
  It no longer matches `.crx` / `.user.js` / MIME. On match it still
  synthesises the `clients2.google.com/service/update2/crx` URL and starts
  an in-process fetch — but now that fetch routes through the new confirm
  flow instead of silently installing.

### Why this over the alternatives

- **`DownloadManager::Observer::OnDownloadCreated`** — works, but by the
  time it fires the `DownloadItem` already has a target path under
  `Downloads/` and the user has already seen a download-starting toast.
  We'd then have to cancel, delete the partial, and show our dialog. Dirty.
- **Keep `NavigationThrottle` for .crx too** — fires too early; user's
  actual complaint. The URL ending in `.crx` is a heuristic, not a download
  decision.
- **Blink-side hook** — doesn't exist at the right layer; downloads bypass
  Blink renderer-side logic.

`ShouldInterceptDownload` is the primitive used upstream desktop for the
same thing (extension install from crx download) and it gates the handoff
cleanly: we return `true`, the download is cancelled before it commits to
disk, and we own the lifecycle from there.

### BUILDFLAG gating

All new code sits under `BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)`,
matching the existing throttle registration at
`chrome_content_browser_client.cc` line 5474. No new flag.

## What triggers the intercept

### Download path (new)

In `ChromeContentBrowserClient::ShouldInterceptDownload`, match **any one
of**:

1. Response MIME is `application/x-chrome-extension` or
   `application/x-chromium-extension`.
2. URL's path (lowercased) ends in `.crx`.
3. URL's path (lowercased) ends in `.user.js` **and** response MIME is
   `text/javascript` / `application/javascript` / `application/x-javascript`.
   (The `.user.js` case is deferred per v0.6 spec non-goals, but we keep
   the match so the existing log-and-install path stays reachable via a
   feature flag later. For v1.2, log and PROCEED — do not intercept.)

Rules 1+2 cover the real download-stage case. Content-Disposition filename
is a nice-to-have for v1.3.

### Store-detail path (narrowed throttle)

In `ExtensionInstallNavigationThrottle::WillStartRequest` /
`WillRedirectRequest`:

- Match only `chromewebstore.google.com/detail/<slug?>/<id>` or
  `chrome.google.com/webstore/detail/<slug?>/<id>`. Keep the existing
  32-char `[a-p]` id validation.
- On match, cancel the navigation and hand the synthesised
  `clients2.google.com/service/update2/crx?...id=<id>` URL to the new
  `CrxInstallCoordinator` (see below), which runs **the same
  download → confirm → install pipeline** as the download path.

Removed from the throttle:

- `.crx` / `.user.js` URL matching.
- MIME-based matching in `WillProcessResponse`.

## Confirmation dialog

### Where it lives

**Recommendation: Java `ModalDialogManager` + a dedicated
`ExtensionInstallConfirmBridge`**, driven from C++ via JNI. Same shape as
the existing `ExtensionInstallBridge` (picker) pair:

- New Java file:
  `chrome/browser/extensions/android/java/src/org/chromium/chrome/browser/extensions/ExtensionInstallConfirmBridge.java`.
- New C++ files:
  `chrome/browser/extensions/android/extension_install_confirm_bridge.{cc,h}`.

Why ModalDialogManager over alternatives:

- **Blink/web bottom-sheet** — wrong layer; the confirm is browser UI, not
  content UI. Also wouldn't survive a renderer crash mid-install.
- **Native `ModalDialogTabHelper`** — no such thing on desktop-android
  today; every native modal still routes through
  `org.chromium.ui.modaldialog.ModalDialogManager` (`content_public` layer)
  anyway.
- **Raw `AlertDialog`** — works but bypasses Chromium's lifecycle hooks
  (tab switch, activity destroy). ModalDialogManager already handles those.

Anchor point: the `WebContents` that initiated the download /
navigation → `WindowAndroid` → `ModalDialogManager` obtained via
`ModalDialogManagerHolder` (same path `ExtensionInstallBridge` uses to
reach the `Activity`).

### What info is shown

Field | Source | Notes
--- | --- | ---
`name` | `manifest.name` (localised via `default_locale` if present) | Unpacked ahead of confirm.
`version` | `manifest.version` | Raw string.
`permissions` | `manifest.permissions` + `manifest.host_permissions` (MV3) or `manifest.permissions` (MV2 mixed list) | Raw strings, joined with ", ". Hidden (row collapsed) when empty.
`source` | "Chrome Web Store" if store-detail flow, otherwise URL host | Derived at coordinator level, passed through JNI as a label.
fine-print | hard-coded resource string | See below.

### Unpack-before-confirm vs confirm-before-unpack

**Unpack first, then confirm.** Reasoning:

- We need `name` + `version` + `permissions` at dialog build time, and all
  three require reading `manifest.json` — which is inside the CRX.
- `DesktopAndroidExtensionInstaller::UnpackOnBlockingThread` already does
  exactly the work needed (CRX header strip + zip extract into a staging
  dir). We refactor it so the "unpack" step is separable from the
  "promote to `<profile>/Extensions/<id>/` + `AddExtension`" step.
- After a cancel, we delete the staging dir. Cheap.
- On mobile with fast UFS storage the 50–500 KB extension unpack is <150 ms
  for 95% of extensions; we swallow that in a spinner state within the
  dialog bridge (see state machine).

### Fine-print copy

Single resource string, 1–2 lines, plain English:

```
Extensions can read and change everything you see on the web, including
passwords and personal data. Only install extensions from sources you trust.
```

String id: `IDS_AFTERBIRD_EXTENSION_INSTALL_FINE_PRINT` (new; lives in our
Afterbird string grd, not upstream's `generated_resources.grd`, so rebase
pain stays local).

Dialog title: `Install "%1$s"?` (`%1$s` = extension name).
Primary button: `Install`. Secondary: `Cancel`. Dismiss on back-press =
Cancel.

## State machine

```
           (download intercept OR store-URL intercept)
                          │
                          ▼
           ┌─────────────────────────────────┐
           │ CrxInstallCoordinator::Start()  │ owns: CrxFetcher, staging dir,
           │                                 │        weak WebContents
           └─────────────┬───────────────────┘
                         │
                         ▼
                   [S1: FETCHING]
                         │
           ┌─────────────┴────────────┐
     success (200, path)          failure (net err / size cap)
           │                            │
           ▼                            ▼
     [S2: UNPACKING]              [T_FAIL] log; delete fetch file; show
           │                               toast "Could not download
           │                               extension"; drop coordinator.
           ▼
     [S3: AWAIT_CONFIRM]  ─ WebContents lost? → [T_CANCEL_SILENT]
           │                                      (delete staging, log)
           │
     ┌─────┼─────┬────────────────────┐
   yes   no    activity gone (dialog dismissed by system)
     │     │         │
     ▼     ▼         ▼
 [S4:     [T_CANCEL] delete staging + fetch file; toast
 INSTALL]            "Install cancelled"; done.
     │
     ▼
 AddExtension() + persist to ExtensionPrefs
     │
 ┌───┴───┐
 ok     fail
 │        │
 ▼        ▼
 toast    log + toast "Install failed: <reason>";
 "Extension installed";         delete staging.
 promote and keep dir on disk.
```

### Edge cases

- **User closes the app mid-confirm** (`S3`): `ExtensionInstallConfirmBridge`
  observes dialog-dismissed from `ModalDialogManager`, routes to
  `T_CANCEL_SILENT`. Staging dir + fetched CRX are deleted on the blocking
  pool. No extension registered.
- **User taps Install and immediately navigates away** (`S3 → S4`): install
  runs on the UI thread + blocking pool independent of the tab. We already
  detach from `WebContents` for install; only used it for dialog anchoring.
  Toast is posted to the current `Activity` via `ContextUtils`, not the
  dead tab.
- **Download fails mid-fetch** (`[T_FAIL]`): no dialog ever shown. Single
  toast.
- **Unpack fails** (bad CRX, zip corrupt): transition `S2 → [T_FAIL]` with
  reason "Extension package is corrupted". Staging dir deleted.
- **WebContents destroyed between fetch and confirm**: `S2 → S3` finds a
  null `WeakPtr<WebContents>`. Silent cancel + cleanup.
- **Duplicate clicks** (user taps same `.crx` link twice): coordinator
  keyed off `(extension_id_from_manifest, source_url)` after unpack. Second
  attempt while first is live becomes a no-op (log + drop). Not v1.2
  critical; ok to skip if it costs more than 30 LOC.
- **Already-installed extension**: `DesktopAndroidExtensionSystem::AddExtension`
  already handles re-install by replacing. Surface a "Replaced <name>
  vX.Y.Z" toast instead of "Installed" when the `ExtensionId` already
  existed in the registry before `S4`.

### Cleanup ownership

Two scratch locations today:

- `<cacheDir>/afterbird_picker/` — used by Java picker *and* the C++
  download helper. Per-file UUID-prefixed so collisions don't matter.
- `<profile>/Extensions/afterbird_install_<ts>/` — staging dir created by
  `DesktopAndroidExtensionInstaller::UnpackOnBlockingThread`.

Rule: `CrxInstallCoordinator` owns both paths for the lifetime of the
install. Every terminal transition (`T_FAIL`, `T_CANCEL`, `T_CANCEL_SILENT`,
install-failure) runs a single blocking-pool `DeletePathRecursively` over
both. Install-success keeps the promoted `<profile>/Extensions/<id>/`
only; staging sibling dir goes away in the same cleanup.

## File-level change list

### New

- `chrome/browser/extensions/desktop_android/crx_install_coordinator.{cc,h}`
  — orchestrates fetch → unpack → confirm → install. Takes over from
  `CrxDownloadInstaller` (private class currently in the throttle .cc).
  Owns `SimpleURLLoader`, staging dir path, `DesktopAndroidExtensionInstaller`.
- `chrome/browser/extensions/android/extension_install_confirm_bridge.{cc,h}`
  — single-shot JNI bridge, same self-owned-pointer pattern as
  `ExtensionInstallCallback` (see
  `chrome/browser/extensions/android/extension_install_bridge.h`).
  Exposes `Show(web_contents, name, version, permissions, source_label,
  ConfirmCallback)`.
- `chrome/browser/extensions/android/java/.../ExtensionInstallConfirmBridge.java`
  — builds a `PropertyModel` for a `ModalDialogProperties`-shaped dialog,
  pushes it onto the WebContents' `ModalDialogManager`, reports back via
  JNI on positive/negative/dismiss.
- Strings (1 new grd entry): `IDS_AFTERBIRD_EXTENSION_INSTALL_FINE_PRINT`
  plus a title format string.

### Modified

- `chrome/browser/extensions/desktop_android/extension_installer.{cc,h}`
  — split `InstallFromFile` into two calls:
  1. `PrepareFromFile(path, PreparedCallback)` — unpacks into staging,
     loads manifest, returns `(Extension, staging_root, error)` without
     promoting.
  2. `CommitPrepared(prepared)` — promotes staging to final install dir and
     calls `AddExtension`.
  `InstallFromFile` stays as a convenience wrapper (prepare + commit with
  no gap) for the developer-mode picker flow (`loadUnpacked` of a `.zip`
  already auto-confirms by virtue of the user tapping Install in the
  picker).
- `chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.{cc,h}`
  — remove `LooksLikeExtensionUrl`, remove `WillProcessResponse` MIME
  handling, remove the nested `CrxDownloadInstaller` class. Keep
  `ExtractWebstoreExtensionId` + `BuildWebstoreCrxUrl`. `WillStartRequest`
  / `WillRedirectRequest` now only match store URLs and hand off to
  `CrxInstallCoordinator::Start`.
- `chrome/browser/chrome_content_browser_client.cc` — implement
  `ShouldInterceptDownload` inside the existing
  `ENABLE_DESKTOP_ANDROID_EXTENSIONS` blocks. Return `true` for the MIME
  / suffix matches listed above, after handing off to
  `CrxInstallCoordinator::StartFromDownload(download_url, web_contents)`.
  Keep the `CreateThrottlesForNavigation` registration as-is; the throttle
  is still created, just does less.
- `chrome/browser/extensions/desktop_android/BUILD.gn` — add the new
  coordinator target; keep throttle target.
- `chrome/browser/extensions/android/BUILD.gn` + `jni_headers` — add the
  confirm bridge to `sources`, `java_sources`, and the `jni_headers`
  group.
- Feature-flag test in `DesktopAndroidExtensionInstallerBrowserTest` (if
  we have one yet; if not, add a scope-limited browsertest for the
  coordinator state machine using a `TestDownloadManagerObserver`-style
  fixture).

### Deleted

- The `CrxDownloadInstaller` class (currently anonymous-namespaced inside
  `extension_install_navigation_throttle.cc`). Its fetch loop migrates
  verbatim into `CrxInstallCoordinator` and gains a new terminal state for
  "await user confirm" between download-done and install.

## Scope boundary (explicit)

We are **not** doing:

- Upstream's `PermissionMessageProvider` humanising. Raw manifest
  permission strings only.
- Remembering a "don't ask again" per origin.
- Differentiating "update an existing extension" from "fresh install" in
  the dialog copy.
- Hooking the stock Android download-manager UI or posting to
  `Downloads/`.
- `.user.js` install. Logged and PROCEED'd at the intercept layer; no
  dialog, no install.
- CRX3 signature checking, blocklist, publisher verification, or the
  Safe Browsing "unwanted software" download check.
- Per-permission granular consent (e.g. "allow this extension on example.com
  only"). Install is all-or-nothing.
- Tests for the store-detail URL → CRX fetch path beyond what v1.1 had;
  that code is unchanged in shape.

## Open questions (for approval round)

1. Should we refactor `CrxInstallCoordinator` to live in
   `//chrome/browser/extensions/desktop_android/` (matches `installer.*`)
   or under `//chrome/browser/extensions/android/` next to the bridges?
   Recommend the former — it's C++ orchestration, not Java boundary code.
2. Should "Replaced v1.2 → v1.3" surface as a distinct dialog title
   ("Update <name>?") or stay a plain "Install"? Recommend staying plain
   for v1.2 and revisiting in v1.3 alongside the deferred `autoUpdate`
   work.
3. Fine-print copy — the text above is a first draft. Final wording to be
   signed off by the user at approval time.
