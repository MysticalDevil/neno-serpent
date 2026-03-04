#!/usr/bin/env bash
set -euo pipefail

# Purpose: generate supervised bot dataset CSV by replaying fixed leaderboard suite.
# Example:
#   ./scripts/dev.sh bot-dataset --output /tmp/bot_dataset.csv

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_PRESET="${BUILD_PRESET:-dev}"
PROFILE="${BOT_DATASET_PROFILE:-debug}"
SUITE_FILE="${BOT_DATASET_SUITE:-${ROOT_DIR}/scripts/ci/bot_leaderboard_suite.tsv}"
OUTPUT_PATH="${BOT_DATASET_OUTPUT:-/tmp/nenoserpent_bot_dataset.csv}"
MAX_SAMPLES_PER_CASE="${BOT_DATASET_MAX_SAMPLES_PER_CASE:-0}"

while (($# > 0)); do
  case "$1" in
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --suite)
      SUITE_FILE="$2"
      shift 2
      ;;
    --output)
      OUTPUT_PATH="$2"
      shift 2
      ;;
    --max-samples-per-case)
      MAX_SAMPLES_PER_CASE="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

cmake --preset "${BUILD_PRESET}"
cmake --build --preset "${BUILD_PRESET}" --target bot-benchmark

rm -f "${OUTPUT_PATH}"
while IFS=$'\t' read -r case_id mode level seed games max_ticks; do
  if [[ -z "${case_id}" || "${case_id}" == \#* ]]; then
    continue
  fi

  echo "[bot-dataset] case=${case_id} mode=${mode} level=${level} seed=${seed}"
  "${ROOT_DIR}/build/${BUILD_PRESET}/bot-benchmark" \
    --games "${games}" \
    --max-ticks "${max_ticks}" \
    --seed "${seed}" \
    --level "${level}" \
    --profile "${PROFILE}" \
    --mode "${mode}" \
    --dump-dataset "${OUTPUT_PATH}" \
    --max-samples "${MAX_SAMPLES_PER_CASE}" \
    | tail -n 1
done < "${SUITE_FILE}"

echo "[bot-dataset] output=${OUTPUT_PATH}"
wc -l "${OUTPUT_PATH}"
