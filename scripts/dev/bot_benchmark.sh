#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

BUILD_PRESET="${BUILD_PRESET:-dev}"
SKIP_BUILD="${NENOSERPENT_SKIP_BUILD:-0}"

if [[ "${SKIP_BUILD}" != "1" ]]; then
  cmake --preset "${BUILD_PRESET}"
  cmake --build --preset "${BUILD_PRESET}" --target bot-benchmark
fi

exec "${ROOT_DIR}/build/${BUILD_PRESET}/bot-benchmark" "$@"
