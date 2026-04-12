# Afterbird Architecture

## Purpose

Afterbird currently acts as a Chromium/Kiwi source-tracking and patch-integration repository. It is not yet a complete standalone Chromium source tree.

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

- `CHROMIUM_VERSION`: Chromium baseline metadata for update/import workflows.
- `KIWI_VERSION`: Kiwi baseline metadata.
- `VERSION`: legacy app version metadata.
- `README.md`, `CHANGELOG.md`, `AGENTS.md`: repository governance and contributor guidance.

### 3) Automation Layer

- `.github/workflows/`: historical automation for Chromium import/rebase, Kiwi rebasing, linting, and release orchestration.
- `.build/production_build_reference/args.gn`: reference build args only (not a complete build profile set).
- `toolbox/`: maintenance scripts.

## Branch Architecture

Current branch roles:

- `chromium`: upstream-tracking baseline for Chromium file updates; expected to receive `[Chromium] ...` commits from import/update workflows.
- `kiwi`: integration branch for Kiwi-specific deltas on top of Chromium-tracked files; historically rebased onto Chromium baselines.
- `afterbird`: fork working branch and current default for governance/revival work; currently aligned with `kiwi` at this baseline.

Conceptual flow:

1. Update/import into `chromium`.
2. Rebase or integrate into `kiwi`.
3. Promote curated changes into `afterbird`.

## Build Architecture Status

Current state is hybrid and partially externalized:

- The repository does not contain a full Chromium checkout on `afterbird`/`kiwi`.
- Legacy workflows depend on external/private Kiwi infrastructure and secrets.
- Build definitions in-repo are incomplete for reproducible local Android builds.

Result: this repository should currently be treated as source + governance infrastructure, not as a complete build root.

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

- Choose target engine baseline (for example remain on 105 lineage or rebase to newer Chromium branch).
- Document exact bootstrap process using full Chromium checkout + overlay from this repo.
- Restore missing `.build/*/args.gn` profiles and deterministic build targets.

### Phase 4: CI Modernization

- Replace or gate private-infra-dependent workflows.
- Add public CI checks runnable by contributors (lint, metadata validation, selected smoke checks).
- Separate CI for docs-only changes vs source changes.

### Phase 5: Release/Distribution Strategy

- Decide whether this repo will publish artifacts directly or stay source-only.
- If publishing, define signing, provenance, and release note policy.
- Track version mapping between branch baseline, app version, and release tags.
