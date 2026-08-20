#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

APK_PATH=""
APP_PACKAGE="${AFTERBIRD_APP_PACKAGE:-com.danosito.afterbird}"
LAUNCH_ACTIVITY="${AFTERBIRD_LAUNCH_ACTIVITY:-}"
MODERN_SITES_FILE="${REPO_ROOT}/tests/emulator/modern_sites.txt"
INTERNAL_PAGES_FILE="${REPO_ROOT}/tests/emulator/internal_pages_smoke.txt"
RUN_DURATION_SECONDS=120
MEMINFO_INTERVAL_SECONDS=10
STRICT_INTERNAL_PAGE_LAUNCH=1
ADB_SERIAL="${ANDROID_SERIAL:-}"
CMDLINE_FILE="/data/local/tmp/chrome-command-line"
LAUNCH_COMPONENT=""
ARTIFACT_DIR="${AFTERBIRD_EMULATOR_ARTIFACT_DIR:-${REPO_ROOT}/tests/artifacts/emulator-$(date +%Y%m%d_%H%M%S)}"

LOGCAT_FILE=""
MEMINFO_FILE=""
MEMINFO_SUMMARY_FILE=""
CRASH_FILE=""
SMOKE_FILE=""
INTERNAL_RESULTS_FILE=""
E2E_RESULTS_FILE=""
LINES=()

LOGCAT_PID=""
MEMINFO_PID=""

usage() {
  cat <<'USAGE'
Usage:
  ci/android_emulator_test.sh --apk <path> [options]

Required:
  --apk <path>                     APK path to install on a running emulator/device.

Options:
  --package <name>                 Android package name (default: com.danosito.afterbird).
  --activity <activity>            Launch activity class (for example org.chromium...ChromeTabbedActivity).
  --serial <serial>                adb serial (if multiple devices are connected).
  --site-list <path>               URL list for modern-site flow (default: tests/emulator/modern_sites.txt).
  --internal-pages <path>          Internal pages list (default: tests/emulator/internal_pages_smoke.txt).
  --duration-sec <seconds>         Total modern-site flow duration (default: 120).
  --mem-interval-sec <seconds>     dumpsys meminfo interval (default: 10).
  --artifact-dir <path>            Output directory for logs/reports.
  --allow-internal-page-failures   Do not fail run when an internal page launch intent fails.
  --help                           Show this help text.

Behavior:
  1) Installs APK
  2) Smoke-launches browser
  3) Runs internal page launchability checks
  4) Runs ~120s modern-site flow
  5) Captures package-scoped logcat crash signals and dumpsys meminfo trend
USAGE
}

log() {
  printf '[emulator-test] %s\n' "$*"
}

die() {
  printf '[emulator-test][error] %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  local cmd="$1"
  command -v "${cmd}" >/dev/null 2>&1 || die "Missing required command: ${cmd}"
}

adb_cmd() {
  if [[ -n "${ADB_SERIAL}" ]]; then
    adb -s "${ADB_SERIAL}" "$@"
  else
    adb "$@"
  fi
}

