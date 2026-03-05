#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT_DIR}/.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${REPO_ROOT}/cache}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${CACHE_ROOT}/deploy}"
mkdir -p "${TMP_ROOT}"
export NENOSERPENT_CACHE_DIR="${CACHE_ROOT}"
export NENOSERPENT_TMP_DIR="${TMP_ROOT}"
export TMPDIR="${TMP_ROOT}"

usage() {
  cat <<'EOF'
Usage:
  ./scripts/deploy.sh android
  ./scripts/deploy.sh wasm
  ./scripts/deploy.sh wasm-serve [args...]
EOF
}

subcommand="${1:-}"
if [[ -z "${subcommand}" ]]; then
  usage
  exit 1
fi
shift

case "${subcommand}" in
  android)
    exec "${ROOT_DIR}/deploy/android.sh" "$@"
    ;;
  wasm)
    exec "${ROOT_DIR}/deploy/wasm.sh" "$@"
    ;;
  wasm-serve)
    exec python3 "${ROOT_DIR}/deploy/wasm_serve.py" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
