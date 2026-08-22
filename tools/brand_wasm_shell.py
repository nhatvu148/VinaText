#!/usr/bin/env python3
"""Puts VinaText's name and icon on Qt's generated WebAssembly shell.

Qt writes vinatext-qt.html from its own wasm_shell.html template at link time,
substituting the TARGET name - so the browser tab reads "vinatext-qt" and carries
no icon at all. That is the first thing anyone sees of the web build, and it says
the name of a CMake target.

THIS PATCHES THE GENERATED FILE RATHER THAN REPLACING IT. A hand-written shell
would have to be kept in step with whatever Qt's template does next - it wires up
the loader, the screen element and the qtloader.js contract - and a shell that
drifts from its Qt version fails by not starting, with nothing to say why. Two
substitutions leave the rest of Qt's file alone.

The icon is inlined as a data: URI rather than shipped as favicon.ico. That is
one fewer file to deploy, and it dodges a real trap: the nginx config serving
this has no mime.types, so a .ico would go out as text/plain, and it sets
X-Content-Type-Options: nosniff, which tells the browser not to second-guess that.

Idempotent - safe to run on an already-patched file, which POST_BUILD will.
"""

import base64
import sys
from pathlib import Path

from ico_util import largest_png

TITLE = "VinaText"


def main(argv):
    if len(argv) != 3:
        print("usage: brand_wasm_shell.py <shell.html> <app.ico>", file=sys.stderr)
        return 2
    shell, ico_path = Path(argv[1]), Path(argv[2])
    if not shell.exists():
        print(f"FATAL: {shell} does not exist", file=sys.stderr)
        return 1
    html = shell.read_text(errors="replace")

    # The title. Qt substitutes the target name; anything between the tags goes.
    start, end = html.find("<title>"), html.find("</title>")
    if start < 0 or end < 0:
        # Loud: a Qt release that stopped emitting a <title> would otherwise
        # leave the tab reading "vinatext-qt" with nothing to explain it.
        print("FATAL: no <title> in the generated shell - has Qt's template changed?",
              file=sys.stderr)
        return 1
    html = html[: start + len("<title>")] + TITLE + html[end:]

    # The icon, inlined. Removed first so re-running replaces rather than stacks.
    marker = '<link rel="icon"'
    while marker in html:
        i = html.find(marker)
        j = html.find(">", i)
        if j < 0:
            # An unterminated tag. Without this the slice below is html[0:] -
            # the whole string again - so the loop never shrinks it and never
            # ends: measured, the script ran until an alarm killed it at exit
            # 142. Loud, like the other checks, rather than a hang.
            print("FATAL: an unterminated <link rel=\"icon\"> in the shell - "
                  "has Qt's template changed?", file=sys.stderr)
            return 1
        html = html[:i] + html[j + 1 :]

    found = largest_png(ico_path.read_bytes())
    if found is None:
        print(f"FATAL: no PNG-encoded entry in {ico_path}", file=sys.stderr)
        return 1
    _width, blob = found
    href = "data:image/png;base64," + base64.b64encode(blob).decode("ascii")
    link = f'<link rel="icon" type="image/png" href="{href}">\n  '
    head_end = html.find("</head>")
    if head_end < 0:
        print("FATAL: no </head> in the generated shell", file=sys.stderr)
        return 1
    html = html[:head_end] + link + html[head_end:]

    shell.write_text(html)
    print(f"branded {shell.name}: title={TITLE!r}, icon inlined ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
