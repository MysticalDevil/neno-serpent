#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT_DIR}/.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${REPO_ROOT}/cache}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${CACHE_ROOT}/dev}"
mkdir -p "${TMP_ROOT}"
export NENOSERPENT_CACHE_DIR="${CACHE_ROOT}"
export NENOSERPENT_TMP_DIR="${TMP_ROOT}"
export TMPDIR="${TMP_ROOT}"

usage() {
  cat <<'EOF'
Usage:
  ./scripts/dev.sh <command> [args...]
  ./scripts/dev.sh help [command]
  ./scripts/dev.sh --help

Commands:
  clang-tidy       Run clang-tidy wrapper on specific files/build-dir.
  android-icons    Generate Android launcher icon assets.
  bot-benchmark    Run bot benchmark suite.
  bot-dataset      Build training dataset from simulations.
  bot-tune         Tune rule-bot parameters.
  bot-train        Train ML model from dataset.
  bot-eval         Evaluate model offline.
  bot-ml-gate      Run ML quality gate pipeline.
  bot-ml-smoke     Quick smoke gate for ML flow.
  bot-ml-online-gate Validate ml-online training/publish loop.
  bot-online-train Continuous online training loop for ml-online hot reload.
  bot-online-run   One-command launcher for ml-online trainer + headful gameplay.
  bot-online-run-smoke Smoke test for bot-online-run process lifecycle.
  bot-run          Run bot in headful/headless mode.
  bot-e2e          Run bot E2E regression.
  bot-leaderboard  Run bot leaderboard regression.
  cache-prune      Prune repository cache by age and size watermarks.

Examples:
  ./scripts/dev.sh clang-tidy build/debug src/sound_manager.cpp
  ./scripts/dev.sh help bot-run
  ./scripts/dev.sh bot-run --backend rule --headful --ui-mode full
EOF
}

subcommand_help() {
  case "${1:-}" in
    clang-tidy)
      cat <<'EOF'
Usage: ./scripts/dev.sh clang-tidy <build-dir> [files...]
Purpose: run clang-tidy through scripts/dev/clang_tidy.sh.
EOF
      ;;
    android-icons)
      cat <<'EOF'
Usage: ./scripts/dev.sh android-icons
Purpose: regenerate Android icon assets (mipmap + Play Store assets).
EOF
      ;;
    bot-benchmark)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-benchmark [--games N --max-ticks M ...]
Purpose: run benchmark scenarios for bot performance.
EOF
      ;;
    bot-dataset)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-dataset [--output cache/<name>.csv]
Purpose: generate bot training dataset from scripted sessions.
EOF
      ;;
    bot-tune)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-tune [--mode balanced --iterations 60 --output cache/<name>.json]
Purpose: tune rule strategy parameters.
EOF
      ;;
    bot-train)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-train [--dataset cache/<name>.csv --model cache/<name>.pt]
Purpose: train bot model via Python training entrypoint.
EOF
      ;;
    bot-eval)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-eval [--dataset cache/<name>.csv --model cache/<name>.pt]
Purpose: evaluate trained model on dataset.
EOF
      ;;
    bot-ml-gate)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-ml-gate [--workspace cache/<dir>]
Purpose: execute ML gate pipeline (dataset + training + compare).
EOF
      ;;
    bot-ml-smoke)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-ml-smoke [build-dir]
Purpose: fast smoke verification of ML gate path.
EOF
      ;;
    bot-ml-online-gate)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-ml-online-gate [--workspace cache/<dir> --rounds N]
       [--min-dataset-samples N --min-publish-round N --publish-cooldown-rounds N --max-blocked-streak N]
Purpose: validate ml-online training/publish loop with reproducible pass conditions.
EOF
      ;;
    bot-online-train)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-online-train [--workspace cache/<dir> --interval-sec N]
       [--gate-games N --gate-max-ticks N --gate-level N --gate-mode name --gate-eps F]
       [--min-dataset-samples N --min-publish-round N --publish-cooldown-rounds N --max-blocked-streak N]
Purpose: periodically regenerate dataset and retrain runtime JSON for ml-online mode.
         New model is published only when gate metrics do not regress.
EOF
      ;;
    bot-online-run)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-online-run [--workspace cache/<dir> --build-preset dev --ui-mode screen]
Purpose: start ml-online trainer loop in background and run game in headful mode.
         Supports the same trainer stability knobs as bot-online-train.
EOF
      ;;
    bot-online-run-smoke)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-online-run-smoke
Purpose: validate bot-online-run starts trainer and cleans it up after game exits.
EOF
      ;;
    bot-run)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-run [--backend off|rule|ml|ml-online|search] [--headful|--headless] \
[--ui-mode full|screen]
Purpose: run gameplay with selected bot backend and UI mode.
EOF
      ;;
    bot-e2e)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-e2e [build-dir] [baseline.tsv]
Purpose: run E2E bot regression versus baseline.
EOF
      ;;
    bot-leaderboard)
      cat <<'EOF'
Usage: ./scripts/dev.sh bot-leaderboard [build-dir] [suite.tsv]
Purpose: run leaderboard suite and compare stability/perf.
EOF
      ;;
    cache-prune)
      cat <<'EOF'
Usage: ./scripts/dev.sh cache-prune [--cache-dir path --max-mb N --target-mb N --max-age-days N]
Purpose: prune cache directory to prevent uncontrolled growth.
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
  bot-ml-online-gate)
    exec "${ROOT_DIR}/dev/bot_ml_online_gate.sh" "$@"
    ;;
  bot-online-train)
    exec "${ROOT_DIR}/dev/bot_online_train.sh" "$@"
    ;;
  bot-online-run)
    exec "${ROOT_DIR}/dev/bot_online_run.sh" "$@"
    ;;
  bot-online-run-smoke)
    exec "${ROOT_DIR}/dev/bot_online_run_smoke.sh" "$@"
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
  cache-prune)
    exec "${ROOT_DIR}/dev/cache_prune.sh" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
