# VinaText → Qt Port: Context Brief

> **For a fresh Claude session working in a clone of `vinadevs/VinaText`.**
> Everything below was verified against the live repo on 2026-07-26 via the GitHub API and
> raw file reads. Decisions marked **DECIDED** have already been made and reasoned through —
> do not re-litigate them; if you think one is wrong, say so once, briefly, then proceed.

---

## 1. Mission

Port VinaText from Windows-only MFC to cross-platform **Qt** (Windows + macOS + Linux),
while the existing Windows app keeps shipping to users continuously throughout.

Trigger: [issue #36](https://github.com/vinadevs/VinaText/issues/36) (open since 2023-05-31),
in Vietnamese, requesting a cross-platform version and citing
[`cxasm/notepad--`](https://github.com/cxasm/notepad--) as the model.

**Honest note on demand:** the signal is thin — one issue, one user, one reaction, 34 repo
stars. This was raised with the project owner and they decided to proceed anyway. That call
is made. Don't re-open it.

---

## 2. Verified repo facts

| Fact | Value |
|---|---|
| Repo | `vinadevs/VinaText`, **MIT** |
| Created / last push | 2023-01-16 / **2024-12-28** (dormant ~19 months) |
| Stars / forks / open issues | 34 / 2 / 11 |
| Size | **163 MB** |
| Toolchain | VS 2017/2019/2022, v141/v142/v143, Win10 SDK, **MFC + ATL**, x86/x64/ARM/ARM64 |
| Total tracked files | 12,099 |

**Tree shape:** `include/` 11,404 files · `src/` 450 · `bin/` 150 · `res/` 36 · `lib/` 19 ·
`webpage/` 11 · `doc/` 11 · `license/` 7 · `installer/` 4 · `.github/` 3

**`src/`:** 193 `.cpp` / 228 `.h` recursive — but that splits into:

| Scope | `.cpp` | `.h` | Notes |
|---|---|---|---|
| `src/*` (top level) — **VinaText's own code** | **137** | **154** | ~101,000 LOC. This is what gets ported. |
| `src/uchardet/` | 26 | 22 | Vendored encoding detector — already portable |
| `src/pdf/UXReader/` | 25 | — | PDFium-based viewer component |
| `src/pdf/ArobatActiveX/` | 1 | — | Acrobat **ActiveX** — Windows-only, likely delete |
| `src/tinyXml/` | 4 | 2 | Vendored — already portable |

`src/localization/` holds data files, no source.

⚠️ Use `src/*.cpp` (137 files) for triage globs, **not** `find src -name '*.cpp'` (193) —
the latter sweeps in vendored third-party code that must not be rewritten.

**Vendored deps** (`include/`): ~~`boost`~~, `checksum`, `curl`, `json`, `scintilla`.
Boost was removed entirely on 2026-07-31 — see the note below.

**Prebuilt binary blobs** (`lib/`, Debug+Release × x64): ~~`boost_python-vc140-mt-1_57`,
`libboost_iostreams`, `libboost_serialization`, `libboost_wserialization`~~, `libcurl`.
→ Boost 1.57 was from 2014, vc140, Windows-x64-only, and the brief called it the single worst
dependency problem in the repo.

> ✅ **RESOLVED, 2026-07-31.** Boost is gone. It turned out to be far less of a dependency
> than it looked:
>
> - **Nothing linked any Boost library.** No `AdditionalDependencies` entry in any of the four
>   configurations, no `#pragma comment(lib, ...)`, and no MSVC auto-link. The ten `.lib`
>   files — python, python3, iostreams, regex, serialization, wserialization — were dead
>   weight. **97 MB, deleted; the Windows linker confirmed it.**
> - **The real dependency was four header-only components, 20 call sites.** `crc.hpp` and
>   `uuid/sha1.hpp` became `core/Checksum.h`, proven byte-identical against the published
>   CRC-32 and FIPS 180-4 vectors. `algorithm/string.hpp` and `algorithm/string/join.hpp`
>   became `core/TextTransform.h`, which performs the *same* `std::transform` against the
>   *same* `std::locale()` rather than an approximation.
> - **`include/boost` deleted: 10,483 files, 119 MB.**
>
> Working tree **451 MB → 246 MB**. Phase 0's "Boost 1.57 → vcpkg" item is closed without
> needing vcpkg at all.

**Licences already tracked** (`license/`): Curl, Mozilla, PDFium, Qt, Scintilla, VinaText.
(`License-Boost.txt` was removed with the dependency; `License-Qt.txt` added with the Qt frontend.)
Follow this same pattern when adding Qt.

**Settings: already portable — no registry work needed.** Config lives in a file
(`%AppData%\VinaText`, made fully portable next to the exe in v1.16 per closed issue #44).
This is a real head start; don't go looking for registry code to unwind.

---

## 3. Measured MFC coupling (this is the key finding)

### 3a. `src/stdafx.h` is a god-header and it is the actual blocker

Every `.cpp` includes it. It pulls in:

```
afxwin.h  afxext.h  afxdisp.h  afxdtctl.h  afxcmn.h  afxcontrolbars.h
afxtempl.h  afxmt.h  afxole.h  afxres.h          <- all of MFC incl. Feature Pack
atlcom.h  atlcoll.h  atlbase.h  atlhost.h            <- ATL
windows.h  shlobj.h  lm.h  uxtheme.h  WindowsX.h  TlHelp32.h
comdef.h  winperf.h  Iphlpapi.h  shlwapi.h  wininet.h
gdiplus.h  (+ #pragma comment(lib, "gdiplus.lib"), "iphlpapi.lib")
EnumDef.h  MacroDef.h
```

**Consequence:** no file in `src/` is portable today — not because the code is Windows-coupled,
but because the PCH is. **Breaking this header gates everything else.**

> **Measured 2026-07-30.** `atlctl.h` and `strsafe.h` were in this list and are now removed —
> nothing in `src/` referenced either. That is the *only* slack in the header. Every remaining
> Win32/MFC include above is used by at least one of the 125 `.cpp`:
>
> ```
> shlwapi.h 277  uxtheme.h 34  shlobj.h 34  Assert.h 27  winperf.h 15  afxtempl.h 10
> afxmt.h    10  gdiplus.h  9  objbase.h 8  codecvt   8  TlHelp32.h 8  Iphlpapi.h  7
> WindowsX.h  6  lm.h       5  wininet.h 4  afxdtctl.h 4  atlhost.h  2  io.h        1
> afxole.h    1  comdef.h   1  afxdisp.h 1
> ```
>
> **Vendored code is excluded and relies on transitive includes.** `src/pdf/UXReader/*.cpp`
> is compiled into the product but is vendored, so it is out of scope for Phase 1 per the
> no-rewriting-vendored-code rule. **Five** of those files use symbols they never include a
> header for, and get them from somewhere in the MFC header chain rather than from `stdafx.h`:
>
> | File | Symbol | Header removed from PCH | Compiled? | Build verified after removal |
> |---|---|---|---|---|
> | `UXReaderLibrary.cpp` | `Gdiplus::GdiplusStartup` | `gdiplus.h` | yes | ✅ PR #10 |
> | `UXReaderDocumentPage.cpp` | `Gdiplus::` | `gdiplus.h` | yes | ✅ PR #10 |
> | `UXReaderDocumentPane.cpp` | `GET_X_LPARAM` | `WindowsX.h` | yes | ✅ PR #10 |
> | `UXReaderMainWindow.cpp` | `GET_X_LPARAM` / `GET_Y_LPARAM` | `WindowsX.h` | **no — absent from the vcxproj** | n/a — never compiled |
> | `UXReaderDocument.cpp` | `PathRemoveExtensionW` / `PathStripPathW` | `shlwapi.h` | yes | ✅ PR #13 |
>
> Every removal above was confirmed by a green Windows build **on the exact commit that
> removed it**, so the MFC header chain does supply these symbols today.
>
> ⚠️ **Read that verification narrowly.** It covers one configuration only: `Release|x64`,
> MSVC v143, `windows-2022`, as built by `.github/workflows/main.yml`. It is **not** evidence
> for `Debug`, for `Win32`, or for any other toolset — those are not built in CI. **It is a
> latent dependency on MFC's own internal includes, and a future toolset could break it.** If
> that happens the fix is a small vendored-code patch, tracked as such.

> **So `stdafx.h` cannot be slimmed further until individual files carry their own includes,
> and Phase 1 is irreducibly per-file.** There is no header diet that shortcuts it — worth
> knowing before starting rather than at file 60.

### 3b. Underneath the PCH, coupling is shallow — it's mostly just `CString`

Measured on core-layer candidate headers:

| File | `CString` uses | Win32/MFC lines | `std::` lines |
|---|---|---|---|
| `StringHelper.h` | **0** | 2 | 57 |
| `TextFormatConverter.h` | **0** | 0 | 22 |
| `UnicodeUtils.h` | **0** | 0 | 13 |
| `Cryptography.h` | 3 | 0 | 0 |
| ~~`LexerParser.h`~~ → `core/Tokenizer.h` (#25) | 9 | 0 | 0 |
| `DiffEngine.h` — algorithm → `core/LineDiff` (#26), HTML renderer stays | 12 | 4 | 0 |
| `FileUtil.h` | 14 | 0 | 42 |
| `AppSettings.h` | 17 | 0 | 3 |
| `PathUtil.h` | **106** | 6 | 5 |

Three files are already portable. Real Win32 API usage in the core layer is near zero.
**The port is a string-type migration plus a UI rewrite** — not a deep untangling.

### Corpus-wide `CString` measurement (full clone, top-level `src/` only)

- **4,138 total `CString` occurrences** across `src/*.cpp` + `src/*.h`
- **85 of 291 files contain zero `CString`** — a much larger ready-to-move core pool than the
  9-file sample above suggested

That 4,138 splits as **2,292** in the `.cpp`, **882** in headers paired with a `.cpp`, and
**964** in headers that have no `.cpp` at all. See [`PORTING.md`](PORTING.md) §1 for the full
reconciliation — it matters because only the first two categories appear in the per-file
triage, so the totals there will not add up to 4,138 without it.

Heaviest files:

| Count | File | Character |
|---|---|---|
| 504 | `modellanguages.h` | **static data table** |
| 495 | `EditorView.cpp` | logic + UI |
| 242 | `FileExplorerCtrl.cpp` | UI |
| 211 | `EditorColorLight.h` | **static data table** |
| 210 | `EditorColorDark.h` | **static data table** |
| 202 | `PathUtil.cpp` | **core logic** |
| 109 | `Compiler.cpp` | platform |
| 106 | `PathUtil.h` | **core logic** |
| 97 | `Editor.cpp` | logic + UI |
| 96 | `AppUtil.cpp` | mixed |

> **The 4,138 figure overstates the difficulty.** 925 of them (22%) live in just three static
> data tables — `modellanguages.h`, `EditorColorLight.h`, `EditorColorDark.h`. Those are
> declarations, not call sites: a mechanical sweep, or better, convert them to external data
> files (JSON/INI) and delete the C++ tables entirely. Do these three first in Phase 2 for a
> fast 22% reduction.

### 3c. The UI is MFC Feature Pack, not stock Win32

From `MainFrm.cpp` and `AppLookDlg.cpp`:

```
23 × CMFCTabCtrl
13 × CMFCVisualManagerOffice   (Office2007)
 8 × CMFCVisualManager
 3 × CMFCVisualManagerVS       (VS2005 / VS2008)
 2 × CMFCVisualManagerWindows
 2 × CMFCToolBar
 1 × CDockablePane
```

`AppLookDlg` is an "Application Look" theme picker (Office 2007 / VS2005 / VS2008).
> ⚠️ **`AppLookDlg.cpp` is not in `src/VinaText.vcxproj` and has never been compiled into
> the shipping product.** It has been deleted — see [`PORTING.md`](PORTING.md) §2. Some of
> the `CMFCVisualManager` counts above came from it, so the live Feature Pack surface is
> smaller than this table suggests.
**So the current UI already overrides the native Windows look with a ~2007-era skin.**
Qt will not reproduce that, and shouldn't try.

### 3d. Inventory to port

- **9 dock panes** (not 11): `BookmarkWindow`, `BreakpointWindow`, `BuildWindow`,
  `MessageWindow`, `OpenTabWindows`, `PathResultWindow`, `SearchResultWindow`,
  `SearchAndReplaceWindow`, `FileExplorerWindow`.
  ~~`TerminalWindow`, `TextReferenceWindow`~~ were never compiled — deleted, see
  [`PORTING.md`](PORTING.md) §2.
- **~40 `*Dlg.cpp`** dialogs (`.rc`, fixed-pixel → must become `.ui` with layouts)
- **Windows-only subsystems:** `ShellContextMenu` (COM `IContextMenu`), `CWMPHost` +
  `CWMPEventDispatch` (Windows Media Player ActiveX), `WebView`/`WebDoc`/`WebHandler`,
  `WindowsPrinter`, `SingleInstanceApp`, `Compiler`, `Debugger`, `SystemInfo`, `OSUtil`.
  ~~`DirectoryNotifier` + `DirectoryNotifyManager` (`ReadDirectoryChangesW`)~~ and
  ~~`TerminalWindow`~~ were never compiled — deleted, see [`PORTING.md`](PORTING.md) §2.

---

## 4. DECIDED — do not re-litigate

**D1. Qt, not Tauri, not wxWidgets.**
Tauri was evaluated and rejected: its webview cannot host native controls (no MFC, no Qt
widget, no Scintilla), so it would be a full rewrite retaining <15% of the code while adding
Rust + TypeScript to a C++ team, landing on a slower editor built from VS Code's own
component. wxWidgets is viable (friendlier licence, `wxStyledTextCtrl` *is* Scintilla) but
Qt wins because `cxasm/notepad--` is a working existence proof of exactly this app shape:
Qt + Scintilla, Windows/Linux/macOS, 9,970 stars, actively maintained (pushed 2026-07-03).

**D2. Use upstream Scintilla's own Qt binding — NOT QScintilla.**
Use `qt/ScintillaEditBase` / `qt/ScintillaEdit` from upstream Scintilla (permissive licence).
Riverbank's **QScintilla is GPL-or-commercial** — it is why `notepad--` had to be GPL-3.0.
Using it would force VinaText off MIT. **This is the single most important licensing trap.**

**D3. Qt under LGPLv3, dynamically linked. VinaText stays MIT.**
Compliance checklist:
- Link Qt **dynamically** (`windeployqt` / `macdeployqt`). Never static — static linking can
  pull the whole app under LGPL unless you also publish relinkable object files.
- Don't defeat relinking (no signing/packaging that blocks swapping the Qt libs).
- Add Qt licence text to `license/` (same pattern as Curl/PDFium/Scintilla). **Done** —
  `license/License-Qt.txt`.
- Add "Uses Qt under LGPLv3" to `AppAboutDlg`, and state the exact Qt version.
- Offer a link to that Qt version's corresponding source.
- Publish any patches you make to Qt itself (you won't make any).

**GPLv3-only Qt modules — never use these:** Qt Graphs, Qt HTTP Server, Qt Quick 3D,
Qt Quick 3D Physics, Qt Virtual Keyboard, Qt Wayland Compositor, Qt MQTT, Qt CoAP, Qt GRPC,
Qt Network Authorization, Qt Canvas Painter, Qt Lottie Animation, Qt Qml Compiler,
Qt Quick Timeline.

**Licence NOT yet confirmed — verify before depending on:** Qt Multimedia, Qt WebEngine,
Qt PDF, Qt Charts. (Qt Multimedia and Qt WebEngine are both currently *proposed* replacements
in Phase 5 — confirm each against the exact Qt version before writing code against them.)

**Safe:** build tools (`moc`, `uic`, `rcc`, `windeployqt`) are GPLv3 **+ Qt GPL exception 1.0**,
which explicitly permits running them on non-GPL code.

**No Mac App Store / Google Play** — their terms conflict with LGPL's "freedoms cannot be
restricted from recipients". Ship `.dmg` / AppImage direct from GitHub, as today.

**D4. Develop in the `nhatvu148/VinaText` fork, on one long-lived `port/cross-platform`
branch. Single consolidated PR to `vinadevs/VinaText` when ready.**

> **Revised twice.** Originally: *"no long-lived port branch — everything lands on `master`
> behind a CMake flag."* Revised 2026-07-26 by the project owner to the integration-branch
> model. Revised again 2026-07-27: development moved to a fork, because the automated review
> tooling the owner relies on cannot run against the `vinadevs` organisation. Both sets of
> reasoning are preserved below — the tradeoffs are real and the mitigations are not optional.

- **Fork as staging, not as a permanent home.** The original objection to forking stands in
  general — sync pain, split releases, split issue tracker. It is accepted here because the
  fork is a *review* environment with a defined exit: one consolidated PR upstream. The issue
  tracker stays on `vinadevs`; the fork carries code and PRs only.
- **Remotes.** `origin` = `nhatvu148/VinaText` (the fork), `upstream` = `vinadevs/VinaText`.
  Deliberately this way round, so a stray `git push` cannot reach the organisation repo.
- **All port work targets `port/cross-platform`.** PRs point at `port/cross-platform`, not
  `master`, and are opened *within the fork*. `port/cross-platform` goes upstream as one
  consolidated PR ~~at cutover (Phase 6)~~ — **superseded by D10: when the macOS/Linux
  editor ships, because D10 blocks Phase 6 indefinitely.** Keep the existing `release_1.x` convention for
  shipping MFC releases off `upstream/master`.
- **Rebase `port/cross-platform` on `upstream/master` weekly** — see the mitigations below.
  The fork adds a second sync hop, which makes the cadence more important, not less.
- Still add `ui-qt/` as a sibling directory gated by a CMake option defaulting OFF:
  ```cmake
  option(VINATEXT_BUILD_QT "Build the Qt frontend" OFF)
  ```
  This stays useful even on a branch: it keeps the Windows build green on `port/cross-platform` itself,
  and it makes the eventual Phase 6 merge to `master` a non-event.

**The cost this incurs, and the two mitigations that make it survivable:**

Phase 1 rewrites the `#include` block of all 137 `.cpp`; Phase 2 *moves* files into `core/`
and rewrites their string type. Renames plus whole-file churn, against a `master` still taking
MFC bugfixes into those same files, is the most expensive class of git conflict there is.
Phases 3–6 are cheap by comparison — `ui-qt/` is all new files and conflicts with nothing.

1. **Rebase `port/cross-platform` onto `master` on a fixed weekly cadence**, not "when it hurts." Drift is
   priced linearly if you pay it weekly and quadratically if you don't.
2. **Land Phase 1 as a single big-bang PR**, not a trickle. A 137-file include sweep is far
   cheaper to conflict-resolve once than fifty times.

Note the tension this creates with **D5**: the strangler fig's core property is that the
shipping MFC app is a continuous regression test on every line moved into `core/`. On a branch,
that test runs against `port/cross-platform`, not against what users actually run. Phases 0–2 regressions
will surface later than they would have on trunk. Budget for it.

**D5. Architecture — Strangler Fig, two frontends over one core.**
```
core/        UI-free, portable.  GROWS as code moves out of ui-mfc/
platform/    Thin OS layer, 3 impls (win/mac/linux)
ui-mfc/      Today's src/.  SHRINKS.  Keeps shipping to Windows users throughout.
ui-qt/       New.  Grows.  Flag-gated until parity.
thirdparty/  vcpkg/Conan manifest — replaces include/ and lib/ blobs
```
Critical property: the MFC app builds and ships the whole time, making it a **continuous
regression test** on every line moved into `core/`.

**D6. `core/` stays dependency-free and uses `std::wstring`. Each frontend converts at its
own boundary.**

> **Revised 2026-07-31 by the project owner.** This originally read *"`core/` uses `QString`,
> not `std::u16string`"*, with the MFC app linking QtCore to make the strangler work. The
> original reasoning is preserved below, because the tradeoff was real — but three things
> measured since made the premise weaker than it looked:
>
> 1. **`core/` already works in both frontends without Qt.** `ui-qt/` links it today and
>    converts at its own boundary; the Qt spike (#16) needed no change to `core/` at all.
> 2. **The convenience gap is smaller than assumed.** D6 argued `std::` has no split, trim,
>    case-insensitive compare or format. `core/StringUtil.h` and `core/TextTransform.h` now
>    supply exactly those in ~250 lines, with tests.
> 3. **The corpus is 684 `CString` in `core/`, not 4,138** ([`PORTING.md`](PORTING.md) §1).
>    The 4,138 spans code that is deleted or rewritten rather than translated, so the figure
>    this decision leaned on overstated the work by 6×.
>
> **What tipped it: the cost D6 did not price.** Linking QtCore into `core/` means the
> *shipping MFC application* gains a Qt dependency — `Qt6Core.dll` shipped to every Windows
> user, and D3's LGPLv3 obligations (dynamic linking, About-dialog attribution, corresponding
> source offer, installer changes) applying to the MFC build immediately. Phases 0–2 are
> supposed to be invisible to users; that is not invisible.
>
> **The rule now:** `core/` takes no third-party dependency, Qt included. It uses
> `std::wstring` where the MFC code used `CString`, and `std::string` for ASCII payloads.
> `ui-mfc/` converts with the existing `AppUtils::CStringToWStd` / `WStdToCString`; `ui-qt/`
> converts with `QString::fromStdWString` / `toStdWString`. Both are one call at the edge.
>
> D6's original mapping table below is still the right guide for `ui-qt/`, where `QString` is
> the natural type — it just applies at the frontend rather than in `core/`.

**Superseded — the original D6 reasoning:**
This is the decision that determines total cost, and the measured corpus makes it decisive:
**there are 4,138 `CString` occurrences** (§3b). `std::u16string` has no split, trim, format,
case-insensitive compare, or number conversion — under Option A every one of those becomes
hand-written code. `CString` → `QString` is near-mechanical instead (both UTF-16, both
copy-on-write):

| MFC | Qt |
|---|---|
| `Left` / `Mid` / `Right` | `left` / `mid` / `right` |
| `Find` / `ReverseFind` | `indexOf` / `lastIndexOf` |
| `MakeLower` / `MakeUpper` | `toLower` / `toUpper` |
| `Trim` | `trimmed` |
| `Format` | `arg` / `asprintf` |
| `Tokenize` | `split` |
| `GetLength` / `IsEmpty` | `length` / `isEmpty` |
| `CStringArray` | `QStringList` |
| `CArray` / `CMap` | `QVector` / `QHash` |

QtCore is LGPL and has no UI dependency. **The MFC app can link QtCore** — that's what makes
the strangler approach work: `core/` migrates to `QString` while MFC is still the shipping
product.

> ⚠️ **Caveat:** QtCore *value* types (`QString`, `QFile`, containers, `QSettings`) work fine
> without a `QCoreApplication` or event loop. `QObject`/signal-based classes
> (`QFileSystemWatcher`, `QProcess`, `QTimer`) **need a running event loop the MFC app does not
> have**. Keep those out of `core/` — they belong in `platform/`, in Phase 5.

**D7. UI look — one unified QSS theme across all platforms** (the VS Code / Sublime model),
rather than native-per-platform. Rationale: VinaText already overrides the native look via MFC
visual managers, so this *preserves* product identity rather than changing it; it delivers the
long-open dark-theme request (issue #11) on all three platforms at once as a stylesheet swap;
and it avoids uncanny valley on macOS/Linux.
If native Windows look is ever wanted instead, Qt 6.7+ ships `QWindows11Style`
(`app.setStyle("windows11")`; also available: `windowsvista`, `windows`, `fusion`).

**D9. Phases 2 and 3 interleave. Extraction is demand-driven from here on.** (Decided
2026-07-31, revising the strictly sequential §5 order.) Phase 3 starts immediately around the
working spike; the remaining `core/` moves happen when — and in the shape that — `ui-qt/`
actually needs them. Four reasons, all from evidence this port already produced:

1. **Speculative extraction keeps mis-estimating; demand-driven extraction has not.** §6c of
   PORTING.md carries five corrections, every one an estimate made without a consumer. The
   moves driven by a real consumer (`LanguageData` for the spike, `Tokenizer`, `LineDiff`)
   landed clean. A second frontend is the ground truth for what `core/` needs; planning
   documents keep proving they are not.
2. **D6's API shapes are only validated by a second consumer.** One frontend (MFC) exercising
   the `std::wstring` boundaries proves nothing about whether they fit Qt. Finding a wrong
   shape at Phase 3 was always inevitable; finding it *now* is strictly cheaper than after
   `PathUtil`'s 353 sites and `AppSettings`' 783 have been committed to that shape.
3. **The two hardest files get easier, not harder.** `PathUtil` (353) and `AppSettings` (783)
   moved wholesale are the two riskiest changes in the plan. Driven by demand they split
   naturally: the slice the Qt shell needs this week, the rest when something needs it. Some
   of `AppSettings` maps to `QSettings` and may never need to move at all.
4. **The user is the tester, and feedback needs a surface.** Every week without a clickable
   alpha is a week of decisions made without the only feedback that matters for an editor.

**What this does NOT change:** the MFC build ships unchanged (its guardrail is untouched —
`ui-qt/` cannot modify `src/` except via `core/` extractions, which the Windows CI job
arbitrates). D6 stands. The alpha is gated by a fixed checklist, not by taste: tabs,
open/save, lexer + both themes from `core/LanguageData`, find, status bar — **no dialogs, no
docking, no settings UI**. Scope beyond the checklist waits for beta, so interleaving cannot
degenerate into everything-half-done.

> **Status, 2026-07-31: the checklist is met.** The 319-line spike is now 1,767 lines across
> nine files in `ui-qt/`, and every item is done: tabs (`QTabWidget`, close/modified/duplicate
> handling), open/save (encoding and line endings preserved), lexer + both themes from
> `core/LanguageData`, find (bar, not dialog — `src/FindDlg.cpp` is 899 lines of `.rc` and is
> Phase 5), status bar (position, language, encoding, EOL).
>
> **Nothing beyond it was built, and one thing was deliberately not pulled:** the user
> extension override in `UserCustomizeData.cpp`, because the shell does not need it yet.
> That is D9 working as intended.
>
> Verified headlessly rather than asserted: `--selftest` runs 117 checks over six files whose
> encodings and line endings are the point (CRLF+BOM, UTF-16, Latin-1, no trailing newline),
> and every document is saved to a copy that must be **byte-identical** to the original.
> `--screenshot` renders both themes to PNG, which CI uploads on Linux and macOS.
>
> The window chrome is still the platform's default — D7's unified QSS theme is not on the
> alpha checklist, so the editor is themed and the menu bar is not.

**D8. Do NOT rewrite git history to shrink the 163 MB.** Two forks exist; a rewrite breaks
every clone and issue link. ~~Stop tracking `include/boost`~~ — **deleted outright, it was
unused**. Stop tracking `bin/*.dll`, `lib/*.lib` going
forward (`git rm --cached` + `.gitignore`) once vcpkg lands. Tell contributors to
`git clone --depth 1`.

**D10. Ship a cross-platform EDITOR first. The IDE features are deferred, not
cancelled.** (Decided 2026-08-02 by the project owner.)

Phase 4 finished and the question "are we on track?" produced a number nobody had
been reporting: the MFC app has **613 `ON_COMMAND` handlers** and `ui-qt/` has
**15 menu actions**. Architecturally the port is proven — the strangler held across
five consecutive PRs with `src/` untouched, D2's Scintilla binding runs on Linux and
macOS in CI, `core/` links into both frontends. But the *product* is 613 commands,
and finishing all of them before shipping anything is how a port dies at 60%.

So the target is restated: **`ui-qt/` becomes a cross-platform text editor, not a
cross-platform VinaText.** Full parity remains the eventual goal and nothing here
forecloses it — every deferred file stays in `src/`, compiled and shipping on
Windows exactly as now.

**Deferred:**

| | LOC | |
|---|---:|---|
| `platform/`'s IDE half — `Compiler`, `Debugger`, `SystemInfo`, `HostView`, `HostManager`, `WindowsPrinter` | ~3,500 | Risk 3 already called `Debugger`/`Compiler` "its own project" |
| viewers and explorer — `FileExplorerCtrl`, `BuildWindow`, `ImageView`, `PdfView`, `MediaView`, `WebView` | ~10,200 | `FileExplorerCtrl` alone is 6,220 |
| 16 of the 30 dialogs | ~3,170 | path tools, project templates, spell-check, password, gamma |
| 6 of the 9 dock panes | — | build, path/search results, breakpoints, file explorer |

**Kept — what an editor needs:**

- **14 dialogs, 4,594 LOC**: `FindDlg`, `ReplaceDlg`, `GotoDlg`, `CodePageMFCDlg`,
  `AppAboutDlg` (required by D3 for the Qt LGPL attribution), the three settings
  pages, and the six small text-transform dialogs.
- **3 dock panes**: `MessageWindow` (done), `OpenTabWindows`, `BookmarkWindow`.
- `platform/`'s portable half: `OSUtil`, `SingleInstanceApp`, `UnicodeUtils`,
  `MultiThreadWorker`, `GuiUtils`.

That roughly halves Phase 5.

**The cost, stated plainly, because it is not free.** Phase 6 as written — "flip
Windows to Qt, delete `ui-mfc/`" — **cannot happen under D10.** A Windows user who
has a compiler and a debugger today cannot be given a Qt build that lacks them. So:

- **macOS and Linux get the Qt editor.** They have nothing today, so an editor
  without IDE features is a gain, not a regression.
- **Windows keeps shipping MFC** until parity, exactly as D5 already requires.
- **Dual maintenance (Risk 2) therefore extends indefinitely rather than ending at
  Phase 6.** That is the price of shipping early, and it is the thing to re-examine
  if the cost starts to bite.

**D10 collides with D4, and the collision has to be resolved rather than left.** D4
makes the fork "staging, not a permanent home" with "a defined exit: one consolidated
PR upstream" — *at Phase 6 cutover*. D10 blocks Phase 6 indefinitely. Taken together
those would strand the port in a fork forever, which is exactly what D4 was written to
prevent.

**So the exit moves earlier: the consolidated PR goes upstream when the macOS/Linux
editor ships, not at cutover.** `ui-qt/` is gated behind `VINATEXT_BUILD_QT` (default
OFF), so merging it into `vinadevs/master` changes nothing for the Windows build — the
same property that made the flag worth having in the first place. After that merge,
`ui-mfc/` and `ui-qt/` coexist on `master` and the deferred features are ordinary
backlog rather than a branch that has to be kept alive.

**And a working rule that follows from it — tier the rigour.** The standard applied
through Phase 4 (differential test against a verbatim transcription, mutation-check
every assertion, a documented section per feature) is correct where correctness is
*invisible*: data keyed five different ways, encoding round-trips, an algorithm that
hung on 6.4% of inputs. It is wrong for a menu command whose behaviour is obvious the
moment you click it. Applied uniformly to 613 commands it never finishes.

- **Heavy rigour**: anything extracted into `core/`, anything data-driven, anything
  where the two frontends could silently disagree.
- **Light rigour**: the shallow, visible tail — menu commands, dialog layouts, text
  transforms. A self-test check and a screenshot, not a differential test.

---

## 5. Phase plan

| Phase | Work | Ships to users |
|---|---|---|
| **0. Build** | `.sln` → CMake; ~~Boost 1.57~~ (removed outright) / curl / PDFium → vcpkg; GH Actions matrix | Windows, unchanged |
| **1. Break `stdafx.h`** | Per-file includes; PCH reduced to std headers only. Mechanical, ~420 files, parallelizable | Windows, unchanged |
| **2. Extract `core/`** | Move + `CString`→`QString`. Start with the 3 zero-`CString` files, end with `PathUtil` (106 sites) | Windows, unchanged |
| **3. Qt shell** | `QMainWindow`, `QTabWidget`, 9 × `QDockWidget`. **Interleaves with Phase 2 from 2026-07-31 (D9)** — alpha checklist ✅ done: tabs, open/save, lexer + themes, find, status bar. Docks are beta | First Linux/macOS **alpha** |
| **4. Editor** | `ScintillaEditBase` + port `EditorLexerDark` / `EditorLexerLight`. ~~`LexerParser`~~ — done early as `core/Tokenizer` (#25); it was a delimiter tokenizer, not a lexer | Usable **beta** |
| **5. Dialogs + platform** | **Scoped by D10 to what an editor needs: 14 of the 30 `*Dlg` (the count is 30, not ~40 — re-derive it), 3 of the 9 dock panes, `platform/`'s portable half.** The IDE half is deferred | macOS + Linux **editor** |
| **6. Cutover** | ~~Flip Windows to Qt, delete `ui-mfc/`~~ — **blocked by D10 and deliberately so.** Windows keeps MFC until parity; unify installers when it arrives | Qt on all three, eventually |

- Phases 0–2 are **~40% of total effort with near-zero user-facing risk**.
- **Phases 0–4 are DONE as of 2026-08-01** (#30–#38). Phase 5 began with #39.
- **Order is 0 → 1 → (2 ∥ 3) → 4 → 5 → 6 since D9**: Phase 2's remainder is a demand-driven
  backlog (`FileUtil` → `FindReplaceTextWorker` → `StringHelper` rest → `PathUtil` →
  `AppSettings`, per PORTING.md §6c correction 5), pulled when `ui-qt/` needs each piece.
- Phase 4 is the cheapest big win — Scintilla's message API is identical across platforms, so
  lexer and theme work survives largely intact.
- Phase 5 is the long tail and the most parallelizable across a team.
- **Total: 6–12 months for 1–2 devs** for full parity. D10's editor-first scope is
  roughly half of Phase 5, so a shippable macOS/Linux editor lands considerably
  sooner — at the price of Phase 6 moving out indefinitely.

### Phase 5 platform mapping

| Current | Replacement |
|---|---|
| `ShellContextMenu` (COM) | Own `QMenu`; drop OS shell integration off Windows |
| `CWMPHost` / `MediaView` | Qt Multimedia *(verify licence)* or libmpv |
| `PdfView` / PDFium | PDFium is already cross-platform — needs a Qt paint surface |
| `WebView` / `WebDoc` | QtWebEngine *(verify licence; heavy — consider dropping)* |
| `WindowsPrinter` | `QPrinter` |
| ~~`DirectoryNotifier`~~ | **Not needed** — never compiled into the product, deleted |
| `SingleInstanceApp` | `QLocalServer` |
| `Compiler` / `Debugger` | gdb / lldb per platform |
| Inno Setup | Keep for Windows; `.dmg` macOS; AppImage Linux |

---

## 6. Risks

1. **Phase 1 is boring and huge** — ~420 files touched, zero visible payoff. It is the phase
   most likely to be abandoned halfway, which leaves the repo *worse* than not starting.
   Timebox it and finish it.
2. **Dual maintenance** of `ui-mfc/` and `ui-qt/` during Phases 3–5 is a real recurring cost.
   Minimize by pushing logic into `core/` aggressively in Phase 2 — fixes there serve both.
3. **`Debugger.cpp` / `Compiler.cpp` are the least portable non-UI code.** gdb/lldb integration
   per platform is its own project. Consider shipping the first macOS/Linux releases with the
   debugger disabled.
4. **The ribbon request (open issue #7) gets harder, not easier.** MFC gives `CMFCRibbonBar`
   free; Qt has no built-in ribbon. Third-party options (SARibbon, Qtitan) exist — **verify
   their licences against the MIT goal**, same trap as QScintilla.
5. ~~**`AppLookDlg`'s Office2007/VS2008 skin picker goes away** under D7.~~ **Retired — this
   risk was based on dead code.** `AppLookDlg.cpp` was never in the build, so no user can
   notice it disappearing. A light/dark/custom QSS theme picker under D7 is now a pure
   addition rather than a replacement.

---

## 7. Open questions (not yet decided)

- Exact Qt version to target (affects `QWindows11Style` availability — needs ≥6.7 — and the
  per-module licence verification in D3).
- vcpkg vs Conan.
- Whether to keep a web viewer at all (QtWebEngine is a Chromium-sized dependency).
- Whether the first macOS/Linux release ships with the debugger disabled (see Risk 3).
- `QTabWidget` vs `QMdiArea` for the document area — leaning `QTabWidget` (drop true MDI).

---

## 8. Suggested first task for this session

The agreed next deliverable is a **per-file triage**: classify all **137** `src/*.cpp` (top
level only — see §2) into `core` / `platform` / `ui-rewrite` / `delete`, with `CString` counts
per file, so Phases 1–2 become assignable, parallelizable work.

Useful commands (all verified against the clone):

```bash
# CString density per file, descending — drives Phase 2 ordering
for f in src/*.h src/*.cpp; do
  printf "%-6s %s\n" "$(grep -oE '\bCString\b' "$f" | wc -l | tr -d ' ')" "$f"
done | sort -rn | head -60

# Zero-CString files — the ready-to-move core pool (expect 85)
for f in src/*.cpp src/*.h; do
  [ "$(grep -cE '\bCString\b' "$f")" -eq 0 ] && echo "$f"
done

# Win32/MFC API surface per file — separates 'core' from 'platform'
grep -lE '\b(afx|CWnd|CDialog|CDocument|CView|HWND|CoCreateInstance|IShell)' src/*.cpp

# Everything that transitively depends on the PCH — note the -i, ALL 137 match
grep -il 'include.*stdafx\.h' src/*.cpp | wc -l
```

> ⚠️ **Corrected.** This section originally read *"133 of 137 `.cpp` include `stdafx.h`. The 4
> that don't are the cheapest possible starting point for Phase 1."* **That is wrong — all 137
> include it.** The four apparent outliers spell it `StdAfx.h`, which a case-sensitive `grep`
> misses; on Windows' case-insensitive filesystem they resolve to the same header. There is no
> free head start, and three of those four are `delete` anyway.
> See [`PORTING.md`](PORTING.md) §2 for the evidence and §4 for the real cheapest starting
> point (7 zero-`CString` core files).

Write the result to `doc/PORTING.md` in the repo (singular `doc/` — see §9).

---

## 8b. Existing project docs worth reading

Already in `doc/` — read before writing code:

- **`code_convention.txt`** — the project's style hints. Notably: **C++11 or newer**; types
  *and* functions/variables start upper case (`MyClass`, `MyMethod`); constants and macros all
  caps; `m_` prefix for private members; Allman braces. Match this in new `core/` and `ui-qt/`
  code — do not silently switch to Qt's own lowerCamelCase method convention.
- **`road_map_2023.md`** — pre-existing feature roadmap (diff view, markdown preview, HTML
  preview, file explorer improvements). Check for overlap before building anything new;
  some items may be cheaper to land in Qt than to port from MFC.
- **`architecure_diagram.png`**, `application_description.txt`, `application_manual.txt` —
  background on intended architecture and current behaviour.
- `Screenshot_App_1..5.png` — the current MFC UI. Useful as the visual baseline when building
  `ui-qt/`, keeping D7 in mind (the look intentionally modernizes; these are reference,
  not a pixel target).

---

## 9. How to use this document

This file lives at **`doc/QT-PORT-BRIEF.md`** (the repo's existing docs directory — note it is
`doc/`, singular). It is genuine project documentation the whole team benefits from, not just
Claude context; commit it. Then add a short `CLAUDE.md` at the repo root:

```markdown
# CLAUDE.md

This repo is mid-migration from Windows-only MFC to cross-platform Qt.

**Read `doc/QT-PORT-BRIEF.md` before proposing any architectural change,
adding any dependency, or touching `stdafx.h`.**
**Read `doc/PORTING.md` before moving, rewriting, or deleting any file in `src/`** —
it classifies all 137 `src/*.cpp` into core / platform / ui-rewrite / delete.

Hard rules:
- VinaText is MIT. Never add a GPL dependency. Never use QScintilla (use upstream
  Scintilla's `qt/ScintillaEditBase`). Never link Qt statically.
- All port work targets the `port/cross-platform` branch. PRs point at `port/cross-platform`, never `master`.
  `master` is for shipping MFC releases only, until Phase 6 cutover.
- The MFC build must keep building and shipping. Do not break it.
- New Qt code goes in `ui-qt/`, gated behind the `VINATEXT_BUILD_QT` CMake option.
```
