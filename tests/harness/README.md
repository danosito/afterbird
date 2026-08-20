# Afterbird test harness

Goal: measure **desktop parity** — does an extension/DevTools behave on Afterbird
(Android) the way it does on desktop Chrome? Same spec, multiple targets, the
result is a diff.

## Engine — split by whether the test needs an extension loaded

Two engines, because Playwright and extension-loading are mutually exclusive on
Android (learned the hard way — see the content-scripts diagnostic):

1. **Extension tests → adb driver (`lib/android.mjs`).** Playwright
   `_android.launchBrowser` **rewrites** `/data/local/tmp/chrome-command-line`
   on launch: it DROPS any `--load-extension` and ADDS `--disable-extensions`.
   So the extension under test never loads under Playwright. The adb driver does
   a plain `am start` with a command line we control and reads results from
   logcat `chromium: [INFO:CONSOLE]` markers — no CDP socket required (that
   socket is created in a *deferred* startup task and isn't reliably up on a
   manual launch).
2. **Extension-less page parity → Playwright `_android` over CDP**
   (`lib/target.mjs::openAndroid`). Fine when no extension is involved.

### HARD REQUIREMENT: emulator must run `-gpu swiftshader_indirect`

On the host-GPU (Metal) path, Chromium's GPU process on desktop-android is
unstable: `eglCreateContext ES 3.0 -> EGL_BAD_ATTRIBUTE`, the GPU process
crashes, and after 3 crashes the whole browser hard-aborts (`"GPU process isn't
usable. Goodbye."`; desktop-android can't fall back to `--disable-gpu`). This is
silent at the test level — pages just fail to render — and it produced a
**multi-session false negative** (content-script injection looked broken; it
wasn't). `lib/android.mjs::preflightGpu()` asserts a software GPU before running.

Relaunch the AVD cleanly:

    adb emu kill
    emulator -avd <name> -gpu swiftshader_indirect -no-snapshot -no-boot-anim

CDP works against both build variants: upstream Chrome for Android always
starts DevToolsServer, and `content::CanUserConnectToDevTools` admits root,
shell, or the app's own uid. What the `test` variant adds is a *debuggable*
APK, which is what lets Chromium read `/data/local/tmp/chrome-command-line` —
without it the harness cannot pass `--load-extension` at all.

### Running against a physical device

Two things differ from the emulator:

1. Chromium only reads `/data/local/tmp/chrome-command-line` on an eng/userdebug
   build or when the package is the system's selected debug app. Build with the
   `test` args variant (`is_java_debug = true`) and run once:

       adb shell am set-debug-app --persistent com.danosito.afterbird

   Without it the browser starts with none of the harness's flags — no
   `--load-extension`, no `--disable-fre` — and every extension check reports
   "uBO did not load".

2. The unnamed `chrome_devtools_remote` socket is first-come-first-served. If
   another Chromium browser is running (a stock Kiwi, say) it owns that name and
   ours listens on `chrome_devtools_remote_<pid>`. `checks/adblock-device.mjs`
   picks the pid-scoped socket and refuses to run if `/json/version` reports a
   different `Android-Package`.

The software-GPU requirement applies to emulators only; `preflightGpu()` detects
a physical device and skips the assertion.

## Reference strategy — split by manifest version

Modern desktop Chrome (149+) **refuses to load MV2 extensions** (`Cannot install
extension because it uses an unsupported manifest version`); the
`ExtensionManifestV2*` feature flags no longer re-enable it. So:

- **MV3 specs** (Dark Reader, Bitwarden, …): live parity diff vs desktop Chromium
  (`lib/target.mjs::openDesktop`). Desktop is a valid reference.
- **MV2 specs** (uBlock Origin): **absolute threshold**, not a live-Chrome diff.
  The known-good uBO behavior on `adblock.turtlecute.org` is ≥90% blocked
  (Kiwi measured ~97%). Afterbird carries a permanent MV2-reenable patch, so its
  own build is the system under test; the "reference" is the fixed threshold.
  (A pinned Chrome ≤137 could serve as a live MV2 reference but we don't maintain
  an EOL browser.)

## Layout

- `lib/target.mjs` — `openDesktop()` / `openAndroid()` targets.
- `specs/*.mjs` — one behavior per file, returns a normalized result object.
- `run-parity.mjs` — runs specs across targets, prints a `PARITY-JSON` row set.

## Run

    npm install
    npx playwright install chromium
    node run-parity.mjs desktop            # reference side
    AB_PKG=com.danosito.afterbird node run-parity.mjs android   # needs CDP-enabled build

## Status (M151 stock overlay + MV2 patch, on swiftshader_indirect)

- **Content scripts / cosmetic filtering: WORKING.** Proven end-to-end on device
  (JS runs, CSS applies) with the adb driver. The earlier "content scripts don't
  inject" finding was a host-GPU emulator artifact, now designed out.
- **uBO network (webRequest) blocking: ~85% on turtlecute (113/133) — FIXED.**
  Was 14%. Root cause was not webRequest dispatch: uBO's MV2 background page
  FATAL-crashed at `GetAPISchema("browserAction")` before registering its
  blocking listener. Fix bundles the browserAction/pageAction schema on
  desktop-android (`patches/m151/0002-*`). See the 2026-07-17 webRequest
  diagnostic.
- adb driver (`lib/android.mjs`): proven against the M151 build.
- Desktop path + adblock spec: proven.
- MV2 desktop reference: proven impossible on Chrome 149 (documented above).
- Next: fix webRequest blocking dispatch; add MV3 specs + install/DevTools specs.
