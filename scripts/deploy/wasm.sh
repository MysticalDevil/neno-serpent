#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${PROJECT_ROOT}/cache}"
mkdir -p "${CACHE_ROOT}"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/scripts/lib/build_paths.sh"

: "${QT_WASM_PREFIX:?Please set QT_WASM_PREFIX, e.g. ~/Qt/6.7.3/wasm_singlethread}"

QT_CMAKE_BIN="${QT_CMAKE_BIN:-${QT_WASM_PREFIX}/bin/qt-cmake}"
BUILD_DIR="${BUILD_DIR:-$(resolve_build_dir wasm)}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
DIST_DIR="${DIST_DIR:-${CACHE_ROOT}/deploy/neno-serpent-wasm-dist}"
WASM_SHIMS_DIR="${WASM_SHIMS_DIR:-${PROJECT_ROOT}/cmake/wasm-shims}"
EMSDK_ROOT="${EMSDK_ROOT:-${HOME}/qt-toolchains/emsdk}"
SERVE="${SERVE:-1}"
PORT="${PORT:-8080}"

if [[ -z "${EMSDK:-}" && -f "${EMSDK_ROOT}/emsdk_env.sh" ]]; then
    # shellcheck disable=SC1090,SC1091
    source "${EMSDK_ROOT}/emsdk_env.sh" >/dev/null
fi

for cmd in cmake ninja; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "[error] Missing command: ${cmd}"
        exit 1
    fi
done

if [[ "${SERVE}" == "1" ]] && ! command -v python3 >/dev/null 2>&1; then
    echo "[error] Missing command: python3"
    exit 1
fi

if [[ ! -x "${QT_CMAKE_BIN}" ]]; then
    echo "[error] qt-cmake not found: ${QT_CMAKE_BIN}"
    exit 1
fi

echo "[info] Project: ${PROJECT_ROOT}"
echo "[info] QT_WASM_PREFIX=${QT_WASM_PREFIX}"
echo "[info] BUILD_DIR=${BUILD_DIR}"
echo "[info] CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
echo "[info] WASM_SHIMS_DIR=${WASM_SHIMS_DIR}"

"${QT_CMAKE_BIN}" \
    -S "${PROJECT_ROOT}" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_MODULE_PATH="${WASM_SHIMS_DIR}" \
    -DNENOSERPENT_OPTIMIZE_SIZE=ON

cmake --build "${BUILD_DIR}" --target NenoSerpent --parallel

mkdir -p "${DIST_DIR}"
cp -f "${BUILD_DIR}/NenoSerpent."{html,js,wasm} "${DIST_DIR}/"
if [[ -f "${BUILD_DIR}/qtloader.js" ]]; then
    cp -f "${BUILD_DIR}/qtloader.js" "${DIST_DIR}/"
fi
if [[ -f "${BUILD_DIR}/NenoSerpent.data" ]]; then
    cp -f "${BUILD_DIR}/NenoSerpent.data" "${DIST_DIR}/"
fi
# Ensure spinner/logo and browser favicon resolve in local static hosting.
cp -f "${PROJECT_ROOT}/src/qml/icon.svg" "${DIST_DIR}/icon.svg"
cp -f "${PROJECT_ROOT}/src/qml/icon.svg" "${DIST_DIR}/qtlogo.svg"
cp -f "${PROJECT_ROOT}/src/qml/icon.svg" "${DIST_DIR}/favicon.ico"

# Inject explicit favicon to prevent browser default /favicon.ico 404 lookups.
if ! rg -q 'rel="icon"' "${DIST_DIR}/NenoSerpent.html"; then
    sed -i '/<title>NenoSerpent<\/title>/a \    <link rel="icon" type="image/svg+xml" href="icon.svg">' "${DIST_DIR}/NenoSerpent.html"
fi

echo "[info] WASM package copied to: ${DIST_DIR}"
echo "[info] Entry: ${DIST_DIR}/NenoSerpent.html"

if [[ "${SERVE}" == "1" ]]; then
    echo "[info] Starting local web server at http://127.0.0.1:${PORT}/NenoSerpent.html"
    echo "[info] Press Ctrl+C to stop"
    exec python3 "${PROJECT_ROOT}/scripts/deploy/wasm_serve.py" --host 127.0.0.1 --port "${PORT}" --dir "${DIST_DIR}"
fi
