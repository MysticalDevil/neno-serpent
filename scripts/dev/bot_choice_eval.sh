#!/usr/bin/env bash
set -euo pipefail

# Purpose: evaluate choice dataset quality metrics.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
mkdir -p "${TMP_ROOT}"
DATASET_PATH="${BOT_CHOICE_EVAL_DATASET:-${TMP_ROOT}/nenoserpent_bot_choice_dataset.csv}"
REPORT_PATH="${BOT_CHOICE_EVAL_REPORT:-${TMP_ROOT}/nenoserpent_bot_choice_eval_report.json}"
SUMMARY_ENV_PATH="${BOT_CHOICE_EVAL_SUMMARY_ENV:-${TMP_ROOT}/nenoserpent_bot_choice_eval.env}"

while (($# > 0)); do
  case "$1" in
    --dataset)
      DATASET_PATH="$2"
      shift 2
      ;;
    --report)
      REPORT_PATH="$2"
      shift 2
      ;;
    --summary-env)
      SUMMARY_ENV_PATH="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

exec uv run python "${ROOT_DIR}/scripts/dev/bot_choice_eval.py" \
  --dataset "${DATASET_PATH}" \
  --report "${REPORT_PATH}" \
  --summary-env "${SUMMARY_ENV_PATH}"
