#!/bin/sh
# Turns the built .app into something you can hand to somebody: Qt copied in,
# signed, and wrapped in a .dmg.
#
# Run it against a bundle built with -DVINATEXT_MACOS_BUNDLE=ON.
#
# Usage: package_macos.sh <path/to/VinaText.app> <output-directory>
set -eu

APP="$1"
OUTDIR="$2"
QTBIN="${QTBIN:-$(brew --prefix qt)/bin}"
QTPLUGINS="${QTPLUGINS:-$(brew --prefix qt)/share/qt/plugins}"

mkdir -p "$OUTDIR"

echo "==> macdeployqt"
"$QTBIN/macdeployqt" "$APP"

# THE OFFSCREEN PLATFORM PLUGIN, on purpose. macdeployqt ships only cocoa, which
# is right for a user - but it means the SHIPPED artifact cannot be run headless,
# and checking the thing we hand out (rather than the thing we built) is the only
# way to know the bundle is sound. 79KB buys `--selftest` against the real .app.
if [ -f "$QTPLUGINS/platforms/libqoffscreen.dylib" ]; then
	cp "$QTPLUGINS/platforms/libqoffscreen.dylib" "$APP/Contents/PlugIns/platforms/"
fi

# AD-HOC SIGNING IS NOT OPTIONAL ON APPLE SILICON. macdeployqt rewrites every
# binary with install_name_tool, which invalidates the signature the linker put
# there; arm64 macOS then KILLS the process on launch. Measured: exit 137
# (SIGKILL) before this line existed, and the failure says nothing about
# signatures - it just dies.
#
# This is ad-hoc (-s -), NOT a Developer ID. Gatekeeper still blocks a download
# signed this way; see doc/PORTING.md 6x for what the real thing costs.
echo "==> codesign (ad-hoc)"
codesign --force --deep --sign - "$APP"
codesign --verify --verbose=1 "$APP"

# D3: the LGPL obligation is about the artifact, not the build. Qt must be
# LINKED, not baked in, and the notices must travel with it.
echo "==> D3 checks"
BIN="$APP/Contents/MacOS/$(basename "$APP" .app)"
if ! otool -L "$BIN" | grep -q "@executable_path/../Frameworks/QtCore.framework"; then
	echo "FATAL: QtCore is not linked from the bundle's own Frameworks" >&2
	exit 1
fi
for NOTICE in License-Qt.txt License-Scintilla.txt License-VinaText.txt; do
	if [ ! -f "$APP/Contents/Resources/license/$NOTICE" ]; then
		echo "FATAL: $NOTICE is missing from the bundle" >&2
		exit 1
	fi
done

echo "==> dmg"
NAME="$(basename "$APP" .app)"
DMG="$OUTDIR/$NAME.dmg"
rm -f "$DMG"
STAGE="$(mktemp -d)"
cp -R "$APP" "$STAGE/"
# The /Applications alias is what makes the window a drag-and-drop installer
# rather than a folder the user has to work out.
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname "$NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
rm -rf "$STAGE"

echo "==> wrote $DMG"
