# Afterbird test harness

Goal: measure **desktop parity** — does an extension/DevTools behave on Afterbird
(Android) the way it does on desktop Chrome? Same spec, multiple targets, the
result is a diff.

## Engine (chosen after evaluating options)

**Playwright `_android` over CDP** is the core. Proven working against Chrome on
the emulator (`lib/target.mjs::openAndroid`). Rejected alternatives:

- *Appium/uiautomator only* — drives native UI but can't read page/JS state
  cleanly; too coarse for extension-behavior assertions.
- *adb + screenshot scraping* — brittle, already how the old manual flow worked.
  Kept only as a thin fallback layer for native-only surfaces (install confirm
  dialog, app menu) that CDP can't reach.

CDP requires the target build to expose the `chrome_devtools_remote` socket.
**Verified: the release (non-debuggable) v1.8 build does NOT expose it even with
`--remote-debugging-socket-name`.** Therefore the Afterbird **test/dev build must
enable remote debugging** (debuggable + CDP socket); release builds keep it shut.
This is a build-args requirement for the M151 test variant.

## Reference strategy — split by manifest version

Modern desktop Chrome (149+) **refuses to load MV2 extensions** (`Cannot install
extension because it uses an unsupported manifest version`); the
`ExtensionManifestV2*` feature flags no longer re-enable it. So:

- **MV3 specs** (Dark Reader, Bitwarden, …): live parity diff vs desktop Chromium
  (`lib/target.mjs::openDesktop`). Desktop is a valid reference.
- **MV2 specs** (uBlock Origin): **absolute threshold**, not a live-Chrome diff.
  The known-good uBO behavior on `adblock.turtlecute.org` is ≥90% blocked
  (Kiwi measured ~97%). Afterbird carries a permanent MV2-reenable patch, so its
  own build is the system under test; the "reference" is the fixed threshold.
  (A pinned Chrome ≤137 could serve as a live MV2 reference but we don't maintain
  an EOL browser.)

## Layout

- `lib/target.mjs` — `openDesktop()` / `openAndroid()` targets.
- `specs/*.mjs` — one behavior per file, returns a normalized result object.
- `run-parity.mjs` — runs specs across targets, prints a `PARITY-JSON` row set.

## Run

    npm install
    npx playwright install chromium
    node run-parity.mjs desktop            # reference side
    AB_PKG=com.danosito.afterbird node run-parity.mjs android   # needs CDP-enabled build

## Status

- Android CDP path: proven against stock Chrome.
- Desktop path + adblock spec: proven.
- MV2 desktop reference: proven impossible on Chrome 149 (documented above).
- Next: point `android` target at the M151 CDP-enabled Afterbird build; add MV3
  specs + extension-install and DevTools specs.
