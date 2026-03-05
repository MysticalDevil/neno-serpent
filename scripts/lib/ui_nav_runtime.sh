#!/usr/bin/env bash

# shellcheck disable=SC1091
# shellcheck source=lib/script_common.sh
source "${ROOT_DIR}/scripts/lib/script_common.sh"
# shellcheck disable=SC1091
# shellcheck source=lib/ui_window_common.sh
source "${ROOT_DIR}/scripts/lib/ui_window_common.sh"

ui_nav_need_cmd() {
  script_require_cmds "$1"
}

ui_nav_require_wayland() {
  if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
    echo "[error] This script expects Wayland (current: ${XDG_SESSION_TYPE:-unknown})"
    exit 1
  fi
}

ui_nav_acquire_lock() {
  local lock_file="$1"
  local wait_prefix="$2"

  exec 9>"${lock_file}"
  if ! flock -n 9; then
    echo "[info] ${wait_prefix}; waiting for ${lock_file}"
    flock 9
  fi
}

ui_nav_build_app() {
  local build_dir="$1"
  local app_bin="$2"

  echo "[info] Building ${build_dir}"
  cmake --build "${build_dir}" --parallel >/dev/null

  if [[ ! -x "${app_bin}" ]]; then
    echo "[error] App binary not found: ${app_bin}"
    exit 1
  fi
}

ui_nav_setup_isolated_config() {
  local enabled="$1"
  local tmp_root="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}"
  mkdir -p "${tmp_root}"

  UI_NAV_CFG_TMP=""
  if [[ "${enabled}" == "1" ]]; then
    ui_window_setup_isolated_config "${tmp_root}/nenoserpent_ui_cfg"
    UI_NAV_CFG_TMP="${UI_WINDOW_CFG_TMP}"
  fi
}

ui_nav_cleanup_runtime() {
  if [[ -n "${UI_NAV_APP_PID:-}" ]]; then
    ui_window_stop_app "${UI_NAV_APP_PID}"
  fi
  ui_window_cleanup_isolated_config "${UI_NAV_CFG_TMP:-}"
}

ui_nav_find_window_for_pid() {
  local pid="$1"
  local wait_seconds="$2"

  if ! ui_window_wait_for_window "${pid}" "${wait_seconds}" "" ""; then
    return 1
  fi
  UI_NAV_WINDOW_ADDR="${UI_WINDOW_ADDR}"
  UI_NAV_GEOM="${UI_WINDOW_GEOM}"
  return 0
}

ui_nav_launch_and_locate() {
  local app_bin="$1"
  local input_file="$2"
  local wait_seconds="$3"
  local runtime_log="$4"
  local max_launch_attempts="${5:-1}"
  local attempt=1
  local -a app_args=()

  if [[ -n "${APP_ARGS:-}" ]]; then
    read -r -a app_args <<<"${APP_ARGS}"
  fi

  UI_NAV_APP_PID=""
  # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
  UI_NAV_WINDOW_ADDR=""
  # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
  UI_NAV_GEOM=""

  while (( attempt <= max_launch_attempts )); do
    ui_window_kill_existing "${app_bin}"

    if [[ "${max_launch_attempts}" -gt 1 ]]; then
      echo "[info] Launching ${app_bin} (attempt ${attempt}/${max_launch_attempts})"
    else
      echo "[info] Launching ${app_bin}"
    fi
    rm -f "${input_file}" >/dev/null 2>&1 || true
    NENOSERPENT_INPUT_FILE="${input_file}" "${app_bin}" "${app_args[@]}" >"${runtime_log}" 2>&1 &
    UI_NAV_APP_PID=$!

    if ui_nav_find_window_for_pid "${UI_NAV_APP_PID}" "${wait_seconds}"; then
      return 0
    fi

    ui_window_stop_app "${UI_NAV_APP_PID}"
    UI_NAV_APP_PID=""
    attempt=$((attempt + 1))
  done

  return 1
}

ui_nav_send_token() {
  local input_file="$1"
  local nav_step_delay="$2"
  local token="$3"

  printf '%s\n' "${token}" >>"${input_file}"
  sleep "${nav_step_delay}"
}

ui_nav_send_konami() {
  local input_file="$1"
  local nav_step_delay="$2"
  local seq=(U U D D L R L R B A)
  local token=""

  for token in "${seq[@]}"; do
    ui_nav_send_token "${input_file}" "${nav_step_delay}" "${token}"
  done
}

ui_nav_send_token_list() {
  local input_file="$1"
  local nav_step_delay="$2"
  local raw="$3"
  local token=""
  local -a tokens=()

  if [[ -z "${raw}" ]]; then
    return
  fi

  script_split_csv_trimmed "${raw}" tokens
  for token in "${tokens[@]}"; do
    token="${token#"${token%%[![:space:]]*}"}"
    token="${token%"${token##*[![:space:]]}"}"
    if [[ -n "${token}" ]]; then
      ui_nav_send_token "${input_file}" "${nav_step_delay}" "${token}"
    fi
  done
}

ui_nav_apply_palette_steps() {
  local input_file="$1"
  local nav_step_delay="$2"
  local palette_token="$3"
  local palette_steps="$4"
  local i=0

  if [[ ! "${palette_steps}" =~ ^[0-9]+$ ]] || (( palette_steps <= 0 )); then
    return
  fi

  while (( i < palette_steps )); do
    ui_nav_send_token "${input_file}" "${nav_step_delay}" "${palette_token}"
    ((i += 1))
  done
}

ui_nav_execute_target_plan() {
  local input_file="$1"
  local nav_step_delay="$2"
  shift 2
  local step=""

  for step in "$@"; do
    case "${step}" in
      TOKEN:*)
        ui_nav_send_token "${input_file}" "${nav_step_delay}" "${step#TOKEN:}"
        ;;
      SLEEP:*)
        sleep "${step#SLEEP:}"
        ;;
      KONAMI)
        ui_nav_send_konami "${input_file}" "${nav_step_delay}"
        ;;
      *)
        echo "[error] Unknown ui-nav step '${step}'"
        return 2
        ;;
    esac
  done
}
