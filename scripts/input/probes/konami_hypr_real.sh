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
WAIT_SECONDS="${WAIT_SECONDS:-14}"
BOOT_SETTLE_SECONDS="${BOOT_SETTLE_SECONDS:-4.0}"
STEP_DELAY="${STEP_DELAY:-0.28}"
WINDOW_CLASS="${WINDOW_CLASS:-devil.org.NenoSerpent}"
WINDOW_TITLE="${WINDOW_TITLE:-Snake GB Edition}"
LOG_DIR="${LOG_DIR:-${TMP_ROOT}/nenoserpent_konami_hypr_probe}"

mkdir -p "${LOG_DIR}"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[error] missing command: $1" >&2
    exit 1
  fi
}

need_cmd cmake
need_cmd hyprctl
need_cmd jq
need_cmd rg
need_cmd awk

if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  echo "[error] this probe requires Wayland session"
  exit 1
fi

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[info] building ${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel >/dev/null
fi

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[error] app not found: ${APP_BIN}" >&2
  exit 1
fi

APP_PID=""
WINDOW_ADDR=""
CFG_TMP=""
RUN_LOG=""

cleanup_case() {
  if [[ -n "${APP_PID}" ]]; then
    kill "${APP_PID}" >/dev/null 2>&1 || true
    wait "${APP_PID}" >/dev/null 2>&1 || true
  fi
  APP_PID=""
  WINDOW_ADDR=""
  if [[ -n "${CFG_TMP}" && -d "${CFG_TMP}" ]]; then
    rm -rf "${CFG_TMP}" >/dev/null 2>&1 || true
  fi
  CFG_TMP=""
}

wait_window_ready() {
  local deadline clients_json window_info
  deadline=$((SECONDS + WAIT_SECONDS))
  while (( SECONDS < deadline )); do
    clients_json="$(hyprctl clients -j 2>/dev/null || true)"
    if ! jq -e . >/dev/null 2>&1 <<<"${clients_json}"; then
      sleep 0.2
      continue
    fi
    window_info="$(jq -r --arg cls "${WINDOW_CLASS}" --arg ttl "${WINDOW_TITLE}" --argjson pid "${APP_PID}" '
      .[] | select((.pid == $pid) or (.class == $cls) or ((.title // "") | contains($ttl))) |
      "\(.address)"
    ' <<<"${clients_json}" | head -n1)"
    if [[ -n "${window_info}" ]]; then
      WINDOW_ADDR="${window_info}"
      return 0
    fi
    sleep 0.2
  done
  return 1
}

send_key() {
  local key="$1"
  hyprctl dispatch sendshortcut ",${key},address:${WINDOW_ADDR}" >/dev/null 2>&1 || \
    hyprctl dispatch sendshortcut ",${key}," >/dev/null 2>&1 || true
  sleep "${STEP_DELAY}"
}

wait_log_contains() {
  local log_file="$1"
  local pattern="$2"
  local timeout_s="${3:-6}"
  local deadline
  deadline=$((SECONDS + timeout_s))
  while (( SECONDS < deadline )); do
    if rg -q "${pattern}" "${log_file}" 2>/dev/null; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

send_konami_keys() {
  local seq=(Up Up Down Down Left Right Left Right X Z)
  local k
  for k in "${seq[@]}"; do
    send_key "${k}"
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
  local start_menu_count

  CFG_TMP="$(mktemp -d "${TMP_ROOT}/nenoserpent_konami_hypr_cfg.XXXXXX")"
  RUN_LOG="${LOG_DIR}/${case_name}_${iter}.log"

  XDG_CONFIG_HOME="${CFG_TMP}" "${APP_BIN}" >"${RUN_LOG}" 2>&1 &
  APP_PID=$!

  if ! wait_window_ready; then
    echo "[fail] case=${case_name} iter=${iter} window not found"
    echo "[fail] log: ${RUN_LOG}"
    cleanup_case
    exit 2
  fi

  hyprctl dispatch focuswindow "address:${WINDOW_ADDR}" >/dev/null 2>&1 || true
  sleep "${BOOT_SETTLE_SECONDS}"

  # Enter paused overlay: Start game, then pause.
  # We verify this in logs before starting Konami to avoid false negatives.
  local entered_playing=0 entered_paused=0
  for _ in 1 2 3; do
    hyprctl dispatch focuswindow "address:${WINDOW_ADDR}" >/dev/null 2>&1 || true
    send_key "Return"
    if wait_log_contains "${RUN_LOG}" "setInternalState: StartMenu -> Playing" 5; then
      entered_playing=1
    fi
    send_key "Return"
    if wait_log_contains "${RUN_LOG}" "setInternalState: Playing -> Paused" 5; then
      entered_paused=1
      break
    fi
  done

  if (( entered_playing == 0 || entered_paused == 0 )); then
    echo "[fail] case=${case_name} iter=${iter} could not reliably enter Paused before Konami"
    echo "[fail] log: ${RUN_LOG}"
    cleanup_case
    exit 2
  fi

  if [[ "${with_konami}" == "1" ]]; then
    send_konami_keys
  fi

  # In icon lab, B exits to menu. In normal paused, B should not exit.
  send_key "X"
  sleep 0.4
  send_key "Escape"
  sleep 0.2

  start_menu_count="$(count_start_menu_requests "${RUN_LOG}")"

  if [[ "${with_konami}" == "1" ]]; then
    if (( start_menu_count < 2 )); then
      echo "[fail] case=${case_name} iter=${iter} expected >=2 StartMenu transitions (boot + Konami+B exit), got ${start_menu_count}"
      echo "[fail] log: ${RUN_LOG}"
      cleanup_case
      exit 2
    fi
    echo "[ok] case=${case_name} iter=${iter} StartMenu transitions=${start_menu_count}"
  else
    if (( start_menu_count != 1 )); then
      echo "[fail] case=${case_name} iter=${iter} expected exactly 1 StartMenu transition (boot only), got ${start_menu_count}"
      echo "[fail] log: ${RUN_LOG}"
      cleanup_case
      exit 2
    fi
    echo "[ok] case=${case_name} iter=${iter} StartMenu transitions=${start_menu_count}"
  fi

  cleanup_case
}

trap cleanup_case EXIT

echo "[info] running Hyprland real-input Konami probe, iterations=${ITERATIONS}"
for i in $(seq 1 "${ITERATIONS}"); do
  run_case "baseline_paused_b" "0" "${i}"
  run_case "konami_then_b" "1" "${i}"
done
echo "[ok] all probes passed. logs: ${LOG_DIR}"
