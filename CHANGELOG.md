# Changelog

All notable changes to this repository are documented in this file.

## 2026-04-13

### Added

- `ARCHITECTURE.md` with source-tree/branch architecture and a practical revival roadmap.
- `AGENTS.md` with contributor/agent workflow rules for branching, QA cross-review, merge policy, and commit hygiene.
- `CHANGELOG.md` baseline.

### Changed

- Rewrote `README.md` to reflect the current Afterbird state, including project rename/purpose, branch roles (`afterbird`, `kiwi`, `chromium`), in-tree version markers with caveats, and realistic build requirements.
- Clarified branch wording to avoid time-sensitive claims about `afterbird`/`kiwi` alignment after future merges.
- Documented how to create a local `chromium` branch when only `origin/chromium` exists: `git switch -c chromium --track origin/chromium`.
- Merged latest `origin/chromium` into `afterbird` on feature branch, resolving large source conflicts by preserving Chromium updates while keeping Afterbird governance/docs files (`README.md`, `ARCHITECTURE.md`, `AGENTS.md`, `CHANGELOG.md`) under project control.
- Updated docs to reflect the Chromium 132 baseline now present on `afterbird` (`CHROMIUM_VERSION=132.0.6834.83`).
