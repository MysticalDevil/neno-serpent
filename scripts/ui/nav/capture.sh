#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/build_paths.sh"
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/ui_nav_runtime.sh"
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/ui_nav_targets.sh"
BUILD_DIR="$(resolve_build_dir dev)"
APP_BIN="${APP_BIN:-${BUILD_DIR}/NenoSerpent}"
WINDOW_CLASS="${WINDOW_CLASS:-devil.org.NenoSerpent}"
WINDOW_TITLE="${WINDOW_TITLE:-Snake GB Edition}"
WAIT_SECONDS="${WAIT_SECONDS:-14}"
BOOT_SETTLE_SECONDS="${BOOT_SETTLE_SECONDS:-4.2}"
NAV_STEP_DELAY="${NAV_STEP_DELAY:-0.25}"
NAV_RETRIES="${NAV_RETRIES:-2}"
POST_NAV_WAIT="${POST_NAV_WAIT:-1.6}"
PALETTE_STEPS="${PALETTE_STEPS:-0}"
PALETTE_TOKEN="${PALETTE_TOKEN:-PALETTE}"
PRE_TOKENS="${PRE_TOKENS:-}"
POST_TOKENS="${POST_TOKENS:-}"
NO_SCREENSHOT="${NO_SCREENSHOT:-0}"
HOLD_OPEN="${HOLD_OPEN:-0}"
ISOLATED_CONFIG="${ISOLATED_CONFIG:-1}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}"
mkdir -p "${TMP_ROOT}"
INPUT_FILE="${INPUT_FILE:-${TMP_ROOT}/nenoserpent_ui_input.txt}"
CAPTURE_LOCK_FILE="${CAPTURE_LOCK_FILE:-${TMP_ROOT}/nenoserpent_ui_nav_capture.lock}"
TARGET="${1:-menu}"
OUT_PNG="${2:-${TMP_ROOT}/nenoserpent_ui_nav_${TARGET}.png}"

ui_nav_need_cmd cmake
ui_nav_need_cmd flock
ui_nav_need_cmd hyprctl
ui_nav_need_cmd jq
ui_nav_need_cmd grim
ui_nav_require_wayland
ui_nav_acquire_lock "${CAPTURE_LOCK_FILE}" "Another UI capture is running"
ui_nav_build_target_plan "${TARGET}" "${NAV_RETRIES}"
APP_ARGS="${UI_NAV_TARGET_APP_ARGS:-${APP_ARGS:-}}"
ui_nav_build_app "${BUILD_DIR}" "${APP_BIN}"
ui_nav_setup_isolated_config "${ISOLATED_CONFIG}"

cleanup() {
  ui_nav_cleanup_runtime
}
trap cleanup EXIT

if ! ui_nav_launch_and_locate "${APP_BIN}" "${INPUT_FILE}" "${WAIT_SECONDS}" \
  "${TMP_ROOT}/nenoserpent_ui_nav_runtime.log"; then
  echo "[error] Could not find game window."
  exit 2
fi

sleep "${BOOT_SETTLE_SECONDS}"

send_token() {
  ui_nav_send_token "${INPUT_FILE}" "${NAV_STEP_DELAY}" "$1"
}

send_konami() {
  ui_nav_send_konami "${INPUT_FILE}" "${NAV_STEP_DELAY}"
}

send_token_list() {
  ui_nav_send_token_list "${INPUT_FILE}" "${NAV_STEP_DELAY}" "$1"
}

send_token_list "${PRE_TOKENS}"

ui_nav_apply_palette_steps "${INPUT_FILE}" "${NAV_STEP_DELAY}" "${PALETTE_TOKEN}" "${PALETTE_STEPS}"

ui_nav_execute_target_plan "${INPUT_FILE}" "${NAV_STEP_DELAY}" "${UI_NAV_TARGET_STEPS[@]}"

send_token_list "${POST_TOKENS}"

sleep "${UI_NAV_TARGET_POST_WAIT_OVERRIDE:-${POST_NAV_WAIT}}"
if ! kill -0 "${UI_NAV_APP_PID}" >/dev/null 2>&1; then
  echo "[error] App exited before screenshot. Recent log:"
  tail -n 80 "${TMP_ROOT}/nenoserpent_ui_nav_runtime.log" || true
  exit 5
fi

if [[ "${HOLD_OPEN}" == "1" ]]; then
  cat <<EOF
[ok] Target ready: ${TARGET}
[ok] PID: ${UI_NAV_APP_PID}
[ok] Window: ${UI_NAV_WINDOW_ADDR}
[ok] Geometry: ${UI_NAV_GEOM}
[ok] Input file: ${INPUT_FILE}
[ok] Runtime log: ${TMP_ROOT}/nenoserpent_ui_nav_runtime.log
[hint] Send more tokens with:
       printf 'START\n' >> ${INPUT_FILE}
[hint] Close the app normally, or press Ctrl+C in this terminal.
EOF
  wait "${UI_NAV_APP_PID}"
  exit $?
fi

if [[ "${NO_SCREENSHOT}" == "1" ]]; then
  echo "[ok] Target: ${TARGET}"
  echo "[ok] Palette steps: ${PALETTE_STEPS}"
  exit 0
fi

mkdir -p "$(dirname "${OUT_PNG}")"
grim -g "${UI_NAV_GEOM}" "${OUT_PNG}"
echo "[ok] Target: ${TARGET}"
echo "[ok] Palette steps: ${PALETTE_STEPS}"
echo "[ok] Screenshot saved: ${OUT_PNG}"
echo "[ok] Geometry: ${UI_NAV_GEOM}"
