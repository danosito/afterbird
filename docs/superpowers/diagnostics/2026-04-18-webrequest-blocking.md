# webRequest blocking doesn't block (uBO 3–4 % vs 97 % on Kiwi)

Followed-up on the parity-audit P1 blocker (`ab_adtest2.png`, 2 %).

## What we can confirm works

Added temporary `LOG(INFO)` traces on
`extensions/browser/api/web_request/web_request_api.cc` on
`feature/v1.8-webrequest-trace` (not merged). Observed on the physical
device (`R5CTA2MHJFA`), cold-launch to `https://adblock.turtlecute.org/`,
with uBlock Origin 1.62.0 loaded via `--load-extension`:

1. **`OnExtensionLoaded`** fires for uBO; `HasAnyWebRequestPermissions`
   returns `true` (it has `webRequest` + `webRequestBlocking`);
   counter increments.
2. **`MayHaveProxies()`** flips to `true`; `ResetURLLoaderFactories()`
   runs via `UpdateMayHaveProxies`.
3. **`MaybeProxyURLLoaderFactory`** is called for the real test-page
   sub-resources (`type=2 kDocumentSubResource`,
   `initiator=https://adblock.turtlecute.org`). We reach the
   `WebRequestProxyingURLLoaderFactory::StartProxying` call.
4. uBO **adds an `onBeforeRequest` listener** with
   `extra_info_spec = BLOCKING (1<<2)` and `success=1` from
   `WebRequestEventRouter::AddEventListener`.

So: extension is loaded with the right permissions, the proxy installs,
and the listener is registered with blocking. The whole happy path from
the source code looks wired.

## What still doesn't work

The turtlecute test reports **4 / 133 blocked (3 %)** — identical to the
pre-v1.7 state. Kiwi 137 on the same test page under the same uBO 1.62
blocks ~130 / 133.

## Hypotheses to chase next

(Ordered by cheapness.)

1. **The proxy is installed but events never reach uBO's JS.** A single
   `LOG(INFO)` in `WebRequestProxyingURLLoaderFactory::InProgressRequest::
   OnBeforeRequest` would confirm whether the router dispatches. If it
   does, the gap is in the renderer-side binding. If it doesn't, the gap
   is in the dispatch path.
2. **Permission check fails at event-delivery time.** The listener is
   registered, but when the proxy fires `OnBeforeRequest`,
   `WebRequestPermissions::CanExtensionAccessURL` might return false
   because the active-tab / host-permissions path on desktop-android
   doesn't know about the loaded extension's hosts. Check
   `web_request_permissions.cc::CanExtensionAccessURL`.
3. **Renderer-side webRequest binding is wired to
   `WebRequestInternalEventHandledFunction` but the renderer never replies
   with `cancel=true`.** Instrument
   `WebRequestInternalEventHandledFunction::Run` to see if it ever fires
   for uBO events. If not, the render-side JS is running but the reply
   never ships back.
4. **Process-type gating.** `extensions/browser/process_map.cc` decides
   whether a given render-process can host an extension. If the extension
   SW / background for uBO isn't running in a process the event-router
   trusts, dispatch is silently dropped.

Any of these would be 10–50 LOC to instrument. Not a one-line fix.

## Diagnostic instrumentation left in tree

On `feature/v1.8-webrequest-trace`:

- `extensions/browser/api/web_request/web_request_api.cc`:
  - `OnExtensionLoaded`: id + name + has_web_request_perms
  - `MaybeProxyURLLoaderFactory`: call#, type, may_have, initiator
  - `AddEventListener`: ext, event, sub_event, extra_info_spec, success

Branch is **not merged**. Keep it as a ready-to-go probe for the next
pass.

## Why this lands in v1.8

The stubs (v1.6 → v1.7) fixed every "Unknown Extension API" that uBO
trips over at startup. The remaining blocker is an internal dispatch
issue, not an API registration one, so the stubs don't actually unlock
it. Shipping v1.8 without the uBO fix is honest about what the stubs
could and couldn't achieve.
