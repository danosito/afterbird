# Afterbird extension API coverage

Snapshot of what `chrome.*` extension APIs are implemented, stubbed, or
intentionally out of scope on the desktop-android build. Keep this in sync
with `chrome/browser/extensions/desktop_android/` when APIs land or change
status.

Status legend:

- `full` — backed by a real implementation that does what upstream does.
- `stub-ok` — responds with a plausible shape so extensions don't crash, but
  without the real side effect. Extensions that check for the API's existence
  and then do something useful with the returned data mostly still work.
- `schema-only` — the IDL is compiled (JS binding exists), but no
  `ExtensionFunction` is registered. Calls log `Unknown Extension API` and
  resolve with `Access to extension API denied.`
- `unavailable` — the IDL itself isn't compiled; `chrome.<ns>` is `undefined`.
- `infeasible` — fundamentally can't be supported on the current Android
  surface. See per-row rationale.

## Implemented

| API | Status | Notes |
| --- | --- | --- |
| `chrome.alarms` | full | Core extensions/browser API; works. |
| `chrome.declarativeNetRequest` | full | Rulesets compile; dynamic rules persist. Tested with the probe. |
| `chrome.i18n` | full | `getUILanguage`, `getMessage`, `getAcceptLanguages` all work against the app locale. |
| `chrome.idle` | full | Upstream core implementation. |
| `chrome.runtime.getManifest/id` | full | |
| `chrome.runtime.sendMessage` / `connect` (cross-extension) | full (since v0.7) | MessageService was null-stubbed pre-v0.7; now routes through the real service. Self-echo (SW → own SW) still fails because the service worker isn't treated as its own listener. |
| `chrome.storage.local` | full | |
| `chrome.storage.session` (MV3) | full | |
| `chrome.contextMenus.create` | stub-ok (moved to Stubbed section in v1.5) | The prior "full" label was inaccurate — no `ContextMenusCreateFunction` was registered on desktop-android; it logged `Unknown Extension API - contextMenus.create` (61 occurrences in the stability report). Now stubbed to accept the registration and return the id. No `onClicked` event ever fires. |
| `chrome.webRequest` (MV2) | full | Blocking listeners work; see probe. |
| `chrome.test` | full | Core extensions/browser API; present. |
| `chrome.events` | full | |
| `chrome.developerPrivate` | partial | Only the handlers chrome://extensions needs (`getExtensionsInfo`, `getProfileConfiguration`, `updateExtensionConfiguration`, `removeMultipleExtensions`, `reload`, `loadUnpacked`). Upstream has ~40 methods. Everything else returns "not implemented". |
| `chrome.webstorePrivate.beginInstallWithManifest3` | partial | Shim that hands off to `CrxInstallCoordinator`. Other webstorePrivate methods are stubs. |

## Stubbed (feature/api-coverage-v2, v1.5)

All live in `chrome/browser/extensions/desktop_android/desktop_android_stub_apis.{cc,h}`.
The intent of every stub is "return a valid-shape default so defensive
`.filter()`/`.map()`/`await` chains don't throw". None of these produce real
side effects. See `docs/superpowers/api-infeasible.md` for why each is
stubbed-not-implemented.

