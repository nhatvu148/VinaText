# thirdparty/

Vendored source for the two upstream projects the Qt frontend builds against.

| | version | licence | upstream |
|---|---|---|---|
| `scintilla/` | 5.6.4 | HPND — `scintilla/License.txt` | https://www.scintilla.org/ |
| `lexilla/` | 5.5.1 | permissive, same text | https://github.com/ScintillaOrg/lexilla |

Both licences are MIT-compatible; see `license/License-Scintilla.txt`, which upstream titles
*"License for Lexilla, Scintilla, and SciTE"* and which covers both.

## Why vendored rather than fetched

PR #16 fetched these with `FetchContent` and a pinned SHA-256, arguing that ~4 MB of
third-party source was not worth adding to a repo D8 already calls too big. Two things
changed that:

- **`scintilla.org` timed out from the GitHub ubuntu runners** during CI on #19 — curl status
  28, five retries, fifteen minutes — while the same URL succeeded from macOS. It is one small
  origin server, not a CDN. A build that cannot run without it stops working for reasons
  nobody here controls.
- **#19 removed 97 MB of unused Boost binaries**, so 4.5 MB is now noise against the tree it
  is measured in.

The reproducibility argument for fetching was real but is fully satisfied by vendoring, which
also removes the network from the build entirely.

## What is here

Only what the build compiles:

```
scintilla/include  scintilla/src  scintilla/qt/ScintillaEditBase
lexilla/include    lexilla/lexlib  lexilla/lexers  lexilla/src
```

The `cocoa`, `gtk`, `win32`, `doc`, `test`, `scripts`, `access` and `examples` trees are not
vendored, nor are the qmake `.pro` files — `CMakeLists.txt` here replaces them.

## Upgrading

1. Download the release tarballs from the upstream links above.
2. Replace the directories listed under *What is here*, plus `License.txt` and `version.txt`.
3. Build with `-DVINATEXT_BUILD_QT=ON` and run `ui-qt/vinatext-qt --selftest <file>`.

If `ScintillaEditBase.pro` gains or loses a source file, mirror that in the explicit
`_sci_core` list in `CMakeLists.txt` — it is transcribed from the `.pro` rather than globbed,
because the `.pro` list is deliberately not the same as `src/*.cxx`.
