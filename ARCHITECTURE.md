# Afterbird Architecture

## Purpose

Afterbird currently acts as a Chromium/Kiwi source-tracking and patch-integration repository. The current branch baseline is Chromium `132.0.6834.83` (from `CHROMIUM_VERSION`), but it is not yet a complete standalone Chromium source tree.

## Source-Tree Architecture

Repository structure today is split into three layers.

### 1) Tracked Source Layer

Primary code directories:

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

These are maintained as a selected subset of Chromium/Kiwi files relevant to the project.

### 2) Project Control Layer

- `CHROMIUM_VERSION`: Chromium baseline metadata for update/import workflows (currently `132.0.6834.83`).
- `KIWI_VERSION`: Kiwi baseline metadata.
- `VERSION`: legacy app version metadata.
- `README.md`, `CHANGELOG.md`, `AGENTS.md`: repository governance and contributor guidance.
- Governance owner/maintainer: `danosito` (`https://github.com/danosito`).

### 3) Automation Layer

- `.github/workflows/`: Chromium import/rebase automation, legacy Kiwi workflows, plus:
  - `chromium_smoke_pipeline.yml` (PR/push smoke checks on `afterbird`)
  - `chromium_full_build.yml` (manual full Android target build)
  - `android_emulator_e2e.yml` (manual + scheduled emulator smoke/e2e telemetry run)
- `.build/production_build_reference/args.gn`: reference build args consumed by the new external pipeline.
- `ci/chromium_android_pipeline.sh`: local/CI pipeline script for exact-tag checkout, sync, overlay, GN generation, smoke graph check, and optional full build.
- `ci/android_emulator_test.sh`: emulator/device automation script for APK install, smoke startup, internal page launchability checks, modern-site traversal, logcat crash scan, and memory trend reporting.
- `ci/fetch_ublock_chromium.sh` + `ci/extensions/ublock_chromium_132.lock.json`: extension prep lock/manifest and deterministic fetch flow for pinned uBlock package (`1.62.0`).
- `tests/emulator/`: smoke URL manifests plus manual-check matrix for extension/devtools validation scope.
- `toolbox/`: maintenance scripts.

## Branch Architecture

Current branch roles:

- `chromium`: upstream-tracking baseline for Chromium file updates; expected to receive `[Chromium] ...` commits from import/update workflows. In some clones this may only exist as `origin/chromium`; create a local branch with `git switch -c chromium --track origin/chromium`.
- `kiwi`: legacy integration/reference branch for Kiwi-specific deltas.
- `afterbird`: fork working branch and current default for governance/revival work; seeded from Kiwi lineage and periodically integrated with `origin/chromium` directly on feature branches.

Conceptual flow:

1. Update/import into `chromium` (or consume latest `origin/chromium`).
2. Merge/rebase Chromium into a topic branch from `afterbird`.
3. Resolve conflicts with Chromium baseline updates while preserving Afterbird governance/docs controls.
4. Re-port Kiwi/Afterbird-specific integrations that are lost or regressed.
5. Merge reviewed topic branch into `afterbird`.

## Build Architecture Status

Current state is hybrid and externalized by design:

- The repository does not contain a full Chromium checkout on `afterbird`/`kiwi`.
- The build path is now formalized around an external Chromium workspace (`ci/chromium_android_pipeline.sh`) rather than attempting in-repo standalone builds.
- Legacy workflows still include private Kiwi infrastructure and secrets and should be treated as legacy/non-baseline.
- Chromium-forward merges can remove or overwrite Kiwi-specific integrations unless they are explicitly re-ported.

Result: this repository should be treated as source + governance + overlay/pipeline control, with actual compilation happening in an external Chromium checkout at the pinned tag.

## Emulator Test Architecture

Emulator coverage intentionally focuses on pragmatic smoke-level signals:

1. Install APK and confirm browser process launchability.
2. Check launchability for extension/devtools-related internal entry points (`chrome://extensions`, `chrome://inspect`, etc.).
3. Run a time-boxed modern-site traversal (~120 seconds by default) to exercise startup + navigation lifecycle.
4. Collect two low-cost telemetry channels:
   - package-scoped `logcat` crash-pattern scan for obvious fatal signals.
   - `dumpsys meminfo` sampled trend summary (min/max/avg/delta PSS).

Validation boundaries are explicit:

- Automated checks: `ci/android_emulator_test.sh` with manifests in `tests/emulator/*.txt`.
- Manual checks: deeper extension/devtools UX/compatibility items listed in `tests/emulator/manual_checks.md`.

## Revival Roadmap

### Phase 1: Baseline Governance (completed in this feature)

- Replace legacy README with factual current-state docs.
- Add architecture and contributor workflow documents.
- Start a changelog with explicit baseline notes.

### Phase 2: Branch Policy Stabilization

- Protect long-lived branches (`afterbird`, `kiwi`, `chromium`) with PR-only merges.
- Define and enforce which change types are valid per branch.
- Record branch sync cadence (manual or workflow-driven).

### Phase 3: Reproducible Build Recovery

- Maintain/update target engine baseline (currently Chromium 132 lineage) with explicit integration cadence.
- Document exact bootstrap process using full Chromium checkout + overlay from this repo.
- Restore missing `.build/*/args.gn` profiles and deterministic build targets.
- Track and re-port Kiwi-specific integrations dropped by Chromium syncs as a first-class backlog.

### Phase 4: CI Modernization

- Add public CI checks runnable by contributors.
- Keep full target builds explicit/manual because of runtime and infrastructure cost.
- Continue replacing or gating private-infra-dependent legacy workflows.

### Phase 5: Release/Distribution Strategy

- Decide whether this repo will publish artifacts directly or stay source-only.
- If publishing, define signing, provenance, and release note policy.
- Track version mapping between branch baseline, app version, and release tags.
