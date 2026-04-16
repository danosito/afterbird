# Extension Management for chrome://extensions on desktop-android

**Status**: approved for implementation
**Branch**: `feature/extension-management`
**Target release**: v0.6.0

## Context

Afterbird ships `chrome://extensions` (v0.5.0) but every mutation is a stub:
the "Load unpacked" button is a no-op, toggles/Remove do nothing, and the
Chrome Web Store's "Add to Chrome" button is blocked. The only way to get an
extension onto the browser today is `--load-extension=<path>` on startup, and
even that list isn't restored after the next launch because
`DesktopAndroidExtensionSystem::InitForRegularProfile` only reads the flag
and doesn't walk `ExtensionPrefs`.

This spec covers the three flows the user asked for in one release:
installing from `.zip`/`.crx`, installing from the Chrome Web Store, and
enabling/disabling/removing existing extensions — all from the standard
`chrome://extensions` UI, not the command line.

## Non-goals

- Full CRX3 signature verification. Matches Kiwi's position. The file is
  trusted the moment the user picked it / confirmed the install sheet.
- `webstorePrivate` API surface. Chrome Web Store flow goes through an
  Android-side bottom sheet + direct `.crx` download, not through the
  webstore JS integration.
- Per-extension policy, Safety Hub, MV2 deprecation UI.
- `.user.js` import (userscript manager style). Deferred to a future
  release.
- Extension updates (`autoUpdate` stays a no-op).

## Out-of-scope UI changes

No patches to `chrome/browser/resources/extensions/*.ts/html`. The standard
Polymer UI already has a "Load unpacked" button and per-item toggles; we
just make them work. Kiwi re-labeled the button to advertise file-picker
support — we don't, because our file picker accepts both a file and a
directory intent and users won't be surprised either way.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│ chrome://extensions  (stock Polymer UI)                      │
│   • "Load unpacked"  → chrome.developerPrivate.loadUnpacked  │
│   • enable toggle    → chrome.developerPrivate.setItemEnabled │
│   • Remove           → chrome.management.uninstall            │
└───────────────────────┬──────────────────────────────────────┘
                        │ existing IPC
┌───────────────────────▼──────────────────────────────────────┐
│ desktop_android_developer_private.cc (expanded)              │
│   loadUnpacked       → ExtensionInstallBridge::ShowFilePicker│
│   setItemEnabled     → ExtensionRegistrar::{Enable,Disable}  │
│   uninstall          → ExtensionRegistrar::RemoveExtension   │
│   reload             → ExtensionRegistrar::ReloadExtension   │
│   installDroppedFile → ExtensionInstaller::InstallFromFile   │
└───────────────────────────────────────────┬──────────────────┘
                                            │
              ┌─────────────────────────────▼─────────────────┐
              │ ExtensionInstaller  (new, ~400 LOC)           │
              │  • Detect .zip/.crx/dir by magic/suffix       │
              │  • Extract to profile/Extensions/<id>/        │
              │  • file_util::LoadExtension → Extension obj   │
              │  • DesktopAndroidExtensionSystem::AddExtension│
              └─────────────────▲─────────────────────────────┘
                                │
