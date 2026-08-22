#!/usr/bin/env python3
"""Fails the build if Emscripten's generated glue is internally inconsistent.

WHY THIS EXISTS. An incremental relink of the Qt WebAssembly build produced a
vinatext-qt.js that CALLS ASM_CONSTS[code] and never DEFINES the ASM_CONSTS
table. Emscripten collects EM_ASM snippets at link time; when that step does not
re-run, the consumer survives and the table does not. Nothing fails: the build
succeeds, the page loads, Qt starts, and then the app dies in the browser with

    Application exit (ASM_CONSTS[code] is not a function)

which names a JavaScript symbol and says nothing about the build that produced
it. Reported by a user, on an artifact that had been "successfully built".

A clean rebuild fixes it, so this is a stale-link hazard rather than a code bug -
which is exactly the sort that hides until somebody is looking at a blank page.

The invariant is narrow on purpose: USES implies DEFINES. A build with no EM_ASM
at all is fine and is not what this is about.
"""

import sys
from pathlib import Path


def main(argv):
    if len(argv) != 2:
        print("usage: check_wasm_glue.py <path/to/generated.js>", file=sys.stderr)
        return 2
    path = Path(argv[1])
    if not path.exists():
        print(f"FATAL: {path} does not exist", file=sys.stderr)
        return 1
    text = path.read_text(errors="replace")

    uses = "ASM_CONSTS[" in text
    defines = "ASM_CONSTS=" in text or "ASM_CONSTS =" in text
    if uses and not defines:
        print(
            f"FATAL: {path.name} calls ASM_CONSTS[...] but never defines the table.\n"
            "  Emscripten's EM_ASM collection did not re-run - the usual cause is an\n"
            "  incremental relink. The app WILL load and then die in the browser with\n"
            '  "Application exit (ASM_CONSTS[code] is not a function)".\n'
            "  Fix: rm -rf the wasm build directory and configure again.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
