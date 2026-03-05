#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${CACHE_ROOT}/input}"
mkdir -p "${TMP_ROOT}"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/build_paths.sh"
# shellcheck source=lib/script_common.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/script_common.sh"
BUILD_DIR="$(resolve_build_dir dev)"
APP_BIN="${APP_BIN:-${BUILD_DIR}/NenoSerpent}"
WINDOW_CLASS="${WINDOW_CLASS:-devil.org.NenoSerpent}"
WINDOW_TITLE="${WINDOW_TITLE:-Snake GB Edition}"
WAIT_SECONDS="${WAIT_SECONDS:-14}"
BOOT_SETTLE_SECONDS="${BOOT_SETTLE_SECONDS:-4.2}"
STEP_DELAY="${STEP_DELAY:-0.22}"
POST_CASE_WAIT="${POST_CASE_WAIT:-0.45}"
FAIL_DIR="${FAIL_DIR:-${TMP_ROOT}/nenoserpent_input_matrix_fail}"
CASE_TIMEOUT="${CASE_TIMEOUT:-120}"

source "${ROOT_DIR}/scripts/lib/input_matrix_common.sh"
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/input/semantics/cases_wayland.sh"

script_require_cmds cmake hyprctl jq grim ps

if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  echo "[error] This script expects Wayland (current: ${XDG_SESSION_TYPE:-unknown})"
  exit 1
fi

trap cleanup_case EXIT

echo "[info] Building ${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --parallel >/dev/null

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[error] App binary not found: ${APP_BIN}"
  exit 1
fi

run_input_semantics_cases

echo "[ok] Input semantics matrix (Wayland) passed"
