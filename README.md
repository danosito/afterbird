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

## What works in v0.6

- `chrome://extensions` renders the real Chromium extensions manager
  (Polymer WebUI).
- **Load Unpacked** opens the Android document picker and installs from
  `.zip`, `.crx`, `.user.js`, or a directory. Packages are staged under
  `<profile>/Extensions/afterbird_install_<rand>/` and registered via
  `ExtensionRegistrar`.
- Enable / disable / remove / reload buttons are wired through
  `developerPrivate.updateExtensionConfiguration` /
  `removeMultipleExtensions` / `reload`.
- Installed extensions persist across restart: on startup the extension
  system walks `ExtensionPrefs::GetInstalledExtensionsInfo()` and
  re-registers every previously-installed extension. Prefs whose staging
  directory was deleted out-of-band are pruned.
- DNR (declarativeNetRequest) based ad blocking via uBlock Origin.
- `--load-extension=/path[,/path2,...]` still works as a dev affordance.
- Main app menu exposes `Extensions` plus one entry per running extension.

Verified end-to-end on device: install uBlock Origin from `.zip` via the
picker, toggle it off/on, remove it, restart — remaining extensions come
back.

## Known limitations

- **No Chrome Web Store install flow yet** — planned for v0.7.
- **JS execution inside `chrome-extension://*` pages is partial.** uBO's
  background page loads but its popup/dashboard render blank. Content
  scripts and DNR rules still run.
- `chrome.management` API is not wired up.
- Extension messaging (`chrome.runtime.sendMessage`) still stubbed.

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
