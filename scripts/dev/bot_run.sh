#!/usr/bin/env bash
set -euo pipefail

# Purpose: run NenoSerpent in bot-oriented manual mode (headful/headless switchable).
# Example:
#   ./scripts/dev.sh bot-run --backend rule --headful
#   ./scripts/dev.sh bot-run --backend ml --ml-model cache/dev/policy.runtime.json --ui-mode screen
#   ./scripts/dev.sh bot-run --backend search --headful

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_PRESET="${BUILD_PRESET:-debug}"
SKIP_BUILD="${NENOSERPENT_SKIP_BUILD:-0}"
HEADFUL=1
UI_MODE="full"
BOT_BACKEND="${BOT_BACKEND:-off}"
ML_MODEL_PATH="${BOT_ML_MODEL:-}"
HUMAN_DATASET_PATH="${NENOSERPENT_BOT_HUMAN_DATASET:-}"
LEVEL_INDEX="${BOT_LEVEL_INDEX:-0}"
DECISION_POLICY="${NENOSERPENT_BOT_DECISION_POLICY:-balanced}"

while (($# > 0)); do
  case "$1" in
    --build-preset)
      BUILD_PRESET="$2"
      shift 2
      ;;
    --headful)
      HEADFUL=1
      shift
      ;;
    --headless)
      HEADFUL=0
      shift
      ;;
    --ui-mode)
      UI_MODE="$2"
      shift 2
      ;;
    --backend)
      BOT_BACKEND="$2"
      shift 2
      ;;
    --ml-model)
      ML_MODEL_PATH="$2"
      shift 2
      ;;
    --human-dataset)
      HUMAN_DATASET_PATH="$2"
      shift 2
      ;;
    --level)
      LEVEL_INDEX="$2"
      shift 2
      ;;
    --decision-policy)
      DECISION_POLICY="$2"
      shift 2
      ;;
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-run [--build-preset <preset>] [--backend off|human|rule|ml|ml-online|search]
                           [--headful|--headless] [--ui-mode full|screen]
                           [--ml-model <runtime-json>] [--human-dataset <dataset-csv>] [--level <index>]
                           [--decision-policy aggressive|balanced|conservative]

Notes:
  - shell ui-mode is debug-only and is intentionally disabled for bot-run.
  - backend=ml and backend=ml-online require --ml-model.
  - backend=human records manual direction samples to --human-dataset
    (default: cache/dev/nenoserpent_human_dataset.csv).
  - --level sets initial menu level index by injecting SELECT N times before START.
  - --decision-policy controls bot decision style (default: balanced).
EOF
        exit 0
      fi
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${UI_MODE}" != "full" && "${UI_MODE}" != "screen" ]]; then
  echo "invalid --ui-mode: ${UI_MODE} (expected full|screen; shell is debug-only)" >&2
  exit 1
fi
if [[ "${BOT_BACKEND}" != "off" && "${BOT_BACKEND}" != "human" && "${BOT_BACKEND}" != "rule" && "${BOT_BACKEND}" != "ml" &&
  "${BOT_BACKEND}" != "ml-online" && "${BOT_BACKEND}" != "search" ]]; then
  echo "invalid --backend: ${BOT_BACKEND} (expected off|human|rule|ml|ml-online|search)" >&2
  exit 1
fi
if ! [[ "${LEVEL_INDEX}" =~ ^[0-9]+$ ]]; then
  echo "invalid --level: ${LEVEL_INDEX} (expected non-negative integer)" >&2
  exit 1
fi
if [[ "${DECISION_POLICY}" != "aggressive" && "${DECISION_POLICY}" != "balanced" &&
  "${DECISION_POLICY}" != "conservative" ]]; then
  echo "invalid --decision-policy: ${DECISION_POLICY} (expected aggressive|balanced|conservative)" >&2
  exit 1
fi
if [[ ("${BOT_BACKEND}" == "ml" || "${BOT_BACKEND}" == "ml-online") && -z "${ML_MODEL_PATH}" ]]; then
  echo "backend=${BOT_BACKEND} requires --ml-model <runtime-json-path>" >&2
  exit 1
fi
if [[ "${BOT_BACKEND}" == "human" && -z "${HUMAN_DATASET_PATH}" ]]; then
  HUMAN_DATASET_PATH="${NENOSERPENT_TMP_DIR:-${ROOT_DIR}/cache/dev}/nenoserpent_human_dataset.csv"
fi

if [[ "${SKIP_BUILD}" != "1" ]]; then
  cmake --preset "${BUILD_PRESET}"
  cmake --build --preset "${BUILD_PRESET}" --target NenoSerpent
fi

APP_PATH="${ROOT_DIR}/build/${BUILD_PRESET}/NenoSerpent"
if [[ ! -x "${APP_PATH}" ]]; then
  echo "missing executable: ${APP_PATH}" >&2
  exit 1
fi

export NENOSERPENT_BOT_BACKEND="${BOT_BACKEND}"
export NENOSERPENT_BOT_DECISION_POLICY="${DECISION_POLICY}"
if [[ -n "${ML_MODEL_PATH}" ]]; then
  export NENOSERPENT_BOT_ML_MODEL="${ML_MODEL_PATH}"
fi
if [[ -n "${HUMAN_DATASET_PATH}" ]]; then
  export NENOSERPENT_BOT_HUMAN_DATASET="${HUMAN_DATASET_PATH}"
fi

INPUT_ENDPOINT="${NENOSERPENT_INPUT_FILE:-${NENOSERPENT_TMP_DIR:-${ROOT_DIR}/cache/dev}/nenoserpent-input.pipe}"
export NENOSERPENT_INPUT_FILE="${INPUT_ENDPOINT}"

inject_level_tokens() {
  local endpoint="$1"
  local level="$2"
  if (( level <= 0 )); then
    return
  fi
  for _ in $(seq 1 "${level}"); do
    sleep 0.25
    "${ROOT_DIR}/scripts/input.sh" inject -p "${endpoint}" SELECT || true
  done
}

wait_for_input_endpoint() {
  local endpoint="$1"
  for _ in $(seq 1 80); do
    if [[ -p "${endpoint}" || -f "${endpoint}" ]]; then
      return 0
    fi
    sleep 0.05
  done
  return 1
}

if [[ "${HEADFUL}" == "1" ]]; then
  "${APP_PATH}" "--ui-mode=${UI_MODE}" &
  APP_PID=$!
  if wait_for_input_endpoint "${INPUT_ENDPOINT}"; then
    inject_level_tokens "${INPUT_ENDPOINT}" "${LEVEL_INDEX}"
  fi
  wait "${APP_PID}"
  exit $?
fi

env QT_QPA_PLATFORM=offscreen NENOSERPENT_INPUT_FILE="${INPUT_ENDPOINT}" \
  "${APP_PATH}" "--ui-mode=${UI_MODE}" &
APP_PID=$!

# In headless mode, kick one START to move bot out of menu state.
if wait_for_input_endpoint "${INPUT_ENDPOINT}"; then
  inject_level_tokens "${INPUT_ENDPOINT}" "${LEVEL_INDEX}"
  # Splash may still be active; pulse START a few times so menu->playing transition is guaranteed.
  (
    for _ in $(seq 1 6); do
      sleep 0.5
      "${ROOT_DIR}/scripts/input.sh" inject -p "${INPUT_ENDPOINT}" START || true
    done
  ) &
fi

wait "${APP_PID}"
