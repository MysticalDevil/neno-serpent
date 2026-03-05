#!/usr/bin/env bash
set -euo pipefail

# Purpose: run fixed bot benchmark scenarios and gate results against baseline thresholds.
# Inputs:
#   1) build dir (default: build/debug)
#   2) baseline file (default: scripts/ci/bot_e2e_baseline.tsv)
# Output:
#   tab-separated report (default: cache/ci/nenoserpent_bot_e2e_results.tsv)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ci}}"
mkdir -p "${TMP_ROOT}"
BUILD_DIR="${1:-build/debug}"
BASELINE_FILE="${2:-${ROOT_DIR}/scripts/ci/bot_e2e_baseline.tsv}"
RESULT_FILE="${BOT_E2E_RESULT_FILE:-${TMP_ROOT}/nenoserpent_bot_e2e_results.tsv}"
PROFILE="${BOT_E2E_PROFILE:-debug}"

BIN_PATH="${ROOT_DIR}/${BUILD_DIR}/bot-benchmark"
if [[ ! -x "${BIN_PATH}" ]]; then
  echo "[bot-e2e] missing benchmark binary: ${BIN_PATH}" >&2
  exit 1
fi
if [[ ! -f "${BASELINE_FILE}" ]]; then
  echo "[bot-e2e] missing baseline file: ${BASELINE_FILE}" >&2
  exit 1
fi

ML_MODEL_PATH="${BOT_ML_MODEL:-}"

printf 'backend\tmode\tlevel\tavg\tp95\tmin_avg\tmin_p95\tgames\tmax_ticks\tseed\tstatus\n' > "${RESULT_FILE}"

failed=0
while IFS=$'\t' read -r backend mode level min_avg min_p95 games max_ticks seed; do
  if [[ -z "${backend}" || "${backend}" == \#* ]]; then
    continue
  fi

  ml_args=()
  if [[ "${backend}" == "ml" ]]; then
    if [[ -z "${ML_MODEL_PATH}" ]]; then
      echo "[bot-e2e] skip ml row without BOT_ML_MODEL: mode=${mode} level=${level}" >&2
      continue
    fi
    ml_args=(--ml-model "${ML_MODEL_PATH}")
  fi

  output="$("${BIN_PATH}" \
    --games "${games}" \
    --max-ticks "${max_ticks}" \
    --seed "${seed}" \
    --level "${level}" \
    --profile "${PROFILE}" \
    --mode "${mode}" \
    --backend "${backend}" \
    "${ml_args[@]}")"

  avg="$(printf '%s\n' "${output}" | rg -o 'score\.avg=[0-9]+(\.[0-9]+)?' | head -n 1 | cut -d= -f2)"
  p95="$(printf '%s\n' "${output}" | rg -o 'score\.p95=[0-9]+' | head -n 1 | cut -d= -f2)"
  if [[ -z "${avg}" || -z "${p95}" ]]; then
    echo "[bot-e2e] parse failed backend=${backend} mode=${mode} level=${level}" >&2
    failed=1
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "${backend}" "${mode}" "${level}" "NA" "NA" "${min_avg}" "${min_p95}" "${games}" "${max_ticks}" "${seed}" \
      "parse-failed" >> "${RESULT_FILE}"
    continue
  fi

  status="pass"
  if ! awk -v value="${avg}" -v min="${min_avg}" 'BEGIN { exit !(value >= min) }'; then
    status="fail-avg"
    failed=1
  fi
  if (( p95 < min_p95 )); then
    if [[ "${status}" == "pass" ]]; then
      status="fail-p95"
    else
      status="${status}+p95"
    fi
    failed=1
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${backend}" "${mode}" "${level}" "${avg}" "${p95}" "${min_avg}" "${min_p95}" "${games}" "${max_ticks}" "${seed}" \
    "${status}" >> "${RESULT_FILE}"
done < "${BASELINE_FILE}"

echo "[bot-e2e] results: ${RESULT_FILE}"
cat "${RESULT_FILE}"

if (( failed != 0 )); then
  echo "[bot-e2e] regression gate failed" >&2
  exit 1
fi
echo "[bot-e2e] regression gate passed"
