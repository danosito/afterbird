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

## What works in v1.3

- **Extension install confirmation dialog** for every network-triggered
  install (Chrome Web Store detail URLs, `.crx` links, redirect chains).
  Install only commits after Accept. Cancel wipes the cached CRX.
- Install triggered at the download layer
  (`DownloadManagerDelegate::InterceptDownloadIfApplicable`) for raw
  `.crx` links; the navigation throttle handles Chrome Web Store
  detail URLs.
- `chrome://extensions` is fully functional: Enable toggle disables the
  real extension, Remove drops the card, Details opens the per-extension
  panel, icons render from inline bytes, dev-mode toggle works.
- Load Unpacked picker for `.zip` / `.crx` / `.user.js` / directory.
- Installed extensions persist across restart.
- `chrome.runtime.sendMessage` + port-based messaging between extension
  frames so dashboards / popups render populated.
- DNR-based ad blocking via uBlock Origin.
- **Main app menu opens extensions in a proper child tab.** Back-swipe
  from the new tab returns to the page the user came from instead of
  killing the app. Per-extension entries now carry the extension's own
  icon.
- **MV2 + MV3 API probe extensions** under
  `third_party/extensions/afterbird-api-probe/` for compatibility
  testing. Load via chrome://extensions → Load Unpacked.

No command-line `--load-extension` hack needed.

## Known limitations

- Only a handful of extensions have been exercised end-to-end.
- Re-installing the same extension still produces duplicate rows (no
  dedup by public key yet).
- DevTools UX is not wired up the Kiwi way yet.
- Extensions without a `browser_action.default_popup` don't surface as
  per-extension menu entries.

## Repository layout and build

This repo is a **delta over stock Chromium + project automation**, not a
standalone Chromium checkout. Builds run against an external Chromium
checkout pinned to the tag in `CHROMIUM_VERSION` (currently
`151.0.7922.38`). The delta is deliberately tiny:

- `chrome/android/java/res_chromium_base/**` — branding (icons, app name),
  rsynced over the checkout by the pipeline.
- `patches/m151/*.patch` — source patches applied with `git apply`:
  MV2 re-enable, browserAction/pageAction schema bundling, extensions-menu
  phone-form-factor NPE fix.
- `.build/args/{test,release}.gn` — GN args variants. `test` (default) is
  debuggable and exposes the CDP socket for the Playwright harness;
  `release` is `is_official_build=true` (experimental).
- `third_party/extensions/**` — MV2/MV3 API probe extensions; uBlock
  Origin zip is fetched by `ci/fetch_ublock_chromium.sh` (not tracked).

Build pipeline: `ci/chromium_android_pipeline.sh` (smoke by default,
`--full-build` for `chrome_public_apk`). Emulator automation:
`ci/android_emulator_test.sh`. Parity harness: `tests/harness/`.

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
