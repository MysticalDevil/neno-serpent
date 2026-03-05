#!/usr/bin/env bash
set -euo pipefail

# Purpose: train imitation policy from bot dataset.
# Example:
#   ./scripts/dev.sh bot-train --dataset cache/dev/nenoserpent_bot_dataset.csv

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
mkdir -p "${TMP_ROOT}"
DATASET_PATH="${BOT_TRAIN_DATASET:-${TMP_ROOT}/nenoserpent_bot_dataset.csv}"
MODEL_PATH="${BOT_TRAIN_MODEL:-${TMP_ROOT}/nenoserpent_bot_policy.pt}"
METADATA_PATH="${BOT_TRAIN_METADATA:-${TMP_ROOT}/nenoserpent_bot_policy_meta.json}"
RUNTIME_JSON_PATH="${BOT_TRAIN_RUNTIME_JSON:-${TMP_ROOT}/nenoserpent_bot_policy_runtime.json}"
EPOCHS="${BOT_TRAIN_EPOCHS:-30}"
BATCH_SIZE="${BOT_TRAIN_BATCH_SIZE:-256}"
LR="${BOT_TRAIN_LR:-0.001}"
TRAIN_RATIO="${BOT_TRAIN_TRAIN_RATIO:-0.9}"
SEED="${BOT_TRAIN_SEED:-20260304}"
HARD_SAMPLE_SCALE="${BOT_TRAIN_HARD_SAMPLE_SCALE:-1.0}"

while (($# > 0)); do
  case "$1" in
    --dataset)
      DATASET_PATH="$2"
      shift 2
      ;;
    --model)
      MODEL_PATH="$2"
      shift 2
      ;;
    --metadata)
      METADATA_PATH="$2"
      shift 2
      ;;
    --runtime-json)
      RUNTIME_JSON_PATH="$2"
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
    --train-ratio)
      TRAIN_RATIO="$2"
      shift 2
      ;;
    --seed)
      SEED="$2"
      shift 2
      ;;
    --hard-sample-scale)
      HARD_SAMPLE_SCALE="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

exec uv run python "${ROOT_DIR}/scripts/dev/bot_train.py" \
  --dataset "${DATASET_PATH}" \
  --output-model "${MODEL_PATH}" \
  --output-metadata "${METADATA_PATH}" \
  --output-runtime-json "${RUNTIME_JSON_PATH}" \
  --epochs "${EPOCHS}" \
  --batch-size "${BATCH_SIZE}" \
  --lr "${LR}" \
  --train-ratio "${TRAIN_RATIO}" \
  --seed "${SEED}" \
  --hard-sample-scale "${HARD_SAMPLE_SCALE}"
