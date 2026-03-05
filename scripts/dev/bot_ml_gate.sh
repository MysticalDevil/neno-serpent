#!/usr/bin/env bash
set -euo pipefail

# Purpose: run an end-to-end reproducible ML bot validation flow.
# Inputs:
#   --build-preset <name>   CMake preset (default: dev)
#   --profile <name>        Bot strategy profile for benchmark/train data (default: debug)
#   --workspace <path>      Output workspace (default: /tmp/nenoserpent_bot_ml_gate)
#   --epochs <count>        Training epochs (default: 24)
#   --batch-size <count>    Training batch size (default: 256)
#   --lr <value>            Learning rate (default: 0.001)
#   --seed <value>          Fixed seed for reproducible training (default: 20260304)
# Output:
#   - dataset/model/runtime-json/eval report under workspace
#   - leaderboard compare report with rule vs ml

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_PRESET="${BOT_ML_GATE_BUILD_PRESET:-dev}"
PROFILE="${BOT_ML_GATE_PROFILE:-debug}"
WORKSPACE="${BOT_ML_GATE_WORKSPACE:-/tmp/nenoserpent_bot_ml_gate}"
EPOCHS="${BOT_ML_GATE_EPOCHS:-24}"
BATCH_SIZE="${BOT_ML_GATE_BATCH_SIZE:-256}"
LR="${BOT_ML_GATE_LR:-0.001}"
SEED="${BOT_ML_GATE_SEED:-20260304}"

while (($# > 0)); do
  case "$1" in
    --build-preset)
      BUILD_PRESET="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --workspace)
      WORKSPACE="$2"
      shift 2
      ;;
    --epochs)
      EPOCHS="$2"
      shift 2
      ;;
    --batch-size)
      BATCH_SIZE="$2"
      shift 2
      ;;
    --lr)
      LR="$2"
      shift 2
      ;;
    --seed)
      SEED="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

if ! uv run python -c "import torch" >/dev/null 2>&1; then
  echo "[bot-ml-gate] missing dependency: torch" >&2
  echo "[bot-ml-gate] install with: uv add --dev torch" >&2
  exit 1
fi

mkdir -p "${WORKSPACE}"
DATASET_PATH="${WORKSPACE}/dataset.csv"
MODEL_PATH="${WORKSPACE}/policy.pt"
METADATA_PATH="${WORKSPACE}/policy.meta.json"
RUNTIME_JSON_PATH="${WORKSPACE}/policy.runtime.json"
EVAL_REPORT_PATH="${WORKSPACE}/eval.report.json"
LEADERBOARD_ROWS_PATH="${WORKSPACE}/leaderboard.rows.tsv"
LEADERBOARD_SUMMARY_PATH="${WORKSPACE}/leaderboard.summary.tsv"
LEADERBOARD_COMPARE_PATH="${WORKSPACE}/leaderboard.compare.tsv"

echo "[bot-ml-gate] phase=dataset"
BUILD_PRESET="${BUILD_PRESET}" \
BOT_DATASET_PROFILE="${PROFILE}" \
BOT_DATASET_OUTPUT="${DATASET_PATH}" \
"${ROOT_DIR}/scripts/dev/bot_dataset.sh"

echo "[bot-ml-gate] phase=train"
BOT_TRAIN_DATASET="${DATASET_PATH}" \
BOT_TRAIN_MODEL="${MODEL_PATH}" \
BOT_TRAIN_METADATA="${METADATA_PATH}" \
BOT_TRAIN_RUNTIME_JSON="${RUNTIME_JSON_PATH}" \
BOT_TRAIN_EPOCHS="${EPOCHS}" \
BOT_TRAIN_BATCH_SIZE="${BATCH_SIZE}" \
BOT_TRAIN_LR="${LR}" \
BOT_TRAIN_SEED="${SEED}" \
"${ROOT_DIR}/scripts/dev/bot_train.sh"

echo "[bot-ml-gate] phase=eval"
"${ROOT_DIR}/scripts/dev/bot_eval.sh" \
  --dataset "${DATASET_PATH}" \
  --model "${MODEL_PATH}" \
  --report "${EVAL_REPORT_PATH}"

echo "[bot-ml-gate] phase=leaderboard-compare"
BOT_ML_MODEL="${RUNTIME_JSON_PATH}" \
BOT_LEADERBOARD_PROFILE="${PROFILE}" \
BOT_LEADERBOARD_REQUIRE_NO_REGRESSION=1 \
BOT_LEADERBOARD_RESULT_FILE="${LEADERBOARD_ROWS_PATH}" \
BOT_LEADERBOARD_SUMMARY_FILE="${LEADERBOARD_SUMMARY_PATH}" \
BOT_LEADERBOARD_COMPARE_FILE="${LEADERBOARD_COMPARE_PATH}" \
"${ROOT_DIR}/scripts/ci/bot_leaderboard_regression.sh" \
  "build/${BUILD_PRESET}"

echo "[bot-ml-gate] done workspace=${WORKSPACE}"
echo "[bot-ml-gate] dataset=${DATASET_PATH}"
echo "[bot-ml-gate] model=${MODEL_PATH}"
echo "[bot-ml-gate] runtime_json=${RUNTIME_JSON_PATH}"
echo "[bot-ml-gate] eval_report=${EVAL_REPORT_PATH}"
echo "[bot-ml-gate] compare=${LEADERBOARD_COMPARE_PATH}"
