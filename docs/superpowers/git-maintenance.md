# Git maintenance

Living inventory of branches and tags in this repo, what to keep, what to prune.
Run the scripts in `scripts/git/` after reviewing; they are intentionally not
run by CI.

## Current state (2026-04-17)

| Category      | Count | Disposition                                        |
|---------------|-------|----------------------------------------------------|
| Local branches | 6    | 4 mergeable (prune); 2 keep (afterbird, kiwi)     |
| Remote branches | 7   | 5 keep; 2 prune (dependabot + heavy-ext)          |
| Tags total    | 453   |                                                    |
|  - Afterbird `vX.Y.Z` | 12 | keep                                          |
|  - Kiwi build numbers | 441 | prune — inherited from Kiwi fork, no release value |

## Branches

### Local — fully merged into `afterbird`, safe to prune

- `feature/api-coverage-v2` — **DO NOT prune yet**: active subagent work.
- `feature/heavy-ext-and-install` — merged in v1.4.0; ok to prune.
- `feature/install-webstore-private-wip` — **keep**: WIP preserved per user
  directive; the Kiwi-style webstorePrivate cutover depends on it.
- `kiwi` — upstream reference branch; keep indefinitely.

### Remote

- `origin/afterbird` — primary.
- `origin/chromium`, `origin/chromium_132.0.6834.83` — upstream refs.
- `origin/kiwi` — upstream Kiwi fork point.
- `origin/feature/install-webstore-private-wip` — keep (WIP).
- `origin/feature/heavy-ext-and-install` — merged; prune.
- `origin/dependabot/github_actions/dot-github/workflows/tj-actions/branch-names-7.0.7`
  — 2023 dependabot PR, superseded; prune.

## Tags

441 numeric build-number tags (regex `^[0-9]{9,}$`) come from the Kiwi
fork's Jenkins pipeline. They don't correspond to Afterbird releases and
clutter `git tag` output. The 12 semantic-version tags (`v0.1.0`–`v1.5.0`)
are the authoritative release history.

Pruning strategy:

- Delete the Kiwi-era numeric tags locally and on `origin`.
- Leave them on the `kiwi` upstream remote if one is configured — we don't
  own that remote anyway.

## Scripts

Two scripts capture the cleanup. They are idempotent and print what they
would do before running.

- `scripts/git/prune_branches.sh` — deletes merged local branches
  (whitelist-protected) and the two listed remote branches.
- `scripts/git/prune_kiwi_tags.sh` — deletes every tag matching
  `^[0-9]{9,}$` locally and on `origin`.

## Commit this branch — do not merge

Per the user directive, this branch (`chore/git-cleanup`) is for review
only. Do not merge into `afterbird` without explicit approval. Run the
scripts after review if you agree with the plan.
