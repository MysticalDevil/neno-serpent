#!/usr/bin/env bash
set -euo pipefail

# Purpose: one-command launcher for ml-online gameplay + online trainer loop.
# Example:
#   ./scripts/dev.sh bot-online-run --workspace cache/dev/nenoserpent_bot_online

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEV_SH="${NENOSERPENT_DEV_SCRIPT:-${ROOT_DIR}/scripts/dev.sh}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
WORKSPACE="${BOT_ONLINE_WORKSPACE:-${TMP_ROOT}/nenoserpent_bot_online}"
BUILD_PRESET="${BUILD_PRESET:-dev}"
UI_MODE="${BOT_ONLINE_UI_MODE:-screen}"
PROFILE="${BOT_ONLINE_PROFILE:-dev}"
INTERVAL_SEC="${BOT_ONLINE_INTERVAL_SEC:-20}"
EPOCHS="${BOT_ONLINE_EPOCHS:-8}"
BATCH_SIZE="${BOT_ONLINE_BATCH_SIZE:-192}"
LR="${BOT_ONLINE_LR:-0.001}"
GATE_GAMES="${BOT_ONLINE_GATE_GAMES:-24}"
GATE_MAX_TICKS="${BOT_ONLINE_GATE_MAX_TICKS:-1600}"
GATE_LEVEL="${BOT_ONLINE_GATE_LEVEL:-0}"
GATE_MODE="${BOT_ONLINE_GATE_MODE:-balanced}"
GATE_EPS="${BOT_ONLINE_GATE_NO_REGRESSION_EPS:-0.0}"
CACHE_MAX_MB="${BOT_ONLINE_CACHE_MAX_MB:-1024}"
CACHE_TARGET_MB="${BOT_ONLINE_CACHE_TARGET_MB:-768}"
MIN_DATASET_SAMPLES="${BOT_ONLINE_MIN_DATASET_SAMPLES:-40}"
MIN_PUBLISH_ROUND="${BOT_ONLINE_MIN_PUBLISH_ROUND:-1}"
PUBLISH_COOLDOWN_ROUNDS="${BOT_ONLINE_PUBLISH_COOLDOWN_ROUNDS:-1}"
MAX_BLOCKED_STREAK="${BOT_ONLINE_MAX_BLOCKED_STREAK:-8}"

while (($# > 0)); do
  case "$1" in
    --workspace)
      WORKSPACE="$2"
      shift 2
      ;;
    --build-preset)
      BUILD_PRESET="$2"
      shift 2
      ;;
    --ui-mode)
      UI_MODE="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --interval-sec)
      INTERVAL_SEC="$2"
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
    --gate-games)
      GATE_GAMES="$2"
      shift 2
      ;;
    --gate-max-ticks)
      GATE_MAX_TICKS="$2"
      shift 2
      ;;
    --gate-level)
      GATE_LEVEL="$2"
      shift 2
      ;;
    --gate-mode)
      GATE_MODE="$2"
      shift 2
      ;;
    --gate-eps)
      GATE_EPS="$2"
      shift 2
      ;;
    --cache-max-mb)
      CACHE_MAX_MB="$2"
      shift 2
      ;;
    --cache-target-mb)
      CACHE_TARGET_MB="$2"
      shift 2
      ;;
    --min-dataset-samples)
      MIN_DATASET_SAMPLES="$2"
      shift 2
      ;;
    --min-publish-round)
      MIN_PUBLISH_ROUND="$2"
      shift 2
      ;;
    --publish-cooldown-rounds)
      PUBLISH_COOLDOWN_ROUNDS="$2"
      shift 2
      ;;
    --max-blocked-streak)
      MAX_BLOCKED_STREAK="$2"
      shift 2
      ;;
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-online-run [options]

Options:
  --workspace <dir>             Workspace for online trainer + runtime json
  --build-preset <name>         Build preset for game binary (default: dev)
  --ui-mode <full|screen>       UI mode for headful game run (default: screen)
  --profile <debug|dev|release> Trainer dataset profile (default: dev)
  --interval-sec <N>            Trainer round interval (default: 20)
  --epochs <N>                  Trainer epochs per round (default: 8)
  --batch-size <N>              Trainer batch size per round (default: 192)
  --lr <float>                  Trainer learning rate (default: 0.001)
  --gate-games <N>              Publish gate games (default: 24)
  --gate-max-ticks <N>          Publish gate max ticks (default: 1600)
  --gate-level <N>              Publish gate level index (default: 0)
  --gate-mode <name>            Publish gate mode (default: balanced)
  --gate-eps <float>            Publish gate regression tolerance (default: 0.0)
  --cache-max-mb <N>            Trainer workspace high-water mark MB (default: 1024)
  --cache-target-mb <N>         Trainer prune target MB (default: 768)
  --min-dataset-samples <N>     Trainer stability floor for dataset rows (default: 40)
  --min-publish-round <N>       Trainer warmup rounds before publish (default: 1)
  --publish-cooldown-rounds <N> Trainer minimum rounds between publishes (default: 1)
  --max-blocked-streak <N>      Trainer stop threshold for consecutive blocked rounds (default: 8)

Behavior:
  - starts bot-online-train in background
  - runs bot-run in foreground with backend=ml-online
  - stops trainer automatically when game exits
EOF
        exit 0
      fi
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${UI_MODE}" != "full" && "${UI_MODE}" != "screen" ]]; then
  echo "invalid --ui-mode: ${UI_MODE} (expected full|screen)" >&2
  exit 1
fi

mkdir -p "${WORKSPACE}"
RUNTIME_JSON_PATH="${WORKSPACE}/nenoserpent_bot_policy_runtime.json"
TRAINER_PID=0

cleanup() {
  if [[ "${TRAINER_PID}" -gt 0 ]] && kill -0 "${TRAINER_PID}" 2>/dev/null; then
    kill "${TRAINER_PID}" 2>/dev/null || true
    wait "${TRAINER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

echo "[bot-online-run] start trainer workspace=${WORKSPACE}"
"${DEV_SH}" bot-online-train \
  --workspace "${WORKSPACE}" \
  --profile "${PROFILE}" \
  --interval-sec "${INTERVAL_SEC}" \
  --epochs "${EPOCHS}" \
  --batch-size "${BATCH_SIZE}" \
  --lr "${LR}" \
  --gate-games "${GATE_GAMES}" \
  --gate-max-ticks "${GATE_MAX_TICKS}" \
  --gate-level "${GATE_LEVEL}" \
  --gate-mode "${GATE_MODE}" \
  --gate-eps "${GATE_EPS}" \
  --cache-max-mb "${CACHE_MAX_MB}" \
  --cache-target-mb "${CACHE_TARGET_MB}" \
  --min-dataset-samples "${MIN_DATASET_SAMPLES}" \
  --min-publish-round "${MIN_PUBLISH_ROUND}" \
  --publish-cooldown-rounds "${PUBLISH_COOLDOWN_ROUNDS}" \
  --max-blocked-streak "${MAX_BLOCKED_STREAK}" &
TRAINER_PID=$!

echo "[bot-online-run] start game backend=ml-online model=${RUNTIME_JSON_PATH}"
"${DEV_SH}" bot-run \
  --build-preset "${BUILD_PRESET}" \
  --backend ml-online \
  --ml-model "${RUNTIME_JSON_PATH}" \
  --headful \
  --ui-mode "${UI_MODE}"
