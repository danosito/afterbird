#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CHROMIUM_VERSION_FILE="${REPO_ROOT}/CHROMIUM_VERSION"
REFERENCE_ARGS_FILE="${REPO_ROOT}/.build/production_build_reference/args.gn"

WORKDIR="${AFTERBIRD_CHROMIUM_WORKDIR:-${HOME}/afterbird-chromium}"
OUT_DIR="${AFTERBIRD_OUT_DIR:-out/afterbird_production}"
TARGET="${AFTERBIRD_BUILD_TARGET:-chrome_public_apk}"
FULL_BUILD=0

usage() {
  cat <<'USAGE'
Usage:
  ci/chromium_android_pipeline.sh [options]

Options:
  --workdir <path>     External Chromium workspace root (default: $HOME/afterbird-chromium)
  --out-dir <path>     GN output directory relative to src (default: out/afterbird_production)
  --target <name>      Ninja target for full build mode (default: chrome_public_apk)
  --full-build         Run real build (autoninja). Default mode is smoke-only.
  --help               Show this help message.

Behavior:
  Default smoke mode runs: checkout pinned tag + gclient sync + overlay + gn gen + target graph check.
  Full build mode runs all smoke steps, then builds the selected target.
USAGE
}

log() {
  printf '[ci-pipeline] %s\n' "$*"
}

die() {
  printf '[ci-pipeline][error] %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  local cmd="$1"
  command -v "${cmd}" >/dev/null 2>&1 || die "Missing required command: ${cmd}"
}

