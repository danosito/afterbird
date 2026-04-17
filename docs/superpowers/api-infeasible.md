# Afterbird extension APIs that are infeasible (or deferred) on desktop-android

Companion to `docs/superpowers/api-coverage.md`. This file lists every
`chrome.*` API that is **not** implemented on the current desktop-android
build, along with a specific reason and a sketch of what a real
implementation would require. Items here should be re-evaluated any time
Chromium's desktop-android port gains a new subsystem; some "infeasibles"
are really just "nobody has done the porting work yet".

Categories:

- **platform-gap** — the Android surface lacks the subsystem the API needs
  (omnibox controller, extension toolbar, contextual menu surface, etc.).
- **missing-infra** — the C++ backing code exists upstream but isn't compiled
  in on desktop-android because it depends on `enable_extensions=true` or
  another gated tree.
- **dead-end** — the API is tied to ChromeOS/Ash-specific or desktop-only
  peripheral stacks that Afterbird won't ever ship.
- **deferred** — technically feasible but non-trivial; owner can pick this
  up when an extension asks for it.

Status of each entry is relative to the `feature/api-coverage-v2` branch
(v1.5-rc3 APK, 2026-04-17).

---

## Tabs, scripting, and per-page APIs

### chrome.tabs.* (get, query, create, remove, update, ...)

- **Status:** stubbed shape-only. `query` returns `[]`, `create` returns a
  synthetic Tab dict with no real side effect, `get` returns an error.
- **Category:** missing-infra (leaning deferred).
- **What a real impl needs:** a mapping layer between Chromium's
  `extensions::WindowController`/`TabStripModel` abstractions and Android's
  `TabModelSelector` / `TabAndroid`. The extensions subsystem wants to
  enumerate tabs across windows, subscribe to `tab_strip_model_observer`,
  and poke at a tab's `WebContents`. All of this is feasible — Android's
  `TabModelSelector` is observable — but it's a ~500-line bridge that
  also needs careful threading (extensions run on the UI thread;
  `TabModelSelector` lives on the Android UI thread, which is the same
  thread but access patterns differ).
- **Who benefits:** Dark Reader (per-tab theme state), uBlock Origin
  (per-tab badge counter), any extension that fans out work by tab.
- **Priority:** high — this is the single biggest gap.

### chrome.scripting.executeScript / insertCSS / removeCSS (MV3)

- **Status:** schema compiled; no C++ function registered.
- **Category:** missing-infra.
- **What a real impl needs:** `chrome/browser/extensions/api/scripting/
  scripting_api.cc` exists and is reachable — what's missing is the
  per-`WebContents` `ScriptExecutor` hookup. Specifically,
  `extensions::ScriptExecutor` is created from
  `ExtensionWebContentsObserver::CreateForWebContents` on desktop builds
  but `DesktopAndroidExtensionWebContentsObserver` doesn't own one. Add
  an owned `ScriptExecutor` member + a getter + register the
  scripting API functions → should unblock most "content script injection"
  use cases. Estimate: ~200 lines.
- **Who benefits:** almost every extension that modifies page content.
- **Priority:** high.

### chrome.tabs.executeScript (MV2)

- Same situation as `chrome.scripting.executeScript`. Shared solution.

---

## Toolbar / action surface

### chrome.action.setIcon, setBadgeText, setTitle, setPopup (MV3)

### chrome.browserAction.* (MV2)

- **Status:** stubbed as no-op setters + identity getters.
- **Category:** platform-gap for the visual effect, missing-infra for the
  state storage.
- **What a real impl needs:** an Android toolbar-icon surface. Afterbird
  currently drops the pinned-to-toolbar UI entirely. A real
  implementation needs: (a) a per-extension `ExtensionAction` state store
  (mirroring `chrome/browser/extensions/extension_action_runtime.cc`);
  (b) an Android Views/Compose element that renders the badge text/color
  on a menu entry or top-bar chip; (c) the `onClicked` dispatch path. The
  store part is cheap (~200 lines) and even alone lets extensions
  round-trip their state; the rendering is a whole UI feature.
- **Priority:** medium — most extensions function without it; some
  (Bitwarden, uBlock) clearly *want* to show a count but don't crash
  without it.

---

## Keyboard / shortcut APIs

### chrome.commands (shortcut routing, not getAll)

