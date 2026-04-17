# Kiwi-style on-device DevTools + lazy socket — design

**Status:** accepted (user asleep, judgment calls documented inline)
**Date:** 2026-04-18
**Branch:** `feature/devtools` (off v1.4.0)
**Owner:** this agent

## Goal

Restore Kiwi Browser's behaviour:

1. A **Developer Tools** entry in the main app menu. Tapping it opens a
   full on-device DevTools view against the current tab.
2. The **remote debugging socket** is NOT created during deferred
   startup. It is created lazily on the first tap of the DevTools menu
   entry, and then left running for the life of the process.

Stock Chromium for Android has neither of these: DevTools on Android is
desktop-side only (`chrome://inspect` over ADB).

## Key finding — Kiwi does not open `devtools://devtools`

On desktop Chrome, `DevToolsWindow::OpenDevToolsWindow(web_contents)` is
the canonical entry point. It does three things:

* spins up `DevToolsUIBindings` (C++ side of the front-end)
* creates a new browser window / tab
* loads `devtools://devtools/bundled/inspector.html?ws=…`

**None of this compiles for Android.** `chrome/browser/devtools/BUILD.gn`
gates `devtools_window.{cc,h}`, `devtools_ui_bindings.{cc,h}`, the whole
device-discovery stack, etc. inside `if (!is_android)`. The WebUI
factory (`chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc`
line 180-190) explicitly returns `nullptr` for `devtools://` on Android.

That means on Kiwi (and by extension on us), opening `devtools://…` in
a tab fails. Kiwi cannot have been doing that. The only plausible
implementation that matches Kiwi's `AppMenuBridge.openDevTools(WebContents)`
signature is:

> Bind the DevTools HTTP server to `127.0.0.1:PORT` instead of (or in
> addition to) the abstract unix socket, then load
> `http://127.0.0.1:PORT/devtools/inspector.html?ws=127.0.0.1:PORT/devtools/page/<agent-id>`
> in a new tab.

The `DevToolsSocketFactory` interface
(`content/public/browser/devtools_socket_factory.h`) takes any
`net::ServerSocket`, so a `TCPServerSocket` bound to loopback works
out-of-the-box with `DevToolsAgentHost::StartRemoteDebuggingServer`.
The HTTP handler (`content/browser/devtools/devtools_http_handler.cc`)
already serves `/devtools/inspector.html` when bundled frontend
resources are present (which they are on Android). We just need a
socket it can live on that a WebView can actually reach.

This is a judgment call (Kiwi's `AppMenuBridge.java` is not in our
snapshot). Writing it up here explicitly.

## Architecture

Three layers, mirroring the existing extension-menu pattern
(`chrome/browser/extensions/android/app_menu_bridge.cc` +
`AppMenuBridge.java`):

```
  main_menu.xml
       │  developer_tools_id
       ▼
  ChromeActivity.onOptionsItemSelected
       │  DevToolsBridge.openForCurrentTab(webContents)
       ▼
  DevToolsBridge.java (new)
       │  JNI → JNI_DevToolsBridge_Open
       ▼
  devtools_bridge.cc (new)
       │  (1) if no TCP server yet → build one, call
       │      DevToolsAgentHost::StartRemoteDebuggingServer
       │  (2) GetOrCreateFor(web_contents).GetFrontendURL()
       │      then return "http://127.0.0.1:PORT/<frontend-url>"
       ▼
  returns std::string URL → Java opens it as a new tab
                            (TabCreator.createNewTab, FROM_LINK)
```

### Lazy socket trigger

The trigger point is `DevToolsBridge::Open()`, first call. This is
the user's exact requirement: "if the 'open DevTools' button hasn't
been tapped, there's no reason to keep the socket open." Rationale
recorded in the commit message of the lazy-socket change.

We DO NOT tear the socket down between uses. Kiwi-style: once up,
stays up until process exit. The expensive part is the `bind(2)` +
`listen(2)` on the unix/TCP socket and the HTTP handler thread; those
are cheap once running.

### Removing the existing eager socket

`chrome/android/java/src/org/chromium/chrome/browser/init/ProcessInitializationHandler.java`
line 663-666 currently does (during deferred startup):

```java
mDevToolsServer = new DevToolsServer(DEV_TOOLS_SERVER_SOCKET_PREFIX);
mDevToolsServer.setRemoteDebuggingEnabled(
        true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);
```

We gate this behind a boolean that's false by default. The socket no
longer comes up unless the user taps DevTools. `DevToolsServer` (the
abstract-unix-socket one used for desktop-side `chrome://inspect`) is
separate from our new TCP localhost server; they can both exist. The
simplest thing is to just **not start the unix server at startup**
and rely on the new TCP one.

That also removes the "chromedevtools_remote socket leaking in ps
output" concern — no socket is created at boot.

## Components

### 1. `chrome/browser/android/devtools_bridge.{cc,h}` (new)

Native helper that:

* Owns a lazily-created `DevToolsSocketFactory` (TCP, 127.0.0.1:0).
* Exposes `JNI_DevToolsBridge_Open(env, profile, web_contents) → std::string`
  which:
  * On first call, creates the factory, calls
    `DevToolsAgentHost::StartRemoteDebuggingServer`, records the
    kernel-assigned port.
  * Calls `DevToolsAgentHost::GetOrCreateFor(web_contents)` to get an
    agent host for the tab.
  * Constructs the inspector URL:
    `http://127.0.0.1:<port>/devtools/inspector.html?ws=127.0.0.1:<port>/devtools/page/<agent->GetId()>`
  * Returns the URL string (empty on failure).

