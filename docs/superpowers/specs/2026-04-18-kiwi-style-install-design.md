# Kiwi-style install flow — design spec

**Status**: design, awaiting approval
**Branch**: `feature/heavy-ext-and-install`
**Target release**: v1.4
**Supersedes (trigger portion only)**: `2026-04-17-install-flow-redesign-design.md`
— that spec added the confirm-dialog + `CrxInstallCoordinator` pipeline.
This spec keeps both and only changes *what fires the coordinator*.

## Context

The v1.2 install flow intercepts the `chromewebstore.google.com/detail/...`
navigation entirely. The user taps a link to the Chrome Web Store and
sees our confirm dialog pop up without ever rendering the store page.
The side-effects are:

1. No description / screenshots / ratings / "related" to help the user
   decide whether to install.
2. No discovery — the user must already have the direct URL.
3. Surprising behaviour. A user expecting the Web Store gets a blocking
   modal instead.

Kiwi (verified in `docs/superpowers/diagnostics/2026-04-17-kiwi-install-flow.md`)
does the opposite: the detail page renders, and the page's own blue
"Install" button triggers install. The trigger lands in Chromium's
`webstorePrivate.beginInstallWithManifest3` handler, which runs the
full `WebstoreStandaloneInstaller` → `ExtensionInstallPrompt` →
`CrxInstaller` pipeline.

## Goal

Replicate Kiwi's trigger point inside Afterbird *without* pulling in the
full upstream permission-prompt UI. The existing
`ExtensionInstallConfirmBridge` dialog and `CrxInstallCoordinator`
pipeline stay; the CWS detail page calls into them via a scoped
`webstorePrivate` shim.

## Non-goals

- Full `webstorePrivate` API surface. We'll implement the minimum
  functions the current CWS detail page calls; adding more is a v1.5
  task if the store UI changes.
- CRX3 signature verification. Matches Kiwi.
- Replacing the `ExtensionInstallConfirmBridge` dialog with the upstream
  `ExtensionInstallPrompt`. Our Java dialog stays as the trust boundary.
  Kiwi's native permission-prompt UI has a ~3000-LOC dependency chain
  (`PermissionMessageProvider`, `ChromePermissionMessageProvider`,
  `ExtensionInstallDialogView`) that is explicitly excluded from
  desktop-android per the v1.1 spec and remains so.
- Keeping the auto-install-on-navigation path as a fallback. It's
  deleted — behaviour is too surprising and the replacement path covers
  all known entry points.
- Inline install from non-CWS origins. `chromewebstore.google.com` and
  the legacy `chrome.google.com/webstore` only.

## Changes at a glance

| Piece | Today | After |
| --- | --- | --- |
| CWS detail navigation | intercepted, cancelled | rendered normally |
| Install trigger | navigation-throttle URL match | CWS detail page's own "Install" button via `webstorePrivate.beginInstallWithManifest3` |
| Confirm dialog | `ExtensionInstallConfirmBridge` (Java modal) | same |
| Download + install | `CrxInstallCoordinator` | same |
| Discovery entry | none (paste URL only) | "+ (from store)" button in `chrome://extensions` toolbar |
| `.crx` / `.user.js` direct download | still caught by `ShouldInterceptDownload` | unchanged |
| `chrome.google.com/webstore/detail/...` legacy URL | intercepted | redirected to CWS equivalent, then rendered |

## Trigger — `webstorePrivate.beginInstallWithManifest3`

### What the CWS page actually calls

The public CWS web app calls three `webstorePrivate` extension-API
functions during the install flow:

1. `webstorePrivate.getWebGLStatus()` — returns a string
   (`"webgl_allowed"`). Pure query, no side effects.
2. `webstorePrivate.beginInstallWithManifest3({ id, manifest, iconUrl,
   iconData, localizedName, ...})` — starts the install. Chromium's
   upstream version parses the manifest, builds permission warnings,
   spawns the permission-prompt UI, and on confirm initiates the CRX
   fetch.
3. `webstorePrivate.completeInstall(id)` — page tells the browser the
   install can now "commit" (signals the page-visible state machine).

The detail page guards on `chrome.runtime.id` being the webstore hosted
app id (`ahfgeienlihckogmohjhadlkjgocpleb`). Since our desktop-android
build doesn't ship the webstore hosted app, we can't rely on that id
being present. Workaround: expose the API to
`chromewebstore.google.com` via its `matches` pattern instead — see
`extensions/common/permissions/api_permission_set.cc` approach. The
webstorePrivate API is **already** restricted to the webstore origin
in upstream; we keep the same restriction.

