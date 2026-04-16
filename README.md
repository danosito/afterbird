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

## What works in v1.1

- **Install from the Chrome Web Store.** Navigate to a
  `chromewebstore.google.com/detail/<slug>/<id>` URL — Afterbird
  intercepts the navigation, fetches the signed `.crx` from Google's
  update endpoint, and installs it.
- **Install from any `.crx` / `.user.js` link on the web.** Same
  throttle, same installer pipeline. No Android "open with" dialog.
- **Load Unpacked picker** for `.zip`, `.crx`, `.user.js`, or a
  directory. Packages are staged under
  `<profile>/Extensions/afterbird_install_<rand>/`.
- `chrome://extensions` renders the real Chromium extensions manager
  (Polymer WebUI) and the cards populate properly.
- Installed extensions persist across restart.
- **Extension messaging.** `chrome.runtime.sendMessage` and
  `chrome.runtime.connect` port-based channels route between extension
  frames, so dashboards and popups (e.g. uBlock Origin Settings) render
  populated instead of blank.
- DNR (declarativeNetRequest) based ad blocking via uBlock Origin.
- Main app menu exposes `Extensions` plus one entry per running extension.

No command-line `--load-extension` hack needed for any end-user flow.

Smoke-tested on device: Dark Reader installs via the CWS detail URL,
uBlock Origin installs via the Load Unpacked picker *and* via a raw
`.crx` URL download, both survive restart, uBO Settings panel renders,
DNR blocks ads.

## Known limitations

- Only a couple of extensions have been exercised end-to-end. Extensions
  that rely on APIs still stubbed in the desktop-android extension
  system (parts of `chrome.management`, `browserAction.setIcon`,
  `tabs.query`, etc.) may misbehave.
- Extension icons on `chrome://extensions` render as broken images —
  `chrome://extension-icon/` is not yet wired up on desktop-android.
- Re-installing the same extension produces a new staging dir + new
  unpacked-location ID, so the list accumulates duplicates.
- The install flow is silent — no permission confirmation sheet yet.
- Enable toggle, Remove, Details, Pack, and the dev-mode toggle on
  `chrome://extensions` still need polish.

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