┌───────────────────────────────┼──────────────────────────────┐
│ Java: ExtensionInstallBridge  │ (new, ~150 LOC JNI)          │
│   • ACTION_OPEN_DOCUMENT file picker                         │
│   • Returns content:// URI to native via JNI callback        │
│   • Copies URI content to app cache dir (Android sandbox)    │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ WebstoreInstallThrottle  (new NavigationThrottle ~200 LOC)   │
│   On {chrome,chromewebstore}.google.com/*/detail/*/<id>/*    │
│     → show Android BottomSheet "Install <Ext>?"              │
│     → download clients2.google.com/service/update2/crx?id=…  │
│     → ExtensionInstaller::InstallFromFile                    │
└──────────────────────────────────────────────────────────────┘
```

## Components

### 1. ExtensionInstaller — core install/extract logic

**New:** `chrome/browser/extensions/desktop_android/extension_installer.{h,cc}`

```cpp
class ExtensionInstaller {
 public:
  using Callback = base::OnceCallback<void(
      scoped_refptr<const Extension>, const std::string& error)>;

  explicit ExtensionInstaller(content::BrowserContext* context);
  ~ExtensionInstaller();

  // `path` is a local file — .zip, .crx, or an unpacked directory.
  // Runs I/O on a thread pool worker; `cb` is posted back to the UI thread.
  void InstallFromFile(const base::FilePath& path, Callback cb);
};
```

Internal flow:

1. Inspect `path`:
   - Directory → skip to step 3 with `path` as the source dir.
   - `.zip` (or first 2 bytes = `PK`) → extract to temp dir (step 2).
   - `.crx` (first 4 bytes = `Cr24`) → strip the CRX3 header using
     `components/crx_file/crx_verifier.cc`'s `ReadCrxHeader` (header-only,
     no verification), feed the remaining byte range to the zip extractor
     (step 2).
2. Pick a stable extension id: parse `manifest.json` (a very small JSON
   read on the worker thread) to pull `key` or `public_key_hash`; fall
   back to `crx_file::id_util::GenerateId(manifest_content)`. Move the
   temp dir to `<profile>/Extensions/<id>/<version>/`.
3. On UI thread: `file_util::LoadExtension(dir, kCommandLine, flags, &err)`
   → `DesktopAndroidExtensionSystem::AddExtension(extension, err)`.
4. Report back via `Callback`.

Uses `//third_party/zlib/google:zip` (already linked through DNR).

### 2. Java file picker + JNI bridge

**New:** `chrome/browser/extensions/android/java/src/…/ExtensionInstallBridge.java`
and its C++ counterpart `chrome/browser/extensions/desktop_android/extension_install_bridge.{h,cc}`.

Java side (mirrors the `AppMenuBridge` pattern we already have for per-extension menu items):

```java
@JNINamespace("extensions::android")
public class ExtensionInstallBridge {
    @CalledByNative
    public static void showFilePicker(long nativeCallback) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT)
            .setType("*/*")
            .addCategory(Intent.CATEGORY_OPENABLE);
        // Launch via WindowAndroid.showIntent, then in the result callback
        // copy the content:// stream to a temp file under
        // context.getCacheDir()/extension_install/<uuid>.dat
        // and call ExtensionInstallBridgeJni.get().onFilePicked(nativeCallback, path)
        // or onFilePickerCancelled(nativeCallback).
    }
}
```

Native side exposes an async API used by the developerPrivate handler:

```cpp
class ExtensionInstallBridge {
 public:
  using PathCallback =
      base::OnceCallback<void(const base::FilePath& /* empty on cancel */)>;
  static void ShowFilePicker(PathCallback cb);
};
```

### 3. developerPrivate handler expansion

Edit existing `desktop_android_developer_private.{h,cc}`. Replace these
NoOps with real implementations:

| Handler | Implementation |
|---|---|
| `loadUnpacked` | Call `ExtensionInstallBridge::ShowFilePicker`. On cancel → respond `NoArguments()`. On pick → `ExtensionInstaller::InstallFromFile`, respond with `LoadError` on failure or empty success. |
| `setItemEnabled` | Pull `{id, enabled}` from args. Resolve `ExtensionSystem::Get(ctx)->extension_service()` path is gated, so use `ExtensionRegistrar::Get(ctx)` directly (still to confirm this static accessor exists on 132; if not, get registrar off `DesktopAndroidExtensionSystem`). Call `EnableExtension(id)` or `DisableExtension(id, disable_reason::USER_ACTION)`. |
| `uninstall` | `ExtensionRegistrar::RemoveExtension(id, UNLOAD_REASON_UNINSTALL)`. Then delete `<profile>/Extensions/<id>/`. |
| `reload` | `ExtensionRegistrar::ReloadExtension(id)`. If the method requires a `LoadErrorBehavior` enum, pass `kNoisy`. |
| `installDroppedFile` | Same as `loadUnpacked` but the filename is already in args. |

All of these are already listed in the API schema JSON we compiled in
v0.5; only the browser-side `ExtensionFunction` classes need to switch
from NoOp to real. Registration in `RegisterDesktopAndroidDeveloperPrivateFunctions`
stays identical.

### 4. WebstoreInstallThrottle

**New:** `chrome/browser/extensions/desktop_android/webstore_install_throttle.{h,cc}`.

Registered in `ChromeContentBrowserClient::CreateThrottlesForNavigation`
(already overlayed for desktop-android). Fires on main-frame navigations
where the host matches `chromewebstore.google.com` or
`chrome.google.com` AND the path matches
`/(webstore/)?detail/[^/]+/([a-p]{32})/?.*` — captures the extension id.

Flow:

1. `WillStartRequest` → let the navigation proceed (so user sees the
   store page).
2. Once the page's title loads, spawn a bottom-sheet using
   `BottomSheetControllerFactory` from the tab's `WebContents`. Sheet
   body: "Установить <extension name>?" with an "Установить" button.
   Name comes from `<title>` on the store page (crude, but renders
   without needing store JS).
3. On tap: download
   `https://clients2.google.com/service/update2/crx?response=redirect&prodversion=132.0&acceptformat=crx2,crx3&x=id%3D<id>%26uc`
   with `network::SimpleURLLoader`. Follow redirects to the raw `.crx`.
