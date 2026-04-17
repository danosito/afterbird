# 2026-04-17 — Extension popup "disconnect" diagnosis

**Owner:** extension-popup-lifecycle agent
**Device:** `emulator-5556` (Afterbird 132.0.6834.83, pre-installed)
**DevTools forward:** `tcp:9226 → localabstract:chrome_devtools_remote`
 (kept off `9222` to avoid clashing with the agent on `emulator-5554`)
**Branch:** `feature/heavy-ext-and-install` (no commits — no fix shipped; see
"Recommendation" below for why)

## Symptoms reported

1. uBlock Origin popup opens but "filter button doesn't work" against the
   user's real site.
2. Popup appears to "flicker" during load.
3. Dark Reader popup "doesn't load".

## Working hypothesis in the dispatch prompt

The popup opens as a new foreground tab, so
`chrome.tabs.query({active:true, currentWindow:true})` returns the popup's own
tab, not the user's original tab. Suggested fixes: mark the popup tab
ignorable, pass the previously-active WebContents through as the popup's
"active tab", or host the popup in an overlay / bottom sheet.

**This hypothesis is wrong in premise.** See evidence below.

## Actual root cause

Every `chrome.tabs.*` and `chrome.windows.*` call from uBO's popup **and**
background page returns `chrome.runtime.lastError.message === "Access to
extension API denied."` — the callback fires immediately, `tabs` is
`undefined`, nothing like a live tab record is ever handed back.

`logcat` shows the browser-side branch that produces that error:

```
E chromium: [ERROR:extension_function_dispatcher.cc(545)]
           Unknown Extension API - tabs.query
E chromium: [ERROR:extension_function_dispatcher.cc(545)]
           Unknown Extension API - windows.getAll
E chromium: [ERROR:extension_function_dispatcher.cc(545)]
           Unknown Extension API - windows.getCurrent
E chromium: [ERROR:extension_function_dispatcher.cc(545)]
           Unknown Extension API - windows.getLastFocused
```

`extensions/browser/extension_function_dispatcher.cc:540-599` has two paths to
the `"Access to extension API denied."` string:

- **line 544** — `ExtensionFunctionRegistry::GetInstance().NewFunction(name)`
  returned null (the function name is **not registered** at all).
- **line 593** — `!function->HasPermission()` (function registered but the
  extension lacks the permission).

Logcat proves this build hits the **first** branch: the functions aren't in
the registry. This is a build-configuration gap, not a permission-manifest
issue and not a popup-lifecycle issue.

### Why the hypothesis is wrong

The prompt assumes `chrome.tabs.query` returns *something*, just the wrong
tab. It doesn't — it returns `undefined` with `lastError` set, before any tab
enumeration can happen. Re-hosting the popup (overlay, bottom sheet, whatever)
cannot fix a filter button whose underlying extension function call is being
rejected at the function-dispatcher level.

### Where the gap is in source

`chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc`
only wires two API providers into the registry:

```cpp
AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
AddAPIProvider(
    std::make_unique<AfterbirdChromeExtensionsBrowserAPIProvider>());
```

The Afterbird provider registers only `developerPrivate` handlers.
`CoreExtensionsBrowserAPIProvider` does not provide chrome-level `tabs`/
`windows` handlers — those live in
`chrome/browser/extensions/chrome_extensions_browser_api_provider.cc`, which
calls `api::ChromeGeneratedFunctionRegistry::RegisterAll(registry)`. That
provider is not currently wired into the desktop-android client, and the
underlying `chrome/browser/extensions/api/tabs/tabs_api.*` functions aren't
being compiled (no `TabsQueryFunction` hit in a repo-wide search).

## Evidence gathered

### 1. Target list on device

```
page  | https://example.com/
page  | chrome-extension://lmgllnjchbmipdlnapnialfclmpkmpfj/popup-fenix.html
other | chrome-extension://lmgllnjchbmipdlnapnialfclmpkmpfj/background.html
```

### 2. Direct probe from inside popup context (CDP `Runtime.evaluate`)

```json
{
  "href": "chrome-extension://.../popup-fenix.html",
  "r1":   { "lastErr": "Access to extension API denied." },
  "rAll": { "lastErr": "Access to extension API denied." },
  "rWins":{ "lastErr": "Access to extension API denied." }
}
```

Same result from the uBO background page.

### 3. Chrome APIs visible in popup

