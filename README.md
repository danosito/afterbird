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

## What works in v1.2

- **Extension install confirmation.** Clicking a `.crx` / `.user.js` link,
  navigating to a Chrome Web Store detail URL, or following a store→CDN
  redirect pops a dialog with the extension's name, version, source,
  manifest permissions, and a fine-print warning. Only after Accept does
  the extension actually register.
- **Install triggered on download, not navigation.** Navigation throttle
  narrowed to Chrome Web Store detail URLs only; `.crx` downloads are
  now caught at `DownloadManagerDelegate::InterceptDownloadIfApplicable`
  (the same primitive desktop Chrome uses).
- `chrome://extensions` is fully functional: Enable toggle disables the
  real extension, Remove drops the card + unregisters, Details opens the
  per-extension panel (version, size, ID, permissions, source, toggles),
  icons render from the extension's own image bytes, dev-mode toggle
  exposes Load Unpacked / Pack / Update.
- Load Unpacked picker still works for `.zip` / `.crx` / `.user.js` /
  directory installs (auto-confirms; the dialog is only shown when the
  flow starts from a network download).
- Installed extensions persist across restart.
- `chrome.runtime.sendMessage` + port-based messaging between extension
  frames (so dashboards and popups render populated).
- DNR-based ad blocking via uBlock Origin.
- Main app menu exposes `Extensions` plus one entry per running extension.

No command-line `--load-extension` hack needed.

Smoke-tested on device: installing Dark Reader from a Chrome Web Store
URL and Honey from a different store URL now show the confirm dialog;
Accept registers + runs, Cancel deletes the cached CRX. chrome://extensions
list + details both render and stay live-synced with toggle changes.

## Known limitations

- Only a few extensions have been exercised end-to-end. Extensions that
  rely on APIs still stubbed in the desktop-android extension system
  (parts of `chrome.management`, `browserAction.setIcon`, `tabs.query`,
  etc.) may misbehave.
- Re-installing the same extension produces a new staging dir + new
  unpacked-location ID — the list still accumulates duplicates instead
  of dedup-by-public-key.
- App menu "Extensions" entry opens inline instead of in a new tab;
  extension submenu entries don't yet have icons.
- DevTools UX is not yet wired up the Kiwi way.

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
