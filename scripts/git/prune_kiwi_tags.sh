#!/usr/bin/env bash
# Delete all Kiwi-era numeric build-number tags (regex ^[0-9]{9,}$) locally
# and on origin. Keeps Afterbird vX.Y.Z semantic tags intact.
set -euo pipefail

PATTERN='^[0-9]{9,}$'

mapfile -t TAGS < <(git tag | grep -E "$PATTERN" || true)
echo "Tags to delete: ${#TAGS[@]}"

if [[ ${#TAGS[@]} -eq 0 ]]; then
  echo "Nothing to prune."
  exit 0
fi

# Local delete in batches (tags list can be long)
echo "Deleting local tags..."
printf '%s\n' "${TAGS[@]}" | xargs -n 100 git tag -d >/dev/null

echo "Deleting remote tags on origin..."
# Build the push refspecs as :refs/tags/<tag>
refspecs=()
for t in "${TAGS[@]}"; do refspecs+=(":refs/tags/$t"); done
# Chunk to avoid argv limits
printf '%s\n' "${refspecs[@]}" | xargs -n 100 git push origin

echo "Remaining tags:"
git tag | wc -l
echo "Done."
