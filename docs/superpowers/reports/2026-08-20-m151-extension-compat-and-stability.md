# M151: extension compatibility matrix and Chromium stability suites

Date: 2026-08-20. Build: Chromium 151.0.7922.38 + `patches/m151/0001-0004`,
args variant `test`. Device: emulator API 35, `-gpu swiftshader_indirect`.

## Extension compatibility — 7/7

Each extension loaded alone in a clean profile
(`tests/harness/checks/extension-matrix.mjs`). "Listeners" is the number of
event listeners its background context registered — zero means the background
never finished starting, which is how the browserAction schema FATAL showed up
back in July.

| Extension | Manifest | Enabled | Listeners | API errors |
|---|---|---|---|---|
| uBlock Origin 1.73 | MV2 | yes | 21 | 0 |
| Dark Reader | MV2 | yes | 5 | 0 |
| Stylus | MV2 | yes | 19 | 0 |
| Violentmonkey | MV2 | yes | 19 | 0 |
| SponsorBlock | MV3 | yes | 10 | 0 |
| Stylus | MV3 | yes | 17 | 0 |
| Violentmonkey | MV3 | yes | 32 | 0 |

No `Unknown API` or extension-related FATAL lines for any of them.

Note on detection: an MV3 service worker sleeps, so its CDP target comes and
goes. Presence must be read from `chrome://extensions-internals`, not from
`/json/list` — the first version of the matrix reported a false failure for
Stylus MV3 on exactly this.

## extensions_unittests — 1649/1653

Run through the official runner (`out/.../bin/run_extensions_unittests`) against
the emulator, which was exposed to the build host over an ssh reverse tunnel
(`ssh -R 15555:localhost:5555`, then `adb connect localhost:15555`). The runner
matters: it pushes the 7873 test-data files a raw `am instrument` invocation
lacks, which is why the manual attempt died immediately.

First pass reported 515 failures, of which **512 were `TIMEOUT`** — the runner
batches tests into one command, and when the batch exceeds the shard timeout it
marks every test in the batch as failed. Its own output says so. Re-running with
`--shard-timeout 900`:

```
[==========] 1653 tests ran.
[  PASSED  ] 1649 tests.
[  FAILED  ] 4 tests
```

The remaining four were all `CRASHED`, with no stack or fatal signal in
logcat — the process was killed (`signal 9`). Run individually
(`isolate-crashers.sh`), **all four pass**:

| Test | Alone |
|---|---|
| `EventRouterTest.AddLazyListenerForUnloadedExtension` | PASSED |
| `EventRouterTest.RemovesOrphanedWebRequestEvents` | PASSED |
| `ExtensionSettingsFrontendTest.EmitUmaLevelDBMetrics` | PASSED |
| `ExtensionSettingsFrontendTest.OnSettingsChanged_RestrictToContextType` | PASSED |

So the batch failures are emulator resource pressure, not defects. On a machine
with more headroom the suite should be clean; treat a non-zero count here as
"re-run those tests alone before believing it".

## Monkey stress

`adb shell monkey -p com.danosito.afterbird --throttle 120 -v 3000` with uBO
loaded: 3000 events injected, same PID afterwards, zero crashes and zero ANRs in
the package-scoped logcat.

## Ad blocking: the turtlecute number was measuring the wrong thing

`adblock.turtlecute.org` probes with HEAD xhr requests. uBO answers most of them
with `redirect-rule=nooptext`, which sends the request to
`data:text/plain;base64,Cg==` — the ad host is never contacted, but the page
sees HTTP 200 and scores it "not blocked". A fully-filtering uBO scores 1-2%
there, and a stock desktop Chromium with the same extension scores ~8%.

Measured against a live ad-carrying page instead
(`www.dictionary.com/browse/test`):

| Configuration | Ad/tracker requests reaching the network |
|---|---|
| no extension | 13 |
| uBlock Origin | 1 |

`tests/harness/specs/adblock.mjs` now counts requests to ad hosts that reach the
network, treating both cancellation (`ERR_BLOCKED_BY_CLIENT`) and redirect to an
inert `data:` URL as neutralized.

The July reports' "85-95% on turtlecute" figures are not comparable to anything
measured after that site changed its probing method; ignore them.

## Reproducing

```
# expose the emulator to the build host
ssh -N -R 15555:localhost:5555 serv &
ssh serv 'adb connect localhost:15555'

# native suites
ssh serv 'cd ~/dev/afterbird-chromium-151/src &&
  out/ab_m151_test/bin/run_extensions_unittests --device localhost:15555 --shard-timeout 900'

# extension matrix + behaviour
cd tests/harness
node checks/extension-matrix.mjs
node checks/extension-function.mjs
```
