#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CACHE_ROOT="${NENOSERPENT_CACHE_DIR:-${PROJECT_ROOT}/cache}"
mkdir -p "${CACHE_ROOT}/deploy"
# shellcheck source=lib/build_paths.sh
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/scripts/lib/build_paths.sh"

: "${QT_ANDROID_PREFIX:?Please set QT_ANDROID_PREFIX, e.g. ~/qt-toolchains/build-qt-android/build-android-arm64/qt-android-install}"

QT_CMAKE_BIN="${QT_CMAKE_BIN:-${QT_ANDROID_PREFIX}/bin/qt-cmake}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${HOME}/Android/Sdk}"
BUILD_DIR="${BUILD_DIR:-$(resolve_build_dir android)}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-MinSizeRel}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-28}"
QT_HOST_PATH="${QT_HOST_PATH:-}"
QT_HOST_INFO_DIR="${QT_HOST_INFO_DIR:-}"
APP_ID="${APP_ID:-org.devil.nenoserpent}"
INSTALL_TO_DEVICE="${INSTALL_TO_DEVICE:-1}"
LAUNCH_AFTER_INSTALL="${LAUNCH_AFTER_INSTALL:-1}"
GRADLE_TIMEOUT_SECS="${GRADLE_TIMEOUT_SECS:-0}"
OUTPUT_APK="${OUTPUT_APK:-${PROJECT_ROOT}/dist/android/${APP_ID}-${ANDROID_ABI}.apk}"

log_phase() {
  echo "[phase] $*"
}

ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-}"
if [[ -z "${ANDROID_NDK_ROOT}" ]]; then
  if [[ -d "${ANDROID_SDK_ROOT}/ndk" ]]; then
    ANDROID_NDK_ROOT="$(find "${ANDROID_SDK_ROOT}/ndk" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -n1)"
  fi
fi

if [[ -z "${ANDROID_NDK_ROOT}" ]]; then
  echo "[error] ANDROID_NDK_ROOT is not set and no NDK found under ${ANDROID_SDK_ROOT}/ndk"
  exit 1
fi

if [[ -z "${QT_HOST_PATH}" ]]; then
  # Common for source-built Qt trees where host tools live next to qt-android-install.
  if [[ -d "$(dirname "${QT_ANDROID_PREFIX}")/qtbase" ]]; then
    QT_HOST_PATH="$(dirname "${QT_ANDROID_PREFIX}")/qtbase"
  else
    QT_HOST_PATH="${QT_ANDROID_PREFIX}"
  fi
fi

if [[ -z "${QT_HOST_INFO_DIR}" ]]; then
  QT_HOST_INFO_DIR="${QT_HOST_PATH}/lib/cmake/Qt6HostInfo"
fi

if [[ -z "${JAVA_HOME:-}" ]]; then
  if command -v javac >/dev/null 2>&1; then
    JAVAC_REAL="$(readlink -f "$(command -v javac)")"
    JAVA_HOME="$(cd "$(dirname "${JAVAC_REAL}")/.." && pwd)"
    export JAVA_HOME
  fi
fi

if [[ -n "${JAVA_HOME:-}" ]]; then
  export PATH="${JAVA_HOME}/bin:${PATH}"
fi

for cmd in cmake ninja keytool; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "[error] Missing command: ${cmd}"
    exit 1
  fi
done

if [[ ! -x "${QT_CMAKE_BIN}" ]]; then
  echo "[error] qt-cmake not found: ${QT_CMAKE_BIN}"
  exit 1
fi

if [[ ! -d "${ANDROID_SDK_ROOT}" ]]; then
  echo "[error] Android SDK not found: ${ANDROID_SDK_ROOT}"
  exit 1
fi

if [[ ! -d "${ANDROID_NDK_ROOT}" ]]; then
  echo "[error] Android NDK not found: ${ANDROID_NDK_ROOT}"
  exit 1
fi

