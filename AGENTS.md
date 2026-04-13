# AGENTS.md

## Scope

This file defines how contributors and coding agents should work in this repository.

## Branching Rules

- Start all work from `afterbird` unless the task is explicitly Chromium-sync (`chromium`) or Kiwi-integration (`kiwi`).
- If `chromium` is missing locally but `origin/chromium` exists, create it with `git switch -c chromium --track origin/chromium`.
- Use short-lived topic branches, for example `feature/<topic>` or `fix/<topic>`.
- Do not push direct commits to `afterbird`, `kiwi`, or `chromium` except approved branch-maintainer workflows.

## Branch Responsibilities

- `chromium`: upstream file tracking and version bump imports only; commit subjects should use `[Chromium] ...`.
- `kiwi`: legacy Kiwi integration/reference branch; keep history focused on integration experiments and conflict resolution.
- `afterbird`: main development and governance branch for this fork; feature/docs/policy updates and Chromium integration PRs land here.

## Current Integration Flow

- For Chromium baseline updates, branch from `afterbird`, merge/rebase latest `origin/chromium`, resolve conflicts, and open a PR back to `afterbird`.
- Treat Kiwi-specific behavior as explicit re-port work after Chromium merges; document dropped integrations in PR risk/follow-up notes.

## QA And Cross-Review

- Every PR must be reviewed by at least one other contributor (human).
- For non-trivial changes, require one independent validation pass (second reviewer or separate agent run) before merge.
- PR description must include what changed, why it changed, how it was validated, and known risks/follow-up work.

## Merge Rules

- Prefer squash merge for feature/doc branches to keep history readable.
- Rebase/force-push is allowed only for branch-maintainer integration workflows on `kiwi`/`chromium`.
- `afterbird` should remain linear and merge cleanly from reviewed PRs.

## Commit Hygiene

- Keep each commit scoped to one logical change.
- Use clear prefixes where possible (`docs:`, `build:`, `ci:`, `[Chromium]`).
- Write imperative subjects, ~72 chars target.
- Include version/source context in commit body when importing upstream files.
- Do not bundle unrelated formatting or drive-by edits.

## Safety Rules

- Assume parallel work is in progress; do not revert unrelated changes.
- If unexpected unrelated diffs appear, stop and confirm intent before proceeding.
- Preserve branch-specific invariants and document exceptions in the PR.
