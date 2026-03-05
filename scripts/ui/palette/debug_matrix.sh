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
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}"
mkdir -p "${TMP_ROOT}"
OUT_DIR="${1:-${TMP_ROOT}/nenoserpent_debug_palette_matrix}"
TARGETS_CSV="${TARGETS:-splash,menu,dbg-catalog,dbg-achievements,dbg-icons,pause,dbg-gameover,dbg-replay,dbg-choice}"
PALETTES_CSV="${PALETTES:-0,1,2,3,4}"
ISOLATED_CONFIG="${ISOLATED_CONFIG:-1}"
POST_NAV_WAIT="${POST_NAV_WAIT:-1.8}"

script_require_cmds bash stat
capture_batch_require_script "${CAPTURE_SCRIPT}"

mkdir -p "${OUT_DIR}"

script_split_csv_trimmed "${TARGETS_CSV}" TARGETS
script_split_csv_trimmed "${PALETTES_CSV}" PALETTES

echo "[info] Out dir: ${OUT_DIR}"
echo "[info] Targets: ${TARGETS_CSV}"
echo "[info] Palettes: ${PALETTES_CSV}"

for palette in "${PALETTES[@]}"; do
  capture_batch_validate_palette_step "${palette}"

  for target in "${TARGETS[@]}"; do
    out_png="${OUT_DIR}/palette_${palette}_${target}.png"
    capture_batch_run_capture "${CAPTURE_SCRIPT}" "${target}" "${out_png}" "${palette}" \
      "${ISOLATED_CONFIG}" "${POST_NAV_WAIT}"
  done
done

echo "[ok] Debug palette capture matrix done."
ls -1 "${OUT_DIR}"/palette_*.png
