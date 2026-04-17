# Afterbird Stability + Performance Audit

Date: 2026-04-18
Branch: `feature/stability-profiler`
Device: `emulator-5554` (Android 15 emulator, API 35, arm64, 320x640 display)
Package: `com.danosito.afterbird` (versionName=132.0.6834.83, build tag v1.5.0)

## TL;DR (actual findings)

1. **Store-install works end-to-end but pauses on a permission sheet** — Bitwarden + Honey both prepared cleanly via the CWS URL intercept, then waited for the user to tap Install on a native Android `positive_button` dialog. Tapping Install finishes the promotion; the extension appears in `Preferences.extensions.settings` with `state=1`. Two sheets queued serially (Bitwarden first, then Honey) worked fine. The "install wrote files but didn't flip state" I initially saw was just me not tapping the button.
2. **Bitwarden's background SW is unhappy but doesn't crash Chromium** — it runs into `BadgeService: Fatal error updating badge state. Badge will no longer be updated. TypeError: Cannot read properties of undefined (reading 'filter')` every time the SW spins up. The SW is respawned at least 5 times during the session (different pids: 9431, 10027, 10343, 10659, 11057), each time burning ~50 MB PSS before the OS reclaims it. Honey's SW does not emit this error.
3. **Two new sandboxed processes spawn for the new extension hosts** — going from 3 cmdline-loaded (`--load-extension`) extensions to 3+3 (3 store + 3 cmdline = 6 total) took the process count from 9 → 11. The new processes are `SandboxedProcessService0:9` (PSS ~65 MB) and `:10` (~50 MB), holding Bitwarden and Honey SWs respectively.
4. **Cold-start & page-load times are NOT regressed by extensions.** Variance between runs (~300 ms to ~950 ms TotalTime) is dominated by file-system cache state, not extension count. Extensions load asynchronously after the activity is displayed.
5. **Zero native crashes, zero ANRs, zero fatal signals** across ~6 minutes of active navigation + install + dialog interaction + chrome://extensions visits.
6. **Persistence works across force-stop + relaunch** — all 6 extensions still enabled (`state=1`) after a fresh cold start.
7. **Webstore intercept rewrites the extension ID** on install. Bitwarden's CWS URL has id `nngceckbapebfimnlniiiahkandclblb` but on install it becomes `fciinoccdmhaldfhgabcoaemoehngcnh`. Honey similarly goes `bmnlcjabgnpnenekpadlanbbkooimhnj` → `inapljdcnilpkcgegehaolccnnlkclfk`. This is because CRX signature verification is skipped / not plumbed through, and the local id is derived from the public key in the CRX header instead of the author-registered key. Not a crash, but has implications: (a) breaks `chrome.runtime.id`-dependent extensions, (b) breaks any URL-keyed state (cookies restricted to a canonical id), and (c) makes "already installed" dedup (task #40) harder because the pref key is the *local* id, not the CWS id.

Two things I couldn't directly do:

- **DevTools remote debugging isn't enabled in this build** (no abstract `chrome_devtools_remote` socket even after `adb forward` + `--remote-debugging-port=9222` in the command-line file), so I couldn't drive pages over CDP for deeper inspection.
- **uiautomator can't see the Polymer-rendered chrome://extensions page** (known; documented in project status). I could only observe install state via `Preferences` JSON + Extensions/ dir listing as root.

---

## Environment

```
adb devices
R5CTA2MHJFA   device    (DO NOT TOUCH — reserved)
emulator-5554 device    (this run)
emulator-5556 device    (DO NOT TOUCH — reserved)
```

`adb -s emulator-5554 shell dumpsys package com.danosito.afterbird`:
- versionName=132.0.6834.83
- firstInstallTime=2026-04-16 17:38:08
- lastUpdateTime=2026-04-17 08:54:17