### Our shim, scope-boundary

New directory: `chrome/browser/extensions/desktop_android/webstore_private/`

New files:

- `webstore_private_api.{cc,h}` — implements three functions:
  - `WebstorePrivateGetWebGLStatusFunction` — returns `"webgl_allowed"`.
  - `WebstorePrivateBeginInstallWithManifest3Function` — extracts the
    extension id from params, builds the CRX URL via the existing
    `BuildWebstoreCrxUrl` helper (moved out of the throttle's
    anonymous namespace into a new `webstore_url_util.{cc,h}`), hands
    off to `CrxInstallCoordinator::StartFromWebstore`. On success the
    function calls back with `"::install_stage::downloading"` status.
  - `WebstorePrivateCompleteInstallFunction` — no-op success. The CWS
    page uses this to advance its client-side spinner; we don't have
    any browser state keyed on it.
- `desktop_android_webstore_private_extension_functions.cc` — the
  function-factory boilerplate registering the three classes with
  `ExtensionFunctionRegistry::RegisterAll`.

New build target entry in `chrome/browser/extensions/desktop_android/BUILD.gn`.

Registration in `chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc`
via `RegisterExtensionFunctions`.

### Permission gate

`webstorePrivate` is a "privileged" API, upstream-gated to the webstore
origin through `extensions/common/permissions/permission_features.json`.
We replicate the gate in our shim by manually checking
`source_url().DomainIs("chromewebstore.google.com")` at the top of each
function's `Run()`. Rejection returns `kUnknownExtension` to match
upstream error shape.

### Why this over the navigation-throttle path

- **Trigger fires exactly when the user intends to install.** No more
  `clients2.google.com` fetch for every paste-into-addressbar.
- **The CWS detail page renders.** User sees description and
  screenshots.
- **Single trigger path.** `CrxInstallCoordinator` is the one and only
  install entry after this; easier to reason about.
- Matches Kiwi exactly.

### Extension ID passthrough

`beginInstallWithManifest3` receives the full manifest text from the CWS
page. Option A: trust the manifest the page gives us, unpack CRX
against it, proceed. Option B: ignore the page-provided manifest, rely
on the CRX's own manifest after download.

Recommend **B**. The manifest the page provides is derived from the CRX
server-side but we can't verify that at the client. Our coordinator
already loads the manifest from the unpacked CRX for the confirm
dialog. No need to trust the page. The only piece we take from the
page is the extension id (needed to build the fetch URL).

## Discovery — "+ (from store)" button

### Where it goes

`chrome/browser/resources/extensions/toolbar.html.ts` currently omits
the button. Add:

```ts
<cr-button id="loadFromStore" @click="${this.onLoadFromStoreClick_}">
  $i18n{toolbarLoadFromStore}
</cr-button>
```

Sited alongside `loadUnpacked` and `packExtensions` inside the dev-drawer
button strip. It is *not* gated on dev mode — this is the normal-user
install path.

### What it does

On click, open `https://chromewebstore.google.com/category/extensions`
in a new foreground tab of the current browser context. Implement via
the existing `ExtensionsBrowserProxy.openURL()` method (need to verify
it exists in our clipped version; add if missing).

### String

New grd entry: `IDS_EXTENSIONS_TOOLBAR_LOAD_FROM_STORE` → `"+ (from
store)"` (matches the Polymer template we never shipped).

## Removing the aggressive throttle

`chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.{cc,h}`
— **delete entirely**.

- Any direct-link navigation to `chromewebstore.google.com/detail/...`
  renders the page normally. The user then uses the page's Install
  button, which lands in our shim.
- Any direct-link navigation to `chrome.google.com/webstore/detail/<id>`
  or `/webstore/detail/<slug>/<id>` — we can't render (Google 301s
  these to the new domain anyway; verify with a quick curl check
  during implementation). If the server doesn't auto-redirect, add a
  tiny client-side throttle that rewrites `chrome.google.com/webstore/
  detail/<...>` to `chromewebstore.google.com/detail/<...>`. Keep that
  as a single 15-line helper, not a full `NavigationThrottle`.

`CreateThrottlesForNavigation` registration in
`chrome_content_browser_client.cc` — remove the line that adds the
throttle; leave the surrounding `BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)`
block.

