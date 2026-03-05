#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CAPTURE_SCRIPT="${CAPTURE_SCRIPT:-${ROOT_DIR}/scripts/ui/nav/capture.sh}"
# shellcheck source=lib/script_common.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/script_common.sh"
# shellcheck source=lib/capture_batch.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/capture_batch.sh"

PALETTE_STEP="${1:-0}"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}"
mkdir -p "${TMP_ROOT}"
OUT_DIR="${2:-${TMP_ROOT}/nenoserpent_palette_single}"
TARGETS_CSV="${3:-${TARGETS:-splash,menu,dbg-static-game,game,replay,dbg-choice,dbg-static-replay,dbg-catalog,dbg-achievements}}"
ISOLATED_CONFIG="${ISOLATED_CONFIG:-1}"
POST_NAV_WAIT="${POST_NAV_WAIT:-1.8}"

script_require_cmds bash stat
capture_batch_require_script "${CAPTURE_SCRIPT}"
capture_batch_validate_palette_step "${PALETTE_STEP}"

mkdir -p "${OUT_DIR}"

script_split_csv_trimmed "${TARGETS_CSV}" TARGETS

echo "[info] Out dir: ${OUT_DIR}"
echo "[info] Targets: ${TARGETS_CSV}"
echo "[info] Palette: ${PALETTE_STEP}"

for target in "${TARGETS[@]}"; do
  out_png="${OUT_DIR}/palette_${PALETTE_STEP}_${target}.png"
  capture_batch_run_capture "${CAPTURE_SCRIPT}" "${target}" "${out_png}" "${PALETTE_STEP}" \
    "${ISOLATED_CONFIG}" "${POST_NAV_WAIT}"
done

echo "[ok] Single-palette capture done."
ls -1 "${OUT_DIR}"/palette_"${PALETTE_STEP}"_*.png
