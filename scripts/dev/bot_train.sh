#!/usr/bin/env bash
set -euo pipefail

# Purpose: train imitation policy from bot dataset.
# Example:
#   ./scripts/dev.sh bot-train --dataset /tmp/nenoserpent_bot_dataset.csv

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DATASET_PATH="${BOT_TRAIN_DATASET:-/tmp/nenoserpent_bot_dataset.csv}"
MODEL_PATH="${BOT_TRAIN_MODEL:-/tmp/nenoserpent_bot_policy.pt}"
METADATA_PATH="${BOT_TRAIN_METADATA:-/tmp/nenoserpent_bot_policy_meta.json}"
EPOCHS="${BOT_TRAIN_EPOCHS:-30}"
BATCH_SIZE="${BOT_TRAIN_BATCH_SIZE:-256}"
LR="${BOT_TRAIN_LR:-0.001}"
TRAIN_RATIO="${BOT_TRAIN_TRAIN_RATIO:-0.9}"
SEED="${BOT_TRAIN_SEED:-20260304}"

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
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

exec python3 "${ROOT_DIR}/scripts/dev/bot_train.py" \
  --dataset "${DATASET_PATH}" \
  --output-model "${MODEL_PATH}" \
  --output-metadata "${METADATA_PATH}" \
  --epochs "${EPOCHS}" \
  --batch-size "${BATCH_SIZE}" \
  --lr "${LR}" \
  --train-ratio "${TRAIN_RATIO}" \
  --seed "${SEED}"
