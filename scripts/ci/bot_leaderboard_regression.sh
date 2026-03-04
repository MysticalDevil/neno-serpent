#!/usr/bin/env bash
set -euo pipefail

# Purpose: run fixed leaderboard benchmark suite and emit comparable report.
# Inputs:
#   1) build dir (default: build/debug)
#   2) suite file (default: scripts/ci/bot_leaderboard_suite.tsv)
# Output:
#   row report: /tmp/nenoserpent_bot_leaderboard.tsv
#   mode summary: /tmp/nenoserpent_bot_leaderboard_summary.tsv

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build/debug}"
SUITE_FILE="${2:-${ROOT_DIR}/scripts/ci/bot_leaderboard_suite.tsv}"
PROFILE="${BOT_LEADERBOARD_PROFILE:-debug}"
RESULT_FILE="${BOT_LEADERBOARD_RESULT_FILE:-/tmp/nenoserpent_bot_leaderboard.tsv}"
SUMMARY_FILE="${BOT_LEADERBOARD_SUMMARY_FILE:-/tmp/nenoserpent_bot_leaderboard_summary.tsv}"

BIN_PATH="${ROOT_DIR}/${BUILD_DIR}/bot-benchmark"
if [[ ! -x "${BIN_PATH}" ]]; then
  echo "[bot-leaderboard] missing benchmark binary: ${BIN_PATH}" >&2
  exit 1
fi
if [[ ! -f "${SUITE_FILE}" ]]; then
  echo "[bot-leaderboard] missing suite file: ${SUITE_FILE}" >&2
  exit 1
fi

printf 'id\tmode\tlevel\tseed\tgames\tmax_ticks\tavg\tp95\ttimeout\trank_score\n' > "${RESULT_FILE}"

while IFS=$'\t' read -r case_id mode level seed games max_ticks; do
  if [[ -z "${case_id}" || "${case_id}" == \#* ]]; then
    continue
  fi

  output="$("${BIN_PATH}" \
    --games "${games}" \
    --max-ticks "${max_ticks}" \
    --seed "${seed}" \
    --level "${level}" \
    --profile "${PROFILE}" \
    --mode "${mode}")"

  avg="$(printf '%s\n' "${output}" | rg -o 'score\.avg=[0-9]+(\.[0-9]+)?' | head -n1 | cut -d= -f2)"
  p95="$(printf '%s\n' "${output}" | rg -o 'score\.p95=[0-9]+' | head -n1 | cut -d= -f2)"
  timeout="$(printf '%s\n' "${output}" | rg -o 'outcomes\.timeout=[0-9]+' | head -n1 | cut -d= -f2)"
  if [[ -z "${avg}" || -z "${p95}" || -z "${timeout}" ]]; then
    echo "[bot-leaderboard] parse failed id=${case_id}" >&2
    exit 1
  fi

  rank_score="$(awk -v avg="${avg}" -v p95="${p95}" -v timeout="${timeout}" \
    'BEGIN { printf "%.3f", avg + 0.35 * p95 - 0.4 * timeout }')"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${case_id}" "${mode}" "${level}" "${seed}" "${games}" "${max_ticks}" \
    "${avg}" "${p95}" "${timeout}" "${rank_score}" >> "${RESULT_FILE}"
done < "${SUITE_FILE}"

printf 'mode\tcases\tavg_mean\tp95_mean\ttimeout_sum\trank_score_mean\n' > "${SUMMARY_FILE}"
for mode in safe balanced aggressive; do
  mode_rows="$(awk -F '\t' -v target="${mode}" 'NR > 1 && $2 == target {print}' "${RESULT_FILE}")"
  if [[ -z "${mode_rows}" ]]; then
    continue
  fi
  case_count="$(printf '%s\n' "${mode_rows}" | wc -l | tr -d ' ')"
  avg_mean="$(printf '%s\n' "${mode_rows}" | awk -F '\t' '{sum += $7} END {printf "%.3f", sum / NR}')"
  p95_mean="$(printf '%s\n' "${mode_rows}" | awk -F '\t' '{sum += $8} END {printf "%.3f", sum / NR}')"
  timeout_sum="$(printf '%s\n' "${mode_rows}" | awk -F '\t' '{sum += $9} END {printf "%d", sum}')"
  rank_mean="$(printf '%s\n' "${mode_rows}" | awk -F '\t' '{sum += $10} END {printf "%.3f", sum / NR}')"
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${mode}" "${case_count}" "${avg_mean}" "${p95_mean}" "${timeout_sum}" "${rank_mean}" >> "${SUMMARY_FILE}"
done

echo "[bot-leaderboard] rows: ${RESULT_FILE}"
cat "${RESULT_FILE}"
echo "[bot-leaderboard] summary: ${SUMMARY_FILE}"
cat "${SUMMARY_FILE}"
