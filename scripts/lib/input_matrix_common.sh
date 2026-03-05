#!/usr/bin/env bash
set -euo pipefail

# shellcheck disable=SC1091
# shellcheck source=lib/script_common.sh
source "${ROOT_DIR}/scripts/lib/script_common.sh"
# shellcheck disable=SC1091
# shellcheck source=lib/ui_window_common.sh
source "${ROOT_DIR}/scripts/lib/ui_window_common.sh"

APP_PID=""
WINDOW_ADDR=""
GEOM=""
CFG_TMP=""
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/input}}"
mkdir -p "${TMP_ROOT}"

app_is_alive() {
  ui_window_app_is_alive "${APP_PID}"
}

stop_app() {
  ui_window_stop_app "${APP_PID}"
  APP_PID=""
}

cleanup_case() {
  stop_app
  ui_window_cleanup_isolated_config "${CFG_TMP}"
  CFG_TMP=""
}

capture_failure() {
  local case_name="$1"
  mkdir -p "${FAIL_DIR}"
  if [[ -n "${GEOM}" ]]; then
    grim -g "${GEOM}" "${FAIL_DIR}/${case_name}.png" >/dev/null 2>&1 || true
  fi
  if [[ -f "${TMP_ROOT}/nenoserpent_input_matrix_runtime.log" ]]; then
    cp "${TMP_ROOT}/nenoserpent_input_matrix_runtime.log" "${FAIL_DIR}/${case_name}.log" >/dev/null 2>&1 || true
  fi
}

die_case() {
  local case_name="$1"
  local msg="$2"
  echo "[error] ${case_name}: ${msg}"
  capture_failure "${case_name}"
  cleanup_case
  exit 2
}

wait_window_ready() {
  if ! ui_window_wait_for_window "${APP_PID}" "${WAIT_SECONDS}" "${WINDOW_CLASS}" "${WINDOW_TITLE}"; then
    return 1
  fi
  WINDOW_ADDR="${UI_WINDOW_ADDR}"
  GEOM="${UI_WINDOW_GEOM}"
  return 0
}

launch_app() {
  ui_window_setup_isolated_config "${TMP_ROOT}/nenoserpent_input_matrix_cfg"
  CFG_TMP="${UI_WINDOW_CFG_TMP}"

  ui_window_kill_existing "${APP_BIN}"

  "${APP_BIN}" >"${TMP_ROOT}/nenoserpent_input_matrix_runtime.log" 2>&1 &
  APP_PID=$!

  if ! wait_window_ready; then
    return 1
  fi

  ui_window_focus "${WINDOW_ADDR}"
  sleep "${BOOT_SETTLE_SECONDS}"
  return 0
}

send_key() {
  local key="$1"
  if ! hyprctl dispatch sendshortcut ",${key},address:${WINDOW_ADDR}" >/dev/null 2>&1; then
    hyprctl dispatch sendshortcut ",${key}," >/dev/null 2>&1 || true
  fi
  sleep "${STEP_DELAY}"
}

send_konami() {
  local seq=(Up Up Down Down Left Right Left Right X Z)
  local k
  for k in "${seq[@]}"; do
    send_key "${k}"
  done
}

expect_alive() {
  local case_name="$1"
  if ! app_is_alive; then
    die_case "${case_name}" "app exited unexpectedly"
  fi
}

expect_exit() {
  local case_name="$1"
  local _
  for _ in $(seq 1 80); do
    if ! app_is_alive; then
      wait "${APP_PID}" >/dev/null 2>&1 || true
      APP_PID=""
      return 0
    fi
    sleep 0.1
  done
  die_case "${case_name}" "app did not exit in time"
}

run_case() {
  local case_name="$1"
  shift
  echo "[case] ${case_name}"
  if ! launch_app; then
    die_case "${case_name}" "failed to launch app window"
  fi
  "$@"
  sleep "${POST_CASE_WAIT}"
  cleanup_case
}
