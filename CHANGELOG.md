# Changelog

All notable changes to this repository are documented in this file.

## 2026-04-13

### Added

- `ARCHITECTURE.md` with source-tree/branch architecture and a practical revival roadmap.
- `AGENTS.md` with contributor/agent workflow rules for branching, QA cross-review, merge policy, and commit hygiene.
- `CHANGELOG.md` baseline.
- `ci/chromium_android_pipeline.sh` to run a pinned Chromium external checkout pipeline:
  - Reads `CHROMIUM_VERSION`
  - Checks out exact Chromium tag into external workdir
  - Runs `gclient sync`
  - Applies Afterbird overlay
  - Runs `gn gen` using `.build/production_build_reference/args.gn`
  - Runs smoke graph checks by default, with optional explicit full build mode (`--full-build`)
- `.github/workflows/chromium_smoke_pipeline.yml` for smoke checks on `push`/`pull_request` to `afterbird`.
- `.github/workflows/chromium_full_build.yml` for manual full build runs (`workflow_dispatch`).
- `ci/android_emulator_test.sh` for emulator APK automation:
  - install APK on emulator/device
  - startup smoke launch check
  - internal pages launchability checks for extension/devtools entry points
  - modern-site e2e flow (~120 seconds default)
  - logcat crash signal scan + `dumpsys meminfo` trend report
- `tests/emulator/modern_sites.txt` and `tests/emulator/internal_pages_smoke.txt` to define automated URL coverage.
- `tests/emulator/manual_checks.md` to explicitly separate manual-only extension/devtools checks from automation scope.
- `.github/workflows/android_emulator_e2e.yml` for manual-dispatch emulator automation (plus optional weekly schedule path).
- `ci/extensions/ublock_chromium_132.lock.json` pinning uBlock Chromium package `1.62.0` (`2025-01-01`) with URL/size/SHA256 metadata.
- `ci/fetch_ublock_chromium.sh` to fetch and verify the pinned uBlock package from the lock manifest.

### Changed

- Rewrote `README.md` to reflect the current Afterbird state, including project rename/purpose, branch roles (`afterbird`, `kiwi`, `chromium`), in-tree version markers with caveats, and realistic build requirements.
- Clarified branch wording to avoid time-sensitive claims about `afterbird`/`kiwi` alignment after future merges.
- Documented how to create a local `chromium` branch when only `origin/chromium` exists: `git switch -c chromium --track origin/chromium`.
- Merged latest `origin/chromium` into `afterbird` on feature branch, resolving large source conflicts by preserving Chromium updates while keeping Afterbird governance/docs files (`README.md`, `ARCHITECTURE.md`, `AGENTS.md`, `CHANGELOG.md`) under project control.
- Updated docs to reflect the Chromium 132 baseline now present on `afterbird` (`CHROMIUM_VERSION=132.0.6834.83`).
- Updated governance docs to reflect current reality: Chromium sync is performed by merging/rebasing `origin/chromium` into a feature branch from `afterbird`, then PR back to `afterbird` (with `kiwi` treated as legacy/reference, not mandatory intermediary).
- Added explicit risk notes that direct Chromium merges may drop Kiwi-specific integrations and that re-porting these integrations is tracked as revival backlog work.
- Updated `README.md` with local prerequisites and exact smoke/full commands for the new external Chromium pipeline.
- Updated `ARCHITECTURE.md` automation/build sections to describe the new smoke/full CI and externalized build model.
- Updated docs to include exact local emulator-test commands/prerequisites and lockfile-based extension package prep workflow.
- Hardened `ci/chromium_android_pipeline.sh` idempotency by resetting/cleaning the external Chromium `src` checkout before overlay application, preventing stale state from previous runs.
- Added strict `--out-dir` validation in `ci/chromium_android_pipeline.sh` to allow only safe relative paths under `src` (rejects absolute and traversal segments).
- Narrowed `chromium_smoke_pipeline.yml` triggers using `paths` filters so docs-only updates do not run the expensive smoke job.
- Replaced top-level `LICENSE` with an explicit combined licensing notice covering both Afterbird/Kiwi fork-origin files and Chromium-origin files, with third-party license caveats.
- Fixed top-level `OWNERS` references to missing paths by replacing unresolved `file://...` targets (`build/OWNERS`, `styleguide/c++/OWNERS`, `styleguide/rust/OWNERS`) with existing in-repo ownership references.
- Noted that the Chromium 132 merge may have dropped historical Kiwi integrations; those are tracked for explicit re-port follow-up.
