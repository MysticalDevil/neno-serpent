#!/usr/bin/env bash
set -euo pipefail

# Purpose: generate supervised bot dataset CSV by replaying fixed leaderboard suite.
# Example:
#   ./scripts/dev.sh bot-dataset --output cache/dev/nenoserpent_bot_dataset.csv

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
mkdir -p "${TMP_ROOT}"
BUILD_PRESET="${BUILD_PRESET:-dev}"
SKIP_BUILD="${NENOSERPENT_SKIP_BUILD:-0}"
PROFILE="${BOT_DATASET_PROFILE:-debug}"
SUITE_FILE="${BOT_DATASET_SUITE:-${ROOT_DIR}/scripts/ci/bot_leaderboard_suite.tsv}"
OUTPUT_PATH="${BOT_DATASET_OUTPUT:-${TMP_ROOT}/nenoserpent_bot_dataset.csv}"
MAX_SAMPLES_PER_CASE="${BOT_DATASET_MAX_SAMPLES_PER_CASE:-0}"
ML_MODEL_PATH="${BOT_ML_MODEL:-}"

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

if [[ "${SKIP_BUILD}" != "1" ]]; then
  cmake --preset "${BUILD_PRESET}"
  cmake --build --preset "${BUILD_PRESET}" --target bot-benchmark
fi

rm -f "${OUTPUT_PATH}"
while IFS=$'\t' read -r case_id backend mode level seed games max_ticks; do
  if [[ -z "${case_id}" || "${case_id}" == \#* ]]; then
    continue
  fi

  ml_args=()
  if [[ "${backend}" == "ml" ]]; then
    if [[ -z "${ML_MODEL_PATH}" ]]; then
      echo "[bot-dataset] skip ml case without BOT_ML_MODEL: ${case_id}" >&2
      continue
    fi
    ml_args=(--ml-model "${ML_MODEL_PATH}")
  fi

  echo "[bot-dataset] case=${case_id} backend=${backend} mode=${mode} level=${level} seed=${seed}"
  "${ROOT_DIR}/build/${BUILD_PRESET}/bot-benchmark" \
    --games "${games}" \
    --max-ticks "${max_ticks}" \
    --seed "${seed}" \
    --level "${level}" \
    --profile "${PROFILE}" \
    --mode "${mode}" \
    --backend "${backend}" \
    "${ml_args[@]}" \
    --dump-dataset "${OUTPUT_PATH}" \
    --dataset-case-id "${case_id}" \
    --dataset-backend "${backend}" \
    --dataset-mode "${mode}" \
    --dataset-seed "${seed}" \
    --max-samples "${MAX_SAMPLES_PER_CASE}" \
    | tail -n 1
done < "${SUITE_FILE}"

echo "[bot-dataset] output=${OUTPUT_PATH}"
wc -l "${OUTPUT_PATH}"
