#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/build_paths.sh"
BUILD_DIR="$(resolve_build_dir dev)"
APP_BIN="${APP_BIN:-${BUILD_DIR}/NenoSerpent}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}"
mkdir -p "${TMP_ROOT}"
OUT_PNG="${1:-${TMP_ROOT}/nenoserpent_ui_check.png}"
WINDOW_CLASS="${WINDOW_CLASS:-devil.org.NenoSerpent}"
WINDOW_TITLE="${WINDOW_TITLE:-Snake GB Edition}"
WAIT_SECONDS="${WAIT_SECONDS:-12}"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[error] Missing command: $1"
    exit 1
  fi
}

need_cmd cmake
need_cmd hyprctl
need_cmd jq
need_cmd grim

if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  echo "[error] This script expects a Wayland session (current: ${XDG_SESSION_TYPE:-unknown})"
  exit 1
fi

echo "[info] Building ${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --parallel >/dev/null

if [[ ! -x "${APP_BIN}" ]]; then
  echo "[error] App binary not found: ${APP_BIN}"
  exit 1
fi

echo "[info] Launching ${APP_BIN}"
"${APP_BIN}" >"${TMP_ROOT}/nenoserpent_ui_check_runtime.log" 2>&1 &
APP_PID=$!
cleanup() {
  kill "${APP_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

DEADLINE=$((SECONDS + WAIT_SECONDS))
GEOM=""
while (( SECONDS < DEADLINE )); do
  GEOM="$(hyprctl clients -j | jq -r --arg cls "${WINDOW_CLASS}" --arg ttl "${WINDOW_TITLE}" \
    '.[] | select(.class==$cls or (.title | contains($ttl))) | "\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"' \
    | head -n1)"
  if [[ -n "${GEOM}" && "${GEOM}" != "null" ]]; then
    break
  fi
  sleep 0.2
done

if [[ -z "${GEOM}" || "${GEOM}" == "null" ]]; then
  echo "[error] Could not find game window. Check WINDOW_CLASS/WINDOW_TITLE."
  exit 2
fi

mkdir -p "$(dirname "${OUT_PNG}")"
grim -g "${GEOM}" "${OUT_PNG}"
echo "[ok] Screenshot saved: ${OUT_PNG}"
echo "[ok] Geometry: ${GEOM}"
