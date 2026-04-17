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
| `chrome.contextMenus.create` | full (registration only) | The create call returns the id; the resulting menu entry is NOT surfaced anywhere because Android has no desktop context menu. Extensions that merely *register* menus to fan out to their own logic via `onClicked` are happy — nothing ever invokes them. |
| `chrome.webRequest` (MV2) | full | Blocking listeners work; see probe. |
| `chrome.test` | full | Core extensions/browser API; present. |
| `chrome.events` | full | |
| `chrome.developerPrivate` | partial | Only the handlers chrome://extensions needs (`getExtensionsInfo`, `getProfileConfiguration`, `updateExtensionConfiguration`, `removeMultipleExtensions`, `reload`, `loadUnpacked`). Upstream has ~40 methods. Everything else returns "not implemented". |
| `chrome.webstorePrivate.beginInstallWithManifest3` | partial | Shim that hands off to `CrxInstallCoordinator`. Other webstorePrivate methods are stubs. |

## Stubbed (this branch, v1.4)

All live in `chrome/browser/extensions/desktop_android/desktop_android_stub_apis.{cc,h}`.

| API | Status | Rationale |
| --- | --- | --- |
| `chrome.permissions.getAll` | stub-ok | Reconstructs `{ permissions, origins }` from the extension's manifest. No runtime permission changes are tracked — granted-at-install is all we know. |
| `chrome.permissions.contains` | stub-ok | Tests against the same manifest-reconstructed set. False for any runtime-requested addition. |
| `chrome.commands.getAll` | stub-ok | Transcribes manifest `commands`. `shortcut` is always empty — desktop-android has no keyboard accelerator surface to bind them to. |
| `chrome.notifications.create` | stub-ok | Echoes or synthesises an id and logs; no actual toast. `update`/`clear`/`getAll`/`getPermissionLevel` are not yet registered, so callers of those still see "Access to extension API denied". |

## Feasible follow-ups (schema-only today, should become `full`/`stub-ok`)

These have IDL bindings compiled but no C++ function registrations. Adding them is mechanical once we decide on the Android-side surface.

| API | Difficulty | Plan |
| --- | --- | --- |
| `chrome.tabs.query / create / remove / update` | medium | Need a `TabAndroid` → extension Tab dict mapper and a way to poke `TabModelSelector` from a non-UI thread (tab_strip_model_observer equivalent). Once in place, `tabs.onCreated/onRemoved/onActivated` events can be fired directly. Dark Reader and uBO both call `tabs.query` at startup; unblocking these unblocks a lot. |
| `chrome.tabs.executeScript` (MV2) / `chrome.scripting.executeScript` (MV3) | medium | Requires a `ScriptExecutor` hookup per `WebContents`. Upstream `extensions/browser/script_executor.cc` exists but isn't reached because `WebContentsUserData` isn't bound to our `DesktopAndroidExtensionWebContentsObserver`. Once wired, most "modifies the page" extensions work. |
| `chrome.action.setBadgeText / setIcon / setTitle / setPopup` (MV3) | small–medium | Currently denied. Needs per-extension toolbar state storage (we already drop their pinned-to-toolbar UI, but the state struct is ~20 getters/setters). No actual toolbar-icon rendering yet; state would be stored and read-back from the same getters. |
| `chrome.browserAction.*` (MV2) | same as `chrome.action` | Same implementation shape as `action`; the two can share a helper class. `browserAction.setBadgeText` already works (present via CoreExtensionsBrowserAPIProvider? verify) — `setIcon` does not. |
| `chrome.notifications.update / clear / getAll / getPermissionLevel` | small | Pairs with the existing `create` stub — all four should move together. |
| `chrome.webNavigation` | medium | Has its own IDL and a clean `WebContentsObserver`-based implementation upstream. Mostly compiles out of the box if enabled. |
| `chrome.cookies` | medium | Schema compiled (MV3 sources.gni lists it). No implementation. Would need a `CookieService` bridge on desktop-android. |
| `chrome.history` / `chrome.bookmarks` / `chrome.downloads` | medium each | Android has history/bookmarks/downloads databases but the extension-side adapters are chrome-desktop-only. |

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
