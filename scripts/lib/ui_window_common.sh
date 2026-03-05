#!/usr/bin/env bash

# shellcheck disable=SC1091
# shellcheck source=lib/script_common.sh
source "${ROOT_DIR}/scripts/lib/script_common.sh"

ui_window_app_is_alive() {
  local pid="$1"
  local stat=""

  if [[ -z "${pid}" ]]; then
    return 1
  fi
  if ! kill -0 "${pid}" 2>/dev/null; then
    return 1
  fi

  stat="$(ps -p "${pid}" -o stat= 2>/dev/null | tr -d '[:space:]')"
  [[ -n "${stat}" && "${stat#Z}" == "${stat}" ]]
}

ui_window_stop_app() {
  local pid="$1"

  if ui_window_app_is_alive "${pid}"; then
    kill "${pid}" >/dev/null 2>&1 || true
    wait "${pid}" >/dev/null 2>&1 || true
  fi
}

ui_window_setup_isolated_config() {
  local template_prefix="${1:-${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}/nenoserpent_ui_cfg}"

  # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
  UI_WINDOW_CFG_TMP="$(mktemp -d "${template_prefix}.XXXXXX")"
  export XDG_CONFIG_HOME="${UI_WINDOW_CFG_TMP}"
}

ui_window_cleanup_isolated_config() {
  local cfg_tmp="$1"

  if [[ -n "${cfg_tmp}" && -d "${cfg_tmp}" ]]; then
    rm -rf "${cfg_tmp}"
  fi
}

ui_window_kill_existing() {
  local app_bin="$1"

  pkill -f "${app_bin}" >/dev/null 2>&1 || true
  sleep 0.2
}

ui_window_wait_for_window() {
  local pid="$1"
  local wait_seconds="$2"
  local window_class="${3:-}"
  local window_title="${4:-}"
  local deadline=$((SECONDS + wait_seconds))
  local clients_json=""
  local window_info=""

  # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
  UI_WINDOW_ADDR=""
  # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
  UI_WINDOW_GEOM=""

  while (( SECONDS < deadline )); do
    clients_json="$(hyprctl clients -j 2>&1 || true)"
    if ! jq -e . >/dev/null 2>&1 <<<"${clients_json}"; then
      sleep 0.2
      continue
    fi

    window_info="$(jq -r --arg cls "${window_class}" --arg ttl "${window_title}" --argjson pid "${pid}" '
      .[] | select(
        (.pid == $pid) or
        ($cls != "" and .class == $cls) or
        ($ttl != "" and ((.title // "") | contains($ttl)))
      ) |
      "\(.address)\t\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"
    ' <<<"${clients_json}" | head -n 1)"

    if [[ -n "${window_info}" ]]; then
      # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
      UI_WINDOW_ADDR="${window_info%%$'\t'*}"
      # shellcheck disable=SC2034 # Consumed by callers after this shared helper returns.
      UI_WINDOW_GEOM="${window_info#*$'\t'}"
      return 0
    fi

    sleep 0.2
  done

  return 1
}

ui_window_focus() {
  local window_addr="$1"

  if [[ -n "${window_addr}" ]]; then
    hyprctl dispatch focuswindow "address:${window_addr}" >/dev/null || true
  fi
}
