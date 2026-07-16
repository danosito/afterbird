# M151 content scripts — RESOLVED: they work; the "bug" was the emulator GPU

Date: 2026-07-16, corrected 2026-07-17. Build: M151 desktop-android debuggable + MV2 patch.

## TL;DR

Content-script injection (JS **and** CSS) works correctly on stock upstream
M151 desktop-android with only the one-line MV2 patch. Cosmetic filtering is not
broken in Chromium. The prior conclusions in this file — first a
"renderer-startup race", then a "withheld host-permissions" theory — were **both
wrong**. Every "content scripts don't inject" observation was an artifact of the
local emulator's GPU: Chromium's GPU process kept crashing mid-render, which
corrupted or aborted real-page rendering. Switching the emulator to
`-gpu swiftshader_indirect` made the crashes vanish and content scripts inject
perfectly.

## How it was proven

Reliable, CDP-free repro (see harness notes): manual launch with
`--load-extension`, then read `chromium: [INFO:CONSOLE]` markers from logcat. A
minimal MV3 probe (`content_scripts` matching `<all_urls>`, `run_at:
document_start`) that (a) `console.log`s a marker and (b) sets
`html,body{background:magenta}` via CSS.

With the emulator on host GPU (Apple Metal), the GPU process crashed with:

```
ui/gl/gl_context_egl.cc  eglCreateContext ES 3.0 failed with error EGL_BAD_ATTRIBUTE
gpu/config/gpu_info_collector.cc  Could not create context for info collection
content/browser/gpu/gpu_process_host.cc  GPU process exited unexpectedly
```

After 3 GPU-process crashes Chromium hard-aborts the whole browser
(`gpu_data_manager_impl_private.cc:417 "GPU process isn't usable. Goodbye."`,
and desktop-android cannot fall back to `--disable-gpu` — that path
NOTREACHED-aborts at `:525 "GPU acceleration is required"`). These crashes
happened while rendering real pages (about:blank survived), so the browser was
dying exactly when a content script would have taken visible effect.

After `adb emu kill` + relaunch with `-gpu swiftshader_indirect`, the same probe
on the same build produced, for `https://example.com/`:

```
[AB-INJ] GetInjections url=https://example.com/ runloc=1 nscripts=1
[AB-INJ] candidate inject_css=1 inject_js=1 script_runloc=1 runloc=1
[AB-TRY] enter cur=1 runloc=1 reqid=-1 host_ok=1
[AB-TRY] branch=ALLOWED            <-- host access ALLOWED, not withheld
[AB-INJECT] should_js=1 should_css=1
[INFO:CONSOLE] "AB-CS-INJECTED at loading url=https://example.com/"  (cs.js)
```

…and the page rendered fully magenta. JS ran, CSS applied, 0 GPU crashes.

## What the instrumentation established along the way (all correct, all fine)

Instrumented `user_script_loader.cc`, `user_script_set.cc`, `script_injection.cc`
(reverted after — only the MV2 one-liner remains in the tree):

- Browser sends content scripts to **web** renderers on desktop-android:
  `SendUpdateIfNeeded`/`SendUpdate` fire for the example.com render process
  (`RPHCreated same=1 ilc=1 -> SENT`). Not just the extension's own process.
- The web renderer receives them: `UserScriptSet` has `nscripts=1`.
- `GetInjectionForScript` matches `<all_urls>` against the http URL and
  `CanExecuteOnFrame` returns **allowed** (not denied, not withheld), so a
  `ScriptInjection` is created at document_start.
- `ScriptInjection::TryToInject` takes the **ALLOWED** branch and `Inject()`
  runs both JS and CSS.

None of this is desktop-android-specific breakage. `enable_extensions=false` +
`enable_extensions_core=true` (this build) still routes `--load-extension`
through `UnpackedInstaller`, which calls `PermissionsUpdater::InitializePermissions`
(that class lives in `extensions/browser/`, available under
`enable_extensions_core`), so host permissions are promoted to active and
`GetEffectivePermissionsToGrant` does not withhold for a freshly-loaded
extension. (This differs from the v1.8 custom-layer world, where
`chrome/browser/extensions/` was fully unlinked; that fix is not needed on the
stock overlay.)

## Consequence for uBO / adblock parity

The earlier "uBO only 68 %, cosmetic filtering missing" number was measured under
the crashing-GPU emulator and is not trustworthy. Re-measure on
`swiftshader_indirect`. Expectation: cosmetic filtering (which is exactly
content-script injection) now works, so uBO should approach desktop parity.

## The real lesson — this was a test-pipeline defect

The "terrible test pipeline" the project set out to replace produced a
multi-session false-negative. Root causes to design out of the new harness:

1. **Emulator GPU must be `swiftshader_indirect`.** Host GPU (Metal) gives an
   unstable Chromium GPU process on desktop-android; the failure is silent
   (renders die, no test-level error).
2. **Playwright `_android.launchBrowser` is unusable for extension tests** — it
   rewrites `/data/local/tmp/chrome-command-line`, dropping `--load-extension`
   and adding `--disable-extensions`. Use a manual `am start` launch instead.
3. **Prefer a non-GPU-dependent signal** (logcat `INFO:CONSOLE` markers) over
   screenshots when the GPU path is in question.