| API | Status | Rationale |
| --- | --- | --- |
| `chrome.permissions.getAll` | stub-ok | Reconstructs `{ permissions, origins }` from the extension's manifest. No runtime permission changes are tracked — granted-at-install is all we know. |
| `chrome.permissions.contains` | stub-ok | Tests against the same manifest-reconstructed set. False for any runtime-requested addition. |
| `chrome.commands.getAll` | stub-ok | Transcribes manifest `commands`. `shortcut` is always empty — desktop-android has no keyboard accelerator surface to bind them to. |
| `chrome.notifications.create` | stub-ok | Echoes or synthesises an id and logs; no actual toast. |
| `chrome.notifications.update/clear` | stub-ok (new in v1.5) | Return `true`. No-op; was "Access denied" pre-v1.5. |
| `chrome.notifications.getAll` | stub-ok (new in v1.5) | Returns `{}` (no active notifications). |
| `chrome.notifications.getPermissionLevel` | stub-ok (new in v1.5) | Returns `"granted"` so extensions don't short-circuit their setup. |
| `chrome.tabs.query` | stub-ok (new in v1.5) | **Critical fix.** Returns `[]`. Was returning `undefined` via the generic "denied" path, which broke `.filter()` chains in Bitwarden's BadgeService. |
| `chrome.tabs.create` | stub-ok (new in v1.5) | Returns a synthetic Tab dict with a millisecond-derived id. No tab is actually opened. |
| `chrome.tabs.remove/reload/goBack/goForward/ungroup/highlight` | stub-ok (new in v1.5) | No-op; shape-correct callback. |
| `chrome.tabs.update` | stub-ok (new in v1.5) | Returns a synthetic Tab dict. |
| `chrome.tabs.duplicate` | stub-ok (new in v1.5) | Returns a synthetic Tab with id+1. |
| `chrome.tabs.get` | stub-err (new in v1.5) | Returns an error ("Tab not found on desktop-android"). Extensions that check `lastError` behave correctly. |
| `chrome.tabs.getCurrent` | stub-ok (new in v1.5) | Returns `null` — matches the upstream semantics when called from a non-tab context (service worker). |
| `chrome.tabs.detectLanguage` | stub-ok (new in v1.5) | Returns `"und"` (undetermined). |
| `chrome.tabs.group` | stub-ok (new in v1.5) | Returns `-1`. |
| `chrome.tabs.discard` | stub-ok (new in v1.5) | Returns `null`. |
| `chrome.windows.getAll/get/getCurrent/getLastFocused/create/update` | stub-ok (new in v1.5) | Returns a single synthetic Window dict with id=0. |
| `chrome.windows.remove` | stub-ok (new in v1.5) | No-op. |
| `chrome.action.*` (MV3) | stub-ok (new in v1.5) | All setters are no-ops; `getTitle/getBadgeText/getPopup` return `""`; `getBadgeBackgroundColor` returns `[0,0,0,255]`. No per-extension action state storage yet. |
| `chrome.browserAction.*` (MV2) | stub-ok (new in v1.5) | Same shape as `action.*`. |
| `chrome.contextMenus.create` | stub-ok (new in v1.5) | Accepts the registration, returns the id (from `createProperties.id` or a synthetic numeric id). **No `onClicked` event ever fires** — there's no contextual-menu surface on Android. |
| `chrome.contextMenus.update/remove/removeAll` | stub-ok (new in v1.5) | No-op. |
| `chrome.cookies.get/set/remove` | stub-ok (new in v1.5) | Returns `null` (not found / no-op set). |
| `chrome.cookies.getAll` | stub-ok (new in v1.5) | Returns `[]`. |
| `chrome.cookies.getAllCookieStores` | stub-ok (new in v1.5) | Returns one synthetic store (`id: "0"`). |
| `chrome.types.ChromeSetting.get/set/clear` | stub-ok (new in v1.5) | `get` returns `{ value: null, levelOfControl: "not_controllable" }`; `set`/`clear` are no-ops. Used by `chrome.privacy.*`. |
| `chrome.extension.isAllowedIncognitoAccess` | stub-ok (new in v1.5) | Returns `false` (matches the actual state — incognito extension access isn't surfaced). |
| `chrome.extension.isAllowedFileSchemeAccess` | stub-ok (new in v1.5) | Returns `false`. |

## Feasible follow-ups (schema-only today, should become `full`)

These have IDL bindings compiled but no C++ function registrations, and
unlike the shape-only stubs above, would benefit from a *real* backing
implementation. See `docs/superpowers/api-infeasible.md` for the full
rationale and estimated effort for each.

| API | Priority | One-line plan |
| --- | --- | --- |
| `chrome.tabs.*` (real backing) | high | Bridge `TabModelSelector` ↔ extension Tab dicts; see api-infeasible.md. Currently shape-only stubs. |
| `chrome.scripting.executeScript / insertCSS` | high | Wire `ScriptExecutor` to `DesktopAndroidExtensionWebContentsObserver`. ~200 lines. Currently schema-only → unknown API denial. |
| `chrome.cookies.*` (real backing) | medium | Plumb `StoragePartition::GetCookieManagerForBrowserProcess` through the extension permission gating. Currently shape-only stubs (`[]`/`null`). |
| `chrome.action.*` state storage (MV3) | medium | Per-extension state store so getters returning setters' values round-trip. Currently stateless stubs. |
| `chrome.browserAction.*` state storage (MV2) | medium | Same implementation as action.*. |
| `chrome.webNavigation` (real events) | medium | Clean `WebContentsObserver`-based impl upstream. Currently schema-only; listener registration works via bindings layer but events don't fire. |
| `chrome.history` / `chrome.bookmarks` | medium each | Android has the stores; extension adapters are desktop-only. |
| `chrome.downloads` | medium | Android `DownloadManager` exists; adapter is desktop-only. |

## Likely infeasible on this build (at least near-term)

| API | Why |
| --- | --- |
| `chrome.storage.sync` | No Google account sync on desktop-android build. `ExtensionSyncService` is compiled out. Would need wire-up to Android's Backup/Sync, which is not in Afterbird's scope. |
| `chrome.management.getAll / setEnabled / ...` | `management.json` schema is explicitly excluded in `extensions/common/api/schema.gni` because the `ManagementGet*` function implementations aren't compiled for desktop-android and cause link errors. We already bridge the *UI* calls (chrome://extensions) through `developerPrivate.*` instead; giving extensions the full `management` surface would require recompiling that whole tree. |
| `chrome.fontSettings` | Android per-site font controls aren't a thing; there's no `FontPrefs` equivalent on this build. Extensions that call this see a `GetAPISchema miss` warning and a denied function call. |
| `chrome.privacy.*` | Relies on `prefs::` keys that only exist in the desktop build. |
| `chrome.bluetooth*`, `chrome.usb`, `chrome.hid`, `chrome.serial`, `chrome.printerProvider`, `chrome.system.*` | Removed from `extensions_api_schema_files_` by `schema.gni` — they're chromeos/desktop-only. Re-enabling means pulling large device-access stacks into desktop-android. |
| `chrome.debugger` | Exposes the devtools protocol to other extensions; gated on `enable_extensions` (full desktop). Not a good fit on Android where DevTools is over ADB. |
| `chrome.identity` / OAuth flows | No Sign-In Services plumbing on desktop-android profile. |
| `chrome.omnibox` | Upstream needs the `OmniboxController` which Android replaces with a completely different `UrlBarCoordinator`. A port is possible but would be a whole feature, not a stub. |
| `chrome.commands` keyboard shortcut *routing* | Schema exists, `getAll` works (see stubs). Actually wiring up `Alt+Shift+X`-style extension shortcuts requires a new `KeyboardShortcutDelegate` on the Android side — our `ChromeActivity` doesn't route custom accelerators to extensions. Treated as infeasible until someone needs it. |

## Source of truth

- IDL inclusion: `extensions/common/api/schema.gni` + `chrome/common/extensions/api/api_sources.gni`.
- Registration of browser-side functions: `DesktopAndroidExtensionsBrowserClient::RegisterExtensionFunctions` (via the Afterbird `AfterbirdChromeExtensionsBrowserAPIProvider`).
- Error path when a function is missing: `extensions/browser/extension_function_dispatcher.cc` — logs `Unknown Extension API - <name>` and responds with `Access to extension API denied.`
