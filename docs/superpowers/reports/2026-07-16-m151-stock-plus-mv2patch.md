# M151 stock + 1-line MV2 patch — strategy validated on device

> **CORRECTION (2026-07-17).** Several numbers/claims below were measured on a
> broken emulator GPU and are wrong. Re-measured on `-gpu swiftshader_indirect`:
> - The "68 % blocked" figure is not reproducible — uBO network blocking is
>   ~**14 %** on turtlecute. Network (webRequest) blocking is **mostly still
>   broken**, not "done" (§5 below is wrong on this point).
> - The "68 %→97 % gap is cosmetic filtering / content scripts" framing (§5) is
>   backwards. **Content scripts WORK** (JS+CSS injection proven on device); the
>   real remaining gap is the webRequest-blocking dispatch, the same v1.8 track.
> - The "fix GPU abort with `--use-gl=angle --use-angle=swiftshader`" advice (§
>   Test-environment) is wrong — forcing in-process SwiftShader is itself a crash
>   source. The fix is to run the **emulator** with `-gpu swiftshader_indirect`.
>
> See `docs/superpowers/diagnostics/2026-07-16-m151-content-scripts.md`.


Date: 2026-07-16
Tree: `~/afterbird-chromium-151` @ tag 151.0.7922.38, `is_desktop_android=true`,
`is_official_build=false` (debuggable), `-j 40`, `symbol_level=0`.
Device: emulator-5554 (Android 15). uBO 1.62.0 via `--load-extension`.

## The headline result

**uBlock Origin network filtering works on upstream M151 with a single 1-line
patch.** adblock.turtlecute.org:

| Build | Blocked | Pct |
|---|---|---|
| Afterbird v1.8 (Chromium 132, custom `desktop_android` layer) | 3–4 / 133 | **2–3 %** |
| M151 stock + custom-nothing + **1-line MV2 patch** | 90 / 133 | **68 %** |

The webRequest blocking dispatch that was fundamentally broken on the 132 custom
layer (the v1.8 "OnBeforeRequest returns net::OK, GetMatchingListeners empty"
blocker) **just works** on upstream M151. No custom code.

## Each blocker, resolved and verified on device

1. **Custom layer is fully droppable.** Stock M151 has no
   `chrome/browser/extensions/desktop_android/` dir; the real stack lives in
   `chrome/browser/extensions/` with `is_android` sections. Built with ZERO
   afterbird overlay (args + package name only).
2. **Extensions gate on `is_desktop_android`,** not the old
   `enable_desktop_android_extensions` alone. Setting `is_desktop_android=true`
   compiles the real stack (and defaults the extensions flag on).
3. **Corp gate does NOT block us.** `ExtensionManagement::
   ExtensionsEnabledForDesktopAndroid()` returns `true` for accountless / non-
   `@google.com` profiles (verified in source + on device). No patch needed for
   the googleless build.
4. **`--load-extension` is honored** on desktop-android M151 (the extension
   system attempted uBO; MV3 probe loaded with no error).
5. **MV2 was the one hard blocker** → "Cannot install extension because it uses
   an unsupported manifest version". Fixed with a **single line** in
   `extensions/browser/mv2_deprecation_impact_checker.cc`
   (`IsExtensionAffected(int,Type,Location)` → `return false;` at top). Rebuild
   was incremental: **69 seconds**. After it, uBO loads and blocks (68 %).
6. **CDP harness attaches** to the debuggable build (Playwright `_android`
   drove example.com). Release build must keep the socket closed.

## Test-environment friction (not product bugs)

- **GPU abort** on the emulator (`gpu_data_manager_impl_private.cc:417 GPU
  process isn't usable`) → launch with `--use-gl=angle --use-angle=swiftshader`.
- **ANR on FirstRunActivity** via an accessibility-binder stall, worsened by
  software GL → bypass with `--disable-fre --no-first-run`. CDP-driven testing
  sidesteps ANRs entirely (drives the renderer, not the Android UI thread).

## Remaining gap to full parity (68 % → ~97 %)

The 32 % not blocked is almost certainly **cosmetic filtering + scriptlet
injection** (element hiding / anti-adblock), which needs content-script /
userScripts wiring verified on desktop-android. Network blocking (the hard part,
the v1.8 blocker) is done. This is refinement, not rearchitecture.

## Next

1. Build the real M151 overlay: drop the 30-file custom layer, carry the MV2
   patch + branding + test/release arg variants as tracked files.
2. Close the cosmetic-filtering gap (content scripts / userScripts on
   desktop-android).
3. Phone-UI glue: extensions entry + action popups (Cromite-style).
4. DevTools menu → upstream `DevToolsWindowAndroid.openDevTools`.
5. Grow the Playwright harness: adblock (done), MV3 API probe, install, DevTools.

Screenshots: `scratchpad/m151/*.png`.