- **Status:** `getAll` stubbed (returns the manifest's commands dict with
  empty shortcuts). Actual shortcut routing is absent.
- **Category:** platform-gap.
- **Reason:** Chrome's extension accelerator routing is done in
  `KeyboardShortcutDelegate` which reaches into the browser's command
  dispatcher. Android's `ChromeActivity` doesn't have an equivalent
  dispatcher chain for extension-registered shortcuts. A port would need
  to intercept `dispatchKeyEvent` at the activity level and route to
  an Android-specific shortcut service that talks to the extension
  event router.
- **Priority:** low — most Android users don't have a keyboard.

---

## Privacy, content settings, browsing data

### chrome.privacy.* (services, network, websites)

- **Status:** routed through the generic `types.ChromeSetting` stub which
  returns `{ value: null, levelOfControl: "not_controllable" }` for every
  get.
- **Category:** missing-infra.
- **Reason:** `chrome.privacy` resolves each sub-setting against a
  `PrefService` key (e.g. `prefs::kNetworkPredictionOptions`). Several
  of those pref keys aren't compiled in on desktop-android because the
  desktop pref scaffolding isn't linked. Reaching per-pref feasibility
  means auditing each pref: which exist on the Android profile, which
  don't, and wiring a PrefService-backed implementation that gates on
  existence.
- **Priority:** low — extensions that set privacy prefs are mostly
  enterprise-oriented and don't run on mobile.

### chrome.contentSettings

- **Status:** schema present (`content_settings.json` is compiled on
  desktop-android); no C++ registrations.
- **Category:** missing-infra.
- **Reason:** `ContentSettingsService` exists on Android (Site Settings
  UI is built on it) but the extension-API adapter is desktop-only. A
  port would use `HostContentSettingsMapFactory::GetForProfile` and map
  each ContentSettingsType to the extension schema's patterns.
- **Priority:** low.

### chrome.browsingData.*

- **Status:** schema present (`browsing_data.json`); no registrations.
- **Category:** missing-infra.
- **Reason:** Upstream `BrowsingDataRemover` is available but the
  extension-side wrappers pull in `ChromeBrowsingDataRemoverDelegate`
  which has desktop-only observers.
- **Priority:** low.

---

## OAuth / account / identity APIs

### chrome.identity.getAuthToken / launchWebAuthFlow / getProfileUserInfo

- **Status:** schema not compiled; `chrome.identity` is `undefined`.
- **Category:** platform-gap + dead-end-ish.
- **Reason:** Relies on `IdentityManager`/`SigninClient`; Afterbird's
  desktop-android profile doesn't run the SigninManager setup
  (`simple_factory_key_map` registers no sign-in services). Porting
  requires deciding whether Afterbird does Google sign-in at all — it
  currently doesn't.
- **Priority:** declined (out of scope for a non-Google-signed browser).

---

## Sync

### chrome.storage.sync

- **Status:** schema compiled; no backing service. Calls succeed at the
  binding layer but writes go nowhere and reads return the last local
  value from the same (non-synced) backend.
- **Category:** missing-infra + dead-end.
- **Reason:** `ExtensionSyncService` is compiled out on desktop-android.
  The whole "sync" pipeline (encryption, DataTypeController, model type
  registration) would have to be enabled, which is a cross-cutting
  feature separate from extensions.
- **Priority:** declined — Afterbird has no sync backend.

---

## Device access

### chrome.bluetooth*, chrome.usb, chrome.hid, chrome.serial

- **Status:** schemas excluded in `extensions/common/api/schema.gni`;
  these namespaces don't exist at all on desktop-android.
- **Category:** dead-end.
- **Reason:** Wire-level device stacks on Android are a different beast
  (Bluetooth via `android.bluetooth.*`, USB via `UsbManager`). The
  extension surface was designed for desktop device managers. A port is
  possible but is a whole feature, not a stub.
- **Priority:** declined.

### chrome.system.cpu / system.memory / system.storage / system.display / system.network

- **Status:** schemas excluded.
- **Category:** dead-end / deferred.
- **Reason:** Same schema.gni exclusion as above. Of these, `system.memory`
  is cheap to stub but hasn't been asked for.
- **Priority:** declined.

### chrome.printerProvider

- **Status:** excluded.
- **Category:** dead-end.
- **Reason:** ChromeOS-specific; desktop-android wouldn't use this even if
  enabled.

---