Keep `ShouldInterceptDownload` handler for direct-`.crx` URLs as-is. If
a user finds a raw `.crx` link on a GitHub release, that still routes
to the coordinator.

## State machine

Unchanged from the v1.2 spec. The trigger changes, not the pipeline.

```
CWS detail "Install" button
        │
        ▼
webstorePrivate.beginInstallWithManifest3 (our shim)
        │
        ▼
CrxInstallCoordinator::StartFromWebstore(ctx, web_contents, crx_url,
                                         "Chrome Web Store")
        │
        ├─ S1 FETCHING → S2 UNPACKING → S3 AWAIT_CONFIRM → S4 INSTALL
        └─ (T_FAIL / T_CANCEL / T_CANCEL_SILENT as per prior spec)
```

### Edge: user taps Install twice

Page-side dedupe first. If it slips through, `CrxInstallCoordinator`
keys off `source_url + extension_id` and drops a second concurrent
attempt (this was a v1.2 "nice to have"; promote to required now since
the shim is easier to double-fire than the throttle was).

### Edge: Install tapped on an already-installed extension

Coordinator replaces the existing install in `AddExtension`. Toast
says "Replaced *<name>* v*X.Y.Z*" instead of "Installed". Matches
v1.2 spec.

### Edge: JS returns error to webstorePrivate caller

The CWS page expects a callback with an error string for bad input.
Our shim returns `kInvalidWebstoreItemId` / `kInstallInProgress` etc.
using the same error codes the page handles upstream. This is a
source-level copy of the error-code enum from
`chrome/browser/extensions/api/webstore_private/webstore_private_api.h`.

## File-level change list

### New

- `chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.{cc,h}`
  — the three function classes + dispatcher.
- `chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.{cc,h}`
  — houses `BuildWebstoreCrxUrl`, moved here from the about-to-be-deleted
  throttle. Added utility: `RewriteLegacyWebstoreUrl(GURL) -> GURL`.

### Modified

- `chrome/browser/extensions/desktop_android/BUILD.gn`
  — add the new webstore_private target; keep coordinator target;
  remove throttle target.
- `chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc`
  — register the three new extension functions.
- `chrome/browser/chrome_content_browser_client.cc`
  — remove throttle registration in the
  `ENABLE_DESKTOP_ANDROID_EXTENSIONS` block; keep
  `ShouldInterceptDownload` handler.
- `chrome/browser/resources/extensions/toolbar.html.ts`
  — add `loadFromStore` button.
- `chrome/browser/resources/extensions/toolbar.ts`
  — add `onLoadFromStoreClick_()` method; open CWS category URL via
  `chrome.tabs.create()` (or our Delegate equivalent if tabs API isn't
  exposed on the extensions page — check at implementation time).
- `chrome/app/extensions_strings.grdp` (or wherever we added
  `IDS_AFTERBIRD_EXTENSION_INSTALL_FINE_PRINT` in v1.2) — add
  `IDS_EXTENSIONS_TOOLBAR_LOAD_FROM_STORE`.

### Deleted

- `chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.{cc,h}`
  — entirely. Its useful helpers (`ExtractWebstoreExtensionId`,
  `BuildWebstoreCrxUrl`) move to the new `webstore_url_util.*`.

## Verification plan

1. **Source build**: ensure `ninja -C out/android chrome_public_apk`
   stays green. Incremental build target; serv box ~3–4 min.
2. **APK install and smoke**: install on `R5CTA2MHJFA`, navigate to
   `https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh`,
   confirm the detail page renders (screenshot diff vs. today's
   immediate-confirm-modal).
