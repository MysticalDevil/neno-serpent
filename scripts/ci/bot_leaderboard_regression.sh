#!/usr/bin/env bash
set -euo pipefail

# Purpose: thin shell wrapper for leaderboard regression.
# Heavy parsing/aggregation lives in Python for readability.

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

exec uv run python "${ROOT_DIR}/scripts/ci/bot_leaderboard_regression.py" \
  --root-dir "${ROOT_DIR}" \
  --build-dir "${BUILD_DIR}" \
  --suite-file "${SUITE_FILE}" \
  --profile "${PROFILE}" \
  --result-file "${RESULT_FILE}" \
  --summary-file "${SUMMARY_FILE}" \
  --compare-file "${COMPARE_FILE}" \
  --ml-model "${ML_MODEL_PATH}" \
  --require-no-regression "${REQUIRE_NO_REGRESSION}"
