#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT_DIR}/.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${REPO_ROOT}/cache}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${CACHE_ROOT}/ui}"
mkdir -p "${TMP_ROOT}"
export NENOSERPENT_CACHE_DIR="${CACHE_ROOT}"
export NENOSERPENT_TMP_DIR="${TMP_ROOT}"
export TMPDIR="${TMP_ROOT}"
export PYTHONDONTWRITEBYTECODE=1

usage() {
  cat <<'EOF'
Usage:
  ./scripts/ui.sh <command> [args...]
  ./scripts/ui.sh help [command]
  ./scripts/ui.sh --help

Commands:
  nav-capture         Capture one navigated UI target into a screenshot.
  nav-debug           Open a target route for manual UI debugging.
  self-check          Run serial UI self-check suite.
  palette-focus       Capture focused palette states.
  palette-matrix      Capture full palette matrix set.
  palette-single      Capture one palette/target combination.
  palette-debug-matrix  Capture static debug palette matrix.
  palette-review      Build palette review report assets.

Examples:
  ./scripts/ui.sh nav-capture menu cache/ui/menu.png
  ./scripts/ui.sh help palette-matrix
  ./scripts/ui.sh palette-single 0 cache/ui menu choice
EOF
}

subcommand_help() {
  case "${1:-}" in
    nav-capture)
      cat <<'EOF'
Usage: ./scripts/ui.sh nav-capture <target> <out.png>
Purpose: run scripted UI navigation and save one screenshot.
EOF
      ;;
    nav-debug)
      cat <<'EOF'
Usage: ./scripts/ui.sh nav-debug <target>
Purpose: open target in manual debug mode without screenshot capture.
EOF
      ;;
    self-check)
      cat <<'EOF'
Usage: ./scripts/ui.sh self-check
Purpose: run serial UI smoke checks and assertions.
EOF
      ;;
    palette-focus)
      cat <<'EOF'
Usage: ./scripts/ui.sh palette-focus <out-dir>
Purpose: capture focused palette set for key targets.
EOF
      ;;
    palette-matrix)
      cat <<'EOF'
Usage: ./scripts/ui.sh palette-matrix <out-dir>
Purpose: generate full palette x target matrix captures.
EOF
      ;;
    palette-single)
      cat <<'EOF'
Usage: ./scripts/ui.sh palette-single <palette-step> <out-dir> [targets...]
Purpose: capture selected targets under one palette step.
EOF
      ;;
    palette-debug-matrix)
      cat <<'EOF'
Usage: ./scripts/ui.sh palette-debug-matrix <out-dir>
Purpose: capture static-debug scenes across palettes.
EOF
      ;;
    palette-review)
      cat <<'EOF'
Usage: ./scripts/ui.sh palette-review <out-dir>
Purpose: render review html/assets from existing palette captures.
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
  nav-capture)
    exec "${ROOT_DIR}/ui/nav/capture.sh" "$@"
    ;;
  nav-debug)
    exec "${ROOT_DIR}/ui/nav/debug.sh" "$@"
    ;;
  self-check)
    exec "${ROOT_DIR}/ui/self_check.sh" "$@"
    ;;
  palette-focus)
    exec "${ROOT_DIR}/ui/palette/focus.sh" "$@"
    ;;
  palette-matrix)
    exec "${ROOT_DIR}/ui/palette/matrix.sh" "$@"
    ;;
  palette-single)
    exec "${ROOT_DIR}/ui/palette/single.sh" "$@"
    ;;
  palette-debug-matrix)
    exec "${ROOT_DIR}/ui/palette/debug_matrix.sh" "$@"
    ;;
  palette-review)
    exec "${ROOT_DIR}/ui/palette/review.sh" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
