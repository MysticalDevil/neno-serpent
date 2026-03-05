#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
mkdir -p "${TMP_ROOT}"
SOURCE_FOREGROUND="${ROOT_DIR}/android/res/icon-src/snake-foreground.png"
RES_DIR="${ROOT_DIR}/android/res"
PLAYSTORE_DIR="${ROOT_DIR}/android/playstore"
ICON_BG_COLOR="${ICON_BG_COLOR:-#DBE88C}"

if [[ ! -f "${SOURCE_FOREGROUND}" ]]; then
  echo "error: source icon not found: ${SOURCE_FOREGROUND}" >&2
  exit 1
fi

if ! command -v magick >/dev/null 2>&1; then
  echo "error: ImageMagick 'magick' is required" >&2
  exit 1
fi

mkdir -p "${PLAYSTORE_DIR}"
TMP_DIR="$(mktemp -d "${TMP_ROOT}/nenoserpent-icons-XXXXXX")"
trap 'rm -rf "${TMP_DIR}"' EXIT

FG_CLEAN="${TMP_DIR}/snake-foreground-clean.png"
FG_MASK="${TMP_DIR}/snake-foreground-mask.png"

prepare_foreground() {
  # Remove low-alpha halo/noise so both normal and themed icons are crisp.
  magick "${SOURCE_FOREGROUND}" \
    -alpha set \
    \( +clone -alpha extract -threshold 10% -morphology Open Square:1 \) \
    -compose CopyOpacity -composite \
    "${FG_CLEAN}"

  magick "${FG_CLEAN}" -alpha extract -threshold 1% "${FG_MASK}"
}

prepare_foreground

declare -A densities=(
  [mipmap-mdpi]=48
  [mipmap-hdpi]=72
  [mipmap-xhdpi]=96
  [mipmap-xxhdpi]=144
  [mipmap-xxxhdpi]=192
)

render_reference_icon() {
  local size="$1"
  local shape="$2"
  local out="$3"
  local fg_size
  fg_size=$((size * 74 / 100))

  if [[ "${shape}" == "round" ]]; then
    magick -size "${size}x${size}" xc:"${ICON_BG_COLOR}" \
      \( "${FG_CLEAN}" -resize "${fg_size}x${fg_size}" \) \
      -gravity center -compose over -composite \
      -alpha set \
      \( -size "${size}x${size}" xc:none -fill white -draw "circle $((size / 2)),$((size / 2)) $((size / 2)),0" \) \
      -compose DstIn -composite \
      "${out}"
    return
  fi

  magick -size "${size}x${size}" xc:"${ICON_BG_COLOR}" \
    \( "${FG_CLEAN}" -resize "${fg_size}x${fg_size}" \) \
    -gravity center -compose over -composite \
    "${out}"
}

render_adaptive_layers() {
  local layer_dir="${RES_DIR}/drawable-nodpi"
  mkdir -p "${layer_dir}"
  local adaptive_canvas=432
  local fg_safe_size=292

  magick -size 432x432 xc:"${ICON_BG_COLOR}" "${layer_dir}/ic_launcher_background_square_layer.png"
  magick -size 432x432 xc:"${ICON_BG_COLOR}" "${layer_dir}/ic_launcher_background_layer.png"
  magick -size "${adaptive_canvas}x${adaptive_canvas}" xc:none \
    \( "${FG_CLEAN}" -resize "${fg_safe_size}x${fg_safe_size}" \) \
    -gravity center -compose over -composite \
    "${layer_dir}/ic_launcher_foreground_layer.png"

  # Themed icon (Monet): monochrome mask derived from foreground alpha.
  magick -size "${adaptive_canvas}x${adaptive_canvas}" xc:black \
    \( "${FG_MASK}" -resize "${fg_safe_size}x${fg_safe_size}" \) \
    -gravity center -compose over -composite \
    -threshold 1% \
    -transparent black \
    -fill white \
    -opaque white \
    "${layer_dir}/ic_launcher_monochrome_layer.png"
}

for density_dir in "${!densities[@]}"; do
  icon_size="${densities[${density_dir}]}"
  target_dir="${RES_DIR}/${density_dir}"
  mkdir -p "${target_dir}"
  render_reference_icon "${icon_size}" "square" "${target_dir}/ic_launcher.png"
  render_reference_icon "${icon_size}" "round" "${target_dir}/ic_launcher_round.png"
done

# Play Store asset: square, flat background + foreground.
render_reference_icon 512 "square" "${PLAYSTORE_DIR}/ic_launcher_512.png"
render_adaptive_layers

echo "generated Android launcher icons (square/round), adaptive layers, and playstore 512px asset"
