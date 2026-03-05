#!/usr/bin/env bash
set -euo pipefail

# Purpose: run an end-to-end reproducible ML bot validation flow.
# Inputs:
#   --build-preset <name>   CMake preset (default: dev)
#   --profile <name>        Bot strategy profile for benchmark/train data (default: debug)
#   --workspace <path>      Output workspace (default: cache/dev/nenoserpent_bot_ml_gate)
#   --epochs <count>        Training epochs (default: 24)
#   --batch-size <count>    Training batch size (default: 256)
#   --lr <value>            Learning rate (default: 0.001)
#   --seed <value>          Fixed seed for reproducible training (default: 20260304)
# Output:
#   - dataset/model/runtime-json/eval report under workspace
#   - leaderboard compare report with rule vs ml

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
mkdir -p "${TMP_ROOT}"
BUILD_PRESET="${BOT_ML_GATE_BUILD_PRESET:-dev}"
PROFILE="${BOT_ML_GATE_PROFILE:-debug}"
WORKSPACE="${BOT_ML_GATE_WORKSPACE:-${TMP_ROOT}/nenoserpent_bot_ml_gate}"
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

TORCH_CHECK_LOG="${TMP_ROOT}/bot-ml-gate-torch-check.log"
if ! uv run python -c "import torch" >/dev/null 2>"${TORCH_CHECK_LOG}"; then
  echo "[bot-ml-gate] torch check failed" >&2
  cat "${TORCH_CHECK_LOG}" >&2
  if rg -q "Failed to initialize cache at" "${TORCH_CHECK_LOG}"; then
    echo "[bot-ml-gate] hint: uv cache permission issue; check access to ~/.cache/uv" >&2
  else
    echo "[bot-ml-gate] hint: install with uv add --dev torch" >&2
  fi
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
CHOICE_DATASET_RULE_PATH="${WORKSPACE}/choice.rule.csv"
CHOICE_DATASET_ML_PATH="${WORKSPACE}/choice.ml.csv"
CHOICE_REPORT_RULE_PATH="${WORKSPACE}/choice.rule.report.json"
CHOICE_REPORT_ML_PATH="${WORKSPACE}/choice.ml.report.json"
CHOICE_SUMMARY_RULE_PATH="${WORKSPACE}/choice.rule.summary.env"
CHOICE_SUMMARY_ML_PATH="${WORKSPACE}/choice.ml.summary.env"
POWER_DATASET_RULE_PATH="${WORKSPACE}/power.rule.csv"
POWER_DATASET_ML_PATH="${WORKSPACE}/power.ml.csv"
POWER_REPORT_RULE_PATH="${WORKSPACE}/power.rule.report.json"
POWER_REPORT_ML_PATH="${WORKSPACE}/power.ml.report.json"
POWER_SUMMARY_RULE_PATH="${WORKSPACE}/power.rule.summary.env"
POWER_SUMMARY_ML_PATH="${WORKSPACE}/power.ml.summary.env"

compare_metric_non_regression() {
  local label="$1"
  local metric_key="$2"
  local direction="$3"
  local rule_env="$4"
  local ml_env="$5"
  local rule_value ml_value
  # shellcheck disable=SC1090
  source "${rule_env}"
  rule_value="${!metric_key}"
  # shellcheck disable=SC1090
  source "${ml_env}"
  ml_value="${!metric_key}"
  case "${direction}" in
    ge)
      if ! awk -v r="${rule_value}" -v m="${ml_value}" 'BEGIN { exit !(m >= r) }'; then
        echo "[bot-ml-gate] ${label} regression: ml=${ml_value} < rule=${rule_value}" >&2
        exit 1
      fi
      ;;
    le)
      if ! awk -v r="${rule_value}" -v m="${ml_value}" 'BEGIN { exit !(m <= r) }'; then
        echo "[bot-ml-gate] ${label} regression: ml=${ml_value} > rule=${rule_value}" >&2
        exit 1
      fi
      ;;
    *)
      echo "[bot-ml-gate] invalid compare direction: ${direction}" >&2
      exit 1
      ;;
  esac
  echo "[bot-ml-gate] ${label} ok rule=${rule_value} ml=${ml_value}"
}

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

echo "[bot-ml-gate] phase=choice-dataset-eval"
BOT_CHOICE_DATASET_PROFILE="${PROFILE}" \
BOT_CHOICE_DATASET_OUTPUT="${CHOICE_DATASET_RULE_PATH}" \
BOT_CHOICE_DATASET_BACKEND=rule \
"${ROOT_DIR}/scripts/dev/bot_choice_dataset.sh"

