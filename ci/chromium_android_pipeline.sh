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
CHROMIUM_SRC_GIT_URL="${AFTERBIRD_CHROMIUM_SRC_GIT_URL:-https://chromium.googlesource.com/chromium/src.git}"
FETCH_RETRIES="${AFTERBIRD_FETCH_RETRIES:-3}"
FETCH_BACKOFF_SECONDS="${AFTERBIRD_FETCH_BACKOFF_SECONDS:-10}"
FETCH_TIMEOUT_SECONDS="${AFTERBIRD_FETCH_TIMEOUT_SECONDS:-600}"
GCLIENT_RETRIES="${AFTERBIRD_GCLIENT_RETRIES:-2}"
GCLIENT_BACKOFF_SECONDS="${AFTERBIRD_GCLIENT_BACKOFF_SECONDS:-20}"
GCLIENT_NO_HISTORY="${AFTERBIRD_GCLIENT_NO_HISTORY:-1}"
GCLIENT_EXTRA_ARGS="${AFTERBIRD_GCLIENT_EXTRA_ARGS:-}"
FORCE_WORKSPACE_CONFIG="${AFTERBIRD_FORCE_WORKSPACE_CONFIG:-0}"
TIMEOUT_WARNING_EMITTED=0

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

Environment overrides:
  AFTERBIRD_CHROMIUM_SRC_GIT_URL   Chromium git remote (default: chromium.googlesource.com)
  AFTERBIRD_FETCH_RETRIES          Retries for tag fetch (default: 3)
  AFTERBIRD_FETCH_BACKOFF_SECONDS  Backoff base for fetch retries (default: 10)
  AFTERBIRD_FETCH_TIMEOUT_SECONDS  Per-attempt timeout for tag fetch (default: 600)
  AFTERBIRD_GCLIENT_RETRIES        Retries for gclient sync (default: 2)
  AFTERBIRD_GCLIENT_BACKOFF_SECONDS Backoff base for gclient retries (default: 20)
  AFTERBIRD_GCLIENT_NO_HISTORY     Use --no-history during sync (default: 1)
  AFTERBIRD_GCLIENT_EXTRA_ARGS     Extra args appended to gclient sync
  AFTERBIRD_FORCE_WORKSPACE_CONFIG Rewrite existing .gclient/origin URL (default: 0)
USAGE
}

log() {
  printf '[ci-pipeline] %s\n' "$*"
}

warn() {
  printf '[ci-pipeline][warn] %s\n' "$*" >&2
}

die() {
  printf '[ci-pipeline][error] %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  local cmd="$1"
  command -v "${cmd}" >/dev/null 2>&1 || die "Missing required command: ${cmd}"
}

validate_non_negative_int() {
  local value="$1"
  local key="$2"
  [[ "${value}" =~ ^[0-9]+$ ]] || die "${key} must be a non-negative integer (got '${value}')"
}

timeout_bin() {
  if command -v timeout >/dev/null 2>&1; then
    printf 'timeout\n'
    return 0
  fi
  if command -v gtimeout >/dev/null 2>&1; then
    printf 'gtimeout\n'
    return 0
  fi
  return 0
}

run_with_timeout() {
  local timeout_seconds="$1"
  shift

  if [[ "${timeout_seconds}" -le 0 ]]; then
    "$@"
    return
  fi

  local tbin
  tbin="$(timeout_bin)"
  if [[ -n "${tbin}" ]]; then
    "${tbin}" "${timeout_seconds}s" "$@"
  else
    if [[ "${TIMEOUT_WARNING_EMITTED}" -eq 0 ]]; then
      warn "No timeout binary found ('timeout'/'gtimeout'). Running without timeout enforcement."
      TIMEOUT_WARNING_EMITTED=1
    fi
    "$@"
  fi
}

