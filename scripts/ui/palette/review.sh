#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
FOCUS_SCRIPT="${FOCUS_SCRIPT:-${ROOT_DIR}/scripts/ui/palette/focus.sh}"
MATRIX_SCRIPT="${MATRIX_SCRIPT:-${ROOT_DIR}/scripts/ui/palette/matrix.sh}"
HTML_RENDER_SCRIPT="${HTML_RENDER_SCRIPT:-${ROOT_DIR}/scripts/ui/palette/render_html.sh}"
# shellcheck source=lib/script_common.sh
# shellcheck disable=SC1091
source "${ROOT_DIR}/scripts/lib/script_common.sh"

TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/ui}}"
mkdir -p "${TMP_ROOT}"
OUT_DIR="${1:-${TMP_ROOT}/nenoserpent_palette_review}"
HTML_OUT="${HTML_OUT:-${OUT_DIR}/index.html}"
FOCUS_TARGETS="${FOCUS_TARGETS:-menu,game,dbg-replay-buff,dbg-choice}"
MATRIX_TARGETS="${MATRIX_TARGETS:-menu,achievements,catalog,icons}"
PALETTES="${PALETTES:-0,1,2,3,4}"
POST_NAV_WAIT="${POST_NAV_WAIT:-1.8}"
MONTAGE_FONT="${MONTAGE_FONT:-$(fc-match -f '%{file}\n' 'DejaVu Sans' | head -n 1)}"

script_require_cmds bash fc-match montage identify

mkdir -p "${OUT_DIR}/focus" "${OUT_DIR}/matrix" "${OUT_DIR}/sheets"

script_split_csv_trimmed "${PALETTES}" PALETTE_LIST
script_split_csv_trimmed "${FOCUS_TARGETS}" FOCUS_LIST
script_split_csv_trimmed "${MATRIX_TARGETS}" MATRIX_LIST

for palette in "${PALETTE_LIST[@]}"; do
  echo "[round] focus palette=${palette}"
  PALETTE_STEPS="${palette}" TARGETS="${FOCUS_TARGETS}" POST_NAV_WAIT="${POST_NAV_WAIT}" \
    "${FOCUS_SCRIPT}" "${OUT_DIR}/focus"

  focus_inputs=()
  for target in "${FOCUS_LIST[@]}"; do
    focus_inputs+=("${OUT_DIR}/focus/nenoserpent_${target}_p${palette}.png")
  done
  montage -font "${MONTAGE_FONT}" "${focus_inputs[@]}" -tile 2x -geometry +8+8 "${OUT_DIR}/sheets/focus_p${palette}.png"
done

TARGETS="${MATRIX_TARGETS}" PALETTES="${PALETTES}" POST_NAV_WAIT="${POST_NAV_WAIT}" \
  "${MATRIX_SCRIPT}" "${OUT_DIR}/matrix"

for target in "${MATRIX_LIST[@]}"; do
  matrix_inputs=()
  for palette in "${PALETTE_LIST[@]}"; do
    matrix_inputs+=("${OUT_DIR}/matrix/palette_${palette}_${target}.png")
  done
  montage -font "${MONTAGE_FONT}" "${matrix_inputs[@]}" -tile 5x -geometry +8+8 "${OUT_DIR}/sheets/matrix_${target}.png"
done

bash "${HTML_RENDER_SCRIPT}" "${HTML_OUT}" "${FOCUS_TARGETS}" "${MATRIX_TARGETS}" "${PALETTES}"

echo "[ok] Review capture complete: ${OUT_DIR}"
for sheet in "${OUT_DIR}"/sheets/*.png; do
  dims="$(identify -format '%wx%h' "${sheet}")"
  echo "[ok] $(basename "${sheet}") ${dims}"
done
echo "[ok] $(basename "${HTML_OUT}")"
