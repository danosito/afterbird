# Afterbird v1.4 — API Probe + Heavy-Extension Compatibility Report

Date: 2026-04-17
Device: `emulator-5554` (Android 15, arm64, API 35)
Base build tested: `v1.3` (Chromium 132.0.6834.83, enable_extensions=false)
Patched build tested: `v1.4` WIP (this branch, `feature/heavy-ext-and-install`)

## 1. What the run covered

1. Loaded the MV2 + MV3 probe extensions under `third_party/extensions/afterbird-api-probe/` via `--load-extension` (equivalent to chrome://extensions → Load Unpacked).
2. Installed uBlock Origin + Dark Reader through the v1.3 CWS detail-page confirm flow.
3. Drove every probe row from background pages / service workers over the CDP WebSocket — DevTools was opened from the host via `adb forward tcp:9222 localabstract:chrome_devtools_remote` and a small Python harness at `/tmp/cdp.py`.
4. Verified the effect of the stubs landed in this branch by reinstalling the new APK on the same emulator and re-running.

## 2. Probe results

### MV3 (opnbnelkeapdlpnhblhidkbaojfgccjb)

| Probe | v1.3 | v1.4 (this branch) |
| --- | --- | --- |
| `chrome.runtime.id` | pass | pass |
| `chrome.runtime.getManifest()` | pass | pass |
| `chrome.runtime.onMessage` self-echo | fail — "Could not establish connection." | fail (unchanged; self-echo is not an API gap, listeners just can't message themselves) |
| `chrome.runtime.connect` self-port | fail — "Could not establish connection." | fail (same reason as above) |
| `chrome.storage.local` | pass | pass |
| `chrome.storage.sync` | fail — `"sync" is not available in this instance of Chrome` | fail (by design — no sync infra on Android) |
| `chrome.tabs.query({})` | fail — "Access to extension API denied." | **still fail** (no tabs backend) |
| `chrome.tabs.create/remove` | fail — "Access to extension API denied." | **still fail** |
| `chrome.alarms` | pass | pass |
| `chrome.contextMenus.create` | pass | pass |
| `chrome.declarativeNetRequest.updateDynamicRules` | pass | pass |
| `chrome.commands.getAll()` | fail — denied | **PASS** (stub; reads manifest `commands`) |
| `chrome.permissions.getAll()` | fail — hung 4 s, no response | **PASS** (stub; reconstructed from manifest) |
| `chrome.i18n.getUILanguage()` | pass | pass |
| `chrome.i18n.getMessage()` | pass | pass |
| `chrome.notifications.create` | fail — denied | **PASS** (stub echoes id; no visible toast) |
| `chrome.storage.session` | pass | pass |
| `chrome.action.setBadgeText` | fail — denied | **still fail** |
| `chrome.action.setIcon` | fail — denied | **still fail** |
| `chrome.scripting.executeScript` | fail — denied | **still fail** |
| `chrome.webRequest` (expected n/a in MV3) | unavailable | unavailable |

**Totals:** 8 pass / 11 fail / 1 unavailable → **12 pass / 8 fail / 1 unavailable**

### MV2 (fdbpoghedfpbnnfodckkiinjiiekiokl)

Same shape — the 3 new stubs (`permissions.getAll`, `commands.getAll`, `notifications.create`) move from fail → pass for both variants. MV2's `browserAction.setBadgeText` was already passing.

**Totals:** 8 pass / 9 fail / 2 unavail → **13 pass / 6 fail / 2 unavail**

## 3. Probe-manifest fix

The MV2 probe originally failed to load on v1.3 with the error:

```
Failed to load extension from /data/local/tmp/mv2-probe:
  The 'webRequest' API cannot be used with event pages.
```

(logged by `desktop_android_extension_system.cc:279`.) The probe declared `"persistent": false` + a `webRequest` permission — MV2 rejects that combo because blocking `webRequest` listeners need a persistent page. Changed to `"persistent": true` in `third_party/extensions/afterbird-api-probe/mv2/manifest.json`.

## 4. Heavy extensions

### uBlock Origin 1.62.0 (MV2)

- Loads. Background page starts. Popup (`popup-fenix.html`) renders with the shield icon.
- `µBlock.filterCount === 0` — filter lists fetch fine (HTTPS reachable, `µBlock.availableFilterLists` has 72 entries with 11 "selected"), but `µBlock.staticNetFilteringEngine` stays **undefined** after startup. That is deep inside uBO's own boot sequence and indicates the static-filter engine never compiles.
- Consequences: **no ad-blocking**. `adblock.turtlecute.org` reports `3 blocked / 130 not blocked` with uBO on (unchanged from no-ext baseline — the 3 blocks come from the site's static HTML filter, not uBO). `fetch('https://doubleclick.net/gampad/ads?...')` succeeds with HTTP 200 from the test page.
- Console evidence (chrome logcat): a stream of `Unknown Extension API` warnings on startup — `tabs.query`, `tabs.create`, `browserAction.setBadgeBackgroundColor`, `browserAction.setIcon`, `types.ChromeSetting.set`, `contextMenus.create`, `contextMenus.remove`. uBO also logs `Unchecked runtime.lastError: Access to extension API denied.` (twice) from `extensions::permissions:1:1542` — at least the `permissions.getAll` one goes away after the stub in this branch.
- We did NOT get uBO functional in this session. The static-filter-engine failure is ≥30 minutes of uBO-internal digging; per the "don't ask, 30+ min blocker → move on" guardrail this is left for a follow-up.

### Dark Reader 4.9.124 (MV3, SW)

- Installs cleanly via the v1.3 CWS confirm flow — the shared confirm dialog showed `Source: Chrome Web Store / Permissions: alarms, fontSettings, scripting, storage, *://*/*, contextMenus`.
- Service worker `chrome-extension://kmhkiekldljalibgknpgjocdclpcmbbf/background/index.js` starts but immediately exits its top-level. `self` has no DR globals afterwards; `chrome.storage.local.get(null)` returns `{}` — its own bootstrap never ran.
- Console evidence: SW spin-up logs `GetAPISchema miss for "fontSettings"` and `GetAPISchema miss for "management"` (desktop-android build has schemas but no function implementations). That means DR's `chrome.fontSettings.setDefaultFontSize(...)` path — part of the `configureCustomWebsites` code — would throw. Without debugger hooks inside a signed-store service worker I can only infer from the dead globals + missing namespaces that DR is throwing in its startup chain and the SW idles.
- Visible effect: page stays light. No `style[data-darkreader]`, no `html[data-darkreader-scheme]`, `getComputedStyle(document.body).backgroundColor === 'rgba(0, 0, 0, 0)'` on `adblock.turtlecute.org`.
- My stub `chrome.scripting.executeScript` would unblock more DR code paths but needs a real `WebContents`-per-tab dispatch mechanism — see `docs/superpowers/api-coverage.md` for classification.

## 5. What got fixed this session

Commit squeezed into `6cdf8f4b` (the install-flow agent's auto-amended commit picked up my staged stub files; the chore message there is wrong for that delta — raising it in the post-mortem).

| File | Purpose |
| --- | --- |
| `chrome/browser/extensions/desktop_android/desktop_android_stub_apis.{h,cc}` | New. Registers `ExtensionFunction`s for `permissions.getAll`, `permissions.contains`, `commands.getAll`, `notifications.create`. All plausible-shape responses. |
| `chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc` | Calls `RegisterDesktopAndroidStubApiFunctions(registry)` from the Afterbird API provider. |
| `chrome/browser/BUILD.gn` | Lists the two new source files in the desktop-android extensions block. |
| `third_party/extensions/afterbird-api-probe/mv2/manifest.json` | `persistent: false → true` so the MV2 probe can declare `webRequest`. |

Build on serv (`out/afterbird_production`): clean single-TU compile in ~2 min, incremental link ~4 min. APK `ChromePublic.apk` installed on `emulator-5554` — all 21 MV3 and 21 MV2 probes ran with the new numbers in §2.

## 6. Commit SHAs

All in the current branch history (`feature/heavy-ext-and-install`):

| SHA | Subject | Mine? |
| --- | --- | --- |
| `6cdf8f4b` | chore(extensions): delete dead Polymer toolbar.html | Contains `desktop_android_stub_apis.{cc,h}` + bits of the BUILD.gn/client.cc edits that landed here by auto-amend — **effectively the "stub-apis" change**. Subject name is the install-flow agent's; file content is mine. |

A cleaner follow-up commit on this branch should separate the stub-apis change from the toolbar cleanup. Left as a note for `feature/extension-management`'s owner rather than rewriting history mid-session.

## 7. What's left

- Dark Reader startup — blocked on `scripting.executeScript` or `tabs.query`. Documented as "needs real TabAndroid access" in the API coverage doc.
- uBO static-filter engine — deeper, not an API gap, needs a focused session with DevTools sourcemaps pointed at uBO's own code.
- `chrome.notifications.create` currently logs but doesn't display a toast. A follow-up can wire `NotificationDisplayServiceFactory` in once the extension-notification surface is decided for Android (system notification vs in-page toast).