echo "[info] Project: ${PROJECT_ROOT}"
echo "[info] QT_ANDROID_PREFIX=${QT_ANDROID_PREFIX}"
echo "[info] QT_HOST_PATH=${QT_HOST_PATH}"
echo "[info] ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT}"
echo "[info] ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}"
echo "[info] JAVA_HOME=${JAVA_HOME:-<not set>}"

log_phase "configure (qt-cmake)"
"${QT_CMAKE_BIN}" \
  -S "${PROJECT_ROOT}" \
  -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
  -DANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}" \
  -DANDROID_NDK="${ANDROID_NDK_ROOT}" \
  -DANDROID_ABI="${ANDROID_ABI}" \
  -DANDROID_PLATFORM="${ANDROID_PLATFORM}" \
  -DQT_HOST_PATH="${QT_HOST_PATH}" \
  -DQt6HostInfo_DIR="${QT_HOST_INFO_DIR}" \
  -DNENOSERPENT_OPTIMIZE_SIZE=ON

log_phase "build native target"
# Build native target first so updated QML/resources are definitely linked.
cmake --build "${BUILD_DIR}" --target NenoSerpent --parallel
log_phase "build apk target"
# Keep compatibility with existing Qt-generated packaging entry.
cmake --build "${BUILD_DIR}" --target apk --parallel

android_build_candidates=(
  "${BUILD_DIR}/src/android-build"
  "${BUILD_DIR}/android-build"
)
ANDROID_BUILD_DIR=""
for candidate in "${android_build_candidates[@]}"; do
  if [[ -d "${candidate}" ]]; then
    ANDROID_BUILD_DIR="${candidate}"
    break
  fi
done
if [[ -z "${ANDROID_BUILD_DIR}" ]]; then
  while IFS= read -r candidate; do
    ANDROID_BUILD_DIR="${candidate}"
    break
  done < <(find "${BUILD_DIR}" -type d -name 'android-build' | sort)
fi
if [[ -z "${ANDROID_BUILD_DIR}" ]]; then
  gradle_wrapper_path="$(find "${BUILD_DIR}" -type f -path '*/android-build/gradlew' | head -n1)"
  if [[ -n "${gradle_wrapper_path}" ]]; then
    ANDROID_BUILD_DIR="$(dirname "${gradle_wrapper_path}")"
  fi
fi
if [[ -z "${ANDROID_BUILD_DIR}" ]]; then
  echo "[warn] Android build dir not found. checked:"
  for candidate in "${android_build_candidates[@]}"; do
    echo "  - ${candidate}"
  done
  echo "[warn] discovered android-build dirs under ${BUILD_DIR}:"
  find "${BUILD_DIR}" -type d -name 'android-build' | sort || true
fi

if [[ -n "${ANDROID_BUILD_DIR}" ]]; then
  if [[ -d "${PROJECT_ROOT}/android/res" ]]; then
    log_phase "overlay android resources"
    mkdir -p "${ANDROID_BUILD_DIR}/res"
    # Overlay project resources only. Do not wipe Qt-generated values/arrays.
    cp -r "${PROJECT_ROOT}/android/res/." "${ANDROID_BUILD_DIR}/res/"
  fi
  if [[ -f "${ANDROID_BUILD_DIR}/gradle.properties" ]]; then
    if rg -q '^androidPackageName=' "${ANDROID_BUILD_DIR}/gradle.properties"; then
      sed -i "s/^androidPackageName=.*/androidPackageName=${APP_ID}/" \
        "${ANDROID_BUILD_DIR}/gradle.properties"
    else
      printf '\nandroidPackageName=%s\n' "${APP_ID}" >>"${ANDROID_BUILD_DIR}/gradle.properties"
    fi
  fi

  build_type_lc="$(echo "${CMAKE_BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
  case "${build_type_lc}" in
    release|minsizerel)
      gradle_task="assembleRelease"
      ;;
    *)
      gradle_task="assembleDebug"
      ;;
  esac

  log_phase "gradle ${gradle_task}"
  gradle_cmd=("${ANDROID_BUILD_DIR}/gradlew" --no-daemon --console=plain -p "${ANDROID_BUILD_DIR}" "${gradle_task}")
  if [[ "${GRADLE_TIMEOUT_SECS}" =~ ^[0-9]+$ ]] && [[ "${GRADLE_TIMEOUT_SECS}" -gt 0 ]]; then
    timeout --preserve-status "${GRADLE_TIMEOUT_SECS}" "${gradle_cmd[@]}"
  else
    "${gradle_cmd[@]}"
  fi
