# DevTools Kiwi-style + Lazy Socket — Implementation Plan

> Executed inline (user asleep). Device verification is primary evidence. Unit tests only where cheap.

**Goal:** Add "Developer Tools" main-menu entry that opens on-device DevTools for current tab; make remote debugging socket lazy.

**Architecture:** `ChromeActivity` → `DevToolsBridge.java` (JNI) → `devtools_bridge.cc`. First tap spins up TCP loopback DevTools HTTP server; opens `http://127.0.0.1:PORT/devtools/inspector.html?ws=…` in a new tab.

**Tech Stack:** C++ (Chromium), Java (Android), jni_zero typed JNI, Ninja/autoninja.

---

## File Structure

| Path | Action | Purpose |
|---|---|---|
| `chrome/browser/android/devtools_bridge.cc` | create | Native: lazy TCP server + URL construction |
| `chrome/browser/android/devtools_bridge.h` | create | Native header |
| `chrome/browser/extensions/android/java/src/org/chromium/chrome/browser/extensions/DevToolsBridge.java` | create | Java JNI wrapper |
| `chrome/browser/extensions/android/BUILD.gn` | modify | Register new .cc/.java + jni |
| `chrome/android/java/res/menu/main_menu.xml` | modify | Add `developer_tools_id` item |
| `chrome/android/java/res/values*/strings.xml` | modify | `main_menu_developer_tools` string |
| `chrome/android/java/src/org/chromium/chrome/browser/app/ChromeActivity.java` | modify | Menu handler |
| `chrome/android/java/src/org/chromium/chrome/browser/init/ProcessInitializationHandler.java` | modify (overlay) | Disable eager startup socket |

---

## Task 1: Native DevToolsBridge (C++)

**Files:**
- Create: `chrome/browser/android/devtools_bridge.h`
- Create: `chrome/browser/android/devtools_bridge.cc`

- [ ] Write `.h` declaring a singleton-ish helper with `EnsureServerStarted()` returning assigned loopback port (or 0 on failure) and `BuildFrontendUrlFor(WebContents*)` returning URL string.

- [ ] Write `.cc`:
  - `TcpLoopbackSocketFactory` subclassing `content::DevToolsSocketFactory`, binds `127.0.0.1:0`.
  - `StartServer()` calls `DevToolsAgentHost::StartRemoteDebuggingServer`, captures port via `GetLocalAddress`.
  - `JNI_DevToolsBridge_Open` obtains/creates agent host via `DevToolsAgentHost::GetOrCreateFor(web_contents)`, constructs `http://127.0.0.1:PORT/devtools/inspector.html?ws=127.0.0.1:PORT/devtools/page/<id>`.
  - Returns `std::string` (empty on failure).

## Task 2: Java DevToolsBridge

**Files:**
- Create: `chrome/browser/extensions/android/java/src/org/chromium/chrome/browser/extensions/DevToolsBridge.java`

- [ ] Thin JNI wrapper with `public static String open(Profile, WebContents)` and `@NativeMethods` interface using `@JniType("Profile*")`, `@JniType("content::WebContents*")`, `@JniType("std::string")` return. Match the exact conventions of the sibling `AppMenuBridge.java`.

## Task 3: BUILD.gn wiring

**Files:**
- Modify: `chrome/browser/extensions/android/BUILD.gn`

- [ ] Add `devtools_bridge.cc`/`.h` to sources, `DevToolsBridge.java` to java sources, add to jni srcjar target.

## Task 4: Menu XML + strings

**Files:**
- Modify: `chrome/android/java/res/menu/main_menu.xml`
- Modify: English strings only (`values/strings.xml` — add one key; skip all 30 locale files)

- [ ] Add `<item android:id="@+id/developer_tools_id" …/>` with icon `@drawable/ic_devtools` (already in tree).
- [ ] Add `<string name="main_menu_developer_tools">Developer tools</string>` in English only.

## Task 5: ChromeActivity handler

**Files:**
- Modify: `chrome/android/java/src/org/chromium/chrome/browser/app/ChromeActivity.java`

- [ ] Add import for `DevToolsBridge`.
- [ ] Add menu-handler block right after the existing extension-menu block (~line 2445): fetch `webContents`, call `DevToolsBridge.open(profile, webContents)`, and if the returned URL is non-empty, open via `TabCreator.createNewTab(new LoadUrlParams(url, PageTransition.LINK), TabLaunchType.FROM_LINK, activeTab)`.

## Task 6: Lazy-socket gating

**Files:**
- Modify (new overlay): `chrome/android/java/src/org/chromium/chrome/browser/init/ProcessInitializationHandler.java`

- [ ] Copy the file from build-server source into our workdir.
- [ ] Replace the eager DevToolsServer start block with a comment explaining the socket is now created lazily by `DevToolsBridge` on first tap.

## Task 7: Build + install

**Files:** N/A

- [ ] Incremental build on serv: `ssh serv 'cd ~/dev/afterbird/.work/chromium_retry_20260413_044137/src && autoninja -C out/afterbird_production chrome_public_apk 2>&1 | tail -40'`
- [ ] Pull APK, install on `R5CTA2MHJFA`.

## Task 8: Device verification

**Files:** N/A

- [ ] **Pre-tap**: `adb -s R5CTA2MHJFA shell "cat /proc/net/unix | grep chrome_devtools_remote"` — expect 0 matches (was 1 before).
- [ ] **Tap menu**: verify via logcat the server starts.
- [ ] **Post-tap**: `cat /proc/net/tcp | grep 7F000001` — expect new LISTEN entry.
- [ ] **Screenshot**: `adb -s R5CTA2MHJFA exec-out screencap -p > /tmp/devtools.png` showing the inspector frontend loaded in a tab.
- [ ] Measure time-from-tap-to-inspector-loaded (manual stopwatch, rough).

## Task 9: Commit + document

**Files:**
- Add release notes to a commit message.

- [ ] Each logical step gets its own commit:
  1. Native bridge + header
  2. Java bridge + BUILD.gn
  3. Menu XML + strings + ChromeActivity handler
  4. Lazy-socket gating in ProcessInitializationHandler
  5. Device evidence (if any new docs)
