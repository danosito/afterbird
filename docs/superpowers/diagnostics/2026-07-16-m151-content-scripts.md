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

## Root cause — a renderer-startup RACE (proven with 5 instrumented builds)

NOTE: an earlier draft of this doc blamed extension *activation*. Instrumentation
disproved that — activation is not required for content-script injection (on
desktop it only fires for extension frames, yet content scripts inject fine).
The real cause is timing.

`LOG(ERROR)` traces were added along the injection path (incremental rebuilds
~40-70 s each) and correlated by renderer pid. For the web renderer that loaded
`example.com` (pid 27982), logcat ordering was:

```
pid=27982 [AB-CS] OnRenderFrameCreated
pid=27982 [AB-CS] InjectScripts loc=1 (document_start)  -> injcount 0
pid=27982 [AB-CS] InjectScripts loc=2 (document_end)    -> injcount 0
pid=27982 [AB-CS] InjectScripts loc=3 (document_idle)   -> injcount 0
pid=27982 [AB-CS OnUpdate] host=<probe-id>   <-- scripts arrive AFTER all 3
```

Ruled out along the way (all fire correctly): `OnRenderFrameCreated`,
`ScriptInjectionManager::InjectScripts` at every run location, `ExtensionFrameHelper`
present (`efh_null=0`). The injection machinery runs; it just finds an **empty
user-script set** (`UserScriptSetManager::scripts_` has no entry for the host
yet), because `OnUpdateUserScripts` — which populates it — is delivered to this
web renderer only *after* the page has already passed document_start / _end /
_idle.

### Why the scripts arrive late

Browser side: `UserScriptLoader::OnRenderProcessHostCreated` →
`SendUpdateIfNeeded` pushes the script shared-memory to a new renderer **only if
`initial_load_complete()`** (`extensions/browser/user_script_loader.cc:295-306`).
When the `example.com` renderer was created, the extension's manifest
content-script load (`UserScriptManager::OnExtensionLoaded` →
`ExtensionUserScriptLoader::AddScriptsForExtensionLoad`, async disk read) had not
completed, so nothing was sent at creation. The load finished later and
broadcast to all hosts (`user_script_loader.cc:483` AllHostsIterator) — that is
the late `OnUpdate`, arriving after injection was already over.

On desktop the manifest-script load completes well before the user navigates, so
new renderers get scripts at creation and injection works. On desktop-android the
initial load is not complete in time — the load appears to start late / not be
driven eagerly at extension load.

## Fix direction (next task)

Make the manifest content-script load ready before web renderers commit a
navigation. Candidates:
1. Confirm (next instrumented check) whether `UserScriptManager::OnExtensionLoaded`
   fires at extension-load time on desktop-android or lazily; if late/lazy, drive
   the `ExtensionUserScriptLoader` initial load eagerly at extension load so
   `initial_load_complete()` is true before the first navigation.
2. If the load is inherently async, have a newly-created web renderer re-run
   content-script injection when the pending user-script update for already-loaded
   extensions arrives (`OnUpdate`), instead of only at the initial run-locations.

Cross-check against Cromite's `Experimental-support-for-extensions-on-Android`
patch (GPL-2.0, read-as-map only) — cosmetic filtering works there, so it
addresses this timing.

All instrumentation was reverted from the serv tree; only the MV2 one-liner
remains.

## Test-harness note

This whole diagnosis ran over the Playwright `_android` CDP harness + a series of
instrumented incremental builds (~40-70 s each). The harness makes this loop
fast: patch → 1-min build → install → probe → logcat.
