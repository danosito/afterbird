# Afterbird

Afterbird is a continuation-focused fork of the Kiwi `src.next` codebase.
The project identity in this repository is now Afterbird.
Repository owner/maintainer: `danosito` (`https://github.com/danosito`).

This repository is currently a tracked source subset, not a complete standalone Chromium checkout. It contains selected Chromium/Kiwi directories and project automation files, and is now aligned to a Chromium 132 baseline (`132.0.6834.83`), but it does not include the full Chromium root tree or full build toolchain definitions needed for turnkey local builds.

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
- Default mode is smoke (`sync + overlay + gn gen + graph check`).
- Full build mode (`autoninja ... chrome_public_apk`) is explicit via `--full-build`.
- GitHub Actions now expose:
  - Smoke on `push`/`pull_request` to `afterbird`.
  - Full build on manual `workflow_dispatch`.

## Local Prerequisites And Commands

Required tools (Linux/macOS):

- `git`, `awk`, `rsync`, `python3`
- Java 17 (or Chromium-compatible JDK)
- `depot_tools` on `PATH` (`gclient`, `gn`, `autoninja`)
- Android build prerequisites expected by Chromium hooks/tooling (SDK/NDK components and host packages)

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

Notes:

- The first run is heavy and can consume significant disk/network/time.
- The script is idempotent for the same tag/workdir: each run resets and cleans `src` before applying overlay files.
- `--out-dir` must be a safe relative path under `src` (absolute paths and `.`/`..` traversal are rejected).

## Next Documentation

- Architecture and revival roadmap: see `ARCHITECTURE.md`
- Contributor/agent workflow rules: see `AGENTS.md`
- Change history baseline: see `CHANGELOG.md`

## License

Top-level project license: BSD 3-Clause (see `LICENSE`).
This repository also includes inherited Kiwi-layer provenance and imported
Chromium/third-party components that may retain their own notices in file
headers, history, and third-party metadata.