`chrome.tabs` and `chrome.windows` exist as JS objects, with methods enumerable
(`query`, `get`, `create`, `getAll`, `getCurrent`, `getLastFocused`, …). They
are wired through to `extension_function_dispatcher.cc` — which then rejects
them because no registrar ever installed the implementations.

### 4. "Flicker" / repeated reloads

Not observed at the Blink level. Across 8 s of CDP `Page.*` events after a
forced reload there was exactly **one** `Page.frameNavigated`, one
`Page.loadEventFired`, and no further navigations. The only console output
was two `Slow network` font-fallback `log.info` entries. After instrumenting
`chrome.tabs.query`, `chrome.windows.getCurrent`, `chrome.windows.getAll` with
call-count hooks, **zero** calls happened over 6 s of steady-state — uBO is
not retrying. The "flicker" the user perceives is almost certainly internal
DOM work inside uBO's own popup code reacting to
`lastError`-poisoned data, not page navigation.

### 5. Dark Reader

Not installed on this device at the time of diagnosis. No valid evidence to
add, and installing it would require touching install-flow code that is out
of my lane.

## Why the Guardrails-legal fixes don't help

The guardrails allow changes in:

- `chrome/android/java/.../ChromeActivity.java` (extension-menu handler at
  lines ~2439-2457; currently opens the popup as a `FROM_LINK` new tab)
- `chrome/browser/extensions/android/java/.../ExtensionMenuManager.java`
- `chrome/browser/extensions/android/app_menu_bridge.cc`

All of them are presentation-layer. No matter how the popup is hosted
(new tab, overlay, bottom sheet), `chrome.tabs.query` inside that popup will
still produce `Access to extension API denied.` because the registry entry is
missing. Re-hosting would change where the user sees the popup but wouldn't
make a single filter toggle work.

The *actual* fix — wiring `ChromeExtensionsBrowserAPIProvider` (or an
equivalent subset that pulls in tabs/windows) into the desktop-android client
and making the underlying tabs/windows implementations compile on Android —
lives in `chrome/browser/extensions/desktop_android/...`, which is explicitly
off-limits per the "Stay out of install flow, API plumbing in
developer_private.cc" Guardrails line.

## Process decision

Per systematic-debugging Iron Law: **no fix without root cause**. Per
Guardrails: root cause is in another agent's zone. Per "30+ min blocker →
sub-branch + move on": documenting, not guessing-and-shipping.

- No code changes made.
- No commits created on `feature/heavy-ext-and-install`.
- Working-tree modification to
  `chrome/browser/extensions/desktop_android/desktop_android_extension_system.cc`
  and the `third_party/extensions/` untracked directory pre-existed this
  session; I did not touch either.

## Recommendation for the orchestrator

1. Re-assign the "popup filter button doesn't work" symptom to the agent that
   owns `desktop_android` API plumbing. The minimal fix is likely:
   - Add `ChromeExtensionsBrowserAPIProvider` (or a narrowed-down provider
     that registers just `tabs.query`, `windows.get*`, and whichever
     neighbouring functions uBO / Dark Reader need) to
     `DesktopAndroidExtensionsBrowserClient`'s ctor `AddAPIProvider` list.
   - Make the supporting sources compile on Android (`tabs_api.cc` and its
     BUILD.gn entries are currently not in the desktop-android build
     surface).
2. The "flicker" symptom almost certainly dissolves once `tabs.query`
   succeeds — uBO won't be rendering against `undefined`-shaped data — but
   that's worth confirming on a follow-up run.
3. The presentation-layer hypothesis ("change how popups are opened")
   remains worth exploring **after** the API surface is fixed, for UX
   reasons (e.g. back-navigation semantics), but is not a fix for any of the
   reported symptoms today.

## Reproduction recipe (for the next agent)

```bash
DEV=emulator-5556
adb -s "$DEV" forward tcp:9226 localabstract:chrome_devtools_remote
# Open Afterbird, navigate to any http(s) page, open extension popup via menu.
curl -s http://localhost:9226/json
# Find the popup target's id; then via CDP Runtime.evaluate:
#   chrome.tabs.query({active:true, currentWindow:true}, t => ...)
# Observe chrome.runtime.lastError.message === "Access to extension API denied."
adb -s "$DEV" logcat -d | grep "Unknown Extension API"
# Observe: tabs.query, windows.getAll, windows.getCurrent, windows.getLastFocused
```
