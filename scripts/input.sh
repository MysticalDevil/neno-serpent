#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT_DIR}/.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${REPO_ROOT}/cache}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${CACHE_ROOT}/input}"
mkdir -p "${TMP_ROOT}"
export NENOSERPENT_CACHE_DIR="${CACHE_ROOT}"
export NENOSERPENT_TMP_DIR="${TMP_ROOT}"
export TMPDIR="${TMP_ROOT}"
export PYTHONDONTWRITEBYTECODE=1

usage() {
  cat <<'EOF'
Usage:
  ./scripts/input.sh <command> [args...]
  ./scripts/input.sh help [command]
  ./scripts/input.sh --help

Commands:
  inject                  Inject runtime input tokens.
  semantics-smoke         Run fast semantics smoke checks.
  semantics-matrix        Run semantics matrix checks.
  semantics-cases         Run semantics case bundle.
  probe-konami-real       Probe real-device konami sequence.
  probe-konami-stability  Probe konami stability repeatedly.

Examples:
  ./scripts/input.sh inject START SELECT
  ./scripts/input.sh help semantics-matrix
EOF
}

subcommand_help() {
  case "${1:-}" in
    inject)
      cat <<'EOF'
Usage: ./scripts/input.sh inject <token...>
Purpose: append or send input tokens for runtime control/debug.
EOF
      ;;
    semantics-smoke)
      cat <<'EOF'
Usage: ./scripts/input.sh semantics-smoke
Purpose: run minimal input semantics smoke suite.
EOF
      ;;
    semantics-matrix)
      cat <<'EOF'
Usage: ./scripts/input.sh semantics-matrix
Purpose: run full input semantics matrix on supported environments.
EOF
      ;;
    semantics-cases)
      cat <<'EOF'
Usage: ./scripts/input.sh semantics-cases
Purpose: run canonical semantics case set.
EOF
      ;;
    probe-konami-real)
      cat <<'EOF'
Usage: ./scripts/input.sh probe-konami-real
Purpose: probe konami input on real setup path.
EOF
      ;;
    probe-konami-stability)
      cat <<'EOF'
Usage: ./scripts/input.sh probe-konami-stability
Purpose: stress-test konami sequence stability.
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
  inject)
    exec "${ROOT_DIR}/input/inject.sh" "$@"
    ;;
  semantics-smoke)
    exec "${ROOT_DIR}/input/semantics/smoke.sh" "$@"
    ;;
  semantics-matrix)
    exec "${ROOT_DIR}/input/semantics/matrix_wayland.sh" "$@"
    ;;
  semantics-cases)
    exec "${ROOT_DIR}/input/semantics/cases_wayland.sh" "$@"
    ;;
  probe-konami-real)
    exec "${ROOT_DIR}/input/probes/konami_hypr_real.sh" "$@"
    ;;
  probe-konami-stability)
    exec "${ROOT_DIR}/input/probes/konami_stability.sh" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
