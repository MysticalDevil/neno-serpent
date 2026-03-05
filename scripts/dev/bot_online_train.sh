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
MAX_ROUNDS="${BOT_ONLINE_MAX_ROUNDS:-0}"
SUMMARY_PATH="${BOT_ONLINE_SUMMARY_PATH:-}"
PUBLISH_HISTORY_PATH="${BOT_ONLINE_PUBLISH_HISTORY_PATH:-}"
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
    --max-rounds)
      MAX_ROUNDS="$2"
      shift 2
      ;;
    --summary)
      SUMMARY_PATH="$2"
      shift 2
      ;;
    --publish-history)
      PUBLISH_HISTORY_PATH="$2"
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
  --max-rounds <N>              Run finite rounds then exit (default: 0=infinite)
  --summary <path>              Optional summary output file (key=value lines)
  --publish-history <path>      Optional publish audit TSV output (default: <workspace>/publish_history.tsv)
  --min-dataset-samples <N>     Require at least N dataset rows per round (default: 40)
  --min-publish-round <N>       Do not publish before this round index (default: 1)
  --publish-cooldown-rounds <N> Min rounds between publishes (default: 1)
  --max-blocked-streak <N>      Stop loop when consecutive blocked rounds reach N (0=disable, default: 8)

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
PUBLISHED_COUNT=0
BLOCKED_COUNT=0
LAST_GATE_REASON="none"
LAST_PUBLISH_ROUND=0
BLOCKED_STREAK=0
MAX_BLOCKED_STREAK_SEEN=0
if [[ -z "${PUBLISH_HISTORY_PATH}" ]]; then
  PUBLISH_HISTORY_PATH="${WORKSPACE}/publish_history.tsv"
fi

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

file_sha256_or_na() {
  local file_path="$1"
  if [[ ! -f "${file_path}" ]]; then
    echo "na"
    return
  fi
  sha256sum "${file_path}" | awk '{print $1}'
}

ensure_publish_history_header() {
  if [[ -f "${PUBLISH_HISTORY_PATH}" ]]; then
    return
  fi
  mkdir -p "$(dirname "${PUBLISH_HISTORY_PATH}")"
  {
    printf 'round\tpublished\treason\tdataset_samples\tbaseline_avg\tbaseline_p95\tcandidate_avg\tcandidate_p95\tcandidate_sha256\tpublished_sha256\n'
  } > "${PUBLISH_HISTORY_PATH}"
}

append_publish_history_row() {
  local round="$1"
  local published="$2"
  local reason="$3"
  local dataset_samples="$4"
  local baseline_avg="$5"
  local baseline_p95="$6"
  local candidate_avg="$7"
  local candidate_p95="$8"
  local candidate_sha="$9"
  local published_sha="${10}"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${round}" "${published}" "${reason}" "${dataset_samples}" "${baseline_avg}" "${baseline_p95}" \
    "${candidate_avg}" "${candidate_p95}" "${candidate_sha}" "${published_sha}" >> "${PUBLISH_HISTORY_PATH}"
}

