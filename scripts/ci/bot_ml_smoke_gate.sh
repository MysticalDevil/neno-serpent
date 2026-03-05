#!/usr/bin/env bash
set -euo pipefail

# Purpose: fast always-on ML smoke gate using repository tiny runtime model.
# Validates that ml backend with conservative confidence gate does not regress
# against rule baseline on a fixed quick benchmark scenario.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build/debug}"
PROFILE="${BOT_ML_SMOKE_PROFILE:-debug}"
MODE="${BOT_ML_SMOKE_MODE:-balanced}"
GAMES="${BOT_ML_SMOKE_GAMES:-24}"
MAX_TICKS="${BOT_ML_SMOKE_MAX_TICKS:-1200}"
SEED="${BOT_ML_SMOKE_SEED:-1337}"
LEVEL="${BOT_ML_SMOKE_LEVEL:-0}"
MODEL_PATH="${BOT_ML_SMOKE_MODEL:-${ROOT_DIR}/scripts/ci/assets/bot_ml_smoke.runtime.json}"

BIN_PATH="${ROOT_DIR}/${BUILD_DIR}/bot-benchmark"
if [[ ! -x "${BIN_PATH}" ]]; then
  echo "[bot-ml-smoke] missing benchmark binary: ${BIN_PATH}" >&2
  exit 1
fi
if [[ ! -f "${MODEL_PATH}" ]]; then
  echo "[bot-ml-smoke] missing model file: ${MODEL_PATH}" >&2
  exit 1
fi

run_case() {
  local backend="$1"
  shift
  "${BIN_PATH}" \
    --games "${GAMES}" \
    --max-ticks "${MAX_TICKS}" \
    --seed "${SEED}" \
    --level "${LEVEL}" \
    --profile "${PROFILE}" \
    --mode "${MODE}" \
    --backend "${backend}" \
    "$@"
}

parse_avg() {
  rg -o 'score\.avg=[0-9]+(\.[0-9]+)?' | head -n1 | cut -d= -f2
}

parse_p95() {
  rg -o 'score\.p95=[0-9]+' | head -n1 | cut -d= -f2
}

rule_output="$(run_case rule)"
ml_output="$(run_case ml --ml-model "${MODEL_PATH}")"

rule_avg="$(printf '%s\n' "${rule_output}" | parse_avg)"
rule_p95="$(printf '%s\n' "${rule_output}" | parse_p95)"
ml_avg="$(printf '%s\n' "${ml_output}" | parse_avg)"
ml_p95="$(printf '%s\n' "${ml_output}" | parse_p95)"

if [[ -z "${rule_avg}" || -z "${rule_p95}" || -z "${ml_avg}" || -z "${ml_p95}" ]]; then
  echo "[bot-ml-smoke] failed to parse benchmark output" >&2
  echo "[bot-ml-smoke] rule_output:" >&2
  printf '%s\n' "${rule_output}" >&2
  echo "[bot-ml-smoke] ml_output:" >&2
  printf '%s\n' "${ml_output}" >&2
  exit 1
fi

echo "[bot-ml-smoke] rule avg=${rule_avg} p95=${rule_p95}"
echo "[bot-ml-smoke] ml   avg=${ml_avg} p95=${ml_p95}"

if awk -v rule="${rule_avg}" -v ml="${ml_avg}" 'BEGIN { exit !(ml < rule) }'; then
  echo "[bot-ml-smoke] regression: ml_avg < rule_avg" >&2
  exit 1
fi
if (( ml_p95 < rule_p95 )); then
  echo "[bot-ml-smoke] regression: ml_p95 < rule_p95" >&2
  exit 1
fi

echo "[bot-ml-smoke] passed"