3. **Install from CWS button**: tap Install on the detail page,
   observe our Java confirm dialog (not upstream's), tap Install,
   observe the extension lands in `chrome://extensions`.
4. **+ (from store) button**: open `chrome://extensions`, tap the new
   button, verify a new tab loads `chromewebstore.google.com/category/extensions`.
5. **Legacy URL**: visit `https://chrome.google.com/webstore/detail/
   ublock-origin/cjpalhdlnbpafiamejdnhcphjbkeiagm` (MV2 uBO, but the
   URL routes us to the MV3 equivalent via Google's 301); confirm
   either a server-side redirect or our client rewrite gets us to
   `chromewebstore.google.com`.
6. **Direct `.crx` download**: fetch a raw `.crx` from a github
   release URL, confirm `ShouldInterceptDownload` still catches it
   and shows our confirm dialog.
7. **Already-installed extension**: from uBOL detail page after a
   prior install, tap Install again, confirm "Replaced" toast.
8. **Broken input**: use devtools on the CWS detail page to call
   `chrome.webstorePrivate.beginInstallWithManifest3({id: "xxx"})`,
   confirm our shim rejects with `kInvalidWebstoreItemId`.

## Scope boundary (explicit)

We are **not** doing in v1.4:

- Inline install from non-CWS origins (including the signed "hosted
  on the publisher's own site" variant).
- `webstorePrivate.enableAppLauncher`, `showPermissionPromptForDelegatedInstall`,
  or any of the 20+ other functions the upstream API has. Add on
  demand.
- Publisher verification. Today's CWS page shows a "Featured" badge
  that we don't surface; our confirm dialog remains the trust
  boundary and shows raw manifest permission strings only.
- Re-implementing upstream's `PermissionMessageProvider` humanisation.
- Telemetry on install success / failure.
- Update flow for webstore-sourced extensions. `autoUpdate` is still
  a no-op.
- Replacing the `ExtensionInstallConfirmBridge` Java modal with a
  native Views dialog. Our Java modal stays.
- Supporting the `chrome-extension://` install button on
  `chrome://extensions` (there isn't one today; the dev-mode
  "Load unpacked" flow covers local installs).

## Known gap discovered during implementation

Implementation committed on `feature/heavy-ext-and-install` (commits
`55a8a5fa` → `6cdf8f4b`). Static source review found one runtime
concern the spec missed:

**Feature-availability gating.** Our shim registers the three
`WebstorePrivate*Function` classes with `ExtensionFunctionRegistry`,
but the renderer side (`extensions/renderer/native_extension_bindings_system.cc`)
gates `chrome.webstorePrivate` exposure to page contexts on
`script_context->GetAvailability("webstorePrivate").is_available()`.
That availability check reads the compiled-in `_api_features.json`
metadata. Desktop-android omits `webstorePrivate` from that metadata,
so even with our browser-side handlers, the CWS page would see
`chrome.webstorePrivate === undefined` and its Install button would
throw.

**Three remediation options** (pick at next session):

1. Add `webstorePrivate` to an Afterbird-owned features JSON that gets
   compiled into desktop-android. Smallest diff, but requires touching
   the upstream feature-provider plumbing — maybe 50 LOC in
   `extensions/common/api/_api_features.json` equivalent plus a GN
   entry.
2. Teach the CWS page's `Install` button to POST to an Afterbird-only
   URL scheme (e.g. `afterbird-install://?id=...`) via an injected
   content-script — dodges the feature system entirely but reintroduces
   the "brittle content-script on store DOM" risk we listed earlier.
3. Expose the function via `browser_context()->GetURLLoaderFactory()`
   and a custom `mojo::Remote<>` bound only to the CWS origin,
   bypassing the extensions dispatch pipeline. Clean but ~200 LOC of
   Mojo boilerplate.

Recommend option 1. Tracking the follow-up is out of scope for this
session since the source work for options 2/3 diverges significantly.

## Open questions (for approval round)

1. **Should the `+ (from store)` button be gated on dev-mode off?**
   Recommend no — dev-mode is a "I know what I'm doing" flag; the
   store button is the primary install entry for normal users and
   should be always visible. Kiwi's main-menu "+ (from store)" is
   also unconditional.
2. **`webstorePrivate.completeInstall` return value.** Upstream
   responds with an empty object on success. Our shim does the
   same but the page's JS state machine has evolved; may need a
   `status` field. Verify at implementation.
3. **`getWebGLStatus` return value.** `"webgl_allowed"` is safe; the
   page also accepts `"webgl_blocked"` and `"webgl_blocked_by_gpu_driver"`
   without breaking. Fixed string is fine for v1.4.
4. **Install button on the detail page shows "Remove" when the
   extension is already installed upstream.** Our shim would need to
   implement the state query (`webstorePrivate.getIsLauncherEnabled`
   / `getExtensionStatus`) for this to render correctly. Recommend
   **punt**: leave the button reading "Install" always. The coordinator
   handles re-install gracefully. Revisit in v1.5.
5. **Throttle deletion safety.** Are there any unit tests or
   browsertests pinned to the throttle class name? Grep confirms none
   in our tree; the only referents are the header include and the
   `CreateThrottlesForNavigation` hook. Safe to delete.