cleanup() {
  if [[ -n "${MEMINFO_PID}" ]] && kill -0 "${MEMINFO_PID}" 2>/dev/null; then
    kill "${MEMINFO_PID}" >/dev/null 2>&1 || true
    wait "${MEMINFO_PID}" >/dev/null 2>&1 || true
  fi

  if [[ -n "${LOGCAT_PID}" ]] && kill -0 "${LOGCAT_PID}" 2>/dev/null; then
    kill "${LOGCAT_PID}" >/dev/null 2>&1 || true
    wait "${LOGCAT_PID}" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --apk)
        [[ $# -ge 2 ]] || die "--apk requires a value"
        APK_PATH="$2"
        shift 2
        ;;
      --package)
        [[ $# -ge 2 ]] || die "--package requires a value"
        APP_PACKAGE="$2"
        shift 2
        ;;
      --activity)
        [[ $# -ge 2 ]] || die "--activity requires a value"
        LAUNCH_ACTIVITY="$2"
        shift 2
        ;;
      --serial)
        [[ $# -ge 2 ]] || die "--serial requires a value"
        ADB_SERIAL="$2"
        shift 2
        ;;
      --site-list)
        [[ $# -ge 2 ]] || die "--site-list requires a value"
        MODERN_SITES_FILE="$2"
        shift 2
        ;;
      --internal-pages)
        [[ $# -ge 2 ]] || die "--internal-pages requires a value"
        INTERNAL_PAGES_FILE="$2"
        shift 2
        ;;
      --duration-sec)
        [[ $# -ge 2 ]] || die "--duration-sec requires a value"
        RUN_DURATION_SECONDS="$2"
        shift 2
        ;;
      --mem-interval-sec)
        [[ $# -ge 2 ]] || die "--mem-interval-sec requires a value"
        MEMINFO_INTERVAL_SECONDS="$2"
        shift 2
        ;;
      --artifact-dir)
        [[ $# -ge 2 ]] || die "--artifact-dir requires a value"
        ARTIFACT_DIR="$2"
        shift 2
        ;;
      --allow-internal-page-failures)
        STRICT_INTERNAL_PAGE_LAUNCH=0
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

  [[ -n "${APK_PATH}" ]] || die "--apk is required"
  [[ -f "${APK_PATH}" ]] || die "APK file does not exist: ${APK_PATH}"
  [[ "${RUN_DURATION_SECONDS}" =~ ^[0-9]+$ ]] || die "--duration-sec must be an integer"
  [[ "${MEMINFO_INTERVAL_SECONDS}" =~ ^[0-9]+$ ]] || die "--mem-interval-sec must be an integer"
  [[ "${MEMINFO_INTERVAL_SECONDS}" -gt 0 ]] || die "--mem-interval-sec must be > 0"
  [[ -f "${MODERN_SITES_FILE}" ]] || die "Missing site list file: ${MODERN_SITES_FILE}"
  [[ -f "${INTERNAL_PAGES_FILE}" ]] || die "Missing internal pages file: ${INTERNAL_PAGES_FILE}"
}

wait_for_device_boot() {
  log "Waiting for device to be available"
  adb_cmd wait-for-device

  local timeout=180
  local elapsed=0
  while true; do
    local boot_status
    boot_status="$(adb_cmd shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' | tr -d '[:space:]')"
    if [[ "${boot_status}" == "1" ]]; then
      break
    fi

    if (( elapsed >= timeout )); then
      die "Timed out waiting for emulator boot"
    fi

    sleep 2
    elapsed=$((elapsed + 2))
  done

  adb_cmd shell input keyevent 82 >/dev/null 2>&1 || true
  log "Device boot complete"
}

prepare_outputs() {
  mkdir -p "${ARTIFACT_DIR}"
  LOGCAT_FILE="${ARTIFACT_DIR}/logcat.txt"
  MEMINFO_FILE="${ARTIFACT_DIR}/meminfo.csv"
  MEMINFO_SUMMARY_FILE="${ARTIFACT_DIR}/meminfo_summary.txt"
  CRASH_FILE="${ARTIFACT_DIR}/crash_signals.txt"
  SMOKE_FILE="${ARTIFACT_DIR}/smoke_startup.txt"
  INTERNAL_RESULTS_FILE="${ARTIFACT_DIR}/internal_pages.tsv"
  E2E_RESULTS_FILE="${ARTIFACT_DIR}/modern_sites.tsv"

  printf 'timestamp_utc,pss_kb\n' > "${MEMINFO_FILE}"
  printf 'timestamp_utc\turl\tstatus\tactivity\n' > "${INTERNAL_RESULTS_FILE}"
  printf 'timestamp_utc\turl\tstatus\tactivity\n' > "${E2E_RESULTS_FILE}"
}

install_apk() {
  log "Installing APK: ${APK_PATH}"
  adb_cmd install -r -d "${APK_PATH}" >/dev/null
}

write_command_line() {
  # Without --disable-fre the browser parks in FirstRunActivity and every
  # navigation intent is dropped, so all page checks read as failures.
  log "Writing ${CMDLINE_FILE} (--disable-fre)"
  adb_cmd shell "echo '_ --disable-fre --no-default-browser-check' > ${CMDLINE_FILE}" >/dev/null 2>&1 || true
  adb_cmd shell "chmod 0644 ${CMDLINE_FILE}" >/dev/null 2>&1 || true
}

resolve_component() {
  if [[ -n "${LAUNCH_ACTIVITY}" ]]; then
    printf '%s/%s\n' "${APP_PACKAGE}" "${LAUNCH_ACTIVITY}"
    return
  fi

  local resolved
  resolved="$(adb_cmd shell cmd package resolve-activity --brief "${APP_PACKAGE}" 2>/dev/null | tr -d '\r' | grep '/' | head -n1 || true)"

  if [[ -n "${resolved}" ]]; then
    printf '%s\n' "${resolved}"
    return
  fi

  printf '%s/%s\n' "${APP_PACKAGE}" "org.chromium.chrome.browser.ChromeTabbedActivity"
}

run_smoke_startup_check() {
  local component="$1"
  log "Running startup smoke check against ${component}"

  local output
  output="$(adb_cmd shell am start -W -n "${component}" 2>&1 | tr -d '\r' || true)"
  printf '%s\n' "${output}" > "${SMOKE_FILE}"

  sleep 5

  local pid
  pid="$(adb_cmd shell pidof "${APP_PACKAGE}" 2>/dev/null | tr -d '\r' || true)"
  if [[ -z "${pid}" ]]; then
    die "Smoke check failed: package ${APP_PACKAGE} did not stay running"
  fi

  if ! printf '%s\n' "${output}" | grep -qi 'status: ok'; then
    die "Smoke check failed: launch status is not ok"
  fi
}

start_logcat_capture() {
  log "Starting logcat capture"
  adb_cmd logcat -c
  adb_cmd logcat -v time > "${LOGCAT_FILE}" 2>&1 &
  LOGCAT_PID=$!
}

capture_meminfo_once() {
  local raw pss
  raw="$(adb_cmd shell dumpsys meminfo "${APP_PACKAGE}" 2>/dev/null | tr -d '\r' || true)"
  pss="$(printf '%s\n' "${raw}" | awk '
    /^TOTAL PSS:/ { print $3; found=1; exit }
    $1 == "TOTAL" && $2 ~ /^[0-9]+$/ { print $2; found=1; exit }
    END { if (!found) exit 1 }
  ' 2>/dev/null || true)"

  if [[ -z "${pss}" ]]; then
    pss="NA"
  fi

  printf '%s,%s\n' "$(date -u +%FT%TZ)" "${pss}" >> "${MEMINFO_FILE}"
}

start_meminfo_sampling() {
  log "Starting meminfo sampling every ${MEMINFO_INTERVAL_SECONDS}s"
  (
    while true; do
      capture_meminfo_once || true
      sleep "${MEMINFO_INTERVAL_SECONDS}"
    done
  ) &
  MEMINFO_PID=$!
}

read_lines() {
  local file="$1"
  LINES=()
  while IFS= read -r line; do
    LINES+=("${line}")
  done < <(grep -Ev '^[[:space:]]*(#|$)' "${file}" | sed 's/[[:space:]]\+$//' || true)

  if [[ "${#LINES[@]}" -eq 0 ]]; then
    die "No entries found in ${file}"
  fi
}

stop_background_capture() {
  if [[ -n "${MEMINFO_PID}" ]] && kill -0 "${MEMINFO_PID}" 2>/dev/null; then
    kill "${MEMINFO_PID}" >/dev/null 2>&1 || true
    wait "${MEMINFO_PID}" >/dev/null 2>&1 || true
  fi
  MEMINFO_PID=""

  if [[ -n "${LOGCAT_PID}" ]] && kill -0 "${LOGCAT_PID}" 2>/dev/null; then
    kill "${LOGCAT_PID}" >/dev/null 2>&1 || true
    wait "${LOGCAT_PID}" >/dev/null 2>&1 || true
  fi
  LOGCAT_PID=""
}

launch_url_and_record() {
  local url="$1"
  local results_file="$2"
  local output
  # Explicit component: upstream M151 registers no intent filter for the
  # chrome:// scheme, so an implicit VIEW intent fails to resolve.
  output="$(adb_cmd shell am start -W -n "${LAUNCH_COMPONENT}" -a android.intent.action.VIEW -d "${url}" 2>&1 | tr -d '\r' || true)"

  local status="fail"
  if printf '%s\n' "${output}" | grep -qi 'status: ok'; then
    status="ok"
  fi

  local activity
  activity="$(printf '%s\n' "${output}" | awk -F': ' '/^Activity:/ {print $2; exit}')"
  printf '%s\t%s\t%s\t%s\n' "$(date -u +%FT%TZ)" "${url}" "${status}" "${activity:-unknown}" >> "${results_file}"

  if [[ "${status}" != "ok" ]]; then
    return 1
  fi

  return 0
}

run_internal_page_checks() {
  log "Running internal page launch checks"
  read_lines "${INTERNAL_PAGES_FILE}"

  local failures=0
  local page
  for page in "${LINES[@]}"; do
    if ! launch_url_and_record "${page}" "${INTERNAL_RESULTS_FILE}"; then
      failures=$((failures + 1))
    fi
    adb_cmd shell input swipe 500 1700 500 700 250 >/dev/null 2>&1 || true
    sleep 3
  done

  if (( failures > 0 )) && (( STRICT_INTERNAL_PAGE_LAUNCH == 1 )); then
    die "Internal page launch checks failed for ${failures} page(s)"
  fi

  if (( failures > 0 )); then
    log "Internal page launch checks had ${failures} failure(s) but run continues (--allow-internal-page-failures)"
  fi
}

run_modern_site_flow() {
  log "Running modern-site flow for ${RUN_DURATION_SECONDS}s"
  read_lines "${MODERN_SITES_FILE}"

  local url_count="${#LINES[@]}"
  local dwell_seconds=$((RUN_DURATION_SECONDS / url_count))
  if (( dwell_seconds < 8 )); then
    dwell_seconds=8
  fi

  local start_epoch
  start_epoch="$(date +%s)"

  local iteration=0
  local failures=0
  while true; do
    local now elapsed remaining
    now="$(date +%s)"
    elapsed=$((now - start_epoch))
    remaining=$((RUN_DURATION_SECONDS - elapsed))

    if (( remaining <= 0 )); then
      break
    fi

    local idx=$((iteration % url_count))
    local url="${LINES[${idx}]}"

    if ! launch_url_and_record "${url}" "${E2E_RESULTS_FILE}"; then
      failures=$((failures + 1))
    fi

    local sleep_for="${dwell_seconds}"
    if (( remaining < sleep_for )); then
      sleep_for="${remaining}"
    fi

    local half=$((sleep_for / 2))
    if (( half > 0 )); then
      adb_cmd shell input swipe 500 1700 500 700 350 >/dev/null 2>&1 || true
      sleep "${half}"
    fi

    local second_half=$((sleep_for - half))
    if (( second_half > 0 )); then
      adb_cmd shell input swipe 500 1700 500 700 350 >/dev/null 2>&1 || true
      sleep "${second_half}"
    fi

    iteration=$((iteration + 1))
  done

  if (( failures > 0 )); then
    die "Modern-site flow had ${failures} failed URL launch(es)"
  fi
}

summarize_meminfo() {
  if ! awk -F, '
    NR > 1 && $2 ~ /^[0-9]+$/ {
      n += 1
      sum += $2
      if (min == "" || $2 < min) min = $2
      if (max == "" || $2 > max) max = $2
    }
    END {
      if (n == 0) {
        print "samples=0"
        exit 1
      }
      printf "samples=%d\n", n
      printf "avg_pss_kb=%.2f\n", (sum / n)
      printf "min_pss_kb=%d\n", min
      printf "max_pss_kb=%d\n", max
      printf "delta_pss_kb=%d\n", (max - min)
    }
  ' "${MEMINFO_FILE}" > "${MEMINFO_SUMMARY_FILE}"; then
    printf 'samples=0\n' > "${MEMINFO_SUMMARY_FILE}"
  fi

  log "Memory summary:"
  cat "${MEMINFO_SUMMARY_FILE}"
}

scan_crash_signals() {
  # Scope crash detection to the app-under-test package and its subprocesses so
  # unrelated emulator/system crashes do not fail the run.
  if awk -v pkg="${APP_PACKAGE}" '
    function add_line(line, lower_line) {
      block_n += 1
      block_lines[block_n] = line
      lower_line = tolower(line)
      if (index(lower_line, pkg_lower) > 0 ||
          index(lower_line, ">>> " pkg_lower " <<<") > 0 ||
          lower_line ~ ("process: " pkg_regex "([,: ]|$)") ||
          lower_line ~ ("cmdline: " pkg_regex "([ :]|$)")) {
        block_match_pkg = 1
      }
    }

    function start_block(reason, window, line) {
      in_block = 1
      block_reason = reason
      block_remaining = window
      block_n = 0
      block_match_pkg = 0
      delete block_lines
      add_line(line)
    }

    function flush_block(i) {
      if (in_block && block_match_pkg) {
        print "--- crash: " block_reason " ---"
        for (i = 1; i <= block_n; i++) {
          print block_lines[i]
        }
        print ""
        found = 1
      }
      in_block = 0
      block_reason = ""
      block_remaining = 0
      block_n = 0
      block_match_pkg = 0
      delete block_lines
    }

    BEGIN {
      pkg_lower = tolower(pkg)
      pkg_regex = pkg
      gsub(/\./, "\\.", pkg_regex)
      in_block = 0
      found = 0
    }

    {
      line = $0
      lower_line = tolower(line)

      if (in_block) {
        add_line(line)
        block_remaining -= 1
        if (block_remaining <= 0) {
          flush_block()
        }
        next
      }

      if (index(lower_line, "fatal exception") > 0) {
        start_block("java_fatal_exception", 20, line)
        next
      }

      if (index(lower_line, "fatal signal ") > 0 ||
          index(lower_line, "sigsegv") > 0 ||
          index(lower_line, "sigabrt") > 0 ||
          index(lower_line, "crash_dump32") > 0 ||
          index(lower_line, "crash_dump64") > 0) {
        start_block("native_fatal_signal", 40, line)
        next
      }
    }

    END {
      flush_block()
      if (found) {
        exit 0
      }
      exit 1
    }
  ' "${LOGCAT_FILE}" > "${CRASH_FILE}"; then
    local count
    count="$(grep -c '^--- crash:' "${CRASH_FILE}" || true)"
    [[ -n "${count}" ]] || count=1
    die "Detected ${count} package-scoped crash signal block(s) in logcat"
  fi

  : > "${CRASH_FILE}"
  log "No package-scoped crash patterns detected"
}

main() {
  parse_args "$@"

  require_cmd adb
  require_cmd awk
  require_cmd grep
  require_cmd sed

  prepare_outputs
  wait_for_device_boot
  install_apk
  write_command_line

  local component
  component="$(resolve_component)"
  LAUNCH_COMPONENT="${component}"

  start_logcat_capture
  start_meminfo_sampling

  run_smoke_startup_check "${component}"
  run_internal_page_checks
  run_modern_site_flow

  stop_background_capture
  summarize_meminfo
  scan_crash_signals

  log "Emulator checks completed successfully"
  log "Artifacts: ${ARTIFACT_DIR}"
}

main "$@"
