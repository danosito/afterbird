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
- Updated governance docs to reflect current reality: Chromium sync is performed by merging/rebasing `origin/chromium` into a feature branch from `afterbird`, then PR back to `afterbird` (with `kiwi` treated as legacy/reference, not mandatory intermediary).
- Added explicit risk notes that direct Chromium merges may drop Kiwi-specific integrations and that re-porting these integrations is tracked as revival backlog work.
- Replaced top-level `LICENSE` with an explicit combined licensing notice covering both Afterbird/Kiwi fork-origin files and Chromium-origin files, with third-party license caveats.
- Fixed top-level `OWNERS` references to missing paths by replacing unresolved `file://...` targets (`build/OWNERS`, `styleguide/c++/OWNERS`, `styleguide/rust/OWNERS`) with existing in-repo ownership references.
- Noted that the Chromium 132 merge may have dropped historical Kiwi integrations; those are tracked for explicit re-port follow-up.