Port discovery: `net::TCPServerSocket::GetLocalAddress` after `Listen`.

### 2. `chrome/browser/extensions/android/java/src/org/chromium/chrome/browser/extensions/DevToolsBridge.java` (new)

Thin JNI wrapper matching the `AppMenuBridge.java` pattern —
`@JniType("std::string") String open(Profile, WebContents)`. Placed
in the extensions/android module because that module already has the
JNI plumbing (generated `jni_headers/DevToolsBridge_jni.h`, existing
BUILD.gn entry for JniType annotations). New dir would require
touching 3 more BUILD.gn files.

Judgment call: putting devtools support under the extensions module is
a little odd semantically but saves ~4 BUILD.gn edits and keeps the
overlay surface area small. Will add a comment.

### 3. Menu entry

* `chrome/android/java/res/menu/main_menu.xml`: add
  `<item android:id="@+id/developer_tools_id" … />` near the existing
  `extensions_id` entry (both are developer-facing).
* `chrome/android/java/res/drawable-*dpi/ic_devtools.png`: already in
  tree (from Kiwi's original PR) — reuse.
* `chrome/android/java/res/values*/strings.xml`: add
  `main_menu_developer_tools` = "Developer Tools".
* `ChromeActivity.java`: handler right after the existing
  extension-menu-item block (~line 2445), calls
  `DevToolsBridge.open(profile, webContents)` and opens the returned
  URL in a new tab with `FROM_LINK` so the back gesture dismisses.

### 4. Lazy socket gating

* `ProcessInitializationHandler.java` deferred startup block at
  ~line 663-666: replace with a comment explaining the startup-time
  server is intentionally disabled (it's replaced by lazy TCP).
* `DevToolsServer.java` (Java) is left as-is; nobody calls it anymore.

This is the whole lazy-socket deliverable: the startup code no longer
creates it.

## Data flow / failure modes

* **`DevToolsAgentHost::GetOrCreateFor` returns null**: tab has no web
  contents (e.g., NTP in a weird state). `Open` returns "", Java no-ops.
* **TCP bind fails**: log-and-swallow, return "". User sees the menu
  click do nothing. Unlikely on 127.0.0.1:0 but belts-and-braces.
* **Multiple taps**: first tap spins up server, subsequent taps reuse
  it and just open new tabs. Old DevTools tabs keep working because
  they hold a WebSocket to the server.
* **Process death**: server dies with process. Next launch → no
  socket. First tap spins it up again.

## Testing

### Unit-testable

* Menu-id resolver — none, just an int compare.
* URL construction — deterministic string format; trivial to unit test
  in native. Add a `DevToolsBridgeTest.FrontendUrlShape` that pumps a
  fake agent-host id and port, checks `http://127.0.0.1:12345/devtools/inspector.html?ws=127.0.0.1:12345/devtools/page/ABC`.

### Device verification (primary evidence)

Physical `R5CTA2MHJFA`. Before-and-after:

1. **Cold launch, no socket**: `adb -s R5CTA2MHJFA shell "cat /proc/net/unix | grep chrome_devtools"` should return 0 matches (currently ~1 match before this change).
2. **Tap DevTools menu entry**: new tab opens loading `http://127.0.0.1:*/devtools/inspector.html?ws=…`. Frontend JS loads. Panels usable.
3. **Post-tap `netstat`**: `adb -s R5CTA2MHJFA shell "cat /proc/net/tcp | grep 7F000001"` should show a new LISTEN on loopback.
4. **Logcat**: `adb -s R5CTA2MHJFA logcat | grep -i devtools` should show the server start line AFTER the menu tap, not at boot.

## Build

Incremental build on serv (~3-4 min). Commands:

```
ssh serv 'cd ~/dev/afterbird/.work/chromium_retry_20260413_044137/src && autoninja -C out/afterbird_production chrome_public_apk'
```

Then pull the APK, install with `adb -s R5CTA2MHJFA install -r …`.

## Scope guards

* Not touching install flow, popup lifecycle, API probe,
  `desktop_android_developer_private.cc`, `desktop_android_stub_apis.*`.
* Not implementing Chromium's full DevToolsUIBindings (would be ~50
  source files).
* Not implementing inspection of service-worker / extension
  backgrounds from this menu entry — only the current tab. That
  matches the Kiwi `openDevTools(webContents)` signature.

## Open judgment calls (recorded here so commit messages can point at them)

1. URL-based DevTools (not `DevToolsWindow`) — forced by Android build.
2. TCP localhost instead of abstract unix socket — required so WebView
   can actually connect.
3. Bridge class lives in the extensions/android module — BUILD.gn
   thrift reduction.
4. Socket stays up after first use — user's explicit guidance.
5. No port pinning — kernel-assigned port per process, URL constructed
   at the time of the tap.

## Sub-branch policy

If any single sub-problem blocks >30 min, cut
`feature/devtools-wip-<topic>` with partial work and return to
`feature/devtools`.

## Rollout

Single commit chain on `feature/devtools`. No push, no merge, per
user instruction.
