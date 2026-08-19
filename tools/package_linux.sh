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
#
# PINNED, NOT "continuous". Upstream overwrites the continuous tag in place, so
# a run could start failing - or quietly produce a DIFFERENT AppImage - with no
# change in this repository. That is precisely the untracked packaging drift this
# whole change exists to catch, so leaving it floating would have made the CI job
# a worse version of the problem it was added to solve. Review caught it.
LINUXDEPLOY_TAG="${LINUXDEPLOY_TAG:-1-alpha-20251107-1}"
LINUXDEPLOY_QT_TAG="${LINUXDEPLOY_QT_TAG:-1-alpha-20250213-1}"

# And checksums, because a pinned TAG is not a pinned FILE: GitHub release assets
# can be replaced. x86_64 only - the checksum for another architecture is a
# different file, so an unknown arch skips the comparison rather than failing on
# a mismatch it was never going to satisfy.
LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
LINUXDEPLOY_QT_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"

TOOLDIR="${LINUXDEPLOY_DIR:-$OUTDIR/tools}"
mkdir -p "$TOOLDIR"
ARCH="$(uname -m)"

fetch_tool() {
	NAME="$1"
	TAG="$2"
	WANT="$3"
	DEST="$TOOLDIR/$NAME"
	[ -x "$DEST" ] && return 0

	# -f, so an HTTP error FAILS instead of being written to the tool path.
	# Without it a 404 leaves the string "Not Found" in the file, chmod +x makes
	# it executable, and the run dies later with "cannot execute binary file" -
	# a confusing symptom two steps from its cause. Measured: curl -sSLo on a
	# missing asset exits 0 and writes the error page.
	curl -fsSLo "$DEST" \
		"https://github.com/linuxdeploy/$NAME/releases/download/$TAG/$NAME-$ARCH.AppImage"

	if [ "$ARCH" = "x86_64" ]; then
		GOT="$(sha256sum "$DEST" | cut -d' ' -f1)"
		if [ "$GOT" != "$WANT" ]; then
			echo "FATAL: $NAME checksum mismatch" >&2
			echo "  expected $WANT" >&2
			echo "  got      $GOT" >&2
			exit 1
		fi
	fi
	chmod +x "$DEST"
}

fetch_tool linuxdeploy "$LINUXDEPLOY_TAG" "$LINUXDEPLOY_SHA256"
fetch_tool linuxdeploy-plugin-qt "$LINUXDEPLOY_QT_TAG" "$LINUXDEPLOY_QT_SHA256"

export PATH="$TOOLDIR:$PATH"
export QMAKE="${QMAKE:-qmake6}"

# THE OFFSCREEN PLATFORM PLUGIN, exactly as on macOS. linuxdeploy-plugin-qt
# deploys only the platform plugin it detects in use - xcb - which is right for a
# user and means the SHIPPED AppImage cannot run headless:
#
#   Could not find the Qt platform plugin "offscreen" in ""
#   Available platform plugins are: xcb.
#
# Checking the thing we hand out rather than the thing we built is the only way
# to know it is sound, so the plugin goes in deliberately. Both deploy tools
# strip it for the same reason, and both had to be told - see doc/PORTING.md 6y.
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so"

"$TOOLDIR/linuxdeploy" --appdir "$APPDIR" --plugin qt --output appimage
mv VinaText*.AppImage "$OUTDIR/" 2>/dev/null || true
echo "==> wrote AppImage in $OUTDIR"
