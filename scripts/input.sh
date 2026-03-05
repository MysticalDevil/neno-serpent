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

usage() {
  cat <<'EOF'
Usage:
  ./scripts/input.sh inject <token...>
  ./scripts/input.sh semantics-smoke
  ./scripts/input.sh semantics-matrix
  ./scripts/input.sh semantics-cases
  ./scripts/input.sh probe-konami-real
  ./scripts/input.sh probe-konami-stability
EOF
}

subcommand="${1:-}"
if [[ -z "${subcommand}" ]]; then
  usage
  exit 1
fi
shift

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
