#!/usr/bin/env bash
set -euo pipefail

# Purpose: prune repository cache directory to avoid unbounded growth.
# Example:
#   ./scripts/dev.sh cache-prune --max-mb 2048 --target-mb 1536

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache}"
MAX_MB="${NENOSERPENT_CACHE_MAX_MB:-2048}"
TARGET_MB="${NENOSERPENT_CACHE_TARGET_MB:-1536}"
MAX_AGE_DAYS="${NENOSERPENT_CACHE_MAX_AGE_DAYS:-14}"

while (($# > 0)); do
  case "$1" in
    --cache-dir)
      CACHE_ROOT="$2"
      shift 2
      ;;
    --max-mb)
      MAX_MB="$2"
      shift 2
      ;;
    --target-mb)
      TARGET_MB="$2"
      shift 2
      ;;
    --max-age-days)
      MAX_AGE_DAYS="$2"
      shift 2
      ;;
    *)
      if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        cat <<'EOF'
Usage:
  ./scripts/dev.sh cache-prune [--cache-dir path --max-mb N --target-mb N --max-age-days N]

Defaults:
  --cache-dir      cache
  --max-mb         2048
  --target-mb      1536
  --max-age-days   14

Behavior:
  1) Remove files older than max-age-days in cache dir.
  2) If size still exceeds max-mb, remove oldest files until <= target-mb.
EOF
        exit 0
      fi
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

if [[ ! -d "${CACHE_ROOT}" ]]; then
  echo "[cache-prune] cache dir missing: ${CACHE_ROOT}"
  exit 0
fi

if [[ "${TARGET_MB}" -gt "${MAX_MB}" ]]; then
  echo "[cache-prune] invalid thresholds: target-mb must be <= max-mb" >&2
  exit 1
fi

size_mb() {
  du -sm "${CACHE_ROOT}" | awk '{print $1}'
}

echo "[cache-prune] cache=${CACHE_ROOT} maxMb=${MAX_MB} targetMb=${TARGET_MB} maxAgeDays=${MAX_AGE_DAYS}"

find "${CACHE_ROOT}" -type f -mtime +"${MAX_AGE_DAYS}" ! -name '.gitignore' -delete

current_mb="$(size_mb)"
if [[ "${current_mb}" -le "${MAX_MB}" ]]; then
  echo "[cache-prune] sizeMb=${current_mb} (within limit)"
  exit 0
fi

echo "[cache-prune] sizeMb=${current_mb} exceeds maxMb=${MAX_MB}; pruning oldest files"
while [[ "${current_mb}" -gt "${TARGET_MB}" ]]; do
  oldest_file="$(find "${CACHE_ROOT}" -type f ! -name '.gitignore' -printf '%T@ %p\n' \
    | sort -n | awk 'NR==1 {print $2}')"
  if [[ -z "${oldest_file}" ]]; then
    echo "[cache-prune] stop: no removable files left"
    break
  fi
  rm -f "${oldest_file}"
  current_mb="$(size_mb)"
  echo "[cache-prune] pruned=${oldest_file} sizeMb=${current_mb}"
done

echo "[cache-prune] done sizeMb=${current_mb}"
