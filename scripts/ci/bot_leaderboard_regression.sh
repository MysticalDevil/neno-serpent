#!/usr/bin/env bash
set -euo pipefail

# Purpose: run fixed leaderboard benchmark suite and emit comparable report.
# Inputs:
#   1) build dir (default: build/debug)
#   2) suite file (default: scripts/ci/bot_leaderboard_suite.tsv)
# Output:
#   row report: cache/ci/nenoserpent_bot_leaderboard.tsv
#   mode summary: cache/ci/nenoserpent_bot_leaderboard_summary.tsv

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ci}}"
mkdir -p "${TMP_ROOT}"
BUILD_DIR="${1:-build/debug}"
SUITE_FILE="${2:-${ROOT_DIR}/scripts/ci/bot_leaderboard_suite.tsv}"
PROFILE="${BOT_LEADERBOARD_PROFILE:-debug}"
RESULT_FILE="${BOT_LEADERBOARD_RESULT_FILE:-${TMP_ROOT}/nenoserpent_bot_leaderboard.tsv}"
SUMMARY_FILE="${BOT_LEADERBOARD_SUMMARY_FILE:-${TMP_ROOT}/nenoserpent_bot_leaderboard_summary.tsv}"
COMPARE_FILE="${BOT_LEADERBOARD_COMPARE_FILE:-${TMP_ROOT}/nenoserpent_bot_leaderboard_compare.tsv}"
ML_MODEL_PATH="${BOT_ML_MODEL:-}"
REQUIRE_NO_REGRESSION="${BOT_LEADERBOARD_REQUIRE_NO_REGRESSION:-0}"

BIN_PATH="${ROOT_DIR}/${BUILD_DIR}/bot-benchmark"
if [[ ! -x "${BIN_PATH}" ]]; then
  echo "[bot-leaderboard] missing benchmark binary: ${BIN_PATH}" >&2
  exit 1
fi
if [[ ! -f "${SUITE_FILE}" ]]; then
  echo "[bot-leaderboard] missing suite file: ${SUITE_FILE}" >&2
  exit 1
fi

printf 'id\tbackend\tmode\tlevel\tseed\tgames\tmax_ticks\tavg\tp95\ttimeout\trank_score\n' > "${RESULT_FILE}"

