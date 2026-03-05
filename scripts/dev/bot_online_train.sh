#!/usr/bin/env bash
set -euo pipefail

# Purpose: continuous training loop for ml-online runtime hot reload.
# Example:
#   ./scripts/dev.sh bot-online-train --workspace cache/dev/nenoserpent_bot_online

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
WORKSPACE="${BOT_ONLINE_WORKSPACE:-${TMP_ROOT}/nenoserpent_bot_online}"
PROFILE="${BOT_ONLINE_PROFILE:-dev}"
INTERVAL_SEC="${BOT_ONLINE_INTERVAL_SEC:-20}"
MAX_SAMPLES_PER_CASE="${BOT_ONLINE_MAX_SAMPLES_PER_CASE:-6}"
EPOCHS="${BOT_ONLINE_EPOCHS:-8}"
BATCH_SIZE="${BOT_ONLINE_BATCH_SIZE:-192}"
LR="${BOT_ONLINE_LR:-0.001}"
SEED_BASE="${BOT_ONLINE_SEED_BASE:-20260306}"

while (($# > 0)); do
  case "$1" in
    --workspace)
      WORKSPACE="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --interval-sec)
      INTERVAL_SEC="$2"
      shift 2
      ;;
    --max-samples-per-case)
      MAX_SAMPLES_PER_CASE="$2"
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
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-online-train [options]

Options:
  --workspace <dir>             Output workspace (default: cache/dev/nenoserpent_bot_online)
  --profile <debug|dev|release> Dataset generation profile (default: dev)
  --interval-sec <N>            Delay between rounds (default: 20)
  --max-samples-per-case <N>    Dataset cap per leaderboard case (default: 6)
  --epochs <N>                  Train epochs per round (default: 8)
  --batch-size <N>              Train batch size per round (default: 192)
  --lr <float>                  Learning rate per round (default: 0.001)

Notes:
  - Keep this running in terminal A.
  - Run game in terminal B with:
    ./scripts/dev.sh bot-run --backend ml-online --ml-model <workspace>/nenoserpent_bot_policy_runtime.json
  - Runtime will hot-reload the model when this loop publishes a newer JSON file.
EOF
        exit 0
      fi
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

mkdir -p "${WORKSPACE}"
DATASET_PATH="${WORKSPACE}/nenoserpent_bot_dataset.csv"
MODEL_PATH="${WORKSPACE}/nenoserpent_bot_policy.pt"
METADATA_PATH="${WORKSPACE}/nenoserpent_bot_policy_meta.json"
RUNTIME_JSON_PATH="${WORKSPACE}/nenoserpent_bot_policy_runtime.json"
ROUND=0

echo "[bot-online-train] workspace=${WORKSPACE}"
echo "[bot-online-train] profile=${PROFILE} intervalSec=${INTERVAL_SEC} epochs=${EPOCHS} batchSize=${BATCH_SIZE}"

while true; do
  ROUND=$((ROUND + 1))
  ROUND_SEED=$((SEED_BASE + ROUND))
  TMP_RUNTIME_JSON="${WORKSPACE}/.runtime.round-${ROUND}.json"
  echo "[bot-online-train] round=${ROUND} phase=dataset"
  "${ROOT_DIR}/scripts/dev.sh" bot-dataset \
    --profile "${PROFILE}" \
    --max-samples-per-case "${MAX_SAMPLES_PER_CASE}" \
    --output "${DATASET_PATH}"

  echo "[bot-online-train] round=${ROUND} phase=train seed=${ROUND_SEED}"
  "${ROOT_DIR}/scripts/dev.sh" bot-train \
    --dataset "${DATASET_PATH}" \
    --model "${MODEL_PATH}" \
    --metadata "${METADATA_PATH}" \
    --runtime-json "${TMP_RUNTIME_JSON}" \
    --epochs "${EPOCHS}" \
    --batch-size "${BATCH_SIZE}" \
    --lr "${LR}" \
    --seed "${ROUND_SEED}"

  mv "${TMP_RUNTIME_JSON}" "${RUNTIME_JSON_PATH}"
  echo "[bot-online-train] round=${ROUND} published=${RUNTIME_JSON_PATH}"
  sleep "${INTERVAL_SEC}"
done
