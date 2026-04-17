# Afterbird API Probe

Two tiny extensions that exercise a range of `chrome.*` APIs and report which
ones are available, work, or fail on the current browser build. Intended for
quickly assessing extension compatibility on Afterbird.

Ships in two flavors:

- `mv2/` - Manifest V2 (uses `browser_action`, `webRequest`, background page)
- `mv3/` - Manifest V3 (uses `action`, `scripting`, `storage.session`, service worker)

Both variants expose the same popup UI: a table of probed APIs with
`pass` / `fail` / `unavailable` rows and a "Re-run probes" button.

## Load

On Afterbird (Android):

1. Open `chrome://extensions`.
2. Tap "Load unpacked".
3. Use the Android file picker to select either the `mv2/` or `mv3/` folder
   (or a zip of it).
4. Tap the extension's toolbar icon to open the popup and review results.

You can install both variants side-by-side to compare.

## What each variant probes

Shared (both MV2 and MV3):

- `chrome.runtime.id`
- `chrome.runtime.getManifest()`
- `chrome.runtime.onMessage` round-trip (send + echo)
- `chrome.runtime.connect` port round-trip
- `chrome.storage.local.set/get/remove`
- `chrome.storage.sync.set/get`
- `chrome.tabs.query({})`
- `chrome.tabs.create` then `chrome.tabs.remove`
- `chrome.alarms.create` + `onAlarm` fires
- `chrome.contextMenus.create`
- `chrome.declarativeNetRequest.updateDynamicRules`
- `chrome.commands.getAll()`
- `chrome.permissions.getAll()`
- `chrome.i18n.getUILanguage()`
- `chrome.i18n.getMessage("extName")`
- `chrome.notifications.create`

MV2 only:

- `chrome.browserAction.setBadgeText`
- `chrome.browserAction.setIcon` (via a canvas-generated `ImageData`)
- `chrome.webRequest.onBeforeRequest.addListener`
- `chrome.scripting` (expected `unavailable` - MV3-only)
- `chrome.storage.session` (expected `unavailable` - MV3-only)

MV3 only:

- `chrome.storage.session.set/get`
- `chrome.action.setBadgeText`
- `chrome.action.setIcon` (via `OffscreenCanvas` `ImageData`)
- `chrome.scripting.executeScript` (against the active tab)
- `chrome.webRequest.onBeforeRequest` (expected `unavailable` - MV2-only)

Each probe is independent - a failure in one doesn't stop the rest.

## Interpreting results

- `pass`         - API is present and the call completed without error.
- `fail`         - API is present but threw or returned an unexpected result.
- `unavailable`  - The `chrome.*` path does not exist on this build.
- `n/a`          - Probe intentionally skipped (not meaningful in this context).

The `note` column shows a short message (returned value, error text, etc.) to
help distinguish "stub that returns nothing" from "truly missing".
