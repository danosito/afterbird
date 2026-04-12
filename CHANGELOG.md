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
