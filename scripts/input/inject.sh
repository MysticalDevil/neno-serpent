#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/build_paths.sh"
DEFAULT_BUILD_DIR="$(resolve_build_dir dev)"

TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/input}}"
mkdir -p "${TMP_ROOT}"
ENDPOINT="${NENOSERPENT_INPUT_PIPE:-${NENOSERPENT_INPUT_FILE:-${TMP_ROOT}/nenoserpent-input.pipe}}"
if [[ "${1:-}" == "-p" ]]; then
  if [[ $# -lt 2 ]]; then
    echo "[error] Missing value for -p"
    exit 1
  fi
  ENDPOINT="$2"
  shift 2
fi

if [[ ! -p "${ENDPOINT}" && ! -f "${ENDPOINT}" ]]; then
  echo "[error] Input endpoint not found: ${ENDPOINT}"
  echo "Start app with:"
  echo "  NENOSERPENT_INPUT_PIPE=${ENDPOINT} ${DEFAULT_BUILD_DIR}/NenoSerpent"
  echo "or"
  echo "  NENOSERPENT_INPUT_FILE=${ENDPOINT} ${DEFAULT_BUILD_DIR}/NenoSerpent"
  exit 1
fi

if [[ "$#" -eq 0 ]]; then
  echo "[info] Interactive mode. Type one token per line (UP/A/START/B/SELECT/F6...), Ctrl-D to exit."
  cat >> "${ENDPOINT}"
  exit 0
fi

for token in "$@"; do
  if [[ -p "${ENDPOINT}" ]]; then
    printf "%s\n" "${token}" > "${ENDPOINT}"
  else
    printf "%s\n" "${token}" >> "${ENDPOINT}"
  fi
done
