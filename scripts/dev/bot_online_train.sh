#!/usr/bin/env bash
set -euo pipefail

# Purpose: thin shell entrypoint for ml-online trainer loop.
# Heavy loop/gate logic lives in Python for readability/testability.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
WORKSPACE="${BOT_ONLINE_WORKSPACE:-${TMP_ROOT}/nenoserpent_bot_online}"
PROFILE="${BOT_ONLINE_PROFILE:-dev}"
DATASET_SUITE="${BOT_ONLINE_DATASET_SUITE:-${ROOT_DIR}/scripts/ci/bot_leaderboard_rule_suite.tsv}"
INTERVAL_SEC="${BOT_ONLINE_INTERVAL_SEC:-20}"
MAX_SAMPLES_PER_CASE="${BOT_ONLINE_MAX_SAMPLES_PER_CASE:-6}"
EPOCHS="${BOT_ONLINE_EPOCHS:-8}"
BATCH_SIZE="${BOT_ONLINE_BATCH_SIZE:-192}"
LR="${BOT_ONLINE_LR:-0.001}"
SEED_BASE="${BOT_ONLINE_SEED_BASE:-20260306}"
GATE_GAMES="${BOT_ONLINE_GATE_GAMES:-24}"
GATE_MAX_TICKS="${BOT_ONLINE_GATE_MAX_TICKS:-1600}"
GATE_LEVEL="${BOT_ONLINE_GATE_LEVEL:-0}"
GATE_MODE="${BOT_ONLINE_GATE_MODE:-balanced}"
GATE_NO_REGRESSION_EPS="${BOT_ONLINE_GATE_NO_REGRESSION_EPS:-0.0}"
CACHE_MAX_MB="${BOT_ONLINE_CACHE_MAX_MB:-1024}"
CACHE_TARGET_MB="${BOT_ONLINE_CACHE_TARGET_MB:-768}"
MAX_ROUNDS="${BOT_ONLINE_MAX_ROUNDS:-0}"
SUMMARY_PATH="${BOT_ONLINE_SUMMARY_PATH:-}"
PUBLISH_HISTORY_PATH="${BOT_ONLINE_PUBLISH_HISTORY_PATH:-}"
MIN_DATASET_SAMPLES="${BOT_ONLINE_MIN_DATASET_SAMPLES:-40}"
MIN_PUBLISH_ROUND="${BOT_ONLINE_MIN_PUBLISH_ROUND:-1}"
PUBLISH_COOLDOWN_ROUNDS="${BOT_ONLINE_PUBLISH_COOLDOWN_ROUNDS:-1}"
MAX_BLOCKED_STREAK="${BOT_ONLINE_MAX_BLOCKED_STREAK:-8}"
BUILD_PRESET="${BUILD_PRESET:-dev}"

usage() {
  cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-online-train [options]

Options:
  --workspace <dir>             Output workspace (default: cache/dev/nenoserpent_bot_online)
  --profile <debug|dev|release> Dataset generation profile (default: dev)
  --suite <path>                Dataset suite TSV (default: scripts/ci/bot_leaderboard_rule_suite.tsv)
  --interval-sec <N>            Delay between rounds (default: 20)
  --max-samples-per-case <N>    Dataset cap per leaderboard case (default: 6)
  --epochs <N>                  Train epochs per round (default: 8)
  --batch-size <N>              Train batch size per round (default: 192)
  --lr <float>                  Learning rate per round (default: 0.001)
  --gate-games <N>              Benchmark games for publish gate (default: 24)
  --gate-max-ticks <N>          Benchmark max ticks for publish gate (default: 1600)
  --gate-level <N>              Benchmark level for publish gate (default: 0)
  --gate-mode <name>            Benchmark mode for publish gate (default: balanced)
  --gate-eps <float>            Allowed regression epsilon (default: 0.0)
  --cache-max-mb <N>            Workspace high-water mark in MB (default: 1024)
  --cache-target-mb <N>         Prune target size in MB when high-water exceeded (default: 768)
  --max-rounds <N>              Run finite rounds then exit (default: 0=infinite)
  --summary <path>              Optional summary output file (key=value lines)
  --publish-history <path>      Optional publish audit TSV output (default: <workspace>/publish_history.tsv)
  --min-dataset-samples <N>     Require at least N dataset rows per round (default: 40)
  --min-publish-round <N>       Do not publish before this round index (default: 1)
  --publish-cooldown-rounds <N> Min rounds between publishes (default: 1)
  --max-blocked-streak <N>      Stop loop when consecutive blocked rounds reach N (0=disable, default: 8)
EOF
}