run_with_retries() {
  local attempts="$1"
  local backoff_seconds="$2"
  local label="$3"
  shift 3

  local try=1
  local rc
  while true; do
    if "$@"; then
      rc=0
    else
      rc=$?
    fi

    if [[ "${rc}" -eq 0 ]]; then
      return 0
    fi

    if [[ "${try}" -ge "${attempts}" ]]; then
      die "${label} failed after ${try} attempt(s) (exit ${rc})"
    fi

    local wait_seconds=$((backoff_seconds * try))
    if [[ "${rc}" -eq 124 ]]; then
      log "${label} timed out (attempt ${try}/${attempts}); retrying in ${wait_seconds}s"
    else
      log "${label} failed (attempt ${try}/${attempts}, exit ${rc}); retrying in ${wait_seconds}s"
    fi

    sleep "${wait_seconds}"
    try=$((try + 1))
  done
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

  if [[ ! -f "${WORKDIR}/.gclient" || "${FORCE_WORKSPACE_CONFIG}" == "1" ]]; then
    if [[ -f "${WORKDIR}/.gclient" ]]; then
      log "Rewriting ${WORKDIR}/.gclient (AFTERBIRD_FORCE_WORKSPACE_CONFIG=1)"
    else
      log "Creating ${WORKDIR}/.gclient"
    fi
    cat > "${WORKDIR}/.gclient" <<GCLIENT
solutions = [
  {
    "name": "src",
    "url": "${CHROMIUM_SRC_GIT_URL}",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]
target_os = ["android"]
GCLIENT
  else
    log "Keeping existing ${WORKDIR}/.gclient (set AFTERBIRD_FORCE_WORKSPACE_CONFIG=1 to rewrite)"
  fi

  if [[ ! -d "${WORKDIR}/src/.git" ]]; then
    log "Initializing Chromium source repo in ${WORKDIR}/src"
    mkdir -p "${WORKDIR}/src"
    git -C "${WORKDIR}/src" init
    git -C "${WORKDIR}/src" remote add origin "${CHROMIUM_SRC_GIT_URL}"
  else
    local existing_origin
    existing_origin="$(git -C "${WORKDIR}/src" remote get-url origin 2>/dev/null || true)"
    if [[ -z "${existing_origin}" ]]; then
      log "Adding missing src origin '${CHROMIUM_SRC_GIT_URL}'"
      git -C "${WORKDIR}/src" remote add origin "${CHROMIUM_SRC_GIT_URL}"
    elif [[ "${existing_origin}" != "${CHROMIUM_SRC_GIT_URL}" && "${FORCE_WORKSPACE_CONFIG}" == "1" ]]; then
      log "Updating src origin from '${existing_origin}' to '${CHROMIUM_SRC_GIT_URL}'"
      git -C "${WORKDIR}/src" remote set-url origin "${CHROMIUM_SRC_GIT_URL}"
    elif [[ "${existing_origin}" != "${CHROMIUM_SRC_GIT_URL}" ]]; then
      log "Keeping existing src origin '${existing_origin}' (set AFTERBIRD_FORCE_WORKSPACE_CONFIG=1 to update)"
    fi
  fi
}

fetch_required_tag() {
  local tag="$1"
  local refspec="+refs/tags/${tag}:refs/tags/${tag}"

  run_with_retries "${FETCH_RETRIES}" "${FETCH_BACKOFF_SECONDS}" "Chromium tag fetch (${tag}) from ${CHROMIUM_SRC_GIT_URL}" \
    run_with_timeout "${FETCH_TIMEOUT_SECONDS}" \
    env GIT_TERMINAL_PROMPT=0 \
    git -c protocol.version=2 -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=60 \
      -C "${WORKDIR}/src" fetch --no-tags --depth=1 origin "${refspec}"
}

checkout_tag() {
  local tag="$1"

  log "Fetching only required Chromium tag ${tag} from ${CHROMIUM_SRC_GIT_URL}"
  if [[ "${FETCH_TIMEOUT_SECONDS}" -gt 0 ]]; then
    log "Fetch timeout per attempt: ${FETCH_TIMEOUT_SECONDS}s"
  fi
  if [[ "${CHROMIUM_SRC_GIT_URL}" == "https://chromium.googlesource.com/chromium/src.git" ]]; then
    log "If googlesource is blocked or slow, set AFTERBIRD_CHROMIUM_SRC_GIT_URL to a reachable mirror."
  fi
  fetch_required_tag "${tag}"

  pushd "${WORKDIR}/src" >/dev/null
  git rev-parse --verify "refs/tags/${tag}" >/dev/null 2>&1 || die "Chromium tag '${tag}' was not found"

  local current
  current="$(git describe --tags --exact-match 2>/dev/null || true)"
  if [[ "${current}" != "${tag}" ]]; then
    log "Checking out Chromium tag ${tag}"
    git checkout --force --detach "${tag}"
  else
    log "Chromium tag ${tag} already checked out"
  fi
  popd >/dev/null
}

sync_dependencies() {
  local sync_args=(-D)
  if [[ "${GCLIENT_NO_HISTORY}" == "1" ]]; then
    sync_args+=(--no-history)
  fi

  if [[ -n "${GCLIENT_EXTRA_ARGS}" ]]; then
    # Intentionally split additional arguments provided as a string override.
    # shellcheck disable=SC2206
    local extra_args=( ${GCLIENT_EXTRA_ARGS} )
    sync_args+=("${extra_args[@]}")
  fi

  log "Running gclient sync (this can take a long time): gclient sync ${sync_args[*]}"
  pushd "${WORKDIR}" >/dev/null
  run_with_retries "${GCLIENT_RETRIES}" "${GCLIENT_BACKOFF_SECONDS}" "gclient sync" \
    gclient sync "${sync_args[@]}"
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

  validate_non_negative_int "${FETCH_RETRIES}" "AFTERBIRD_FETCH_RETRIES"
  validate_non_negative_int "${FETCH_BACKOFF_SECONDS}" "AFTERBIRD_FETCH_BACKOFF_SECONDS"
  validate_non_negative_int "${FETCH_TIMEOUT_SECONDS}" "AFTERBIRD_FETCH_TIMEOUT_SECONDS"
  validate_non_negative_int "${GCLIENT_RETRIES}" "AFTERBIRD_GCLIENT_RETRIES"
  validate_non_negative_int "${GCLIENT_BACKOFF_SECONDS}" "AFTERBIRD_GCLIENT_BACKOFF_SECONDS"

  if [[ "${FETCH_RETRIES}" -eq 0 ]]; then
    die "AFTERBIRD_FETCH_RETRIES must be at least 1"
  fi
  if [[ "${GCLIENT_RETRIES}" -eq 0 ]]; then
    die "AFTERBIRD_GCLIENT_RETRIES must be at least 1"
  fi
  if [[ "${GCLIENT_NO_HISTORY}" != "0" && "${GCLIENT_NO_HISTORY}" != "1" ]]; then
    die "AFTERBIRD_GCLIENT_NO_HISTORY must be '0' or '1'"
  fi
  if [[ "${FORCE_WORKSPACE_CONFIG}" != "0" && "${FORCE_WORKSPACE_CONFIG}" != "1" ]]; then
    die "AFTERBIRD_FORCE_WORKSPACE_CONFIG must be '0' or '1'"
  fi

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
  log "Chromium source URL: ${CHROMIUM_SRC_GIT_URL}"
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