while IFS=$'\t' read -r case_id backend mode level seed games max_ticks; do
  if [[ -z "${case_id}" || "${case_id}" == \#* ]]; then
    continue
  fi

  ml_args=()
  if [[ "${backend}" == "ml" ]]; then
    if [[ -z "${ML_MODEL_PATH}" ]]; then
      echo "[bot-leaderboard] skip ml row without BOT_ML_MODEL: id=${case_id}" >&2
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

  avg="$(printf '%s\n' "${output}" | rg -o 'score\.avg=[0-9]+(\.[0-9]+)?' | head -n1 | cut -d= -f2)"
  p95="$(printf '%s\n' "${output}" | rg -o 'score\.p95=[0-9]+' | head -n1 | cut -d= -f2)"
  timeout="$(printf '%s\n' "${output}" | rg -o 'outcomes\.timeout=[0-9]+' | head -n1 | cut -d= -f2)"
  if [[ -z "${avg}" || -z "${p95}" || -z "${timeout}" ]]; then
    echo "[bot-leaderboard] parse failed id=${case_id}" >&2
    exit 1
  fi

  rank_score="$(awk -v avg="${avg}" -v p95="${p95}" -v timeout="${timeout}" \
    'BEGIN { printf "%.3f", avg + 0.35 * p95 - 0.4 * timeout }')"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${case_id}" "${backend}" "${mode}" "${level}" "${seed}" "${games}" "${max_ticks}" \
    "${avg}" "${p95}" "${timeout}" "${rank_score}" >> "${RESULT_FILE}"
done < "${SUITE_FILE}"

printf 'backend\tmode\tcases\tavg_mean\tp95_mean\ttimeout_sum\trank_score_mean\n' > "${SUMMARY_FILE}"
while IFS=$'\t' read -r backend mode; do
  rows="$(awk -F '\t' -v b="${backend}" -v m="${mode}" 'NR > 1 && $2 == b && $3 == m {print}' "${RESULT_FILE}")"
  if [[ -z "${rows}" ]]; then
    continue
  fi
  case_count="$(printf '%s\n' "${rows}" | wc -l | tr -d ' ')"
  avg_mean="$(printf '%s\n' "${rows}" | awk -F '\t' '{sum += $8} END {printf "%.3f", sum / NR}')"
  p95_mean="$(printf '%s\n' "${rows}" | awk -F '\t' '{sum += $9} END {printf "%.3f", sum / NR}')"
  timeout_sum="$(printf '%s\n' "${rows}" | awk -F '\t' '{sum += $10} END {printf "%d", sum}')"
  rank_mean="$(printf '%s\n' "${rows}" | awk -F '\t' '{sum += $11} END {printf "%.3f", sum / NR}')"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${backend}" "${mode}" "${case_count}" "${avg_mean}" "${p95_mean}" "${timeout_sum}" "${rank_mean}" >> "${SUMMARY_FILE}"
done < <(awk -F '\t' 'NR > 1 {print $2 "\t" $3}' "${RESULT_FILE}" | sort -u)

printf 'id\tmode\tlevel\tseed\trule_avg\tml_avg\tdelta_avg\trule_p95\tml_p95\tdelta_p95\tstatus\n' > "${COMPARE_FILE}"
if awk -F '\t' 'NR > 1 && $2 == "ml" {found=1} END {exit !found}' "${RESULT_FILE}"; then
  while IFS=$'\t' read -r case_id mode level seed rule_avg ml_avg rule_p95 ml_p95; do
    delta_avg="$(awk -v rule="${rule_avg}" -v ml="${ml_avg}" 'BEGIN { printf "%.3f", ml - rule }')"
    delta_p95="$((ml_p95 - rule_p95))"
    status="pass"
    if awk -v rule="${rule_avg}" -v ml="${ml_avg}" 'BEGIN { exit !(ml < rule) }'; then
      status="fail-avg"
    fi
    if (( ml_p95 < rule_p95 )); then
      if [[ "${status}" == "pass" ]]; then
        status="fail-p95"
      else
        status="${status}+p95"
      fi
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "${case_id}" "${mode}" "${level}" "${seed}" "${rule_avg}" "${ml_avg}" "${delta_avg}" \
      "${rule_p95}" "${ml_p95}" "${delta_p95}" "${status}" >> "${COMPARE_FILE}"
  done < <(awk -F '\t' '
    NR > 1 && $2 == "rule" {key=$1 FS $3 FS $4 FS $5; rule_avg[key]=$8; rule_p95[key]=$9}
    NR > 1 && $2 == "ml" {
      key=$1 FS $3 FS $4 FS $5;
      if (key in rule_avg) {
        print $1 FS $3 FS $4 FS $5 FS rule_avg[key] FS $8 FS rule_p95[key] FS $9;
      }
    }' "${RESULT_FILE}")
fi

echo "[bot-leaderboard] rows: ${RESULT_FILE}"
cat "${RESULT_FILE}"
echo "[bot-leaderboard] summary: ${SUMMARY_FILE}"
cat "${SUMMARY_FILE}"
echo "[bot-leaderboard] compare: ${COMPARE_FILE}"
cat "${COMPARE_FILE}"

if [[ "${REQUIRE_NO_REGRESSION}" == "1" ]] && [[ -s "${COMPARE_FILE}" ]]; then
  if awk -F '\t' 'NR > 1 && $11 != "pass" {exit 1}' "${COMPARE_FILE}"; then
    echo "[bot-leaderboard] no-regression gate passed"
  else
    echo "[bot-leaderboard] no-regression gate failed" >&2
    exit 1
  fi
fi
