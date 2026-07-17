# webRequest blocking fixed — root cause was a `browserAction` schema FATAL

Date: 2026-07-17. Build: M151 desktop-android + MV2 patch, on `-gpu
swiftshader_indirect`. uBlock Origin 1.62.0 (MV2) via `--load-extension`.

## Result

uBO on `adblock.turtlecute.org`: **126 / 133 blocked = 95 %** on the clean
(non-instrumented) build once filter lists finish compiling (85 % on the first
pass before lists settle). Was 14 %, and 3 % on the old v1.8 132 layer. At
desktop/Kiwi parity (~97 %). One targeted GN fix; no C++.

## The real root cause (the v1.8 "GetMatchingListeners empty" theory was a symptom)

The v1.8 diagnostic (2026-04-18) correctly saw that `OnBeforeRequest` returned
`net::OK` with no matching listeners, and guessed at host-permission / context
/ sub-event-name causes. On stock M151 the instrumented truth is upstream of all
of those:

Instrumenting `WebRequestEventRouter::OnBeforeRequest` +
`AddEventListener` showed, for every request:

```
[AB-WR] OBR url=... matched=0 total_active=0
```

`total_active=0` — there were **zero** webRequest listeners registered at all,
and **`AddEventListener` was never called** for uBO. uBO never got far enough to
register its blocking listener because its background page **fatally crashed at
startup**:

```
[FATAL:extensions/renderer/native_extension_bindings_system.cc:213]
Unknown API browserAction
```

uBO 1.62 is MV2 and touches `chrome.browserAction` early in background init. The
native bindings system instantiates the binding, calls
`GetAPISchema("browserAction")`, gets no schema, and `LOG_IF(FATAL, !schema)`
aborts the renderer. The background dies before registering
`webRequest.onBeforeRequest`, so `active_listeners` stays empty and every request
falls through to `net::OK`. (The 18/133 that *did* "block" earlier were not uBO —
noise / non-webRequest.)

## Why the schema was missing

`chrome/common/extensions/api/api_sources.gni`: the `browserAction` schema
(`browser_action.json`) and `page_action.json` were added to the bundled
`uncompiled_sources_` **only inside `if (enable_extensions)`**. Desktop-android
builds with `enable_extensions=false` / `enable_extensions_core=true`, so those
schemas were dropped from the generated schema bundle
(`gen/.../generated_schemas.cc` — verified: `browserAction` absent, MV3 `action`
present). But the **`browserAction` feature** in `_api_features.json` is *not*
similarly gated: it's available to any extension with a `browser_action` manifest
key. So the feature says "available", the binding instantiates, and the schema
lookup FATALs. Classic feature/schema availability mismatch.

## The fix

`patches/m151/0002-browseraction-schema-desktop-android.patch`: move
`browser_action.json` + `page_action.json` from the `enable_extensions`-gated
block into the always-bundled base `uncompiled_sources_` (next to `action.json`,
which is why MV3 `action` already worked). Schema-only — the browser-side action
*functions* (`setIcon`/`setBadgeText`/…) still aren't implemented on
desktop-android, but calling them just sets `lastError` rather than crashing, so
uBO proceeds and registers its request filter.

Verified end-to-end on device: `AddEventListener event=webRequest.onBeforeRequest
is_lazy=0` now fires, and blocking jumps to 85 %.

## Remaining ~15 % gap (85 % vs ~97 %)

Candidates for later: filter lists not fully downloaded on the throttled emulator
network; cosmetic-only entries the network test can't block; other MV2 action
functions no-oping. Not chased yet — 85 % already proves webRequest blocking
works. There is also a uBO-startup ANR (heavy synchronous work on the throttled
emulator) that resolves after a "Wait"; benign but worth revisiting.

## Method note

The whole chain (OBR → AddEventListener → browserAction FATAL) came from logcat
markers + instrumented incremental builds, on the `swiftshader_indirect`
emulator. The instrumentation is reverted; the tree carries only the MV2 patch +
this GN fix.