## Debugger / devtools integration

### chrome.debugger.attach / sendCommand / detach

- **Status:** schema not compiled.
- **Category:** platform-gap + intentional.
- **Reason:** `chrome.debugger` exposes the DevTools protocol to other
  extensions. Afterbird v1.5 has on-device DevTools but it's reachable
  over loopback, not via the extension `debugger` API. Exposing it
  adds an attack surface and breaks the loopback-only security model.
- **Priority:** declined.

---

## Omnibox

### chrome.omnibox.setDefaultSuggestion / onInputChanged / onInputEntered

- **Status:** schema not compiled.
- **Category:** platform-gap.
- **Reason:** Upstream needs `OmniboxController`. Android replaces that
  with `UrlBarCoordinator` — a completely different component with a
  different API shape. Porting means writing an Android-specific bridge
  (~400 lines) that funnels keyword triggers into the UrlBar.
- **Priority:** medium — a clean feature request, not coupled to any
  extension that's been tested.

---

## Management

### chrome.management.getAll / setEnabled / uninstall / getPermissionWarningsById / ...

- **Status:** schema explicitly excluded in
  `extensions/common/api/schema.gni` (because the `ManagementGet*`
  implementations would cause link errors without the full extensions
  framework).
- **Category:** missing-infra (intentional).
- **Reason:** The `ManagementGet*` function implementations live in
  `chrome/browser/extensions/api/management/` and drag in
  `ExtensionSyncService`, `LaunchPackagedApp`, etc. All of that has been
  compiled out for desktop-android. We already bridge the UI paths
  (chrome://extensions) through `developerPrivate` so the *user* can
  manage extensions; giving extensions the full management surface would
  require recompiling a large tree.
- **Priority:** declined; use `developerPrivate` where internal chrome://
  pages need it.

---

## Font / accessibility settings

### chrome.fontSettings

- **Status:** schema not compiled; `chrome.fontSettings` is `undefined`.
- **Category:** dead-end.
- **Reason:** Android's WebView/blink uses different FontService plumbing
  — per-site font preferences aren't a concept on mobile (Chrome Android
  doesn't expose this UI either).
- **Priority:** declined.

### chrome.accessibilityFeatures

- **Status:** schema compiled only when `enable_extensions=true`. Not on
  desktop-android.
- **Category:** dead-end.
- **Reason:** ChromeOS-centric settings (spoken feedback, screen
  magnifier, etc.). Android has its own accessibility pipeline.
- **Priority:** declined.

---

## Downloads / history / bookmarks / reading list

### chrome.downloads.download / search / cancel / pause / resume

- **Status:** schema compiled only under `enable_extensions=true`; not
  compiled on desktop-android.
- **Category:** missing-infra (deferred).
- **Reason:** Android has a `DownloadManager`-backed pipeline, but the
  extension API wrapper (`chrome/browser/extensions/api/downloads/`) is
  wired to `DownloadCoreService` which has desktop-only dependencies.
  Porting means creating an Android bridge for each of the ~12 download
  methods.
- **Priority:** medium.

### chrome.history.search / addUrl / deleteUrl / deleteAll

- **Status:** schema compiled (`history.json` is in `api_sources.gni` for
  desktop-android); no C++ registrations.
- **Category:** missing-infra (deferred).
- **Reason:** Android has a history database but the adapter
  (`chrome/browser/extensions/api/history/history_api.cc`) isn't
  compiled in.
- **Priority:** medium — extensions want history for features like
  "find the last tab I closed" (Dark Reader doesn't need it).

### chrome.bookmarks.create / update / remove / getTree / search

- **Status:** schema compiled; no registrations.
- **Category:** missing-infra.
- **Reason:** Similar to history — Android has a bookmarks store but the
  extensions wrapper isn't compiled. Porting is mostly mechanical.
- **Priority:** medium.

---

## Cookies

### chrome.cookies.get / getAll / set / remove / getAllCookieStores

- **Status:** stubbed: `getAll` returns `[]`, `get`/`set`/`remove` return
  null, `getAllCookieStores` returns one synthetic store.
- **Category:** missing-infra.
- **Reason:** Upstream `CookiesAPI` wires to `CookieManager` via
  `StoragePartition::GetCookieManagerForBrowserProcess`. This would work
  on desktop-android (the network service is live, `CookieManager` is
  reachable) — the blocker is porting the ~400 lines of permission
  gating, host-access checks, and the Mojo observer that routes
  `onChanged` events. Not structural, just tedious.
- **Priority:** medium — Honey and Bitwarden both call `cookies.getAll`
  on startup.

---

## Web APIs that aren't on desktop-android

### chrome.pageCapture.saveAsMHTML

- Schema only compiled under `enable_extensions=true`. Not on
  desktop-android.
- Platform-gap: MHTML capture depends on a desktop-only serializer.

### chrome.tabCapture.*

- Same story: only compiled with `enable_extensions=true`. MediaRouter and
  the Cast integration aren't wired on desktop-android.

### chrome.desktopCapture.chooseDesktopMedia

- Depends on a desktop window picker. Declined.

### chrome.webAuthenticationProxy.*

- Schema only compiled with `enable_extensions=true`. Declined.

---

## Store / extensions / developer APIs

### chrome.management

- See "Management" above. Declined.

### chrome.webstore.install (deprecated)

- Not implemented. Declined — uses the deprecated inline-install flow.

### chrome.webstorePrivate.*

- Only `beginInstallWithManifest3` is implemented as a shim for the CRX
  install coordinator. The other ~15 methods (getStoreLogin,
  getBrowserLogin, setStoreLogin, completeInstall, etc.) are stubs or
  missing. This is intentional — the UI flows that use them (Chrome
  Web Store page decorations, account-aware install) don't run on
  desktop-android.

### chrome.developerPrivate.*

- Partial. Only the methods chrome://extensions calls (`getExtensionsInfo`,
  `updateExtensionConfiguration`, `removeMultipleExtensions`, `reload`,
  `loadUnpacked`) are implemented. Other ~35 methods return "not
  implemented". Not expected to be called by real extensions.

---

## Infrastructure bug: store install rewrites extension IDs (P2)

This is **not an API** but a plumbing bug uncovered in the v1.5 stability
audit. Documented here because it has API-visible effects and because the
fix is medium-complexity:

- **Symptom:** a CRX downloaded from the Chrome Web Store is installed
  under an id derived from the CRX's own public key, not the CWS-registered
  id. Bitwarden: `nngceckbapebfimnlniiiahkandclblb` → installed as
  `fciinoccdmhaldfhgabcoaemoehngcnh`. Honey:
  `bmnlcjabgnpnenekpadlanbbkooimhnj` → `inapljdcnilpkcgegehaolccnnlkclfk`.
- **API impact:** `chrome.runtime.id` returns a different id from what the
  CWS listing shows; any state keyed by the CWS id (hosted login URLs,
  server-side dedup) doesn't match.
- **Root cause (unconfirmed, but strong evidence):** in
  `chrome/browser/extensions/desktop_android/crx_install_coordinator.cc`,
  the `StartFromWebstore` / `StartFromFile` paths do not plumb the
  `key` field from the CWS manifest response into the CRX install. The
  install uses the CRX header's public key to derive an id instead of
  pinning to the author-registered key. Upstream solves this via
  `CrxInstaller::set_public_key()` / `set_expected_id()`.
- **What a real fix needs:**
  1. Capture the CWS-declared id (the 32-char a-p string in the CWS URL)
     before download starts.
  2. Thread that id into `CrxInstallCoordinator::StartFromWebstore` as an
     `expected_id`.
  3. Override the local id computation so the CRX is installed under the
     expected id, not the computed one.
  4. Verify CRX signature against the expected id's public key (possibly
     out of scope for a first pass).
- **Priority:** P2. Not blocking extensions from running; only breaks
  dedup of "already installed" state and any `runtime.id`-based server
  checks.
- **Deferred:** This is not in scope for `feature/api-coverage-v2`; it's
  filed as a follow-up. Needs a small design doc + test.

---

## Source of truth

- IDL inclusion: `extensions/common/api/schema.gni` +
  `chrome/common/extensions/api/api_sources.gni`
- Registration of browser-side functions:
  `DesktopAndroidExtensionsBrowserClient::RegisterExtensionFunctions` (via
  `AfterbirdChromeExtensionsBrowserAPIProvider` →
  `RegisterDesktopAndroidStubApiFunctions`).
- Error path when a function is missing:
  `extensions/browser/extension_function_dispatcher.cc` — logs
  `Unknown Extension API - <name>` and responds with
  `Access to extension API denied.`