BOT_CHOICE_DATASET_PROFILE="${PROFILE}" \
BOT_CHOICE_DATASET_OUTPUT="${CHOICE_DATASET_ML_PATH}" \
BOT_CHOICE_DATASET_BACKEND=ml \
BOT_ML_MODEL="${RUNTIME_JSON_PATH}" \
"${ROOT_DIR}/scripts/dev/bot_choice_dataset.sh"

"${ROOT_DIR}/scripts/dev/bot_choice_eval.sh" \
  --dataset "${CHOICE_DATASET_RULE_PATH}" \
  --report "${CHOICE_REPORT_RULE_PATH}" \
  --summary-env "${CHOICE_SUMMARY_RULE_PATH}"
"${ROOT_DIR}/scripts/dev/bot_choice_eval.sh" \
  --dataset "${CHOICE_DATASET_ML_PATH}" \
  --report "${CHOICE_REPORT_ML_PATH}" \
  --summary-env "${CHOICE_SUMMARY_ML_PATH}"

compare_metric_non_regression "choice.top1_acc" "top1_acc" "ge" \
  "${CHOICE_SUMMARY_RULE_PATH}" "${CHOICE_SUMMARY_ML_PATH}"
compare_metric_non_regression "choice.top2_acc" "top2_acc" "ge" \
  "${CHOICE_SUMMARY_RULE_PATH}" "${CHOICE_SUMMARY_ML_PATH}"
compare_metric_non_regression "choice.avg_rank" "avg_rank" "le" \
  "${CHOICE_SUMMARY_RULE_PATH}" "${CHOICE_SUMMARY_ML_PATH}"

echo "[bot-ml-gate] phase=power-dataset-eval"
BOT_POWER_DATASET_PROFILE="${PROFILE}" \
BOT_POWER_DATASET_OUTPUT="${POWER_DATASET_RULE_PATH}" \
BOT_POWER_DATASET_BACKEND=rule \
"${ROOT_DIR}/scripts/dev/bot_power_dataset.sh"

BOT_POWER_DATASET_PROFILE="${PROFILE}" \
BOT_POWER_DATASET_OUTPUT="${POWER_DATASET_ML_PATH}" \
BOT_POWER_DATASET_BACKEND=ml \
BOT_ML_MODEL="${RUNTIME_JSON_PATH}" \
"${ROOT_DIR}/scripts/dev/bot_power_dataset.sh"

"${ROOT_DIR}/scripts/dev/bot_power_eval.sh" \
  --dataset "${POWER_DATASET_RULE_PATH}" \
  --report "${POWER_REPORT_RULE_PATH}" \
  --summary-env "${POWER_SUMMARY_RULE_PATH}"
"${ROOT_DIR}/scripts/dev/bot_power_eval.sh" \
  --dataset "${POWER_DATASET_ML_PATH}" \
  --report "${POWER_REPORT_ML_PATH}" \
  --summary-env "${POWER_SUMMARY_ML_PATH}"

compare_metric_non_regression "power.top1_acc" "top1_acc" "ge" \
  "${POWER_SUMMARY_RULE_PATH}" "${POWER_SUMMARY_ML_PATH}"
compare_metric_non_regression "power.top2_acc" "top2_acc" "ge" \
  "${POWER_SUMMARY_RULE_PATH}" "${POWER_SUMMARY_ML_PATH}"
compare_metric_non_regression "power.avg_rank" "avg_rank" "le" \
  "${POWER_SUMMARY_RULE_PATH}" "${POWER_SUMMARY_ML_PATH}"

echo "[bot-ml-gate] done workspace=${WORKSPACE}"
echo "[bot-ml-gate] dataset=${DATASET_PATH}"
echo "[bot-ml-gate] model=${MODEL_PATH}"
echo "[bot-ml-gate] runtime_json=${RUNTIME_JSON_PATH}"
echo "[bot-ml-gate] eval_report=${EVAL_REPORT_PATH}"
echo "[bot-ml-gate] compare=${LEADERBOARD_COMPARE_PATH}"
echo "[bot-ml-gate] choice_report_rule=${CHOICE_REPORT_RULE_PATH}"
echo "[bot-ml-gate] choice_report_ml=${CHOICE_REPORT_ML_PATH}"
echo "[bot-ml-gate] power_report_rule=${POWER_REPORT_RULE_PATH}"
echo "[bot-ml-gate] power_report_ml=${POWER_REPORT_ML_PATH}"