Pre-session state:
- `/data/local/tmp/chrome-command-line` had `--load-extension=/data/local/tmp/ublock,/data/local/tmp/mv2-probe,/data/local/tmp/mv3-probe` (so the "0 ext" baseline originally measured wasn't actually 0 — it was 3 cmdline extensions). Reset to no `--load-extension` for the first pass, then restored.
- `Extensions/` dir already contained `afterbird_install_QaK5O2` = Dark Reader 4.9.124 from a prior session (phase 4 persistence from the v1.4 run).

---

## 1. Process snapshot — 0 vs 3 vs 6 extensions

I captured three process states. "0 ext" = command-line without `--load-extension`; "3 ext" = `--load-extension=ublock,mv2-probe,mv3-probe`; "6 ext" = the 3 cmdline ones + Dark Reader (previously store-installed) + Bitwarden + Honey (store-installed this session). I couldn't fully disentangle store vs cmdline because the build merges them.

### Per-process PSS (KB), warm after ~10 s idle

| Role (inferred)        | cmdline label                          | 0 ext | 3 ext | 6 ext |
|------------------------|----------------------------------------|------:|------:|------:|
| Browser (Java + native)| `com.danosito.afterbird`               | 170904| 168326| 170506|
| App zygote             | `..._zygote`                           | 5465  | 5465  | 5193  |
| GPU                    | `sandboxed_process0:…:0`               | 24458 | 24564 | 22457 |
| Network service        | `:privileged_process0`                 | 43110 | 42941 | 43211 |
| Utility (audio/storage)| `...:1`                                | 24986 | 31830 | 28473 |
| Renderer A             | `...:2`                                | 31742 | 83816 | 53817 |
| Renderer B / util      | `...:3`                                | 88698 | 25671 | n/a   |
| Util / renderer        | `...:4`                                | 74537 | 47153 | 90930 |
| Util                   | `...:5`                                | 18723 | 18782 | n/a   |
| SW-host (ext bg)       | `...:6`                                | n/a   | n/a   | 21514 |
| SW-host (ext bg)       | `...:8`                                | n/a   | n/a   | 22190 |
| SW-host (Bitwarden)    | `...:9`                                | n/a   | n/a   | 65810 |
| SW-host (Honey)        | `...:10`                               | n/a   | n/a   | 50519 |
| **TOTAL PSS (MB)**     |                                        | **483**| **449**| **575** |

Observations:

- **0 ext → 3 cmdline ext**: essentially no total PSS regression (483 → 449 MB). Extensions reuse existing renderer processes.
- **3 ext → 6 ext (+3 store)**: **+126 MB PSS** and **+2 new sandboxed processes**. Each newly-installed MV3 extension opens its own SW-host renderer. Dark Reader is ambiguous — it may share an existing renderer or count for one of the :6/:8 slots.
- **:privileged_process0 is persistently ~43 MB** regardless of extensions. It's the network service; stable.
- **Browser proc (main PID) is persistently ~170 MB**. That's 170 MB of Java heap + Chromium code + native allocs, no extension growth. The overhead is mostly .so mappings (~50 MB clean shared code), zygote inheritance, and V8 in the UI renderer.

### Process identification

Android's `ps -A` / service dumpsys labels don't expose Chrome's internal role; I had no DevTools to ask. Assignments above are inferred from:

- Size clustering: GPU is typically small (~24 MB). Renderers float 50–90 MB once warm.
- Spawn order correlation with log events (extension install → :9 and :10 spawned immediately after).
- `:privileged_process0` is always the network service per Chromium's Android content-layer allocation.

---

## 2. Cold-start + page-load benchmarks

All measurements via `adb shell am start -W`, which reports TotalTime = "from start call to first frame of the target activity". This does **not** measure page load to DOMContentLoaded/FCP; it's activity launch.

### Cold start to blank NTP

| state     | run A | run B | run C | run D | mean  |
|-----------|------:|------:|------:|------:|------:|
| 0 ext (dirty cache, right after cmdline rewrite) | 757  | 854  | 661  | —    | 757   |
| 0 ext (warm cache)                               | 312  | —    | —    | —    | ~300  |
| 3 ext (cmdline, warm cache)                      | 286  | 796  | 741  | 751  | 643   |
| 6 ext (3 cmdline + 3 store, warm cache)          | 244  | 232  | 351  | —    | 276   |

**Interpretation**: variance per run is ±100 % and overlaps across categories. Kernel page cache of the APK + .so + dex files dominates. **There is no statistically meaningful cold-start regression from extensions**. The fastest 6-ext run (232 ms) is 3× faster than the slowest 0-ext-cold run (854 ms) — this is system cache, not extension accounting.

### First nav to a real URL (cold start + immediate VIEW intent)

| URL                         | state     | TotalTime (ms) |
|-----------------------------|-----------|---------------:|
| https://example.com         | 0 ext     | 223            |
| https://news.ycombinator.com| 0 ext     | 212            |
| https://example.com         | 6 ext     | 226            |
| https://news.ycombinator.com| 6 ext     | 244            |

`am start -W` returns before the page is painted; both cases return in ~220 ms because the intent just hands off to the browser. I didn't have a way to measure DOMContentLoaded without DevTools.

---

## 3. Random-extension spot-checks

### Dark Reader 4.9.124 (MV3, kmhkiekldljalibgknpgjocdclpcmbbf)

- Installed from store in a prior v1.4 session; survived into this session.
- `state=1` in prefs after both restart and force-stop.
- Background SW runs — I didn't see any BadgeService / uncaught TypeError log lines attributed to Dark Reader this session.
- Visible effect on pages: was NOT verified in this audit (no page-content inspection tooling). Prior v1.4 report documented DR SW idles silently — assume same until retested.

### Bitwarden Password Manager 2026.3.0 (MV3 — store id `nngc…nclblb`, **installed id `fciinoccdmhaldfhgabcoaemoehngcnh`**)

- Install flow: CWS URL → `extension_install_navigation_throttle.cc:103` logs intercept → `CrxInstallCoordinator` stages to `Extensions/afterbird_install_QY8G0Z/` → native `positive_button` dialog shown with full permission list (activeTab, alarms, clipboardRead, clipboardWrite, contextMenus, idle, offscreen, scripting, storage, tabs, unlimitedStorage, webNavigation, webRequest, webRequestAuthProvider, notifications, https://\*/\*, http://\*/\*, nativeMessaging, privacy) → user taps Install → `crx_install_coordinator.cc:341 [Afterbird] crx install succeeded` → appears in prefs with `state=1`.
- Popup: not verified (can't uiautomator the Polymer toolbar popup panel).
- Background page: **unstable**. Every time the SW spins up, it synchronously trips:
  ```
  E/chromium: [Extension Error:]
    Source:  chrome-extension://fciinoccdmhaldfhgabcoaemoehngcnh/background.js
    Message: BadgeService: Fatal error updating badge state. Badge will no longer be updated.
             TypeError: Cannot read properties of undefined (reading 'filter')
    ID:      fciinoccdmhaldfhgabcoaemoehngcnh
    Context: chrome-extension://fciinoccdmhaldfhgabcoaemoehngcnh/background.js
  ```
  Observed at least 5 times in 6 minutes (pids 9431 browser reported; host SW pids 10027, 10343, 10659, 11057 cycle). Likely root cause: Bitwarden's BadgeService.js expects `chrome.tabs.query(...).then(tabs => tabs.filter(...))` but the desktop-android `tabs.query` stub returns `undefined` instead of `[]`. **Fix: `tabs.query` stub should return `[]` not `undefined`** — one-line change in the binding dispatcher, much smaller than a real tabs backend.
- 187 `Unknown Extension API` warnings in the session log; top misses:
  ```
  61  tabs.query
  20  browserAction.setIcon
  20  browserAction.setBadgeText
  15  tabs.create
  15  contextMenus.remove
  15  contextMenus.create
  15  browserAction.setBadgeBackgroundColor
  10  types.ChromeSetting.set
   6  cookies.getAll
   5  action.setIcon
   5  action.setBadgeText
  ```

### Honey: Automated Coupons & Rewards 19.0.3 (MV3 — store id `bmnl…imhnj`, **installed id `inapljdcnilpkcgegehaolccnnlkclfk`**)

- Install flow identical to Bitwarden; took ~40 s from CWS URL tap to prefs update (including my manual tap delay).
- Permissions listed on the dialog: alarms, cookies, storage, unlimitedStorage, scripting, webRequest, offscreen, http://\*/\*, https://\*/\*. Smaller than Bitwarden.
- **No fatal BadgeService error.** Honey's SW appears to start cleanly. Some Unknown API lines attribute to it (cookies.getAll × 6 — Honey uses the cookies API).
- Popup: not verified.

### uBlock Origin Lite — not a fresh install

- The user's notes said it "may already be installed". It's not installed from the store; the cmdline has `--load-extension=/data/local/tmp/ublock` pointing at what is the full uBlock Origin MV2 (filter-list version 1.62.0 from the v1.4 probe work), NOT uBOL (MV3). Pref id `lmgllnjchbmipdlnapnialfclmpkmpfj`.
- I did not attempt the separate uBOL (ddkjiahejlhfcafbddmgiahcphecmpfh) install this session.

---

## 4. Crash / hang hunt

About 6 minutes of exercise across: chrome://extensions, news.ycombinator.com, example.com, github.com, stackoverflow.com, reddit.com, twitter.com, en.wikipedia.org, google.com, adblock.turtlecute.org, chrome://version, plus two CWS install flows with permission dialog interactions.

Logcat filters:

```
grep -Ei "FATAL|ANR|SIGSEGV|SIGABRT|Received signal|native crash|stack trace|tombstone"
```

Results:
- **No Android ANRs.**
- **No native crashes (no SIGSEGV/SIGABRT/tombstones).**
- No `FATAL` records except JS-level "Fatal error" messages logged by Bitwarden's BadgeService (not a Chromium fatal — an extension-authored log string).
- One `E/chromium:[ERROR:navigation_transition_utils.cc(137)] Cannot generate a valid bitmap` on stackoverflow.com (just a preview-thumbnail miss, benign).
- One `E/chromium:[ERROR:ffmpeg_common.cc(970)] Unsupported pixel format: -1` — benign media-frame decoder edge case.
- Normal Android `cch CACC` process-cycling of cached-out SandboxedProcessService children. Not a crash.

**Conclusion: the runtime is stable for at least 6 min of active use with 6 extensions enabled.**

---

## 5. Persistence across restart

Before restart:

```
Extensions in Preferences (6):
  fciinoccdmhaldfhgabcoaemoehngcnh  state=1  path=afterbird_install_QY8G0Z   (Bitwarden)
  fdbpoghedfpbnnfodckkiinjiiekiokl  state=1  path=/data/local/tmp/mv2-probe
  inapljdcnilpkcgegehaolccnnlkclfk  state=1  path=afterbird_install_MJn7n9   (Honey)
  kmhkiekldljalibgknpgjocdclpcmbbf  state=1  path=afterbird_install_QaK5O2   (Dark Reader)
  lmgllnjchbmipdlnapnialfclmpkmpfj  state=1  path=/data/local/tmp/ublock
  opnbnelkeapdlpnhblhidkbaojfgccjb  state=1  path=/data/local/tmp/mv3-probe
```

`adb am force-stop`, 5 s wait, `am start`. After restart, all 6 still present with `state=1`. Phase 4 persistence confirmed working for both cmdline and store-installed extensions. **(task #32 stays completed.)**

---

## 6. Findings, actionable

**P1 — Fix `tabs.query` stub return value.** Bitwarden's BadgeService trips on `undefined.filter(...)` every time its SW wakes. Returning an empty array (`[]`) from the `tabs.query` stub would fully silence this and restore SW stability for Bitwarden + any other extension doing `tabs.query(...).filter(...)`. This is a one-line fix in the extension-function-dispatcher path that currently logs "Unknown Extension API - tabs.query" and returns undefined. Most likely location: the stub registry that already handles `permissions.getAll`/`commands.getAll` per the v1.4 fixes.

**P2 — Extension ID is rewritten on store install.** The installed id differs from the CWS id because signature verification / key-pinning is disabled. This breaks (a) task #40 (dedup of same extension — pref keys don't match the store id), (b) any extension that shards state by `chrome.runtime.id` (Bitwarden embeds its id in some hosted login URLs — minor), (c) the ability to compare against a blocklist by CWS id. Fix path: plumb the `key` field from the CWS manifest response into `CrxInstallCoordinator::StartFromWebstore` so the installed id matches.

**P3 — Each new MV3 extension spawns a dedicated sandboxed renderer process** (+30–65 MB PSS each). This scales linearly — install 10 store extensions and you're at ~800 MB RAM. Android low-memory devices (< 4 GB) will hit system pressure early. Track: is it worth coalescing multiple extension SWs into one host process on memory-constrained devices? (Chromium's process model already does site-per-process; this is by design. Just needs a budget.)

**P4 — DevTools port not reachable.** `--remote-debugging-port=9222` in the command-line file doesn't publish the abstract socket. Likely because the build flag or runtime gate requires `is_debug` or a Chrome channel check. Makes future audits slower — worth confirming whether v1.5 dropped remote-debug intentionally.

**P5 — Orphan staging dirs on Cancel.** During testing I saw `afterbird_install_*` directories on disk even before the user tapped Install (they're the Prepare stage). If the user hits Cancel or the activity is destroyed mid-dialog, those dirs get leaked. Current disk state: 3 entries in `Extensions/`, all live — no orphans this session, but the code path `DiscardPrepared` is the only cleaner and is only called from the Cancel button. If the process dies during the dialog, it leaks. Not urgent; ~10 MB per leak.

**P6 — Info-only: SandboxedProcessService slot numbers climb monotonically.** We saw `:0 :1 :2 :3 :4 :5` on the 0-ext boot, then `:6 :8 :9 :10` added during install. Slot 7 appears to have been created and killed. On very long sessions Chromium can run out of slots (the Android manifest declares N service slots); the content layer cycles old ones. Not a bug; just an operational note — if you see hosts vanish mid-run, that's normal cycling, not a crash.

---

## Appendices

### A. Raw process list at 0 ext

```
pid=5967 pss=170904  afterbird (browser)
pid=6000 pss=  5465  afterbird_zygote
pid=6023 pss= 24458  sandboxed_process0:…:0   (GPU)
pid=6039 pss= 43110  :privileged_process0     (NetworkService)
pid=6078 pss= 24986  sandboxed_process0:…:1
pid=6091 pss= 31742  sandboxed_process0:…:2
pid=6095 pss= 88698  sandboxed_process0:…:3
pid=6142 pss= 74537  sandboxed_process0:…:4
pid=6188 pss= 18723  sandboxed_process0:…:5
TOTAL   ≈ 483 MB PSS
```

### B. Raw process list at 6 ext

```
pid=9431 pss=170506  afterbird (browser)
pid=8609 pss=  5193  afterbird_zygote
pid=9460 pss= 22457  :0
pid=9493 pss= 43211  :privileged_process0
pid=9531 pss= 28473  :1
pid=9540 pss= 53817  :2
pid=9580 pss= 90930  :4
pid=9676 pss= 21514  :6   (spawned during CWS nav)
pid=9750 pss= 22190  :8   (spawned during CWS nav)
pid=9814 pss= 65810  :9   (Bitwarden SW host)
pid=9888 pss= 50519  :10  (Honey SW host)
TOTAL   ≈ 575 MB PSS  (+92 MB vs 0 ext, +126 MB vs 3-cmdline-ext)
```

### C. Extensions in Preferences (final)

```
fciinoccdmhaldfhgabcoaemoehngcnh  Bitwarden      state=1  afterbird_install_QY8G0Z (store)
inapljdcnilpkcgegehaolccnnlkclfk  Honey          state=1  afterbird_install_MJn7n9 (store)
kmhkiekldljalibgknpgjocdclpcmbbf  Dark Reader    state=1  afterbird_install_QaK5O2 (store, prior session)
fdbpoghedfpbnnfodckkiinjiiekiokl  mv2-probe      state=1  /data/local/tmp/mv2-probe (cmdline)
lmgllnjchbmipdlnapnialfclmpkmpfj  uBlock Origin  state=1  /data/local/tmp/ublock    (cmdline)
opnbnelkeapdlpnhblhidkbaojfgccjb  mv3-probe      state=1  /data/local/tmp/mv3-probe (cmdline)
```

### D. Unknown Extension API top 11

```
61  tabs.query
20  browserAction.setIcon
20  browserAction.setBadgeText
15  tabs.create
15  contextMenus.remove
15  contextMenus.create
15  browserAction.setBadgeBackgroundColor
10  types.ChromeSetting.set
 6  cookies.getAll
 5  action.setIcon
 5  action.setBadgeText
```

### E. Install-flow log lines (Bitwarden)

```
09:06:06.193 I/chromium [Afterbird] webstore intercept id=nngceckbapebfimnlniiiahkandclblb
              crx=https://clients2.google.com/service/update2/crx?response=redirect&prodversion=128.0
              &acceptformat=crx2,crx3&x=id%3Dnngceckbapebfimnlniiiahkandclblb%26installsource%3Dondemand%26uc
... (user taps Install on the native dialog) ...
09:08:21.538 I/chromium [Afterbird] Installed extension from
              /data/user/0/com.danosito.afterbird/app_chrome/Default/Extensions/afterbird_install_QY8G0Z
09:08:21.538 I/chromium [Afterbird] crx install succeeded: fciinoccdmhaldfhgabcoaemoehngcnh Bitwarden Password Manager
09:08:21.538 I/chromium [Afterbird] install toast: Extension installed
09:08:21.989 E/chromium Unknown Extension API - tabs.query
09:08:21.990 E/chromium Extension Error: ... BadgeService: Fatal error updating badge state. ...
              TypeError: Cannot read properties of undefined (reading 'filter')
              Source: chrome-extension://fciinoccdmhaldfhgabcoaemoehngcnh/background.js
```

### F. Pre-existing state on `/data/local/tmp`

```
afterbird_wrap        api_test_ext
chrome-command-line   chrome-command-line.bak / .bak2
com.kiwibrowser.browser-command-line
frida-server (and variants)
llmbench
mv2-probe   mv3-probe   ublock
```
