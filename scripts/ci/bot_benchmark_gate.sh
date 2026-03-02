#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

BUILD_DIR="${1:-build/debug}"
GAMES="${BOT_BENCH_GAMES:-120}"
MAX_TICKS="${BOT_BENCH_MAX_TICKS:-3000}"
PROFILE="${BOT_BENCH_PROFILE:-dev}"
MODE="${BOT_BENCH_MODE:-balanced}"
MIN_AVG="${BOT_BENCH_MIN_AVG:-70}"
MIN_P95="${BOT_BENCH_MIN_P95:-120}"

BIN_PATH="${ROOT_DIR}/${BUILD_DIR}/bot-benchmark"
if [[ ! -x "${BIN_PATH}" ]]; then
  echo "[bot-gate] missing benchmark binary: ${BIN_PATH}" >&2
  exit 1
fi

OUTPUT="$("${BIN_PATH}" --games "${GAMES}" --max-ticks "${MAX_TICKS}" --profile "${PROFILE}" --mode "${MODE}")"
echo "${OUTPUT}"

AVG="$(printf '%s\n' "${OUTPUT}" | rg -o 'score\.avg=[0-9]+(\.[0-9]+)?' | head -n1 | cut -d= -f2)"
P95="$(printf '%s\n' "${OUTPUT}" | rg -o 'score\.p95=[0-9]+' | head -n1 | cut -d= -f2)"

if [[ -z "${AVG}" || -z "${P95}" ]]; then
  echo "[bot-gate] failed to parse benchmark output" >&2
  exit 1
fi

awk -v avg="${AVG}" -v min_avg="${MIN_AVG}" 'BEGIN { if (avg < min_avg) exit 1 }' || {
  echo "[bot-gate] avg score gate failed: avg=${AVG} < min=${MIN_AVG}" >&2
  exit 1
}

if (( P95 < MIN_P95 )); then
  echo "[bot-gate] p95 score gate failed: p95=${P95} < min=${MIN_P95}" >&2
  exit 1
fi

echo "[bot-gate] passed avg=${AVG} p95=${P95}"
