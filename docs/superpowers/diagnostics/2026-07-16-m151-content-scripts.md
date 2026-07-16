# M151 content-script injection gap — precisely localized

Date: 2026-07-16. Build: M151 desktop-android debuggable + MV2 patch.

## Symptom

uBlock Origin blocks 68 % on adblock.turtlecute.org (network filtering works) but
not ~97 % — the missing part is **cosmetic filtering** (element hiding), which
needs content scripts. Direct probe (a minimal MV3 extension with a
`content_scripts` entry setting `document.documentElement` attribute + injecting
CSS) confirmed: **content scripts do not inject at all** on desktop-android
M151. Both JS and CSS injection fail, on first load and after reload.

## What IS wired (ruled out by static read)

- `ContentScriptsHandler` registered unconditionally
  (`extensions/common/common_manifest_handlers.cc:69`) → manifest `content_scripts`
  are parsed.
- Browser `UserScriptManager` created unconditionally
  (`chrome/browser/extensions/chrome_extension_system.cc:229`).
- Renderer `UserScriptSetManager` + `ScriptInjectionManager` created
  (`extensions/renderer/dispatcher.cc:307-310`).
- Desktop-android uses the SAME `ChromeExtensionSystem` (no separate android
  extension system exists in M151).

## Root cause (localized with instrumented build)

Added `LOG(ERROR)` traces to `Dispatcher::ActivateExtension` and
`Dispatcher::UpdateUserScripts` (renderer), incremental rebuild (48 s),
loaded the probe, captured logcat:

- **`UpdateUserScripts host=<probe-id>` FIRES** in the web-page renderer — the
  browser delivers the extension's content scripts to the renderer's
  `UserScriptSetManager`. Delivery works.
- **`ActivateExtension` NEVER fires** — the extension is never *activated* in the
  web-page renderer process. `ScriptInjectionManager` won't inject a user script
  whose host extension isn't active in that process.

So: scripts arrive, but the injection host is inactive → no injection.

### Why activation is skipped

`RendererStartupHelper::ActivateExtensionInProcess` (the thing that populates
`pending_active_extensions_` and sends `mojom::Renderer::ActivateExtension`) is
called from exactly one place —
`extensions/browser/extension_web_contents_observer.cc:197` — and only for frames
that **are themselves extension frames** (the code above it resolves
`GetExtensionFromFrame` and proceeds for extension-hosted frames). The in-code
comment (lines 189-196) notes activation is meant to happen "at the process
level" and is "redundant" for extension frames, and anticipates removal "once
site isolation is turned on."

On desktop, content-script extensions get activated in ordinary web renderers
through the process-level path. On desktop-android that path does not fire for
web-page renderers, so a content-script-only extension (uBO cosmetic, our probe)
is delivered but never activated → no content-script injection.

## Fix direction (next task)

Ensure content-script extensions are activated in web-page renderer processes on
desktop-android. Candidates, in order of preference:
1. Browser-side: when a web renderer is initialized/loads extensions
   (`RendererStartupHelper::InitializeProcess`), also activate extensions that
   have content scripts matching, not only extension-frame processes.
2. Trigger `ActivateExtensionInProcess` for content-script extensions on
   navigation for regular tabs (extend the observer path beyond extension
   frames).

Cross-check against Cromite's `Experimental-support-for-extensions-on-Android`
patch (GPL-2.0, read-as-map only) — cosmetic filtering works there, so it
addresses this activation path.

## Test-harness note

This whole diagnosis ran over the Playwright `_android` CDP harness + one
instrumented incremental build (48 s). The instrumentation was reverted from the
serv tree after use. The harness makes this loop fast: patch → 1-2 min build →
CDP probe → logcat.
