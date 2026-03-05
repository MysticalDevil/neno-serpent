#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/build_paths.sh"
BUILD_DIR="$(resolve_build_dir dev)"
APP_BIN="${APP_BIN:-${BUILD_DIR}/NenoSerpent}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/input}}"
mkdir -p "${TMP_ROOT}"
INPUT_FILE="${INPUT_FILE:-${TMP_ROOT}/nenoserpent-input.queue}"
LOG_FILE="${LOG_FILE:-${TMP_ROOT}/nenoserpent_input_semantics_smoke.log}"
RUN_QPA="${RUN_QPA:-offscreen}"

echo "[info] Building ${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --parallel >/dev/null

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[error] App binary not found: ${APP_BIN}"
  exit 1
fi

rm -f "${INPUT_FILE}"

echo "[info] Launching app with input file: ${INPUT_FILE} (QPA=${RUN_QPA})"
QT_QPA_PLATFORM="${RUN_QPA}" \
NENOSERPENT_KEEP_STDERR=1 \
NENOSERPENT_INPUT_FILE="${INPUT_FILE}" \
"${APP_BIN}" >"${LOG_FILE}" 2>&1 &
APP_PID=$!

# shellcheck disable=SC2329  # Invoked indirectly via trap on script exit.
cleanup() {
  kill "${APP_PID}" >/dev/null 2>&1 || true
  rm -f "${INPUT_FILE}"
}
trap cleanup EXIT

sleep 1.5
if [[ ! -f "${INPUT_FILE}" ]]; then
  echo "[error] Input file was not created: ${INPUT_FILE}"
  tail -n 80 "${LOG_FILE}" || true
  exit 2
fi

send_token() {
  printf '%s\n' "$1" >> "${INPUT_FILE}"
  sleep 0.15
}

process_stat() {
  ps -p "${APP_PID}" -o stat= 2>/dev/null | tr -d '[:space:]'
}

process_exited() {
  local stat
  stat="$(process_stat)"
  [[ -z "${stat}" || "${stat}" == Z* ]]
}

echo "[info] Injecting non-exit action sequence"
for token in UP RIGHT DOWN LEFT A B START SELECT B; do
  send_token "${token}"
done

sleep 0.8
if process_exited; then
  echo "[error] App exited unexpectedly after non-exit actions"
  tail -n 80 "${LOG_FILE}" || true
  exit 3
fi

echo "[ok] App remained alive after injected semantics sequence"
exit 0