validate_out_dir() {
  [[ -n "${OUT_DIR}" ]] || die "--out-dir cannot be empty"
  [[ "${OUT_DIR}" != /* ]] || die "--out-dir must be relative to src, not absolute"

  if [[ "${OUT_DIR}" =~ (^|/)\.\.?(/|$) ]]; then
    die "--out-dir must not contain '.' or '..' path traversal segments"
  fi

  case "${OUT_DIR}" in
    *$'\n'*|*$'\r'*)
      die "--out-dir must not contain newlines"
      ;;
  esac
}

read_version_part() {
  local key="$1"
  local value
  value="$(awk -F= -v key="${key}" '$1 == key { gsub(/[[:space:]]/, "", $2); print $2 }' "${CHROMIUM_VERSION_FILE}" | tail -n1)"
  [[ -n "${value}" ]] || die "Unable to read ${key} from ${CHROMIUM_VERSION_FILE}"
  [[ "${value}" =~ ^[0-9]+$ ]] || die "Invalid ${key} value '${value}' in ${CHROMIUM_VERSION_FILE}"
  printf '%s\n' "${value}"
}

ensure_workspace() {
  mkdir -p "${WORKDIR}"

  if [[ ! -f "${WORKDIR}/.gclient" ]]; then
    log "Creating ${WORKDIR}/.gclient"
    cat > "${WORKDIR}/.gclient" <<'GCLIENT'
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]
target_os = ["android"]
GCLIENT
  fi

  if [[ ! -d "${WORKDIR}/src/.git" ]]; then
    log "Cloning Chromium source into ${WORKDIR}/src"
    git clone --filter=blob:none https://chromium.googlesource.com/chromium/src.git "${WORKDIR}/src"
  fi
}

checkout_tag() {
  local tag="$1"

  pushd "${WORKDIR}/src" >/dev/null
  log "Fetching Chromium tags"
  git fetch --force --tags origin

  git rev-parse --verify "refs/tags/${tag}" >/dev/null 2>&1 || die "Chromium tag '${tag}' was not found"

  local current
  current="$(git describe --tags --exact-match 2>/dev/null || true)"
  if [[ "${current}" != "${tag}" ]]; then
    log "Checking out Chromium tag ${tag}"
    git checkout --force "${tag}"
  else
    log "Chromium tag ${tag} already checked out"
  fi
  popd >/dev/null
}

sync_dependencies() {
  log "Running gclient sync (this can take a long time)"
  pushd "${WORKDIR}" >/dev/null
  gclient sync --with_branch_heads --with_tags -D
  popd >/dev/null
}

clean_source_tree() {
  pushd "${WORKDIR}/src" >/dev/null
  log "Resetting source tree to a clean ${PWD} state before overlay"
  git reset --hard HEAD
  git clean -ffd
  popd >/dev/null
}

apply_overlay() {
  local manifest
  manifest="$(mktemp)"

  git -C "${REPO_ROOT}" ls-files -z -- . \
    ':(exclude).github/**' \
    ':(exclude)ci/**' \
    ':(exclude)toolbox/**' \
    ':(exclude)AGENTS.md' \
    ':(exclude)ARCHITECTURE.md' \
    ':(exclude)CHANGELOG.md' \
    ':(exclude)README.md' \
    ':(exclude)CHROMIUM_VERSION' \
    ':(exclude)KIWI_VERSION' \
    ':(exclude)VERSION' \
    ':(exclude)fetch_from_upstream.sh' \
    ':(exclude)kiwi_logo_circle.svg' > "${manifest}"

  if [[ ! -s "${manifest}" ]]; then
    rm -f "${manifest}"
    die "Overlay manifest is empty"
  fi

  log "Applying Afterbird overlay onto Chromium tree"
  rsync -a --from0 --files-from="${manifest}" "${REPO_ROOT}/" "${WORKDIR}/src/"
  rm -f "${manifest}"
}

run_gn_checks() {
  [[ -f "${REFERENCE_ARGS_FILE}" ]] || die "Missing reference args file: ${REFERENCE_ARGS_FILE}"

  local out_path="${WORKDIR}/src/${OUT_DIR}"
  mkdir -p "${out_path}"
  cp "${REFERENCE_ARGS_FILE}" "${out_path}/args.gn"

  pushd "${WORKDIR}/src" >/dev/null
  log "Running gn gen ${OUT_DIR}"
  gn gen "${OUT_DIR}"

  # Smoke graph check: verifies the build graph can be resolved for chrome_public_apk.
  log "Running gn graph check for //chrome/android:chrome_public_apk"
  gn desc "${OUT_DIR}" //chrome/android:chrome_public_apk deps --all >/dev/null
  popd >/dev/null
}

run_full_build() {
  pushd "${WORKDIR}/src" >/dev/null
  log "Running full build target ${TARGET}"
  autoninja -C "${OUT_DIR}" "${TARGET}"
  popd >/dev/null
}

main() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --workdir)
        [[ $# -ge 2 ]] || die "--workdir requires a value"
        WORKDIR="$2"
        shift 2
        ;;
      --out-dir)
        [[ $# -ge 2 ]] || die "--out-dir requires a value"
        OUT_DIR="$2"
        shift 2
        ;;
      --target)
        [[ $# -ge 2 ]] || die "--target requires a value"
        TARGET="$2"
        shift 2
        ;;
      --full-build)
        FULL_BUILD=1
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

  [[ -f "${CHROMIUM_VERSION_FILE}" ]] || die "Missing ${CHROMIUM_VERSION_FILE}"
  validate_out_dir

  require_cmd awk
  require_cmd git
  require_cmd gclient
  require_cmd gn
  require_cmd rsync

  if [[ "${FULL_BUILD}" -eq 1 ]]; then
    require_cmd autoninja
  fi

  local major minor build patch tag
  major="$(read_version_part CHROMIUM_MAJOR)"
  minor="$(read_version_part CHROMIUM_MINOR)"
  build="$(read_version_part CHROMIUM_BUILD)"
  patch="$(read_version_part CHROMIUM_PATCH)"
  tag="${major}.${minor}.${build}.${patch}"

  log "Pinned Chromium tag: ${tag}"
  log "Workspace: ${WORKDIR}"
  log "Output dir: ${OUT_DIR}"

  ensure_workspace
  checkout_tag "${tag}"
  sync_dependencies
  clean_source_tree
  apply_overlay
  run_gn_checks

  if [[ "${FULL_BUILD}" -eq 1 ]]; then
    run_full_build
  else
    log "Smoke pipeline completed"
  fi

  log "Pipeline finished successfully"
}

main "$@"
