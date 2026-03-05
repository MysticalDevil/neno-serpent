#!/usr/bin/env bash
set -euo pipefail

# Purpose: generate power-up chase decision dataset CSV from benchmark suite.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
mkdir -p "${TMP_ROOT}"
BUILD_PRESET="${BUILD_PRESET:-dev}"
PROFILE="${BOT_POWER_DATASET_PROFILE:-debug}"
SUITE_FILE="${BOT_POWER_DATASET_SUITE:-${ROOT_DIR}/scripts/ci/bot_leaderboard_suite.tsv}"
OUTPUT_PATH="${BOT_POWER_DATASET_OUTPUT:-${TMP_ROOT}/nenoserpent_bot_power_dataset.csv}"
MAX_SAMPLES_PER_CASE="${BOT_POWER_DATASET_MAX_SAMPLES_PER_CASE:-0}"
BACKEND="${BOT_POWER_DATASET_BACKEND:-rule}"
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
    --backend)
      BACKEND="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

case "${BACKEND}" in
  rule|ml|search) ;;
  *)
    echo "[bot-power-dataset] backend must be rule|ml|search" >&2
    exit 1
    ;;
esac

cmake --preset "${BUILD_PRESET}"
cmake --build --preset "${BUILD_PRESET}" --target bot-benchmark

rm -f "${OUTPUT_PATH}"
while IFS=$'\t' read -r case_id _ mode level seed games max_ticks; do
  if [[ -z "${case_id}" || "${case_id}" == \#* ]]; then
    continue
  fi

  ml_args=()
  if [[ "${BACKEND}" == "ml" ]]; then
    if [[ -z "${ML_MODEL_PATH}" ]]; then
      echo "[bot-power-dataset] skip case without BOT_ML_MODEL: ${case_id}" >&2
      continue
    fi
    ml_args=(--ml-model "${ML_MODEL_PATH}")
  fi

  echo "[bot-power-dataset] case=${case_id} backend=${BACKEND} mode=${mode} level=${level} seed=${seed}"
  "${ROOT_DIR}/build/${BUILD_PRESET}/bot-benchmark" \
    --games "${games}" \
    --max-ticks "${max_ticks}" \
    --seed "${seed}" \
    --level "${level}" \
    --profile "${PROFILE}" \
    --mode "${mode}" \
    --backend "${BACKEND}" \
    "${ml_args[@]}" \
    --dump-power-dataset "${OUTPUT_PATH}" \
    --dataset-case-id "${case_id}" \
    --dataset-backend "${BACKEND}" \
    --dataset-mode "${mode}" \
    --dataset-seed "${seed}" \
    --max-power-samples "${MAX_SAMPLES_PER_CASE}" \
    | rg "power_dataset.samples=" -m1
done < "${SUITE_FILE}"

echo "[bot-power-dataset] output=${OUTPUT_PATH}"
wc -l "${OUTPUT_PATH}"
