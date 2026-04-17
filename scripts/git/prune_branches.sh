#!/usr/bin/env bash
# Delete local branches fully merged into afterbird, plus two named remote
# branches. Whitelist-protects critical refs.
#
# Idempotent: prints what it would do; re-run safely.
set -euo pipefail

KEEP_LOCAL=(afterbird kiwi feature/install-webstore-private-wip feature/api-coverage-v2)
PRUNE_REMOTE=(feature/heavy-ext-and-install dependabot/github_actions/dot-github/workflows/tj-actions/branch-names-7.0.7)

is_kept() {
  local b="$1"
  for k in "${KEEP_LOCAL[@]}"; do
    [[ "$b" == "$k" ]] && return 0
  done
  return 1
}

echo "=== Local branches to delete (merged into afterbird, not in whitelist) ==="
while IFS= read -r branch; do
  branch="${branch## }"
  branch="${branch#\* }"
  [[ -z "$branch" ]] && continue
  if is_kept "$branch"; then
    echo "  keep    $branch"
  else
    echo "  delete  $branch"
    git branch -d "$branch"
  fi
done < <(git branch --merged afterbird)

echo
echo "=== Remote branches to delete ==="
for b in "${PRUNE_REMOTE[@]}"; do
  if git show-ref --verify --quiet "refs/remotes/origin/$b"; then
    echo "  delete  origin/$b"
    git push origin --delete "$b"
  else
    echo "  absent  origin/$b (nothing to do)"
  fi
done

echo
echo "Done."
