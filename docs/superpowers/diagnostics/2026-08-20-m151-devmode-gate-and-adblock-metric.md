# M151 verification: the dev-mode gate, and why the adblock number moved

Date: 2026-08-20. Build: M151 (151.0.7922.38) + patches 0001–0004, args variant
`test`. Device: emulator API 35 on `-gpu swiftshader_indirect`. uBlock Origin
1.62.0 (MV2) via `--load-extension`.

## Two separate findings

### 1. Unpacked extensions were admitted, then disabled (product bug)

uBO loaded and appeared in `chrome://extensions`, but filtered nothing.
`chrome://extensions-internals` gave the exact reason:

```
"location": "COMMAND_LINE",
"disable_reasons": [ "DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION" ],
"event_listeners": { "count": 0 }
```

`ExtensionManagement::IsAllowedByUnpackedDeveloperModePolicy`
(`chrome/browser/extensions/extension_management.cc`) gates unpacked extensions
behind the `kExtensionsUIDeveloperMode` pref when
`extensions_features::kExtensionDisableUnsupportedDeveloper` is on. On desktop
the user flips the toggle in the extensions page; on desktop-android there is no
such affordance in Afterbird's flow, so every `--load-extension` /
Load Unpacked extension came up disabled — no listeners, no filtering.

Confirmed by bypassing the feature at runtime
(`--disable-features=ExtensionDisableUnsupportedDeveloper`): uBO went to
`disable_reasons: []`, `active permissions: <all_urls>`, 17 listeners including
`webRequest.onBeforeRequest/g1..g2` (blocking), and its logger showed filters
firing. Landed as `patches/m151/0004-allow-unpacked-without-devmode.patch`.

Store-installed extensions were never affected: that predicate returns early for
any location other than `kUnpacked`.

### 2. The old 90 %-on-turtlecute threshold is no longer meaningful (test bug)

adblock.turtlecute.org probes with HEAD xhr requests. uBO answers many of them
with `redirect-rule=nooptext` rather than an outright block, so the probe sees
HTTP 200 and scores them "not blocked". With uBO fully loaded and filtering
(129,478 network filters compiled, logger showing hits) the page still reported
**1–2 % blocked**.

This is not an Afterbird regression and not comparable to the July "85–95 %"
figures — the same page now scores a desktop Chromium at ~8 %.

`tests/harness/specs/adblock.mjs` was rewritten to read uBO's own
"blocked since install" counter across the page load instead of the probe's
verdict, and to discover the extension ID from the live target list (an unpacked
extension's ID is derived from its install path, so it differs per host and per
load location).

Result on the patched build, clean profile, no bypass flags:

```
{"blocked":101,"allowed":142,"attempted":243,"pct":42,"pass":true}
```

### Desktop is no longer a usable reference for this spec

Current desktop Chromium (Playwright's Chrome for Testing 149) refuses to load
uBO at all — MV2 enforcement, exactly what `patches/m151/0001` removes on
Afterbird. `backgroundPages()`, `serviceWorkers()`, and `Target.getTargets` all
come back empty. The spec now returns `{skipped: true}` on that target rather
than a false failure.

## Harness changes

- `tests/harness/checks/adblock-device.mjs` (new): launches the browser through
  the adb driver with uBO loaded, then attaches over CDP (`connectOverCDP` does
  not rewrite the command line the way `_android.launchBrowser` does), waits for
  list compilation, and runs the spec.
- `ci/android_emulator_test.sh`: package default was still
  `com.kiwibrowser.browser`; without `--disable-fre` the browser parks in
  `FirstRunActivity` and drops navigation intents; M151 registers no intent
  filter for the `chrome://` scheme, so page checks now target the resolved
  component explicitly.

## Environment note

The build host OOM'd during the first full build: `autoninja` without `-j`
derives parallelism from core count alone (80 cores, 94 GiB, no swap), and the
errorprone `javac` step (8.5 GiB RSS) pushed it over. The
`chrome_java__errorprone` failure in that run was the OOM kill, not a source
error. `ci/chromium_android_pipeline.sh` now defaults to
`-j min(cores, RAM_GiB/2)`.