while (($# > 0)); do
  case "$1" in
    --workspace) WORKSPACE="$2"; shift 2 ;;
    --profile) PROFILE="$2"; shift 2 ;;
    --suite) DATASET_SUITE="$2"; shift 2 ;;
    --interval-sec) INTERVAL_SEC="$2"; shift 2 ;;
    --max-samples-per-case) MAX_SAMPLES_PER_CASE="$2"; shift 2 ;;
    --epochs) EPOCHS="$2"; shift 2 ;;
    --batch-size) BATCH_SIZE="$2"; shift 2 ;;
    --lr) LR="$2"; shift 2 ;;
    --gate-games) GATE_GAMES="$2"; shift 2 ;;
    --gate-max-ticks) GATE_MAX_TICKS="$2"; shift 2 ;;
    --gate-level) GATE_LEVEL="$2"; shift 2 ;;
    --gate-mode) GATE_MODE="$2"; shift 2 ;;
    --gate-eps) GATE_NO_REGRESSION_EPS="$2"; shift 2 ;;
    --cache-max-mb) CACHE_MAX_MB="$2"; shift 2 ;;
    --cache-target-mb) CACHE_TARGET_MB="$2"; shift 2 ;;
    --max-rounds) MAX_ROUNDS="$2"; shift 2 ;;
    --summary) SUMMARY_PATH="$2"; shift 2 ;;
    --publish-history) PUBLISH_HISTORY_PATH="$2"; shift 2 ;;
    --min-dataset-samples) MIN_DATASET_SAMPLES="$2"; shift 2 ;;
    --min-publish-round) MIN_PUBLISH_ROUND="$2"; shift 2 ;;
    --publish-cooldown-rounds) PUBLISH_COOLDOWN_ROUNDS="$2"; shift 2 ;;
    --max-blocked-streak) MAX_BLOCKED_STREAK="$2"; shift 2 ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

mkdir -p "${WORKSPACE}"
if [[ -z "${PUBLISH_HISTORY_PATH}" ]]; then
  PUBLISH_HISTORY_PATH="${WORKSPACE}/publish_history.tsv"
fi

if [[ "${NENOSERPENT_SKIP_BUILD:-0}" != "1" ]]; then
  echo "[bot-online-train] prebuild preset=${BUILD_PRESET} target=bot-benchmark"
  cmake --preset "${BUILD_PRESET}"
  cmake --build --preset "${BUILD_PRESET}" --target bot-benchmark
  export NENOSERPENT_SKIP_BUILD=1
fi

exec uv run python "${ROOT_DIR}/scripts/dev/bot_online_train_loop.py" \
  --root-dir "${ROOT_DIR}" \
  --workspace "${WORKSPACE}" \
  --profile "${PROFILE}" \
  --suite "${DATASET_SUITE}" \
  --interval-sec "${INTERVAL_SEC}" \
  --max-samples-per-case "${MAX_SAMPLES_PER_CASE}" \
  --epochs "${EPOCHS}" \
  --batch-size "${BATCH_SIZE}" \
  --lr "${LR}" \
  --seed-base "${SEED_BASE}" \
  --gate-games "${GATE_GAMES}" \
  --gate-max-ticks "${GATE_MAX_TICKS}" \
  --gate-level "${GATE_LEVEL}" \
  --gate-mode "${GATE_MODE}" \
  --gate-eps "${GATE_NO_REGRESSION_EPS}" \
  --cache-max-mb "${CACHE_MAX_MB}" \
  --cache-target-mb "${CACHE_TARGET_MB}" \
  --max-rounds "${MAX_ROUNDS}" \
  --summary "${SUMMARY_PATH}" \
  --publish-history "${PUBLISH_HISTORY_PATH}" \
  --min-dataset-samples "${MIN_DATASET_SAMPLES}" \
  --min-publish-round "${MIN_PUBLISH_ROUND}" \
  --publish-cooldown-rounds "${PUBLISH_COOLDOWN_ROUNDS}" \
  --max-blocked-streak "${MAX_BLOCKED_STREAK}"
