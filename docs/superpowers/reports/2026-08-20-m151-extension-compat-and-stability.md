# M151: extension compatibility matrix and Chromium stability suites

Date: 2026-08-20. Build: Chromium 151.0.7922.38 + `patches/m151/0001-0004`,
args variant `test`. Device: emulator API 35, `-gpu swiftshader_indirect`.

## Extension compatibility — 9/9

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
| Bitwarden | MV3 | yes | 34 | 0 |
| uBO Lite | MV3 | yes | 4 | 0 |

No `Unknown API` or extension-related FATAL lines for any of them.

Note on detection: an MV3 service worker sleeps, so its CDP target comes and
goes. Presence must be read from `chrome://extensions-internals`, not from
`/json/list` — the first version of the matrix reported a false failure for
Stylus MV3 on exactly this.

## declarativeNetRequest needs a writable extension directory

Loading uBO Lite (MV3, DNR-based) from `/data/local/tmp` fails:

```
Failed to load extension from: /data/local/tmp/ext/ubolite.
ublock-filters.json: Internal error while parsing rules.
```

The message is misleading. That string is `kErrorPersisting`
(`extensions/browser/api/declarative_net_request/constants.cc`), and
`file_backed_ruleset_source.cc` emits it when `PersistIndexedRuleset` fails —
after indexing succeeded. Chromium writes the indexed ruleset into a
`_metadata/generated_indexed_rulesets` directory **next to the unpacked
extension**, and the browser process cannot write to `/data/local/tmp`
(`drwxrwx--x shell shell`).

Confirmed with a one-rule probe extension: it fails from `/data/local/tmp` and
loads from an app-writable directory, where `_metadata` then appears. Same for
uBO Lite with its 5414-rule main ruleset.

So any DNR extension must be loaded from a path the browser can write to:

```
adb shell "run-as com.danosito.afterbird cp -r /data/local/tmp/ext/ubolite \
  /data/data/com.danosito.afterbird/ubolite"
# then --load-extension=/data/data/com.danosito.afterbird/ubolite
```

This affects sideloading only — store-installed extensions live under the
profile directory, which is writable. Worth keeping in mind for Load Unpacked:
if the picker copies into a read-only location, every MV3 blocker will fail this
way.

## Functional behaviour — 4/4

Loading is not the same as working, so `checks/extension-function.mjs` drives
each extension until it produces an observable effect:

| Extension | Evidence |
|---|---|
| uBlock Origin | 7 ad requests neutralized, 2 reached the network (13 with no extension) |
| Dark Reader | 9 injected style nodes, page background darkened to `rgb(34, 36, 38)` |
| Violentmonkey | management UI rendered (44 nodes, script-install entries present) |
| Stylus | management UI rendered (41 nodes, style list and sort controls present) |

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

## chrome_public_unit_test_apk — 1365/1612, and why the rest do not count

231 failures, but **227 of them come from one line**:

```
Skia Gold comparison raised exception: --git-revision not passed and unable to determine from git
```

Render tests upload their screenshots to Skia Gold for comparison against
golden images, and Gold needs a git revision to key them by. The build tree is a
detached checkout of tag `151.0.7922.38`, so there is no revision to report and
every render test fails before any pixel is compared. This is infrastructure,
not the browser.

The four genuine failures are all upstream areas unrelated to extensions or to
anything the Afterbird patches touch:

| Test | Failure |
|---|---|
| `OptimizationGuidePushNotificationManagerUnitTest` (×2) | `expected:<[LITE_PAGE, LITE_VIDEO]> but was:<[PERFORMANCE_HINTS, LITE_PAGE, LITE_VIDEO]>` — feature-set mismatch |
| `ContactsPickerDialogTest#testNoSelection` | no contacts on the emulator |
| `AppModalPresenterTest#testDialogDimensionsWithNonZeroSystemBarsInsets` | emulator system-bar insets differ from the expectation |

Note that the runner also needs the test APKs installed: `ChromePublicUnitTest.apk`
is 511 MB and installing it over the ssh reverse tunnel exceeds devil's adb
timeouts, which denylists the device and kills the run before the first test.
Install it (and `ChromiumNetTestSupport.apk`) directly from the machine hosting
the emulator; the runner then skips installation on a checksum match.

## Monkey stress

`adb shell monkey -p com.danosito.afterbird --throttle 120 -v 3000` with uBO
loaded: 3000 events injected, same PID afterwards, zero crashes and zero ANRs in
the package-scoped logcat.

## Ad blocking: 100% once the filter lists match the device locale

**Result: `adblock.turtlecute.org` reports 132/132 = 100% blocked** on a clean
profile with uBlock Origin 1.73, no bypass flags.

Getting there took a wrong turn worth recording. On an `en-US` emulator the same
setup scored **1%**, and the first explanation — that uBO answers the page's HEAD
probes with `redirect-rule=nooptext` so the page misreads them — was only half
right. The network-level truth on that run was: 102 requests redirected to an
inert `data:` URL, 0 cancelled, and **24 ad/tracker requests reaching their
servers**. Those 24 were the real problem, and the logger showed uBO applying no
filter at all to them: appmetrica.yandex.ru, ads-api.tiktok.com, udcm.yahoo.com,
*-analytics-events.apple.com, and similar.

The cause is filter-list selection, not the browser. uBO auto-selects regional
lists from `navigator.language`. The emulator ran `en-US`, so `RUS-0` was never
enabled while turtlecute probes a large number of RU/CN/regional hosts. Kiwi on
the (Russian) test phone had 16 lists selected against our 11 — that is the
entire difference between its 99% and our 1%.

Verified both directions:

| Configuration | turtlecute | ad requests reaching network |
|---|---|---|
| `en-US`, 11 lists (uBO default for that locale) | 1% | 24 |
| `en-US`, lists added manually (RUS-0, annoyances) | 100% | 0 |
| `ru-RU`, clean profile, uBO auto-selects RUS-0 | 100% | 0 |

The locale plumbing is correct: with the system set to `ru-RU`,
`navigator.language` reports `ru-RU` (after a device reboot — `setprop` alone
does not apply it) and uBO picks up `RUS-0` on its own.

Takeaway for testing: a low turtlecute score means "check which lists are
enabled" before suspecting the browser. Compare `µBlock.selectedFilterLists`
against the reference browser's before drawing any conclusion.

## Historical note: why the earlier turtlecute numbers disagreed

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
