# Afterbird

Afterbird is a continuation-focused fork of the Kiwi `src.next` codebase.
The project identity in this repository is now Afterbird.

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

- `.github/workflows/` (branch rebasing/import automation and legacy build pipelines)
- `.build/production_build_reference/args.gn` (reference GN args file)
- `CHROMIUM_VERSION`, `KIWI_VERSION`, `VERSION`
- `toolbox/` scripts

## Branches And Their Roles

Current branch model in this repository:

- `afterbird`: main branch for this fork's ongoing work; it was bootstrapped from Kiwi history and is expected to evolve independently through periodic upstream sync/rebase work.
- `kiwi`: legacy Kiwi integration branch in this fork; contains Kiwi-era files, workflows, and metadata.
- `chromium`: Chromium tracking branch (upstream file sync baseline) used before Kiwi-specific integration/rebase steps. In some clones this may only exist as `origin/chromium`; create a local tracking branch with `git switch -c chromium --track origin/chromium`.

Operationally, the historical workflow has been: update `chromium` -> rebase/integrate into `kiwi` -> carry fork-specific changes on top.

## Known Versions In-Tree

From files on the current `afterbird` branch:

- `VERSION`: `93.0.4577.21`
- `CHROMIUM_VERSION`: `132.0.6834.83`
- `KIWI_VERSION`: `105.0.5195.33`

Important caveat:

- Numeric Git tags in this repo (for example `14310011181`, `15616141394`) are release/build identifiers, not guaranteed to match Chromium major versions.
- Future updates on `chromium` may diverge from `afterbird` until the next integration merge.

## Build Status (Realistic)

Current status: no verified, self-contained local build path from this branch alone.

Why:

- `afterbird`/`kiwi` still do not represent a full Chromium checkout and are missing required directories/tooling for standalone builds (for example `build/`, `tools/`, full dependency payloads), even though root manifests like `BUILD.gn` and `DEPS` are now present.
- Existing CI workflows reference private/legacy Kiwi infrastructure and secrets (`longbuild.find.kiwi`, `build.find.kiwi`, storage/release credentials).
- `build_and_sign_release_apk.yml` expects `.build/android_arm/args.gn`, which is not present in the current tree.

What is required to build reliably:

- A full Chromium checkout at a chosen baseline tag.
- Local Android/Chromium build prerequisites (depot_tools, GN/Ninja, Android SDK/NDK, supported JDK, system deps).
- A reproducible overlay/patch application step from this repository onto that full checkout.
- Reconstructed/validated GN args profiles and documented target commands.

Until those are formalized, treat this repository as a source-tracking and patch-integration base, not a one-command build environment.

## Next Documentation

- Architecture and revival roadmap: see `ARCHITECTURE.md`
- Contributor/agent workflow rules: see `AGENTS.md`
- Change history baseline: see `CHANGELOG.md`
