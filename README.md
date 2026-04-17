# Afterbird

Afterbird is a Chromium-based Android browser with MV2 extension support,
built on Chromium 132 and inspired by
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

This repo is a **tracked source subset + project automation**, not a
standalone Chromium checkout. Top-level source coverage: `base/`,
`chrome/`, `components/`, `content/`, `extensions/`, `net/`, `remoting/`,
`services/`, `third_party/`, `ui/`. Builds run against an external
Chromium 132 checkout overlaid by this repo.

- Build pipeline: `ci/chromium_android_pipeline.sh` (smoke by default,
  `--full-build` for `chrome_public_apk`).
- Emulator automation: `ci/android_emulator_test.sh`.
- Pinned reference GN args: `.build/production_build_reference/args.gn`.
- Branch roles:
  - `afterbird` — main integration branch for this fork.
  - `chromium` — upstream Chromium tracking baseline
    (`CHROMIUM_VERSION=132.0.6834.83`).
  - `kiwi` — legacy Kiwi reference branch.

## More

- Change history: [CHANGELOG.md](CHANGELOG.md)
- Contributor / agent workflow rules: [AGENTS.md](AGENTS.md)
- Architecture and revival roadmap: [ARCHITECTURE.md](ARCHITECTURE.md)

## License

BSD 3-Clause (see `LICENSE`). Inherited Kiwi provenance and imported
Chromium / third-party components retain their own notices in file
headers and third-party metadata.
