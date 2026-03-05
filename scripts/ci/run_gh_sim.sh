#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-ci}"
ROOT_DIR="/workspace"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache}"
mkdir -p "${CACHE_ROOT}/ci"

if [[ ! -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
  echo "[error] CMakeLists.txt not found under ${ROOT_DIR}"
  exit 1
fi

cd "${ROOT_DIR}"

case "${MODE}" in
  ci)
    cmake --preset debug --fresh
    cmake --build --preset debug
    QT_QPA_PLATFORM=offscreen xvfb-run -a ctest --preset debug --output-on-failure
    ;;
  cd)
    cmake --preset release --fresh
    cmake --build --preset release

    mkdir -p dist/linux
    cp -f build/release/NenoSerpent dist/linux/
    cp -f linux/org.devil.nenoserpent.desktop dist/linux/ || true
    cp -f src/qml/icon.svg dist/linux/nenoserpent.svg || true

    mkdir -p dist
    echo "WASM build requires Qt WASM toolchain in CI. Provide QT_WASM_PREFIX and emsdk to enable." \
      > dist/wasm-placeholder.txt
    echo "Android build requires Qt Android toolchain, SDK/NDK, and signing; provide secrets to enable." \
      > dist/android-placeholder.txt
    ;;
  cd-android-preflight)
    missing=0
    for env_name in ANDROID_KEYSTORE_B64 ANDROID_KEYSTORE_PASS ANDROID_KEY_PASS ANDROID_KEY_ALIAS; do
      if [[ -z "${!env_name:-}" ]]; then
        echo "[error] missing env: ${env_name}"
        missing=1
      fi
    done
    if [[ "${missing}" -ne 0 ]]; then
      exit 1
    fi

    mkdir -p "${CACHE_ROOT}/ci/gh-android-secrets"
    echo "${ANDROID_KEYSTORE_B64}" | base64 -d > "${CACHE_ROOT}/ci/gh-android-secrets/nenoserpent-release.jks"
    if [[ ! -s "${CACHE_ROOT}/ci/gh-android-secrets/nenoserpent-release.jks" ]]; then
      echo "[error] decoded keystore is empty"
      exit 1
    fi

    echo "[ok] Android signing secrets decoded"
    echo "[info] alias=${ANDROID_KEY_ALIAS}"
    echo "[info] keystore=${CACHE_ROOT}/ci/gh-android-secrets/nenoserpent-release.jks"
    echo "[info] This preflight container does not include full Android Qt toolchain."
    ;;
  *)
    echo "Usage: run-gh-sim [ci|cd|cd-android-preflight]"
    exit 1
    ;;
esac
