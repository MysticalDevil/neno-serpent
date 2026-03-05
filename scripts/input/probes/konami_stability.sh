#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${CACHE_ROOT}/input}"
mkdir -p "${TMP_ROOT}"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/build_paths.sh"
BUILD_DIR="$(resolve_build_dir dev)"
APP_BIN="${APP_BIN:-${BUILD_DIR}/NenoSerpent}"
ITERATIONS="${ITERATIONS:-5}"
BOOT_WAIT="${BOOT_WAIT:-3.8}"
STEP_DELAY="${STEP_DELAY:-0.20}"
LOG_DIR="${LOG_DIR:-${TMP_ROOT}/nenoserpent_konami_probe}"

mkdir -p "${LOG_DIR}"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[error] missing command: $1" >&2
    exit 1
  fi
}

need_cmd cmake
need_cmd awk
need_cmd rg

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[info] building ${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel >/dev/null
fi

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[error] app not found: ${APP_BIN}" >&2
  exit 1
fi

send_token() {
  local file="$1"
  local token="$2"
  printf "%s\n" "${token}" >>"${file}"
  sleep "${STEP_DELAY}"
}

send_konami() {
  local file="$1"
  local seq=(U U D D L R L R B A)
  local token
  for token in "${seq[@]}"; do
    send_token "${file}" "${token}"
  done
}

count_start_menu_requests() {
  local log_file="$1"
  rg -n "\\[StateFlow\\]\\[GameLogic\\] requestStateChange -> StartMenu" "${log_file}" | wc -l | awk '{print $1}'
}

run_case() {
  local case_name="$1"
  local with_konami="$2"
  local iter="$3"
  local cfg_dir input_file log_file
  cfg_dir="$(mktemp -d "${TMP_ROOT}/nenoserpent_konami_cfg.XXXXXX")"
  input_file="${TMP_ROOT}/nenoserpent_konami_input_${case_name}_${iter}.txt"
  log_file="${LOG_DIR}/${case_name}_${iter}.log"
  : > "${input_file}"

  XDG_CONFIG_HOME="${cfg_dir}" NENOSERPENT_INPUT_FILE="${input_file}" "${APP_BIN}" >"${log_file}" 2>&1 &
  local app_pid=$!

  cleanup() {
    kill "${app_pid}" >/dev/null 2>&1 || true
    wait "${app_pid}" >/dev/null 2>&1 || true
    rm -rf "${cfg_dir}" >/dev/null 2>&1 || true
    rm -f "${input_file}" >/dev/null 2>&1 || true
  }
  trap cleanup RETURN

  sleep "${BOOT_WAIT}"

  # enter game then pause overlay
  send_token "${input_file}" "START"
  send_token "${input_file}" "START"

  if [[ "${with_konami}" == "1" ]]; then
    send_konami "${input_file}"
  fi

  # In icon lab: B exits to menu. In normal paused overlay: B should not.
  send_token "${input_file}" "B"
  sleep 0.5
  send_token "${input_file}" "ESC"
  sleep 0.2

  local start_menu_count
  start_menu_count="$(count_start_menu_requests "${log_file}")"

  if [[ "${with_konami}" == "1" ]]; then
    if (( start_menu_count < 2 )); then
      echo "[fail] case=${case_name} iter=${iter} expected >=2 StartMenu transitions (boot + Konami+B exit), got ${start_menu_count}"
      echo "[fail] log: ${log_file}"
      return 1
    fi
    echo "[ok] case=${case_name} iter=${iter} StartMenu transitions=${start_menu_count}"
  else
    if (( start_menu_count != 1 )); then
      echo "[fail] case=${case_name} iter=${iter} expected exactly 1 StartMenu transition (boot only) for paused+B without Konami, got ${start_menu_count}"
      echo "[fail] log: ${log_file}"
      return 1
    fi
    echo "[ok] case=${case_name} iter=${iter} StartMenu transitions=${start_menu_count}"
  fi
}

echo "[info] running Konami stability probe, iterations=${ITERATIONS}"
for i in $(seq 1 "${ITERATIONS}"); do
  run_case "baseline_paused_b" "0" "${i}"
  run_case "konami_then_b" "1" "${i}"
done
echo "[ok] all probes passed. logs: ${LOG_DIR}"
