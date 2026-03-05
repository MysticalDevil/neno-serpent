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
  ./scripts/deploy.sh <command> [args...]
  ./scripts/deploy.sh help [command]
  ./scripts/deploy.sh --help

Commands:
  android     Build/deploy Android package flow.
  wasm        Build/deploy wasm artifacts.
  wasm-serve  Serve wasm output locally.

Examples:
  ./scripts/deploy.sh android
  ./scripts/deploy.sh help wasm-serve
EOF
}

subcommand_help() {
  case "${1:-}" in
    android)
      cat <<'EOF'
Usage: ./scripts/deploy.sh android
Purpose: run Android deploy pipeline script.
EOF
      ;;
    wasm)
      cat <<'EOF'
Usage: ./scripts/deploy.sh wasm
Purpose: run wasm deploy/build script.
EOF
      ;;
    wasm-serve)
      cat <<'EOF'
Usage: ./scripts/deploy.sh wasm-serve [args...]
Purpose: launch local wasm static server helper.
EOF
      ;;
    *)
      echo "Unknown command: ${1:-<empty>}" >&2
      usage >&2
      return 1
      ;;
  esac
}

subcommand="${1:-}"
if [[ -z "${subcommand}" ]]; then
  usage
  exit 0
fi

case "${subcommand}" in
  -h|--help)
    usage
    exit 0
    ;;
  help)
    if [[ $# -ge 2 ]]; then
      subcommand_help "${2}"
    else
      usage
    fi
    exit $?
    ;;
esac

shift

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  subcommand_help "${subcommand}"
  exit $?
fi

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
