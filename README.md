# Afterbird

Afterbird is a Chromium-based Android browser with MV2 extension support,
built on Chromium 151 (desktop-android extension stack) and inspired by
[Kiwi Browser](https://github.com/kiwibrowser/src.next) by
[Arnaud Granal](https://github.com/arnaudgranal).

**Package:** `com.danosito.afterbird`
**Maintainer:** danosito ·
[GitHub](https://github.com/danosito) ·
[Telegram](https://t.me/danosito)

## Install

Grab the latest APK from
[Releases](https://github.com/danosito/afterbird/releases) and install it.
Android 15+ is supported (emulator and physical devices).

## What works in v1.9

Afterbird runs the **upstream desktop-android extension stack** from Chromium
151, so extension support is Chromium's own rather than a reimplementation.
Four patches make it usable on a phone (see `patches/m151/`): Manifest V2 is
re-enabled, the `browserAction`/`pageAction` schemas are bundled, unpacked
extensions load without the developer-mode toggle, and the extensions menu no
longer crashes on a phone form factor.

### Extensions verified on device

Every extension below loads, enables itself and registers its event listeners
with no `Unknown API` errors (emulator, API 35, clean profile each time):

| Extension | Manifest |
|---|---|
| uBlock Origin | MV2 |
| Dark Reader | MV2 |
| Stylus | MV2 |
| Violentmonkey | MV2 |
| SponsorBlock | MV3 |
| Stylus | MV3 |
| Violentmonkey | MV3 |
| Bitwarden | MV3 |
| uBO Lite | MV3 |

Four of them were also checked for observable behaviour, not just loading:
uBlock Origin keeps ad requests off the network, Dark Reader injects its styles
and darkens the page, Violentmonkey and Stylus render their management UIs.

### Ad blocking

uBlock Origin scores **132/132 (100%)** on `adblock.turtlecute.org` with zero
ad or tracker requests reaching the network.

If you see a much lower score, check the enabled filter lists before suspecting
the browser: uBO auto-selects regional lists from the device language, and a
device set to English never enables regional lists that many test pages probe
heavily.

## Known limitations

- **DNR extensions must load from a writable directory.** Chromium writes
  indexed rulesets into `_metadata/` next to the unpacked extension. Loading an
  MV3 blocker from a read-only path (`/data/local/tmp`, say) fails with a
  misleading `Internal error while parsing rules`.
- The `release` build variant is newer than the `test` one and less exercised.
- Chrome Web Store install flow has not been re-verified since the M151 bump.
- Kiwi-era conveniences dropped with the 132 layer (custom install dialog,
  DevTools bridge, per-extension menu icons) have not been re-ported.

## Repository layout and build

This repo is a **delta over stock Chromium + project automation**, not a
standalone Chromium checkout. Builds run against an external Chromium
checkout pinned to the tag in `CHROMIUM_VERSION` (currently
`151.0.7922.38`). The delta is deliberately tiny:

- `chrome/android/java/res_chromium_base/**` — branding (icons, app name),
  rsynced over the checkout by the pipeline.
- `patches/m151/*.patch` — source patches applied with `git apply`:
  MV2 re-enable, browserAction/pageAction schema bundling, extensions-menu
  phone-form-factor NPE fix, unpacked-without-developer-mode.
- `.build/args/{test,release}.gn` — GN args variants. `test` (default) builds a
  Java-debuggable APK, which is what lets the harness pass flags through
  `/data/local/tmp/chrome-command-line`; `release` is `is_official_build=true`
  and not debuggable.
- `third_party/extensions/**` — MV2/MV3 API probe extensions; uBlock
  Origin zip is fetched by `ci/fetch_ublock_chromium.sh` (not tracked).

Build pipeline: `ci/chromium_android_pipeline.sh` (smoke by default,
`--full-build` for `chrome_public_apk`). Emulator automation:
`ci/android_emulator_test.sh`. Extension and ad-blocking checks live in
`tests/harness/checks/`; test results and the reasoning behind them are in
`docs/superpowers/reports/`.

Branch roles:
- `afterbird` — main integration branch for this fork.
- `chromium` — historical upstream import baseline (132-era; no longer
  used by the build).
- `kiwi` — legacy Kiwi reference branch.

Bumping Chromium = edit `CHROMIUM_VERSION` + rebase `patches/m<major>/`
against the new tag; the pipeline's `git apply --check` gate fails loudly
on drift.

## More

- Change history: [CHANGELOG.md](CHANGELOG.md)
- Contributor / agent workflow rules: [AGENTS.md](AGENTS.md)
- Architecture and revival roadmap: [ARCHITECTURE.md](ARCHITECTURE.md)

## License

BSD 3-Clause (see `LICENSE`). Inherited Kiwi provenance and imported
Chromium / third-party components retain their own notices in file
headers and third-party metadata.
