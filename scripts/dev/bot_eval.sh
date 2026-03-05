#!/usr/bin/env bash
set -euo pipefail

# Purpose: evaluate imitation model checkpoint against dataset CSV.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DATASET_PATH="${BOT_EVAL_DATASET:-/tmp/nenoserpent_bot_dataset.csv}"
MODEL_PATH="${BOT_EVAL_MODEL:-/tmp/nenoserpent_bot_policy.pt}"
REPORT_PATH="${BOT_EVAL_REPORT:-/tmp/nenoserpent_bot_eval_report.json}"

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
    --report)
      REPORT_PATH="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

exec uv run python "${ROOT_DIR}/scripts/dev/bot_eval.py" \
  --dataset "${DATASET_PATH}" \
  --model "${MODEL_PATH}" \
  --report "${REPORT_PATH}"
