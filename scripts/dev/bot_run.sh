#!/usr/bin/env bash
set -euo pipefail

# Purpose: run NenoSerpent in bot-oriented manual mode (headful/headless switchable).
# Example:
#   ./scripts/dev.sh bot-run --backend rule --headful
#   ./scripts/dev.sh bot-run --backend ml --ml-model cache/dev/policy.runtime.json --ui-mode screen
#   ./scripts/dev.sh bot-run --backend search --headful

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_PRESET="${BUILD_PRESET:-dev}"
HEADFUL=1
UI_MODE="full"
BOT_BACKEND="${BOT_BACKEND:-off}"
ML_MODEL_PATH="${BOT_ML_MODEL:-}"

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
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-run [--build-preset <preset>] [--backend off|rule|ml|search]
                           [--headful|--headless] [--ui-mode full|screen]
                           [--ml-model <runtime-json>]

Notes:
  - shell ui-mode is debug-only and is intentionally disabled for bot-run.
  - backend=ml requires --ml-model.
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
if [[ "${BOT_BACKEND}" != "off" && "${BOT_BACKEND}" != "rule" && "${BOT_BACKEND}" != "ml" &&
  "${BOT_BACKEND}" != "search" ]]; then
  echo "invalid --backend: ${BOT_BACKEND} (expected off|rule|ml|search)" >&2
  exit 1
fi
if [[ "${BOT_BACKEND}" == "ml" && -z "${ML_MODEL_PATH}" ]]; then
  echo "backend=ml requires --ml-model <runtime-json-path>" >&2
  exit 1
fi

cmake --preset "${BUILD_PRESET}"
cmake --build --preset "${BUILD_PRESET}" --target NenoSerpent

APP_PATH="${ROOT_DIR}/build/${BUILD_PRESET}/NenoSerpent"
if [[ ! -x "${APP_PATH}" ]]; then
  echo "missing executable: ${APP_PATH}" >&2
  exit 1
fi

export NENOSERPENT_BOT_BACKEND="${BOT_BACKEND}"
if [[ -n "${ML_MODEL_PATH}" ]]; then
  export NENOSERPENT_BOT_ML_MODEL="${ML_MODEL_PATH}"
fi

if [[ "${HEADFUL}" == "1" ]]; then
  exec "${APP_PATH}" "--ui-mode=${UI_MODE}"
fi

INPUT_ENDPOINT="${NENOSERPENT_INPUT_FILE:-${NENOSERPENT_TMP_DIR:-${ROOT_DIR}/cache/dev}/nenoserpent-input.pipe}"

env QT_QPA_PLATFORM=offscreen NENOSERPENT_INPUT_FILE="${INPUT_ENDPOINT}" \
  "${APP_PATH}" "--ui-mode=${UI_MODE}" &
APP_PID=$!

# In headless mode, kick one START to move bot out of menu state.
for _ in $(seq 1 80); do
  if [[ -p "${INPUT_ENDPOINT}" || -f "${INPUT_ENDPOINT}" ]]; then
    break
  fi
  sleep 0.05
done
if [[ -p "${INPUT_ENDPOINT}" || -f "${INPUT_ENDPOINT}" ]]; then
  # Splash may still be active; pulse START a few times so menu->playing transition is guaranteed.
  (
    for _ in $(seq 1 6); do
      sleep 0.5
      "${ROOT_DIR}/scripts/input.sh" inject -p "${INPUT_ENDPOINT}" START || true
    done
  ) &
fi

wait "${APP_PID}"
