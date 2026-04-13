#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

LOCK_FILE="${REPO_ROOT}/ci/extensions/ublock_chromium_132.lock.json"
OUT_DIR="${REPO_ROOT}/third_party/extensions/ublock"
FORCE=0

usage() {
  cat <<'USAGE'
Usage:
  ci/fetch_ublock_chromium.sh [options]

Options:
  --lock-file <path>   Lock/manifest JSON (default: ci/extensions/ublock_chromium_132.lock.json)
  --out-dir <path>     Download output directory (default: third_party/extensions/ublock)
  --force              Redownload even if file already exists
  --help               Show this help message

The script downloads the pinned Chromium package declared in the lock file and
verifies SHA256.
USAGE
}

log() {
  printf '[ublock-fetch] %s\n' "$*"
}

die() {
  printf '[ublock-fetch][error] %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  local cmd="$1"
  command -v "${cmd}" >/dev/null 2>&1 || die "Missing required command: ${cmd}"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --lock-file)
        [[ $# -ge 2 ]] || die "--lock-file requires a value"
        LOCK_FILE="$2"
        shift 2
        ;;
      --out-dir)
        [[ $# -ge 2 ]] || die "--out-dir requires a value"
        OUT_DIR="$2"
        shift 2
        ;;
      --force)
        FORCE=1
        shift
        ;;
      --help)
        usage
        exit 0
        ;;
      *)
        die "Unknown argument: $1"
        ;;
    esac
  done

  [[ -f "${LOCK_FILE}" ]] || die "Lock file not found: ${LOCK_FILE}"
}

read_lock_field() {
  local field="$1"
  python3 - "$LOCK_FILE" "$field" <<'PY'
import json
import sys

lock_path = sys.argv[1]
field = sys.argv[2]
with open(lock_path, 'r', encoding='utf-8') as f:
    data = json.load(f)
value = data.get(field, '')
if value is None:
    value = ''
print(value)
PY
}

sha256_of_file() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${file}" | awk '{print $1}'
  else
    shasum -a 256 "${file}" | awk '{print $1}'
  fi
}

verify_checksum() {
  local file="$1"
  local expected="$2"
  local actual
  actual="$(sha256_of_file "${file}")"
  [[ "${actual}" == "${expected}" ]] || die "Checksum mismatch for ${file}: expected ${expected}, got ${actual}"
}

main() {
  parse_args "$@"

  require_cmd curl
  require_cmd awk
  require_cmd python3

  local source_url filename expected_sha version
  source_url="$(read_lock_field source_url)"
  filename="$(read_lock_field filename)"
  expected_sha="$(read_lock_field sha256)"
  version="$(read_lock_field version)"

  [[ -n "${source_url}" ]] || die "Lock file field 'source_url' is empty"
  [[ -n "${filename}" ]] || die "Lock file field 'filename' is empty"
  [[ -n "${expected_sha}" ]] || die "Lock file field 'sha256' is empty"

  mkdir -p "${OUT_DIR}"

  local output_path="${OUT_DIR}/${filename}"

  if [[ -f "${output_path}" ]] && [[ "${FORCE}" -eq 0 ]]; then
    log "File already exists, verifying checksum: ${output_path}"
    verify_checksum "${output_path}" "${expected_sha}"
    log "Checksum verified for pinned uBlock ${version}"
    printf '%s\n' "${output_path}"
    exit 0
  fi

  log "Downloading pinned uBlock ${version} package"
  curl -fL --retry 3 --retry-delay 2 --retry-all-errors -o "${output_path}" "${source_url}"

  log "Verifying SHA256"
  verify_checksum "${output_path}" "${expected_sha}"

  log "Downloaded and verified: ${output_path}"
  printf '%s\n' "${output_path}"
}

main "$@"
