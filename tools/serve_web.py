#!/usr/bin/env python3
"""Serves the WebAssembly build with caching turned OFF.

WHY NOT `python3 -m http.server`. It sends Last-Modified and no Cache-Control,
so a browser is free to reuse what it already has. On a wasm build that is not a
stale page - it is a stale HALF of one: the browser pairs a cached
vinatext-qt.js with a freshly fetched vinatext-qt.wasm, the EM_ASM indices no
longer line up, and the app dies with

    Application exit (ASM_CONSTS[code] is not a function)

which is indistinguishable from the build-side version of the same failure that
tools/check_wasm_glue.py exists to catch. One cause is fixed at the link, the
other here; the symptom is identical, which is why both are worth removing.

Usage: tools/serve_web.py [port] [directory]
"""

import functools
import http.server
import sys
from pathlib import Path


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        # Browsers refuse to stream-compile a wasm served as anything else, and
        # the fallback path is slower and quieter about why.
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".data": "application/octet-stream",
    }

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def log_message(self, fmt, *args):
        sys.stderr.write("  %s\n" % (fmt % args))


def main(argv):
    port = int(argv[1]) if len(argv) > 1 else 8712
    root = Path(argv[2]) if len(argv) > 2 else Path("qtwasm/ui-qt")
    if not (root / "vinatext-qt.html").exists():
        print(f"FATAL: no vinatext-qt.html under {root}", file=sys.stderr)
        print("  Build it first - see doc/PORTING.md 6z.", file=sys.stderr)
        return 1
    handler = functools.partial(NoCacheHandler, directory=str(root))
    print(f"serving {root} on http://localhost:{port}/vinatext-qt.html  (no caching)")
    http.server.ThreadingHTTPServer(("", port), handler).serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
