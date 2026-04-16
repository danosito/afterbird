# Afterbird

Afterbird is a continuation-focused fork of the Kiwi `src.next` codebase,
bringing MV2 extension support to a modern Chromium baseline on Android.
Repository owner/maintainer: `danosito` (`https://github.com/danosito`).

## Current Status

**v0.2 — Kiwi-style extensions menu** — see [Releases](https://github.com/danosito/afterbird/releases).

- Browser launches on Android 15+ (emulator and physical devices)
- `--load-extension=/path/to/ext[,/path2,...]` loads one or more unpacked extensions
- uBlock Origin loads successfully; content scripts execute
- DNR (declarativeNetRequest) based ad blocking works
- Main app menu shows:
  - "Extensions" entry (opens `chrome://extensions`)
  - One entry per running extension (opens its popup URL in a new tab)
- Built on Chromium 132.0.6834.83

### Known Limitations in v0.2

- Extension popup pages open but JavaScript doesn't execute yet
  (extensions/renderer pipeline for desktop-android not wired up)
- Extension messaging API stubbed (`chrome.runtime.sendMessage` drops silently)
- `chrome://extensions` WebUI page blank (desktop-only string resources)
- No UI to enable/disable/manage extensions at runtime

### Roadmap

- Fix JavaScript execution in extension pages (unblocks popups, content scripts)
- Enable full MessageService for extension messaging
- Re-enable `chrome://extensions` WebUI with Android-compatible resources
- DevTools parity on mobile

## What This Repository Contains

This is a **tracked source subset + project automation**, not a complete
standalone Chromium checkout. It contains selected Chromium/Kiwi directories
and project automation files, aligned to Chromium 132 baseline (`132.0.6834.83`).

Builds run against an external Chromium checkout; see the build instructions below.

## What This Repository Contains Today

Top-level source coverage currently includes:

- `base/`
- `chrome/`
- `components/`
- `content/`
- `extensions/`
- `net/`
- `remoting/`
- `services/`
- `third_party/`
- `ui/`

Plus project metadata and automation:

- `.github/workflows/` (branch rebasing/import automation, plus Chromium smoke/full-build pipelines)
- `.build/production_build_reference/args.gn` (reference GN args file)
- `CHROMIUM_VERSION`, `KIWI_VERSION`, `VERSION`
- `ci/chromium_android_pipeline.sh` (external Chromium checkout + overlay + smoke/full build)
- `ci/android_emulator_test.sh` (APK install/startup + internal-pages smoke + ~120s modern-site flow + crash/memory telemetry)
- `ci/fetch_ublock_chromium.sh` and `ci/extensions/ublock_chromium_132.lock.json` (pinned extension package fetch metadata)
- `tests/emulator/` manifests (`modern_sites.txt`, `internal_pages_smoke.txt`, `manual_checks.md`)
- `toolbox/` scripts

## Branches And Their Roles

Current branch model in this repository:

- `afterbird`: main branch for this fork's ongoing work; it was bootstrapped from Kiwi history and is expected to evolve independently through periodic upstream sync/rebase work.
- `kiwi`: legacy Kiwi integration branch in this fork; contains Kiwi-era files, workflows, and metadata.
- `chromium`: Chromium tracking branch (upstream file sync baseline). In some clones this may only exist as `origin/chromium`; create a local tracking branch with `git switch -c chromium --track origin/chromium`.

Current intended upstream-sync flow:

1. Sync/update `chromium` from upstream Chromium.
2. Create a feature branch from `afterbird` and merge/rebase latest `origin/chromium` there.
3. Resolve conflicts while preserving Afterbird governance/docs and project policy files.
4. Re-port required Kiwi/Afterbird integrations on top before merging back into `afterbird` via PR.

`kiwi` remains available as a legacy integration/reference branch, but it is not required as an intermediary for every Chromium sync.

## Known Versions In-Tree

From files on the current `afterbird` branch:

- `VERSION`: `93.0.4577.21`
- `CHROMIUM_VERSION`: `132.0.6834.83`
- `KIWI_VERSION`: `105.0.5195.33`

Important caveat:

- Numeric Git tags in this repo (for example `14310011181`, `15616141394`) are release/build identifiers, not guaranteed to match Chromium major versions.
- Future updates on `chromium` may diverge from `afterbird` until the next integration merge.
- Direct Chromium merges can drop Kiwi-specific integrations; missing behavior must be identified and re-ported as explicit follow-up work.

## Build Status (Realistic)

Current status: this repo still is not a full standalone Chromium checkout, but it now includes an explicit external build pipeline script and CI entry points.

Why this is still externalized:

- `afterbird`/`kiwi` do not contain a complete Chromium source tree and dependency payload by themselves.
- Build/sync steps require Chromium tooling and infrastructure (`depot_tools`, `gclient`, GN/Ninja, Android toolchain).
- Existing legacy Kiwi workflows still reference private infrastructure and are not a public baseline.

What is now available:

- `ci/chromium_android_pipeline.sh` reads `CHROMIUM_VERSION`, checks out exact Chromium tag into an external workdir, runs `gclient sync`, applies this repo as overlay, runs `gn gen` with `.build/production_build_reference/args.gn`, and validates the `chrome_public_apk` graph.
- The pipeline fetches only the required Chromium tag (instead of fetching all tags) and retries transient fetch/sync failures with backoff.
- Default mode is smoke (`sync + overlay + gn gen + graph check`).
- Full build mode (`autoninja ... chrome_public_apk`) is explicit via `--full-build`.
- `ci/android_emulator_test.sh` provides emulator/device smoke automation:
  - APK install
  - startup check
  - internal page launchability checks for extension/devtools entry points
  - ~120s modern-site traversal
  - package-scoped logcat crash signal scan + `dumpsys meminfo` trend summary
- GitHub Actions now expose:
  - Smoke on `push`/`pull_request` to `afterbird`.
  - Full build on manual `workflow_dispatch`.
  - Emulator smoke/e2e telemetry run via manual dispatch (and optional schedule) in `android_emulator_e2e.yml`.

## Local Prerequisites And Commands

Required tools (Linux/macOS):

- `git`, `awk`, `rsync`, `python3`
- Java 17 (or Chromium-compatible JDK)
- `depot_tools` on `PATH` (`gclient`, `gn`, `autoninja`)
- Android build prerequisites expected by Chromium hooks/tooling (SDK/NDK components and host packages)
- Android emulator test prerequisites:
  - `adb` on `PATH`
  - Android SDK emulator + system image (`android-34` / x86_64 recommended)
  - `curl` plus `sha256sum` or `shasum` for extension package pin verification

One-time `depot_tools` setup:

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$HOME/depot_tools"
export PATH="$HOME/depot_tools:$PATH"
```

Run smoke pipeline locally (default mode):

```bash
ci/chromium_android_pipeline.sh --workdir "$HOME/afterbird-chromium"
```

Run full build locally (explicit):

```bash
ci/chromium_android_pipeline.sh \
  --workdir "$HOME/afterbird-chromium" \
  --full-build \
  --target chrome_public_apk
```

Network/performance overrides (optional):

```bash
# Use a mirror instead of googlesource
AFTERBIRD_CHROMIUM_SRC_GIT_URL="https://github.com/chromium/chromium.git" \
  ci/chromium_android_pipeline.sh --workdir "$HOME/afterbird-chromium"

# For an existing workspace, also force rewrite of .gclient/origin when switching remotes
AFTERBIRD_CHROMIUM_SRC_GIT_URL="https://github.com/chromium/chromium.git" \
AFTERBIRD_FORCE_WORKSPACE_CONFIG=1 \
  ci/chromium_android_pipeline.sh --workdir "$HOME/afterbird-chromium"

# Keep lightweight sync defaults and tune retries/timeouts
AFTERBIRD_GCLIENT_NO_HISTORY=1 \
AFTERBIRD_FETCH_RETRIES=4 \
AFTERBIRD_FETCH_TIMEOUT_SECONDS=900 \
  ci/chromium_android_pipeline.sh --workdir "$HOME/afterbird-chromium"
```

Fetch pinned uBlock Chromium package (version `1.62.0`, published `2025-01-01`) for extension prep:

```bash
ci/fetch_ublock_chromium.sh
```

Run emulator smoke/e2e checks locally (after booting an emulator or connecting a test device):

```bash
ci/android_emulator_test.sh \
  --apk /absolute/path/to/afterbird.apk \
  --package com.kiwibrowser.browser \
  --activity org.chromium.chrome.browser.ChromeTabbedActivity \
  --duration-sec 120 \
  --artifact-dir "$PWD/tests/artifacts/local-emulator"
```

Automated vs manual extension/devtools verification boundary:

- Automated smoke coverage is defined in:
  - `tests/emulator/internal_pages_smoke.txt`
  - `tests/emulator/modern_sites.txt`
  - `ci/android_emulator_test.sh`
- Manual checks still required are listed in:
  - `tests/emulator/manual_checks.md`

Notes:

- The first run is heavy and can consume significant disk/network/time.
- The script is idempotent for the same tag/workdir: each run resets and cleans `src` before applying overlay files.
- `--out-dir` must be a safe relative path under `src` (absolute paths and `.`/`..` traversal are rejected).
- Default sync mode uses `gclient sync -D --no-history`; override via `AFTERBIRD_GCLIENT_NO_HISTORY=0` when full history is required.
- Existing workspace config is preserved by default; set `AFTERBIRD_FORCE_WORKSPACE_CONFIG=1` to rewrite `.gclient` and update `src` origin URL.
- If `timeout`/`gtimeout` is unavailable, the script logs a warning and continues without enforced per-attempt timeout.

## Next Documentation

- Architecture and revival roadmap: see `ARCHITECTURE.md`
- Contributor/agent workflow rules: see `AGENTS.md`
- Change history baseline: see `CHANGELOG.md`

## License

Top-level project license: BSD 3-Clause (see `LICENSE`).
This repository also includes inherited Kiwi-layer provenance and imported
Chromium/third-party components that may retain their own notices in file
headers, history, and third-party metadata.
