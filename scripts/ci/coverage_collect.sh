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

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "[coverage] missing build dir: ${BUILD_DIR}" >&2
  exit 1
fi

mkdir -p "$(dirname "${OUTPUT_JSON}")"

gcovr \
  --root "${ROOT_DIR}" \
  --object-directory "${BUILD_DIR}" \
  --filter "${ROOT_DIR}/src" \
  --json-summary-pretty \
  --output "${OUTPUT_JSON}"

echo "[coverage] wrote summary: ${OUTPUT_JSON}"
