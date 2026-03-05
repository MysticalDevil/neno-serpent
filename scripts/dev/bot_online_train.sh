#!/usr/bin/env bash
set -euo pipefail

# Purpose: continuous training loop for ml-online runtime hot reload.
# Example:
#   ./scripts/dev.sh bot-online-train --workspace cache/dev/nenoserpent_bot_online

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
WORKSPACE="${BOT_ONLINE_WORKSPACE:-${TMP_ROOT}/nenoserpent_bot_online}"
PROFILE="${BOT_ONLINE_PROFILE:-dev}"
INTERVAL_SEC="${BOT_ONLINE_INTERVAL_SEC:-20}"
MAX_SAMPLES_PER_CASE="${BOT_ONLINE_MAX_SAMPLES_PER_CASE:-6}"
EPOCHS="${BOT_ONLINE_EPOCHS:-8}"
BATCH_SIZE="${BOT_ONLINE_BATCH_SIZE:-192}"
LR="${BOT_ONLINE_LR:-0.001}"
SEED_BASE="${BOT_ONLINE_SEED_BASE:-20260306}"
GATE_GAMES="${BOT_ONLINE_GATE_GAMES:-24}"
GATE_MAX_TICKS="${BOT_ONLINE_GATE_MAX_TICKS:-1600}"
GATE_LEVEL="${BOT_ONLINE_GATE_LEVEL:-0}"
GATE_MODE="${BOT_ONLINE_GATE_MODE:-balanced}"
GATE_NO_REGRESSION_EPS="${BOT_ONLINE_GATE_NO_REGRESSION_EPS:-0.0}"
CACHE_MAX_MB="${BOT_ONLINE_CACHE_MAX_MB:-1024}"
CACHE_TARGET_MB="${BOT_ONLINE_CACHE_TARGET_MB:-768}"

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
    --interval-sec)
      INTERVAL_SEC="$2"
      shift 2
      ;;
    --max-samples-per-case)
      MAX_SAMPLES_PER_CASE="$2"
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
      GATE_NO_REGRESSION_EPS="$2"
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
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh bot-online-train [options]

Options:
  --workspace <dir>             Output workspace (default: cache/dev/nenoserpent_bot_online)
  --profile <debug|dev|release> Dataset generation profile (default: dev)
  --interval-sec <N>            Delay between rounds (default: 20)
  --max-samples-per-case <N>    Dataset cap per leaderboard case (default: 6)
  --epochs <N>                  Train epochs per round (default: 8)
  --batch-size <N>              Train batch size per round (default: 192)
  --lr <float>                  Learning rate per round (default: 0.001)
  --gate-games <N>              Benchmark games for publish gate (default: 24)
  --gate-max-ticks <N>          Benchmark max ticks for publish gate (default: 1600)
  --gate-level <N>              Benchmark level for publish gate (default: 0)
  --gate-mode <name>            Benchmark mode for publish gate (default: balanced)
  --gate-eps <float>            Allowed regression epsilon (default: 0.0)
  --cache-max-mb <N>            Workspace high-water mark in MB (default: 1024)
  --cache-target-mb <N>         Prune target size in MB when high-water exceeded (default: 768)

Notes:
  - Keep this running in terminal A.
  - Run game in terminal B with:
    ./scripts/dev.sh bot-run --backend ml-online --ml-model <workspace>/nenoserpent_bot_policy_runtime.json
  - Runtime will hot-reload the model when this loop publishes a newer JSON file.
EOF
        exit 0
      fi
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

mkdir -p "${WORKSPACE}"
DATASET_PATH="${WORKSPACE}/nenoserpent_bot_dataset.csv"
MODEL_PATH="${WORKSPACE}/nenoserpent_bot_policy.pt"
METADATA_PATH="${WORKSPACE}/nenoserpent_bot_policy_meta.json"
RUNTIME_JSON_PATH="${WORKSPACE}/nenoserpent_bot_policy_runtime.json"
ROUND=0

extract_score_pair() {
  local text="$1"
  local score_line
  score_line="$(printf '%s\n' "${text}" | rg '^\[bot-benchmark\] score\.max=' || true)"
  if [[ -z "${score_line}" ]]; then
    echo "nan nan"
    return 1
  fi
  local avg p95
  avg="$(printf '%s\n' "${score_line}" | awk '{
    for (i=1; i<=NF; ++i) if ($i ~ /^score\.avg=/) { sub(/^score\.avg=/, "", $i); print $i; exit }
  }')"
  p95="$(printf '%s\n' "${score_line}" | awk '{
    for (i=1; i<=NF; ++i) if ($i ~ /^score\.p95=/) { sub(/^score\.p95=/, "", $i); print $i; exit }
  }')"
  echo "${avg} ${p95}"
}

run_gate_benchmark() {
  local model_path="$1"
  local seed="$2"
  "${ROOT_DIR}/scripts/dev.sh" bot-benchmark \
    --profile "${PROFILE}" \
    --mode "${GATE_MODE}" \
    --backend ml \
    --ml-model "${model_path}" \
    --games "${GATE_GAMES}" \
    --max-ticks "${GATE_MAX_TICKS}" \
    --level "${GATE_LEVEL}" \
    --seed "${seed}"
}

