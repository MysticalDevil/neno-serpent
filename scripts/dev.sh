#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<'EOF'
Usage:
  ./scripts/dev.sh clang-tidy <build-dir> [files...]
  ./scripts/dev.sh android-icons
  ./scripts/dev.sh bot-benchmark [--games N --max-ticks M ...]
  ./scripts/dev.sh bot-dataset [--output /tmp/bot_dataset.csv]
  ./scripts/dev.sh bot-tune [--mode balanced --iterations 60 --output /tmp/tuned.json]
  ./scripts/dev.sh bot-train [--dataset /tmp/bot_dataset.csv --model /tmp/bot_policy.pt]
  ./scripts/dev.sh bot-eval [--dataset /tmp/bot_dataset.csv --model /tmp/bot_policy.pt]
  ./scripts/dev.sh bot-ml-gate [--workspace /tmp/nenoserpent_bot_ml_gate]
  ./scripts/dev.sh bot-ml-smoke [build-dir]
  ./scripts/dev.sh bot-run [--backend off|rule|ml --headful|--headless --ui-mode full|screen|shell]
  ./scripts/dev.sh bot-e2e [build-dir] [baseline.tsv]
  ./scripts/dev.sh bot-leaderboard [build-dir] [suite.tsv]
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
  bot-dataset)
    exec "${ROOT_DIR}/dev/bot_dataset.sh" "$@"
    ;;
  bot-tune)
    exec "${ROOT_DIR}/dev/bot_tune.sh" "$@"
    ;;
  bot-train)
    exec "${ROOT_DIR}/dev/bot_train.sh" "$@"
    ;;
  bot-eval)
    exec "${ROOT_DIR}/dev/bot_eval.sh" "$@"
    ;;
  bot-ml-gate)
    exec "${ROOT_DIR}/dev/bot_ml_gate.sh" "$@"
    ;;
  bot-ml-smoke)
    exec "${ROOT_DIR}/ci/bot_ml_smoke_gate.sh" "$@"
    ;;
  bot-run)
    exec "${ROOT_DIR}/dev/bot_run.sh" "$@"
    ;;
  bot-e2e)
    exec "${ROOT_DIR}/ci/bot_e2e_regression.sh" "$@"
    ;;
  bot-leaderboard)
    exec "${ROOT_DIR}/ci/bot_leaderboard_regression.sh" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
