#!/usr/bin/env python3
"""Pulls the largest PNG out of a Windows .ico.

The Linux packaging needs a PNG icon and the macOS one needs an .icns, but the
artwork lives in res/app.ico - the file src/VinaText.rc ships as IDR_MAINFRAME.
Keeping one source and deriving the rest is the difference between redrawing an
icon once and redrawing it three times and missing one.

STANDARD LIBRARY ONLY, on purpose. macOS has sips; a Linux CI runner has neither
sips nor, reliably, ImageMagick, and adding a pip dependency to draw one icon
would be a poor trade. It happens that the 256x256 entry in this .ico is stored
as a PNG already, so the "conversion" is a byte-for-byte extraction with no
decoding at all - and if that ever stops being true this says so rather than
writing something malformed.
"""

import struct
import sys
from pathlib import Path


def largest_png(ico: bytes):
    """Returns (width, blob) for the biggest PNG-encoded entry, or None."""
    if len(ico) < 6:
        return None
    reserved, kind, count = struct.unpack("<HHH", ico[:6])
    if reserved != 0 or kind != 1:
        return None
    best = None
    for i in range(count):
        entry = ico[6 + i * 16 : 6 + i * 16 + 16]
        if len(entry) < 16:
            break
        width, _h, _c, _r, _p, _bpp, size, offset = struct.unpack("<BBBBHHII", entry)
        width = width or 256          # 0 means 256 in the ICO format
        blob = ico[offset : offset + size]
        # The PNG signature. Entries that are not PNG are BMP-encoded, which
        # would need a real decoder - skipped rather than half-handled.
        if blob[:8] == b"\x89PNG\r\n\x1a\n" and (best is None or width > best[0]):
            best = (width, blob)
    return best


def main(argv):
    if len(argv) != 3:
        print("usage: extract_ico_png.py <input.ico> <output.png>", file=sys.stderr)
        return 2
    found = largest_png(Path(argv[1]).read_bytes())
    if found is None:
        # Loud, not silent. A zero-byte icon would sail through packaging and
        # only show up as a blank tile in somebody's launcher.
        print(f"FATAL: no PNG-encoded entry in {argv[1]}", file=sys.stderr)
        return 1
    width, blob = found
    Path(argv[2]).write_bytes(blob)
    print(f"wrote {argv[2]} ({width}x{width}, {len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
