#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<'EOF'
Usage:
  ./scripts/dev.sh clang-tidy <build-dir> [files...]
  ./scripts/dev.sh android-icons
  ./scripts/dev.sh bot-benchmark [--games N --max-ticks M ...]
EOF
}

subcommand="${1:-}"
if [[ -z "${subcommand}" ]]; then
  usage
  exit 1
fi
shift

case "${subcommand}" in
  clang-tidy)
    exec "${ROOT_DIR}/dev/clang_tidy.sh" "$@"
    ;;
  android-icons)
    exec "${ROOT_DIR}/dev/android_icons.sh" "$@"
    ;;
  bot-benchmark)
    exec "${ROOT_DIR}/dev/bot_benchmark.sh" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
