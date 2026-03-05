#!/usr/bin/env bash
set -euo pipefail

# Purpose: collect gcovr JSON summary for a build directory.
# Usage:
#   ./scripts/ci/coverage_collect.sh <build_dir> <output_json>

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <build_dir> <output_json>" >&2
  exit 1
fi

BUILD_DIR="$1"
OUTPUT_JSON="$2"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ "${BUILD_DIR}" != /* ]]; then
  BUILD_DIR="${ROOT_DIR}/${BUILD_DIR}"
fi
if [[ "${OUTPUT_JSON}" != /* ]]; then
  OUTPUT_JSON="${ROOT_DIR}/${OUTPUT_JSON}"
fi

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "[coverage] missing build dir: ${BUILD_DIR}" >&2
  exit 1
fi

mkdir -p "$(dirname "${OUTPUT_JSON}")"

cleanup_orphan_coverage_artifacts() {
  local gcno_path=""
  local stem=""
  local rel_source=""
  local source_path=""
  local source_path_src=""
  local removed_gcno=0
  local removed_gcda=0
  local gcda_path=""

  while IFS= read -r -d '' gcno_path; do
    stem="${gcno_path%.gcno}"
    if [[ "${stem}" != *".dir/"* ]]; then
      continue
    fi
    rel_source="${stem##*.dir/}"
    if [[ "${rel_source}" == "${stem}" ]]; then
      continue
    fi
    case "${rel_source}" in
      *.c|*.cc|*.cpp|*.cxx) ;;
      *) continue ;;
    esac
    source_path="${ROOT_DIR}/${rel_source}"
    source_path_src="${ROOT_DIR}/src/${rel_source}"
    if [[ ! -f "${source_path}" && -f "${source_path_src}" ]]; then
      source_path="${source_path_src}"
    fi
    if [[ ! -f "${source_path}" ]]; then
      rm -f "${gcno_path}"
      removed_gcno=$((removed_gcno + 1))
      if [[ -f "${stem}.gcda" ]]; then
        rm -f "${stem}.gcda"
        removed_gcda=$((removed_gcda + 1))
      fi
    fi
  done < <(find "${BUILD_DIR}" -type f -name "*.gcno" -print0)

  while IFS= read -r -d '' gcda_path; do
    stem="${gcda_path%.gcda}"
    if [[ ! -f "${stem}.gcno" ]]; then
      rm -f "${gcda_path}"
      removed_gcda=$((removed_gcda + 1))
    fi
  done < <(find "${BUILD_DIR}" -type f -name "*.gcda" -print0)

  if (( removed_gcno > 0 || removed_gcda > 0 )); then
    echo "[coverage] cleaned orphan artifacts: gcno=${removed_gcno} gcda=${removed_gcda}"
  fi
}

GCOVR_BIN="${GCOVR_BIN:-}"
GCOVR_SOURCE="unset"
GCOVR_PREFIX=()
if [[ -n "${GCOVR_BIN}" ]]; then
  if [[ ! -x "${GCOVR_BIN}" ]]; then
    echo "[coverage] GCOVR_BIN is set but not executable: ${GCOVR_BIN}" >&2
    exit 1
  fi
  GCOVR_SOURCE="env"
elif command -v uv >/dev/null 2>&1 && uv run --no-sync gcovr --version >/dev/null 2>&1; then
  GCOVR_PREFIX=(uv run --no-sync)
  GCOVR_BIN="gcovr"
  GCOVR_SOURCE="uv-run"
else
  GCOVR_BIN="$(command -v gcovr || true)"
  GCOVR_SOURCE="system"
fi

if [[ -z "${GCOVR_BIN}" ]]; then
  echo "[coverage] missing gcovr: expected uv-managed env gcovr or PATH gcovr" >&2
  exit 1
fi

GCOV_EXECUTABLE="${GCOV_EXECUTABLE:-/usr/lib/llvm/21/bin/llvm-cov gcov}"
GCOV_WORKDIR="${GCOV_WORKDIR:-${ROOT_DIR}/cache/coverage/gcov}"
mkdir -p "${GCOV_WORKDIR}"
cleanup_orphan_coverage_artifacts
(
  cd "${GCOV_WORKDIR}"
  "${GCOVR_PREFIX[@]}" "${GCOVR_BIN}" \
    --root "${ROOT_DIR}" \
    --object-directory "${BUILD_DIR}" \
    --filter "${ROOT_DIR}/src" \
    --exclude ".*\\.h$" \
    --exclude "${ROOT_DIR}/src/tools" \
    --gcov-executable "${GCOV_EXECUTABLE}" \
    --json-summary-pretty \
    --output "${OUTPUT_JSON}"
)

echo "[coverage] gcovr=${GCOVR_BIN}"
echo "[coverage] gcovr_source=${GCOVR_SOURCE}"
echo "[coverage] gcov_workdir=${GCOV_WORKDIR}"
echo "[coverage] wrote summary: ${OUTPUT_JSON}"