else
  build_type_lc="$(echo "${CMAKE_BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
  echo "[warn] skip gradle assemble: android-build dir unavailable; trying direct APK discovery"
fi
apk_roots=(
  "${BUILD_DIR}/build/outputs/apk"
  "${BUILD_DIR}/android-build/build/outputs/apk"
  "${BUILD_DIR}/src/android-build/build/outputs/apk"
)
if [[ -n "${ANDROID_BUILD_DIR}" ]]; then
  apk_roots=(
    "${ANDROID_BUILD_DIR}/build/outputs/apk"
    "${apk_roots[@]}"
  )
fi
apk_output_dirs=()
for root in "${apk_roots[@]}"; do
  if [[ -d "${root}/${build_type_lc}" ]]; then
    apk_output_dirs+=("${root}/${build_type_lc}")
  fi
  if [[ -d "${root}/release" ]]; then
    apk_output_dirs+=("${root}/release")
  fi
  if [[ -d "${root}/debug" ]]; then
    apk_output_dirs+=("${root}/debug")
  fi
done

UNSIGNED_APK=""
for dir in "${apk_output_dirs[@]}"; do
  UNSIGNED_APK="$(find "${dir}" -maxdepth 1 -type f -name '*-unsigned.apk' | head -n1)"
  if [[ -n "${UNSIGNED_APK}" ]]; then
    break
  fi
done

if [[ -z "${UNSIGNED_APK}" ]]; then
  for root in "${apk_roots[@]}"; do
    if [[ -d "${root}" ]]; then
      UNSIGNED_APK="$(find "${root}" -type f -name '*-unsigned.apk' | head -n1)"
      if [[ -n "${UNSIGNED_APK}" ]]; then
        break
      fi
    fi
  done
fi

SIGNED_INPUT_APK=""
if [[ -n "${ANDROID_BUILD_DIR}" ]]; then
  SIGNED_INPUT_APK="$(find "${ANDROID_BUILD_DIR}" -maxdepth 2 -type f -name '*.apk' ! -name '*-unsigned.apk' | head -n1)"
fi
if [[ -z "${UNSIGNED_APK}" ]]; then
  for root in "${apk_roots[@]}"; do
    if [[ -d "${root}" ]]; then
      SIGNED_INPUT_APK="$(find "${root}" -type f -name '*.apk' ! -name '*-unsigned.apk' | head -n1)"
      if [[ -n "${SIGNED_INPUT_APK}" ]]; then
        break
      fi
    fi
  done
fi

if [[ -z "${UNSIGNED_APK}" && -z "${SIGNED_INPUT_APK}" ]]; then
  SIGNED_INPUT_APK="$(find "${BUILD_DIR}" -type f -name '*.apk' ! -name '*-unsigned.apk' | head -n1)"
fi

if [[ -z "${UNSIGNED_APK}" && -z "${SIGNED_INPUT_APK}" ]]; then
  echo "[error] APK not found under Android build output roots:"
  for root in "${apk_roots[@]}"; do
    echo "  - ${root}"
  done
  exit 1
fi

ANDROID_BUILD_TOOLS_DIR="${ANDROID_BUILD_TOOLS_DIR:-}"
if [[ -z "${ANDROID_BUILD_TOOLS_DIR}" ]]; then
  ANDROID_BUILD_TOOLS_DIR="$(find "${ANDROID_SDK_ROOT}/build-tools" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -n1)"
fi

ZIPALIGN_BIN="${ZIPALIGN_BIN:-${ANDROID_BUILD_TOOLS_DIR}/zipalign}"
APKSIGNER_BIN="${APKSIGNER_BIN:-${ANDROID_BUILD_TOOLS_DIR}/apksigner}"
ADB_BIN="${ADB_BIN:-${ANDROID_SDK_ROOT}/platform-tools/adb}"

for tool in "${ZIPALIGN_BIN}" "${APKSIGNER_BIN}"; do
  if [[ ! -x "${tool}" ]]; then
    echo "[error] Missing Android build tool: ${tool}"
    exit 1
  fi
done

DEBUG_KEYSTORE_PATH="${DEBUG_KEYSTORE_PATH:-${HOME}/.android/debug.keystore}"
DEBUG_KEY_ALIAS="${DEBUG_KEY_ALIAS:-androiddebugkey}"
DEBUG_KEYSTORE_PASS="${DEBUG_KEYSTORE_PASS:-android}"
DEBUG_KEY_PASS="${DEBUG_KEY_PASS:-android}"

if [[ ! -f "${DEBUG_KEYSTORE_PATH}" ]]; then
  mkdir -p "$(dirname "${DEBUG_KEYSTORE_PATH}")"
  keytool -genkeypair -v \
    -keystore "${DEBUG_KEYSTORE_PATH}" \
    -storepass "${DEBUG_KEYSTORE_PASS}" \
    -alias "${DEBUG_KEY_ALIAS}" \
    -keypass "${DEBUG_KEY_PASS}" \
    -dname "CN=Android Debug,O=Android,C=US" \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 >/dev/null 2>&1
fi

if [[ -n "${SIGNED_INPUT_APK}" ]]; then
  log_phase "reuse signed apk from gradle output"
  SIGNED_APK="${SIGNED_APK:-${SIGNED_INPUT_APK}}"
else
  log_phase "zipalign + apksigner"
  ALIGNED_APK="${ALIGNED_APK:-${CACHE_ROOT}/deploy/${APP_ID}-${ANDROID_ABI}-aligned.apk}"
  SIGNED_APK="${SIGNED_APK:-${CACHE_ROOT}/deploy/${APP_ID}-${ANDROID_ABI}.apk}"
  "${ZIPALIGN_BIN}" -f 4 "${UNSIGNED_APK}" "${ALIGNED_APK}"
  "${APKSIGNER_BIN}" sign \
    --ks "${DEBUG_KEYSTORE_PATH}" \
    --ks-pass "pass:${DEBUG_KEYSTORE_PASS}" \
    --key-pass "pass:${DEBUG_KEY_PASS}" \
    --ks-key-alias "${DEBUG_KEY_ALIAS}" \
    --out "${SIGNED_APK}" \
    "${ALIGNED_APK}"
  "${APKSIGNER_BIN}" verify "${SIGNED_APK}"
fi

mkdir -p "$(dirname "${OUTPUT_APK}")"
cp -f "${SIGNED_APK}" "${OUTPUT_APK}"
SIGNED_APK="${OUTPUT_APK}"

echo "[info] Signed APK: ${SIGNED_APK}"

if [[ "${INSTALL_TO_DEVICE}" == "1" ]]; then
  log_phase "install to device"
  if [[ ! -x "${ADB_BIN}" ]]; then
    echo "[error] adb not found: ${ADB_BIN}"
    exit 1
  fi
  "${ADB_BIN}" install -r "${SIGNED_APK}"
  if [[ "${LAUNCH_AFTER_INSTALL}" == "1" ]]; then
    "${ADB_BIN}" shell monkey -p "${APP_ID}" -c android.intent.category.LAUNCHER 1 >/dev/null
    echo "[info] Launched: ${APP_ID}"
  fi
fi
