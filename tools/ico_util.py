#!/usr/bin/env python3
"""Reading Windows .ico files, with the standard library only.

Shared by tools/extract_ico_png.py and tools/brand_wasm_shell.py, which had
identical copies of this parser - two copies that would drift the first time one
of them was fixed. Review caught it.

Standard library on purpose: macOS has sips and a Linux CI runner has neither
sips nor, reliably, ImageMagick, and a pip dependency to read one icon would be a
poor trade. res/app.ico happens to store its 256x256 entry as a PNG already, so
the interesting case is a byte-for-byte extraction with no decoding at all.
"""

import struct

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def largest_png(ico: bytes):
    """Returns (width, blob) for the biggest PNG-encoded entry, or None.

    Entries that are not PNG are BMP-encoded and would need a real decoder;
    they are skipped rather than half-handled.
    """
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
        width, _height, _colours, _rsv, _planes, _bpp, size, offset = struct.unpack(
            "<BBBBHHII", entry
        )
        width = width or 256          # 0 means 256 in the ICO format
        blob = ico[offset : offset + size]
        if blob[:8] == PNG_SIGNATURE and (best is None or width > best[0]):
            best = (width, blob)
    return best
