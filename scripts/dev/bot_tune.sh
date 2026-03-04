#!/usr/bin/env bash
set -euo pipefail

# Purpose: build bot-benchmark and run offline bot tuning.
# Example:
#   ./scripts/dev.sh bot-tune --mode balanced --iterations 60 --output /tmp/bot_tuned.json

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_PRESET="${BUILD_PRESET:-dev}"
PROFILE="${BOT_TUNE_PROFILE:-debug}"
MODE="${BOT_TUNE_MODE:-balanced}"
LEVELS="${BOT_TUNE_LEVELS:-0,1}"
SEEDS="${BOT_TUNE_SEEDS:-1337,1438,1539,1640}"
GAMES="${BOT_TUNE_GAMES:-20}"
MAX_TICKS="${BOT_TUNE_MAX_TICKS:-2400}"
ITERATIONS="${BOT_TUNE_ITERATIONS:-60}"
OUTPUT_PATH="${BOT_TUNE_OUTPUT:-/tmp/nenoserpent_bot_tuned_${MODE}.json}"
REPORT_PATH="${BOT_TUNE_REPORT:-/tmp/nenoserpent_bot_tune_report_${MODE}.json}"
RANDOM_SEED="${BOT_TUNE_RANDOM_SEED:-20260304}"

while (($# > 0)); do
  case "$1" in
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --mode)
      MODE="$2"
      shift 2
      ;;
    --levels)
      LEVELS="$2"
      shift 2
      ;;
    --seeds)
      SEEDS="$2"
      shift 2
      ;;
    --games)
      GAMES="$2"
      shift 2
      ;;
    --max-ticks)
      MAX_TICKS="$2"
      shift 2
      ;;
    --iterations)
      ITERATIONS="$2"
      shift 2
      ;;
    --output)
      OUTPUT_PATH="$2"
      shift 2
      ;;
    --report)
      REPORT_PATH="$2"
      shift 2
      ;;
    --seed)
      RANDOM_SEED="$2"
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

exec python3 "${ROOT_DIR}/scripts/dev/bot_tune.py" \
  --benchmark-bin "${ROOT_DIR}/build/${BUILD_PRESET}/bot-benchmark" \
  --strategy-file "${ROOT_DIR}/src/adapter/bot/strategy_profiles.json" \
  --profile "${PROFILE}" \
  --mode "${MODE}" \
  --levels "${LEVELS}" \
  --seeds "${SEEDS}" \
  --games "${GAMES}" \
  --max-ticks "${MAX_TICKS}" \
  --iterations "${ITERATIONS}" \
  --output "${OUTPUT_PATH}" \
  --report "${REPORT_PATH}" \
  --seed "${RANDOM_SEED}"