4. Save into `cache_dir/extension_install/<id>.crx`.
5. `ExtensionInstaller::InstallFromFile` → toast result.

If the user navigates away before tapping Install, the sheet closes.
No modal blocking.

### 5. Persistence: reload installed extensions on restart

Edit `chrome/browser/extensions/desktop_android/desktop_android_extension_system.cc`.

Today `InitForRegularProfile` only processes `--load-extension` and never
revisits `ExtensionPrefs`. After the existing command-line block, add:

```cpp
ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
for (const auto& [id, pref] : *prefs->GetInstalledExtensionsInfoPrefs()) {
  base::FilePath dir = prefs->GetExtensionPath(id);
  if (dir.empty() || IsAlreadyLoaded(id)) continue;
  std::string error;
  auto ext = file_util::LoadExtension(dir, prefs->GetInstalledExtensionLocation(id),
                                      Extension::NO_FLAGS, &error);
  if (ext) AddExtension(std::move(ext), error);
}
```

(exact API names are approximate — confirm during implementation).

### 6. BUILD wiring

Edit `chrome/browser/BUILD.gn`'s `enable_desktop_android_extensions`
block to add:

```
"extensions/desktop_android/extension_installer.cc",
"extensions/desktop_android/extension_installer.h",
"extensions/desktop_android/extension_install_bridge.cc",
"extensions/desktop_android/extension_install_bridge.h",
"extensions/desktop_android/webstore_install_throttle.cc",
"extensions/desktop_android/webstore_install_throttle.h",
```

`deps += [ "//components/crx_file", "//third_party/zlib/google:zip" ]`.

For the Java bridge, add a file under `chrome/browser/extensions/android/`
and wire `generate_jni` like the existing `AppMenuBridge`.

## Phasing

Implement bottom-up, build-and-test after each phase:

1. **Phase 1 — enable/disable/uninstall/reload**
   - Pure C++ in `desktop_android_developer_private.cc`.
   - No Java, no new files. Just replace NoOp stubs.
   - Build, test: install via `--load-extension=ublock`, toggle in
     UI, confirm registry state via DevTools.
2. **Phase 2 — ExtensionInstaller (InstallFromFile, no picker yet)**
   - Pure C++, no UI. Triggered from tests or a debug command-line
     switch `--install-extension=<path>`.
   - Build, test: drop a `.zip` or `.crx` on disk, launch with switch,
     confirm it appears.
3. **Phase 3 — Java file picker + JNI, wire loadUnpacked**
   - Now the "Load unpacked" button works end-to-end.
4. **Phase 4 — Persistence on restart**
   - Installed extensions survive app relaunch without re-picking.
5. **Phase 5 — WebstoreInstallThrottle + bottom sheet**
   - Store install flow.

## Risks

| Risk | Mitigation |
|---|---|
| `ExtensionRegistrar::EnableExtension` may be protected or missing on 132. | Fallback: `ExtensionRegistry::AddEnabled` + dispatch `EXTENSION_ENABLED` event manually. Verify during phase 1. |
| `ExtensionPrefs` doesn't expose an iteration API without `ExtensionService`. | Fallback: walk `<profile>/Extensions/` directory directly. |
| `BottomSheetController` may not be reachable from the throttle (lives on Android tab layer). | Fallback: use `InfoBar` or a minimal custom PopupWindow. |
| Store page detects mobile UA and refuses to render. | Sheet appears before page JS runs — install works even if the page itself is broken. |
| CRX3 header format drift. | Use `components/crx_file` helper; it's versioned with Chromium. |

## Success criteria

- From chrome://extensions: "Load unpacked" opens a file picker,
  pick a `.zip`, the extension appears in the list and
  `chrome.developerPrivate.getExtensionsInfo` returns it.
- Toggle on each item flips enabled state (verified via DevTools +
  page reload).
- "Remove" on an item removes it from the registry AND from disk.
- Browser restart: previously-installed extensions are still present.
- Navigate to a Chrome Web Store detail page, confirm bottom sheet,
  tap Install → extension appears in list.

## Files added / edited (target counts)

- **New**: 6 C++ files, 1 Java file, 1 BUILD edit. ~900 LOC.
- **Edited**: `desktop_android_developer_private.cc` (replace 4 NoOps
  with real impls, ~200 LOC), `desktop_android_extension_system.cc`
  (~30 LOC for restart persistence), `chrome/browser/BUILD.gn`.
- **No**: WebUI patches.
