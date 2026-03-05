#!/usr/bin/env bash
set -euo pipefail

# Purpose: reproducible validation gate for ml-online training/publish loop.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
WORKSPACE="${BOT_ML_ONLINE_GATE_WORKSPACE:-${TMP_ROOT}/nenoserpent_bot_ml_online_gate}"
PROFILE="${BOT_ML_ONLINE_GATE_PROFILE:-dev}"
ROUNDS="${BOT_ML_ONLINE_GATE_ROUNDS:-3}"
INTERVAL_SEC="${BOT_ML_ONLINE_GATE_INTERVAL_SEC:-0}"
GATE_GAMES="${BOT_ML_ONLINE_GATE_GAMES:-16}"
GATE_MAX_TICKS="${BOT_ML_ONLINE_GATE_MAX_TICKS:-1200}"
GATE_LEVEL="${BOT_ML_ONLINE_GATE_LEVEL:-0}"
GATE_MODE="${BOT_ML_ONLINE_GATE_MODE:-balanced}"
GATE_EPS="${BOT_ML_ONLINE_GATE_EPS:-0.0}"

while (($# > 0)); do
  case "$1" in
    --workspace)
      WORKSPACE="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --rounds)
      ROUNDS="$2"
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
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-ml-online-gate [options]

Options:
  --workspace <dir>      Output workspace (default: cache/dev/nenoserpent_bot_ml_online_gate)
  --profile <name>       Dataset profile (default: dev)
  --rounds <N>           Online-train rounds to execute (default: 3)
  --gate-games <N>       Per-round publish gate games (default: 16)
  --gate-max-ticks <N>   Per-round publish gate max ticks (default: 1200)
  --gate-level <N>       Gate level index (default: 0)
  --gate-mode <name>     Gate strategy mode (default: balanced)
  --gate-eps <float>     No-regression tolerance (default: 0.0)

Pass conditions:
  - summary exists
  - rounds >= requested rounds
  - published >= 1
  - runtime_json file exists
EOF
        exit 0
      fi
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

if ! uv run python -c "import torch" >/dev/null 2>&1; then
  echo "[bot-ml-online-gate] missing dependency: torch" >&2
  echo "[bot-ml-online-gate] install with: uv add --dev torch" >&2
  exit 1
fi

mkdir -p "${WORKSPACE}"
SUMMARY_PATH="${WORKSPACE}/online.summary.env"

echo "[bot-ml-online-gate] phase=online-train rounds=${ROUNDS}"
"${ROOT_DIR}/scripts/dev.sh" bot-online-train \
  --workspace "${WORKSPACE}" \
  --profile "${PROFILE}" \
  --max-rounds "${ROUNDS}" \
  --interval-sec "${INTERVAL_SEC}" \
  --gate-games "${GATE_GAMES}" \
  --gate-max-ticks "${GATE_MAX_TICKS}" \
  --gate-level "${GATE_LEVEL}" \
  --gate-mode "${GATE_MODE}" \
  --gate-eps "${GATE_EPS}" \
  --summary "${SUMMARY_PATH}"

if [[ ! -f "${SUMMARY_PATH}" ]]; then
  echo "[bot-ml-online-gate] missing summary: ${SUMMARY_PATH}" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "${SUMMARY_PATH}"
rounds="${rounds:-0}"
published="${published:-0}"
blocked="${blocked:-0}"
last_reason="${last_reason:-unknown}"
runtime_json="${runtime_json:-}"
echo "[bot-ml-online-gate] rounds=${rounds} published=${published} blocked=${blocked} last_reason=${last_reason}"

if [[ "${rounds}" -lt "${ROUNDS}" ]]; then
  echo "[bot-ml-online-gate] rounds check failed: rounds=${rounds} expected>=${ROUNDS}" >&2
  exit 1
fi
if [[ "${published}" -lt 1 ]]; then
  echo "[bot-ml-online-gate] publish check failed: published=${published}" >&2
  exit 1
fi
if [[ -z "${runtime_json}" || ! -f "${runtime_json}" ]]; then
  echo "[bot-ml-online-gate] runtime_json missing: ${runtime_json:-<empty>}" >&2
  exit 1
fi

echo "[bot-ml-online-gate] passed runtime_json=${runtime_json}"
