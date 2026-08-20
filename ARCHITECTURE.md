# Afterbird Architecture

## Purpose

Afterbird is a **thin delta over stock Chromium** plus project automation. The pinned baseline is Chromium `151.0.7922.38` (from `CHROMIUM_VERSION`), built with the upstream desktop-android extension stack (`is_desktop_android=true`). The repository does not track Chromium source; compilation happens in an external checkout at the pinned tag.

## Source-Tree Architecture

Repository structure is split into three layers.

### 1) Tracked Delta Layer

- `chrome/android/java/res_chromium_base/**`: branding (launcher icons,
  app name strings) — rsynced verbatim over the external checkout.
- `patches/m151/*.patch`: source patches applied by the pipeline with
  `git apply`:
  - `0001-mv2-reenable.patch` — `MV2DeprecationImpactChecker::IsExtensionAffected`
    returns `false`; re-enables MV2 extensions (uBlock Origin).
  - `0002-browseraction-schema-desktop-android.patch` — bundles
    `browser_action.json`/`page_action.json` schemas outside
    `enable_extensions`; without it MV2 backgrounds FATAL at
    `GetAPISchema("browserAction")`.
  - `0003-extensions-menu-null-coordinator-fallback.patch` — phone form
    factor: null coordinator falls back to `chrome://extensions` instead
    of NPE.
- `third_party/extensions/afterbird-api-probe/**`: MV2/MV3 API probe
  extensions for compatibility testing.

The 132-era Kiwi tracked-source subset (~9.8k files across `base/`,
`chrome/`, `third_party/blink/`, `net/`, …) was pruned in the M151 bump;
it survives in git history and on the `chromium`/`kiwi` branches.

### 2) Project Control Layer

- `CHROMIUM_VERSION`: pinned Chromium tag consumed by the pipeline (currently `151.0.7922.38`). Bumping = edit this file + rebase `patches/m<major>/`.
- `KIWI_VERSION`: Kiwi baseline metadata.
- `VERSION`: legacy app version metadata.
- `README.md`, `CHANGELOG.md`, `AGENTS.md`: repository governance and contributor guidance.
- Governance owner/maintainer: `danosito` (`https://github.com/danosito`).

### 3) Automation Layer

- `.github/workflows/`: Chromium import/rebase automation, legacy Kiwi workflows, plus:
  - `chromium_smoke_pipeline.yml` (PR/push smoke checks on `afterbird`)
  - `chromium_full_build.yml` (manual full Android target build)
  - `android_emulator_e2e.yml` (manual + scheduled emulator smoke/e2e telemetry run)
- `.build/args/test.gn` / `.build/args/release.gn`: GN args variants selected via `AFTERBIRD_ARGS_VARIANT` (default `test`: debuggable, exposes the CDP socket for the Playwright harness; `release`: `is_official_build=true`, experimental).
- `ci/chromium_android_pipeline.sh`: local/CI pipeline script for exact-tag checkout, sync, branding overlay (include-list), patch application (`git apply` with check/reverse/3-way gates), GN generation, smoke graph check, and optional full build.
- `ci/android_emulator_test.sh`: emulator/device automation script for APK install, smoke startup, internal page launchability checks, modern-site traversal, logcat crash scan, and memory trend reporting.
- `ci/fetch_ublock_chromium.sh` + `ci/extensions/ublock_chromium_132.lock.json`: deterministic fetch of the pinned uBlock package (`1.62.0`, Chromium-version-agnostic despite the lock filename); output `third_party/extensions/ublock/` is gitignored.
- `tests/harness/`: Playwright/adb parity harness (desktop + android targets; uBO adblock spec).
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

Current state is externalized by design:

- The build path is formalized around an external Chromium workspace (`ci/chromium_android_pipeline.sh`); the effective overlay is ~17 branding files + 3 patches.
- Validated baseline (2026-07): M151 test build on device reaches uBO ~85–95% network blocking (`docs/superpowers/reports/2026-07-16-m151-stock-plus-mv2patch.md` and follow-up diagnostics).
- Legacy workflows still include private Kiwi infrastructure and secrets and should be treated as legacy/non-baseline.
- Kiwi-era behaviors (custom install dialog, chrome://extensions bridges, DevTools bridge) were superseded by the upstream desktop-android stack; anything worth re-porting is explicit backlog, recoverable from git history.

Result: this repository should be treated as delta + governance + pipeline control, with actual compilation happening in an external Chromium checkout at the pinned tag.

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

- Maintain/update target engine baseline (currently Chromium 151 lineage) with explicit integration cadence.
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