workspace_size_mb() {
  du -sm "${WORKSPACE}" | awk '{print $1}'
}

prune_workspace_if_needed() {
  local current_mb
  current_mb="$(workspace_size_mb)"
  if [[ "${current_mb}" -lt "${CACHE_MAX_MB}" ]]; then
    return
  fi
  echo "[bot-online-train] cache high-water hit sizeMb=${current_mb} maxMb=${CACHE_MAX_MB}; pruning to <=${CACHE_TARGET_MB}MB"
  while [[ "${current_mb}" -gt "${CACHE_TARGET_MB}" ]]; do
    local oldest_file
    oldest_file="$(find "${WORKSPACE}" -maxdepth 1 -type f \
      ! -name 'nenoserpent_bot_policy_runtime.json' \
      ! -name '.gitignore' \
      -printf '%T@ %p\n' | sort -n | awk 'NR==1 {print $2}')"
    if [[ -z "${oldest_file}" ]]; then
      echo "[bot-online-train] prune stopped: no removable files in workspace"
      break
    fi
    rm -f "${oldest_file}"
    current_mb="$(workspace_size_mb)"
    echo "[bot-online-train] pruned file=${oldest_file} sizeMb=${current_mb}"
  done
}

echo "[bot-online-train] workspace=${WORKSPACE}"
echo "[bot-online-train] profile=${PROFILE} intervalSec=${INTERVAL_SEC} epochs=${EPOCHS} batchSize=${BATCH_SIZE}"
echo "[bot-online-train] gate games=${GATE_GAMES} maxTicks=${GATE_MAX_TICKS} level=${GATE_LEVEL} mode=${GATE_MODE} eps=${GATE_NO_REGRESSION_EPS}"
echo "[bot-online-train] cache maxMb=${CACHE_MAX_MB} targetMb=${CACHE_TARGET_MB}"

while true; do
  ROUND=$((ROUND + 1))
  ROUND_SEED=$((SEED_BASE + ROUND))
  TMP_RUNTIME_JSON="${WORKSPACE}/.runtime.round-${ROUND}.json"
  echo "[bot-online-train] round=${ROUND} phase=dataset"
  "${ROOT_DIR}/scripts/dev.sh" bot-dataset \
    --profile "${PROFILE}" \
    --max-samples-per-case "${MAX_SAMPLES_PER_CASE}" \
    --output "${DATASET_PATH}"

  echo "[bot-online-train] round=${ROUND} phase=train seed=${ROUND_SEED}"
  "${ROOT_DIR}/scripts/dev.sh" bot-train \
    --dataset "${DATASET_PATH}" \
    --model "${MODEL_PATH}" \
    --metadata "${METADATA_PATH}" \
    --runtime-json "${TMP_RUNTIME_JSON}" \
    --epochs "${EPOCHS}" \
    --batch-size "${BATCH_SIZE}" \
    --lr "${LR}" \
    --seed "${ROUND_SEED}"

  SHOULD_PUBLISH=1
  if [[ -f "${RUNTIME_JSON_PATH}" ]]; then
    echo "[bot-online-train] round=${ROUND} phase=gate baseline=current candidate=new"
    BASELINE_OUTPUT="$(run_gate_benchmark "${RUNTIME_JSON_PATH}" "${ROUND_SEED}" || true)"
    CANDIDATE_OUTPUT="$(run_gate_benchmark "${TMP_RUNTIME_JSON}" "${ROUND_SEED}" || true)"
    read -r BASELINE_AVG BASELINE_P95 < <(extract_score_pair "${BASELINE_OUTPUT}")
    read -r CANDIDATE_AVG CANDIDATE_P95 < <(extract_score_pair "${CANDIDATE_OUTPUT}")
    echo "[bot-online-train] round=${ROUND} gate baseline.avg=${BASELINE_AVG} baseline.p95=${BASELINE_P95} candidate.avg=${CANDIDATE_AVG} candidate.p95=${CANDIDATE_P95}"
    if ! awk -v cand="${CANDIDATE_AVG}" -v base="${BASELINE_AVG}" -v eps="${GATE_NO_REGRESSION_EPS}" \
      'BEGIN { exit ((cand + eps) >= base ? 0 : 1) }'; then
      SHOULD_PUBLISH=0
      echo "[bot-online-train] round=${ROUND} gate=blocked reason=avg-regression"
    fi
    if ! awk -v cand="${CANDIDATE_P95}" -v base="${BASELINE_P95}" -v eps="${GATE_NO_REGRESSION_EPS}" \
      'BEGIN { exit ((cand + eps) >= base ? 0 : 1) }'; then
      SHOULD_PUBLISH=0
      echo "[bot-online-train] round=${ROUND} gate=blocked reason=p95-regression"
    fi
  fi

  if [[ "${SHOULD_PUBLISH}" == "1" ]]; then
    mv "${TMP_RUNTIME_JSON}" "${RUNTIME_JSON_PATH}"
    echo "[bot-online-train] round=${ROUND} published=${RUNTIME_JSON_PATH}"
  else
    rm -f "${TMP_RUNTIME_JSON}"
    echo "[bot-online-train] round=${ROUND} kept-current=${RUNTIME_JSON_PATH}"
  fi
  prune_workspace_if_needed
  sleep "${INTERVAL_SEC}"
done