dataset_sample_count() {
  if [[ ! -f "${DATASET_PATH}" ]]; then
    echo "0"
    return
  fi
  awk 'END { print (NR > 0 ? NR - 1 : 0) }' "${DATASET_PATH}"
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
echo "[bot-online-train] maxRounds=${MAX_ROUNDS} summary=${SUMMARY_PATH:-<none>}"
echo "[bot-online-train] stability minSamples=${MIN_DATASET_SAMPLES} minPublishRound=${MIN_PUBLISH_ROUND} cooldownRounds=${PUBLISH_COOLDOWN_ROUNDS} maxBlockedStreak=${MAX_BLOCKED_STREAK}"
echo "[bot-online-train] publishHistory=${PUBLISH_HISTORY_PATH}"
ensure_publish_history_header

while true; do
  ROUND=$((ROUND + 1))
  ROUND_SEED=$((SEED_BASE + ROUND))
  TMP_RUNTIME_JSON="${WORKSPACE}/.runtime.round-${ROUND}.json"
  BASELINE_AVG="na"
  BASELINE_P95="na"
  CANDIDATE_AVG="na"
  CANDIDATE_P95="na"
  echo "[bot-online-train] round=${ROUND} phase=dataset"
  "${ROOT_DIR}/scripts/dev.sh" bot-dataset \
    --profile "${PROFILE}" \
    --max-samples-per-case "${MAX_SAMPLES_PER_CASE}" \
    --output "${DATASET_PATH}"
  SAMPLE_COUNT="$(dataset_sample_count)"
  echo "[bot-online-train] round=${ROUND} dataset.samples=${SAMPLE_COUNT}"

  SHOULD_PUBLISH=1
  if [[ "${SAMPLE_COUNT}" -lt "${MIN_DATASET_SAMPLES}" ]]; then
    SHOULD_PUBLISH=0
    LAST_GATE_REASON="insufficient-samples"
    echo "[bot-online-train] round=${ROUND} gate=blocked reason=insufficient-samples samples=${SAMPLE_COUNT} min=${MIN_DATASET_SAMPLES}"
  fi
  if [[ "${ROUND}" -lt "${MIN_PUBLISH_ROUND}" ]]; then
    SHOULD_PUBLISH=0
    LAST_GATE_REASON="warmup-round"
    echo "[bot-online-train] round=${ROUND} gate=blocked reason=warmup-round minRound=${MIN_PUBLISH_ROUND}"
  fi
  if [[ "${LAST_PUBLISH_ROUND}" -gt 0 && "${PUBLISH_COOLDOWN_ROUNDS}" -gt 0 ]]; then
    ROUNDS_SINCE_PUBLISH=$((ROUND - LAST_PUBLISH_ROUND))
    if [[ "${ROUNDS_SINCE_PUBLISH}" -lt "${PUBLISH_COOLDOWN_ROUNDS}" ]]; then
      SHOULD_PUBLISH=0
      LAST_GATE_REASON="publish-cooldown"
      echo "[bot-online-train] round=${ROUND} gate=blocked reason=publish-cooldown since=${ROUNDS_SINCE_PUBLISH} need=${PUBLISH_COOLDOWN_ROUNDS}"
    fi
  fi

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
      LAST_GATE_REASON="avg-regression"
      echo "[bot-online-train] round=${ROUND} gate=blocked reason=avg-regression"
    fi
    if ! awk -v cand="${CANDIDATE_P95}" -v base="${BASELINE_P95}" -v eps="${GATE_NO_REGRESSION_EPS}" \
      'BEGIN { exit ((cand + eps) >= base ? 0 : 1) }'; then
      SHOULD_PUBLISH=0
      LAST_GATE_REASON="p95-regression"
      echo "[bot-online-train] round=${ROUND} gate=blocked reason=p95-regression"
    fi
  fi

  if [[ "${SHOULD_PUBLISH}" == "1" ]]; then
    mv "${TMP_RUNTIME_JSON}" "${RUNTIME_JSON_PATH}"
    PUBLISHED_COUNT=$((PUBLISHED_COUNT + 1))
    LAST_GATE_REASON="published"
    LAST_PUBLISH_ROUND="${ROUND}"
    BLOCKED_STREAK=0
    CANDIDATE_SHA="$(file_sha256_or_na "${RUNTIME_JSON_PATH}")"
    append_publish_history_row \
      "${ROUND}" "1" "${LAST_GATE_REASON}" "${SAMPLE_COUNT}" \
      "${BASELINE_AVG}" "${BASELINE_P95}" "${CANDIDATE_AVG}" "${CANDIDATE_P95}" \
      "${CANDIDATE_SHA}" "${CANDIDATE_SHA}"
    echo "[bot-online-train] round=${ROUND} published=${RUNTIME_JSON_PATH}"
  else
    CANDIDATE_SHA="$(file_sha256_or_na "${TMP_RUNTIME_JSON}")"
    rm -f "${TMP_RUNTIME_JSON}"
    BLOCKED_COUNT=$((BLOCKED_COUNT + 1))
    BLOCKED_STREAK=$((BLOCKED_STREAK + 1))
    if [[ "${BLOCKED_STREAK}" -gt "${MAX_BLOCKED_STREAK_SEEN}" ]]; then
      MAX_BLOCKED_STREAK_SEEN="${BLOCKED_STREAK}"
    fi
    append_publish_history_row \
      "${ROUND}" "0" "${LAST_GATE_REASON}" "${SAMPLE_COUNT}" \
      "${BASELINE_AVG}" "${BASELINE_P95}" "${CANDIDATE_AVG}" "${CANDIDATE_P95}" \
      "${CANDIDATE_SHA}" "$(file_sha256_or_na "${RUNTIME_JSON_PATH}")"
    echo "[bot-online-train] round=${ROUND} kept-current=${RUNTIME_JSON_PATH}"
  fi
  prune_workspace_if_needed
  if [[ "${MAX_BLOCKED_STREAK}" -gt 0 && "${BLOCKED_STREAK}" -ge "${MAX_BLOCKED_STREAK}" ]]; then
    LAST_GATE_REASON="blocked-streak-limit"
    echo "[bot-online-train] stop reason=blocked-streak-limit streak=${BLOCKED_STREAK} limit=${MAX_BLOCKED_STREAK}" >&2
    break
  fi
  if [[ "${MAX_ROUNDS}" -gt 0 && "${ROUND}" -ge "${MAX_ROUNDS}" ]]; then
    break
  fi
  sleep "${INTERVAL_SEC}"
done

if [[ -n "${SUMMARY_PATH}" ]]; then
  mkdir -p "$(dirname "${SUMMARY_PATH}")"
  {
    echo "rounds=${ROUND}"
    echo "published=${PUBLISHED_COUNT}"
    echo "blocked=${BLOCKED_COUNT}"
    echo "runtime_json=${RUNTIME_JSON_PATH}"
    echo "last_reason=${LAST_GATE_REASON}"
    echo "last_publish_round=${LAST_PUBLISH_ROUND}"
    echo "max_blocked_streak=${MAX_BLOCKED_STREAK_SEEN}"
    echo "publish_history=${PUBLISH_HISTORY_PATH}"
  } > "${SUMMARY_PATH}"
  echo "[bot-online-train] summary=${SUMMARY_PATH}"
fi
