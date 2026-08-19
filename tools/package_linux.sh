#!/bin/sh
# Builds an AppDir from a plain ui-qt build, and - on Linux - wraps it in an
# AppImage.
#
# TWO STEPS ON PURPOSE. The AppDir is a prefix layout (usr/bin, usr/share) and
# can be assembled and TESTED anywhere, including on the macOS box this was
# written on; only the final appimagetool step needs Linux. Splitting them means
# the part that can be checked before CI is checked before CI, rather than the
# whole thing being taken on trust until a runner says otherwise.
#
# The layout is the THIRD candidate in ResourcePaths::Candidates -
# <exe>/../share/vinatext/<leaf> - which has existed since 6u and until now had
# never been exercised by anything.
#
# Usage: package_linux.sh <path/to/vinatext-qt> <output-directory>
set -eu

BINARY="$1"
OUTDIR="$2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APPDIR="$OUTDIR/VinaText.AppDir"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/vinatext" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$BINARY" "$APPDIR/usr/bin/vinatext-qt"
cp -R "$ROOT/Packages/data-packages" "$APPDIR/usr/share/vinatext/data"
cp -R "$ROOT/license" "$APPDIR/usr/share/vinatext/license"

# The icon, from the same res/app.ico the Windows and macOS builds use.
"$ROOT/tools/extract_ico_png.py" "$ROOT/res/app.ico" \
	"$APPDIR/usr/share/icons/hicolor/256x256/apps/vinatext.png"
# AppImage wants one at the AppDir root as well, named after the desktop entry.
cp "$APPDIR/usr/share/icons/hicolor/256x256/apps/vinatext.png" "$APPDIR/vinatext.png"

# NoDisplay is NOT set: this belongs in the menu. MimeType lists what the editor
# can actually lex - claiming every file type would be a promise the deferred
# viewers (D10) do not keep.
cat > "$APPDIR/usr/share/applications/vinatext.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=VinaText
GenericName=Text Editor
Comment=A lightweight text and source editor
Exec=vinatext-qt %F
Icon=vinatext
Terminal=false
Categories=Development;TextEditor;Utility;
MimeType=text/plain;text/x-c;text/x-c++;text/x-python;text/x-java;application/json;application/xml;
DESKTOP
cp "$APPDIR/usr/share/applications/vinatext.desktop" "$APPDIR/vinatext.desktop"

# AppRun. exec, not a subshell, so signals and the exit code reach the caller -
# a wrapper that swallows the exit status would make --selftest useless in CI.
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/vinatext-qt" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

echo "==> AppDir at $APPDIR"

# D3, against the layout rather than the build: the notices travel with it.
for NOTICE in License-Qt.txt License-Scintilla.txt License-VinaText.txt; do
	if [ ! -f "$APPDIR/usr/share/vinatext/license/$NOTICE" ]; then
		echo "FATAL: $NOTICE is missing from the AppDir" >&2
		exit 1
	fi
done

if [ "$(uname -s)" != "Linux" ]; then
	echo "==> not Linux: AppDir built and checked, skipping appimagetool"
	exit 0
fi

# linuxdeploy pulls in Qt and writes the AppImage. Downloaded rather than
# vendored: it is a build tool, not a dependency of the product.
TOOLDIR="${LINUXDEPLOY_DIR:-$OUTDIR/tools}"
mkdir -p "$TOOLDIR"
ARCH="$(uname -m)"
for TOOL in linuxdeploy linuxdeploy-plugin-qt; do
	if [ ! -x "$TOOLDIR/$TOOL" ]; then
		curl -sSLo "$TOOLDIR/$TOOL" \
			"https://github.com/linuxdeploy/$TOOL/releases/download/continuous/$TOOL-$ARCH.AppImage"
		chmod +x "$TOOLDIR/$TOOL"
	fi
done

export PATH="$TOOLDIR:$PATH"
export QMAKE="${QMAKE:-qmake6}"
"$TOOLDIR/linuxdeploy" --appdir "$APPDIR" --plugin qt --output appimage
mv VinaText*.AppImage "$OUTDIR/" 2>/dev/null || true
echo "==> wrote AppImage in $OUTDIR"
