#!/bin/sh
# Builds VinaText.icns from res/app.ico - the SAME artwork the Windows build
# ships as IDR_MAINFRAME, so there is one icon in the repository and not two
# that drift.
#
# WHY A SCRIPT AND NOT A COMMITTED .icns: a generated binary in the tree is a
# second copy of the artwork that nothing keeps in step. Redraw app.ico and this
# follows on the next build; commit an .icns and it silently does not.
#
# sips and iconutil both ship with macOS, so this adds no dependency. It is only
# ever run on macOS - the bundle is the only thing that consumes an .icns.
#
# Usage: make_macos_icon.sh <path/to/app.ico> <output.icns>
set -eu

SOURCE="$1"
OUTPUT="$2"
WORK="$(dirname "$OUTPUT")/VinaText.iconset"

rm -rf "$WORK"
mkdir -p "$WORK"

BASE="$(dirname "$OUTPUT")/icon-base.png"
sips -s format png "$SOURCE" --out "$BASE" >/dev/null

# 16, 32 and 128 with their @2x partners, plus a plain 256. NOTHING ABOVE 256:
# the artwork in app.ico is 256x256, and asking for 512 or 1024 would upscale it
# - a blurry Dock icon looks worse than a small sharp one, and macOS is happy to
# scale down from the largest slot it is given.
for SIZE in 16 32 128; do
	sips -z "$SIZE" "$SIZE" "$BASE" --out "$WORK/icon_${SIZE}x${SIZE}.png" >/dev/null
	DOUBLE=$((SIZE * 2))
	sips -z "$DOUBLE" "$DOUBLE" "$BASE" --out "$WORK/icon_${SIZE}x${SIZE}@2x.png" >/dev/null
done
sips -z 256 256 "$BASE" --out "$WORK/icon_256x256.png" >/dev/null

iconutil -c icns "$WORK" -o "$OUTPUT"
rm -rf "$WORK" "$BASE"
