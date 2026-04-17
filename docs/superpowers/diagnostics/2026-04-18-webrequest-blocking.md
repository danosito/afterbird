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

## Narrowed root cause

Added `LOG(INFO)` to
`WebRequestProxyingURLLoaderFactory::InProgressRequest::RestartInternal`
at the `WebRequestEventRouter::OnBeforeRequest()` call site. Observed:

- **`OnBeforeRequest` fires 263 times** across a single cold-launch pass
  for extension-internal URLs AND every turtlecute sub-resource.
- **Every single call returns `net::OK`.** Not one returns
  `ERR_BLOCKED_BY_CLIENT` (synchronous cancel) or `ERR_IO_PENDING`
  (async pending for listener reply).

Means the router path runs, but `GetMatchingListeners(...)` returns an
empty vector for every request, so nothing is dispatched to uBO — the
router falls through to `net::OK` without asking the listener.

## Why `GetMatchingListeners` returns empty

Next-pass hypothesis — the listener IS registered (`success=1`,
`extra_info_spec=BLOCKING`) but at dispatch time one of these rejects:

1. **`CanExtensionAccessURL`** inside `GetMatchingListeners` checks host
   permissions. If `permissions_data()` on desktop-android doesn't
   populate effective host permissions for `--load-extension` extensions,
   every URL is denied.
2. **Event name keying.** uBO registered for
   `sub_event_name=webRequest.onBeforeRequest/g1` (sub-event with
   `/g<seq>` suffix). If the router is keyed on the base event name but
   matches on sub-event, or vice-versa, listeners never match.
3. **`data_[ctx_id].active_listeners`** may be segregated by
   `ExtensionsBrowserClient::GetOriginalContext` and the dispatch site
   looks up the wrong bucket.

To confirm: instrument `GetMatchingListeners` with:

    LOG(INFO) << "GetMatchingListeners url=" << request->url
              << " listeners_total=" << <all_listeners_count>
              << " listeners_matched=" << result.size();

and a per-reject `LOG(INFO)` on the filter-in predicate inside that
function. Then we can see which check rejects.

Any of (1)-(3) is a targeted fix (10–30 LOC). Not shipping a fix in
v1.7; logged for v1.8.

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
