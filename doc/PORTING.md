# VinaText → Qt: per-file triage of `src/*.cpp`

> Deliverable for **§8** of [`QT-PORT-BRIEF.md`](QT-PORT-BRIEF.md). Classifies all **137**
> top-level `src/*.cpp` into `core` / `platform` / `ui-rewrite` / `delete` so Phases 1–2
> become assignable, parallelizable work.
>
> Measured against the working tree on **2026-07-26**. Vendored trees (`src/uchardet/`,
> `src/pdf/`, `src/tinyXml/`) are **excluded** — they are not ours to rewrite (§2).

---

## 1. Summary

| Bucket | Files | `CString` in `.cpp` | `CString` in paired `.h` | LOC | Meaning |
|---|---:|---:|---:|---:|---|
| **core/** | 29 | 411 | 273 | 10,717 | Moves to `core/`, `CString`→`QString`. UI-free, portable, links into **both** frontends. |
| **platform/** | 13 | 234 | 145 | 5,571 | Moves to `platform/`, needs win/mac/linux impls behind one interface. |
| **ui-rewrite** | 60 | 1,537 | 405 | 49,475 | Rebuilt in `ui-qt/`. Logic worth salvaging; the MFC shell is not. |
| **delete** | 35 | 110 | 59 | 11,329 | No Qt counterpart needed — framework scaffolding, custom controls Qt ships natively, or Windows-only ActiveX/COM. |
| **Total** | **137** | **2,292** | **882** | **77,092** | |

### The 137 is a frozen baseline, not a live count

Every figure in this document is the **2026-07-26 measurement** and is deliberately not
re-run as work lands, so that §4's estimates stay comparable against the ground they were
made on. Files that have since left `src/` are annotated **→ `core/`** in the tables below
and listed here:

| moved | to | PR | rows annotated |
|---|---|---|---|
| `LexerParser.{h,cpp}` | `core/Tokenizer.{h,cpp}` | #25 | §3, §5, §6c |
| `DiffEngine.cpp` (algorithm only) | `core/LineDiff.{h,cpp}` | #26 | §3, §5, §6c |

Three files left the `core/` list without moving anywhere — `EditorDatabase` (duplicates a
`core/` type), `SpellChecker` (→ `platform/`) and `UserExtension` (→ `ui-rewrite`). §2's
`core/` bucket of 26 is therefore **23**. See §6c correction 5.

**`src/*.cpp` is 124 today, not 137** — §2's thirteen dead files account for the rest. Re-run
§7's commands for a current count; do not read one off these tables. The first draft of this
very paragraph said "136", by subtracting 1 from the frozen 137 instead of counting, which is
the mistake the paragraph exists to warn against.

`CString` is **3,146** by §7's `\bCString\b` convention, and reconciles against §2's 3,165
exactly: −22 for `LexerParser`, +2 for the two `CString(...)` boundary conversions #25 adds at
the call sites, +1 for #26's (`DiffEngine.cpp` loses one declaration in the moved algorithm and
gains two in the adapter). (Note the word boundary: an unanchored `grep CString` returns 3,947,
because it also counts `CStringArray`, `CStringList` and `CStringT`.)

**Boundary conversions mean `CString` will not fall monotonically.** Each extraction removes
uses from the moved code and adds a few back at the frontend edge; the count only drops for
real once a call site stops speaking `CString` at all, which is Phase 3+. Judge Phase 2 by what
has moved, not by this number.

### Reconciling with the brief's 4,138

The table above accounts for 2,292 + 882 = **3,174** `CString` occurrences, not 4,138. The
missing 964 are in **18 headers that have no paired `.cpp`**, so they never appear in a
per-file triage row:

| `CString` | Header | |
|---:|---|---|
| 504 | `modellanguages.h` | **dead code — deleted**, see §4 |
| 211 | `EditorColorLight.h` | static data table → JSON, see §4 |
| 210 | `EditorColorDark.h` | static data table → JSON, see §4 |
| 15 | `SearchEngine.h` | |
| 8 | `STLHelper.h` | |
| 8 | `EditorDataStructure.h` | |
| 7 | `Templates.h` | |
| 1 | `EditorCommonDef.h` | |
| **964** | (10 further headers contain none) | |

`2,292 + 882 + 964 = 4,138.` ✅

The three data tables are 925 of that 964 — the 22% the brief flags as a mechanical win. The
remaining **39** are spread across five ordinary headers and will be carried along with
whichever `.cpp` files include them; they need no separate plan.

**LOC, likewise:** the brief's "~101,000 LOC" is `.cpp` **+** `.h` combined (101,110
measured). The 77,092 above is the **137 `.cpp` only**. Both figures are correct; they measure
different things.

**What this changes about the plan:**

- **Phase 2 (`core/` extraction) is 684 `CString` sites across 29 files, not 4,138.**
  Another 379 land in `platform/` (Phase 5), 169 are deleted outright and never
  migrated at all, and 1,942 sit in code that gets rewritten rather than translated.
- **35 of 137 files (11,329 LOC) are deletions, not ports.** Roughly a quarter of the
  file count is MFC scaffolding with a direct Qt built-in replacement.
- **`ui-rewrite` is 60 files and 49,475 LOC — half the corpus and the real cost centre.**
  Phases 3–5 dominate; Phases 0–2 remain the cheap, low-risk part as the brief says.

---

## 2. Thirteen files were never in the build — deleted

> ⚠️ **The triage tables in §1 and §3 describe the tree as measured on 2026-07-26, before
> this deletion.** They still sum to 137 files and 4,138 `CString`. The current tree is
> **125 `.cpp` / 3,165 `CString`**. The rows for the thirteen files below are left in place so
> the reasoning stays auditable — but the bucket totals move, and **§1's counts are the ones
> to correct when planning work**:
>
> | Bucket | §1 says | Now | Deleted from it |
> |---|---:|---:|---|
> | `core/` | 29 | **26** | `Observer`, `Subject`, `FixedBlockMemory` |
> | `platform/` | 13 | **11** | `DirectoryNotifier`, `DirectoryNotifyManager` |
> | `ui-rewrite` | 60 | **55** | `FindResult`, `LearnPrograming`, `ProjectManager`, `TerminalWindow`, `TextReferenceWindow` |
> | `delete` | 35 | **32** | `AppLookDlg`, `MDIClientWnd`, `SearchEditToolBar` |
> | **Total** | **137** | **125** | |
>
> Note `ui-rewrite` absorbs five of the thirteen — the dead code was concentrated in the
> most expensive bucket, so the saving lands where it helps most.

`src/VinaText.vcxproj` lists every `.cpp` it compiles — MSBuild does not glob. Thirteen files
are absent from it, so they have never been compiled into the shipping product, and no built
code references any of their classes:

| File | LOC | The brief described it as |
|---|---:|---|
| `AppLookDlg` | 243 | the Office2007/VS2008 skin picker — §3c, **Risk 5** |
| `TerminalWindow` | 62 | one of the 11 dock panes; a Windows-only subsystem |
| `TextReferenceWindow` | 61 | one of the 11 dock panes |
| `DirectoryNotifier` | 343 | Windows-only subsystem → `QFileSystemWatcher` (Phase 5) |
| `DirectoryNotifyManager` | 35 | same |
| `ProjectManager` | 262 | — |
| `LearnPrograming` | 212 | — |
| `FindResult` | 548 | — |
| `MDIClientWnd` | 46 | — |
| `SearchEditToolBar` | 71 | — |
| `Observer` / `Subject` | 84 | Wave 1 "cheapest start" (§4) |
| `FixedBlockMemory` | 58 | Wave 1 "cheapest start" (§4) |

Their headers went with them. One built file, `EditorView.cpp`, carried a leftover
`#include "DirectoryNotifier.h"` without using the class; that include was removed.

**Consequences for the plan — all of these are now less work, not more:**

- **The "11 dock panes" are 9.** Built: `BookmarkWindow`, `BreakpointWindow`, `BuildWindow`,
  `MessageWindow`, `OpenTabWindows`, `PathResultWindow`, `SearchResultWindow`,
  `SearchAndReplaceWindow`, `FileExplorerWindow`.
- **`DirectoryNotifier` → `QFileSystemWatcher` is work that does not need doing.**
- **Risk 5 is moot.** Users cannot notice the skin picker disappearing; it is not in the
  product they run.

**How this was missed.** The triage scored files by `CString` count and MFC/Win32 token
density. Neither measures *build membership*. Three of these files were sitting in §4's
"Wave 1 — the true cheapest start" until an attempt to move them found that nothing uses
them. §7 of this document already warned that bucket assignment "is not mechanically
derivable from the counts alone" — that caveat applied here and was not applied.

---

## 2b. One correction to the brief

> §8 states *"133 of 137 `.cpp` include `stdafx.h`. The 4 that don't are the cheapest
> possible starting point for Phase 1."*

**All 137 include the PCH.** The four outliers spell it `StdAfx.h` (capitalised), which the
case-sensitive grep missed — on Windows' case-insensitive filesystem they resolve to the
same header:

```
src/DockPaneBase.cpp:9          #include "StdAfx.h"
src/FixedBlockMemory.cpp:9      #include "StdAfx.h"
src/ProgramStatusBarEx.cpp:33   #include "StdAfx.h"
src/VinaTextProgressBar.cpp:33  #include "StdAfx.h"
```

Use `grep -il` when counting. There is no free head start; **Phase 1 is 137/137.**
Three of those four are `delete` anyway, so they were never a useful starting point.

The real cheapest Phase 1 starting point is §4 below.

---

## 3. Full triage

Columns: **CS** = `CString` occurrences in the `.cpp` (`+n` = additional occurrences in the
paired `.h`, which moves with it) · **Win32** = Win32/COM API tokens · **MFC** = MFC/ATL
class tokens · **std** = `std::` occurrences.

### 3.1 `core/` — 29 files

| File | LOC | CS | Win32 | MFC | std | Note |
|---|---:|---:|---:|---:|---:|---|
| `PathUtil.cpp` | 1,585 | 202 +106 | 101 | 0 | 51 | THE Phase 2 boss file: 202 CString in .cpp + 106 in .h, plus 101 Win32 path-API tokens. Do last. |
| `FilePartition.cpp` | 530 | 42 +25 | 15 | 0 | 0 | CArray subclass -> QVector. |
| `FindReplaceTextWorker.cpp` | 362 | 30 +32 | 18 | 4 | 8 | Find/replace over Scintilla buffers. Threading -> platform/. |
| `EditorDatabase.cpp` | 92 | 16 +24 | 0 | 0 | 0 | **Not an open-document registry** — `CLanguageDatabase` is per-language metadata, and `core/`'s `SLanguageInfo` already holds it. **Stays in `ui-mfc/`** (§6c correction 5). |
| `FileUtil.cpp` | 406 | 19 +14 | 7 | 0 | 41 | Already heavily std::-based (41 std:: lines). |
| `DiffEngine.cpp` | 324 | 20 +12 | 6 | 0 | 0 | **Algorithm → `core/LineDiff` (#26); HTML renderer stays.** Row understates it: the file is ~half report generator. See §6c correction 4. |
| `FindPathWorker.cpp` | 241 | 16 +10 | 8 | 0 | 6 | Path search worker. Threading moves to platform/. |
| `AppSettings.cpp` | 414 | 8 +17 | 6 | 0 | 0 | Config already file-based (issue #44). Maps onto QSettings. |
| `LexerParser.cpp` | 297 | 13 +9 | 14 | 0 | 0 | **→ `core/Tokenizer` (#25).** Not language parsing — a delimiter tokenizer; 7 of 10 methods had no caller. See §6c correction 3. |
| `UserExtension.cpp` | 170 | 12 +6 | 2 | 2 | 0 | **→ `ui-rewrite`, not `core/` (§6c correction 5):** `LoadMenuUserExtensions(CMenu*)` is MFC in the public API. |
| `TemplateCreator.cpp` | 145 | 7 +2 | 2 | 1 | 0 | File-template generation. |
| `RecentCloseFileManager.cpp` | 61 | 4 +4 | 2 | 0 | 1 | MRU list. |
| `UserCustomizeData.cpp` | 48 | 5 +2 | 0 | 0 | 5 | std::fstream config reader. |
| `EditorLexerDark.cpp` | 995 | 4 +1 | 0 | 0 | 0 | Scintilla lexer/theme table. Phase 4; SCI_ API is identical on Qt. |
| `EditorLexerLight.cpp` | 995 | 4 +1 | 0 | 0 | 0 | Scintilla lexer/theme table. Phase 4; SCI_ API is identical on Qt. |
| `LocalizationDatabase.cpp` | 183 | 2 +2 | 16 | 0 | 1 | String table. |
| `RAIIUtils.cpp` | 62 | 1 +2 | 0 | 0 | 0 | Split: CCriticalSectionLock/CMemoryGuard/CBenchmarkTest are core; CLockCtrlRedraw/CLockCtrlUpdate/CMultipleSelectionKeeper are editor-widget guards -> ui-qt/. |
| `LocalizationHandler.cpp` | 85 | 2 | 3 | 3 | 1 | Loads localization files; port to QTranslator or keep as-is. |
| `SpellChecker.cpp` | 279 | 1 +1 | 11 | 1 | 15 | **→ `platform/`, not `core/` (§6c correction 5):** holds `ISpellChecker*` members; Windows COM, needs a different backend per OS. |
| `TemporarySettings.cpp` | 10 | 0 +2 | 0 | 0 | 0 | 9 lines, trivial. |
| `Textfile.cpp` | 1,551 | 1 +1 | 130 | 0 | 0 | Encoding detection (uchardet) + file I/O. Split: swap Win32 CreateFile/ReadFile for QFile, keep the codec logic. |
| `WebHandler.cpp` | 191 | 2 | 0 | 0 | 19 | curl-based HTTP. 19 std:: lines, zero Win32. |
| `ComboboxRegexHelper.cpp` | 156 | 0 | 0 | 3 | 0 | **MISCLASSIFIED — belongs in `ui-rewrite`.** Its API is `PopulateRegexFields(CComboBox&)`; it is the combobox binding, not a data table. Zero `CString` hid pure MFC UI. |
| `FixedBlockMemory.cpp` | 58 | 0 | 0 | 0 | 0 | Custom allocator. No Win32, no PCH. |
| `Observer.cpp` | 26 | 0 | 2 | 0 | 0 | Observer pattern base. |
| `StringHelper.cpp` | 477 | 0 | 13 | 0 | 28 | Zero CString, 28 std:: lines. Pure string ops. |
| `Subject.cpp` | 58 | 0 | 2 | 0 | 1 | Observer pattern base. |
| `TextFormatConverter.cpp` | 563 | 0 | 0 | 0 | 86 | Zero CString, 86 std:: lines. Formerly used boost::algorithm; now `core/Checksum.h`. |
| `UnicodeUtils.cpp` | 353 | 0 | 5 | 0 | 46 | **MISCLASSIFIED — belongs in `platform/`.** 8 Win32 codepage calls (`MultiByteToWideChar`, `WideCharToMultiByte`, `CP_ACP`/`CP_UTF8`). Needs a portable codec, not a straight move. |

### 3.2 `platform/` — 13 files

| File | LOC | CS | Win32 | MFC | std | Note |
|---|---:|---:|---:|---:|---:|---|
| `Compiler.cpp` | 1,373 | 109 +68 | 9 | 0 | 40 | Toolchain invocation. Risk 3 in the brief. |
| `OSUtil.cpp` | 457 | 35 +24 | 50 | 0 | 11 | 50 Win32 tokens. Needs win/mac/linux impls. |
| `Debugger.cpp` | 644 | 40 +15 | 2 | 0 | 4 | gdb/lldb per platform. Risk 3 - consider shipping alpha with it disabled. |
| `SystemInfo.cpp` | 817 | 17 +8 | 49 | 0 | 19 | 49 Win32 tokens (winperf, iphlpapi). |
| `HostView.cpp` | 258 | 9 +4 | 17 | 9 | 0 | Embeds a foreign HWND via job objects. No portable analogue - rebuild on QProcess. |
| `GuiUtils.cpp` | 240 | 5 +4 | 24 | 9 | 4 | Split: DPI/metrics/shell-open helpers are platform; the CWnd drawing helpers are ui-rewrite. |
| `Cryptography.cpp` | 436 | 5 +3 | 33 | 0 | 0 | BCrypt* (CNG) AES. Needs a portable crypto backend; QtCore only gives you hashing. |
| `DirectoryNotifier.cpp` | 343 | 4 +4 | 18 | 2 | 1 | ReadDirectoryChangesW -> QFileSystemWatcher. Needs an event loop, so platform/ not core/ (D6 caveat). |
| `HostManager.cpp` | 73 | 2 +6 | 2 | 2 | 0 | Registry of embedded external processes. |
| `MultiThreadWorker.cpp` | 438 | 4 +4 | 58 | 0 | 3 | 58 Win32 tokens (threads/events) -> QThread/QThreadPool. Event-loop dependent. |
| `DirectoryNotifyManager.cpp` | 35 | 2 +2 | 1 | 0 | 0 | Owner of the above. |
| `WindowsPrinter.cpp` | 348 | 1 +2 | 11 | 0 | 0 | -> QPrinter. Also drives Scintilla print messages. |
| `SingleInstanceApp.cpp` | 109 | 1 +1 | 12 | 0 | 0 | -> QLocalServer/QLocalSocket. |

### 3.3 `ui-rewrite` — 60 files

| File | LOC | CS | Win32 | MFC | std | Note |
|---|---:|---:|---:|---:|---:|---|
| `EditorView.cpp` | 9,860 | 495 +37 | 103 | 15 | 228 | 9,859 lines, 495 CString, 228 std:: lines. Split hard: most command handlers are core logic behind an MFC message map. |
| `FileExplorerCtrl.cpp` | 6,217 | 242 +77 | 105 | 43 | 15 | 6,217 lines, 242 CString -> QFileSystemModel + QTreeView. |
| `Editor.cpp` | 5,221 | 97 +64 | 262 | 2 | 50 | CEditorCtrl : CWnd Scintilla wrapper -> ScintillaEditBase (D2). The SCI_* message logic survives largely intact. |
| `AppUtil.cpp` | 2,478 | 96 +59 | 52 | 89 | 124 | Split: BasicColors + generic AppUtils helpers are core; LOG_*_MESSAGE routes into dock panes and is UI. |
| `MainFrm.cpp` | 4,486 | 88 +13 | 91 | 136 | 9 | CMDIFrameWndEx + 56 CMFC* -> QMainWindow. The single biggest UI rewrite. |
| `BuildWindow.cpp` | 1,885 | 59 +17 | 48 | 38 | 2 | Dock pane. CRichEditCtrl -> QPlainTextEdit. |
| `ReplaceDlg.cpp` | 981 | 39 +11 | 20 | 20 | 10 | Dialog -> .ui |
| `SearchResultWindow.cpp` | 1,175 | 40 +7 | 19 | 28 | 11 | Dock pane. |
| `PathResultWindow.cpp` | 1,168 | 37 +6 | 23 | 28 | 8 | Dock pane. |
| `FindDlg.cpp` | 898 | 31 +10 | 18 | 20 | 10 | Dialog -> .ui |
| `FindResult.cpp` | 548 | 30 +10 | 13 | 15 | 0 | CDockablePane + CListBox. |
| `BookmarkWindow.cpp` | 832 | 32 +2 | 10 | 26 | 2 | Dock pane. |
| `QuickReplace.cpp` | 516 | 27 +5 | 8 | 7 | 1 | Inline editor popup. |
| `BreakpointWindow.cpp` | 831 | 29 +2 | 10 | 26 | 2 | Dock pane. |
| `QuickSearch.cpp` | 493 | 21 +5 | 8 | 7 | 2 | Inline editor popup. |
| `VinaTextApp.cpp` | 949 | 17 +8 | 16 | 14 | 6 | CWinAppEx -> QApplication. Doc templates and GDI+ init disappear. |
| `MessageWindow.cpp` | 582 | 18 +4 | 14 | 28 | 0 | Dock pane. |
| `BookMarkPathDlg.cpp` | 468 | 18 +2 | 10 | 1 | 0 | Dialog -> .ui |
| `SearchNavigatePathDlg.cpp` | 554 | 13 +4 | 6 | 3 | 3 | Dialog -> .ui |
| `FileExplorerView.cpp` | 810 | 7 +9 | 27 | 44 | 1 | CView + 29 COM tokens (shell interfaces). |
| `ImageView.cpp` | 1,248 | 10 +3 | 14 | 13 | 0 | GDI+ -> QImage/QGraphicsView. |
| `PathComparatorDlg.cpp` | 155 | 8 +2 | 8 | 8 | 0 | Dialog -> .ui |
| `SelectedPathDlg.cpp` | 149 | 9 +1 | 1 | 1 | 0 | Dialog -> .ui |
| `CreateNewMultiplePath.cpp` | 161 | 9 | 3 | 1 | 0 | Dialog -> .ui |
| `SearchAndReplaceDlg.cpp` | 444 | 5 +4 | 6 | 16 | 0 | Dialog -> .ui |
| `SearchAndReplaceWindow.cpp` | 124 | 4 +4 | 6 | 0 | 0 | Dock pane. |
| `FileExplorerDlg.cpp` | 249 | 5 +2 | 5 | 10 | 0 | Explorer dialog. |
| `GotoDlg.cpp` | 291 | 6 +1 | 6 | 18 | 1 | Dialog -> .ui |
| `MediaView.cpp` | 311 | 6 +1 | 3 | 2 | 0 | Qt Multimedia or libmpv - licence unverified (D3). |
| `PdfView.cpp` | 480 | 6 +1 | 3 | 3 | 2 | PDFium is already portable; needs a Qt paint surface. |
| `QuickSearchDialog.cpp` | 332 | 4 +3 | 1 | 19 | 0 | Dialog -> .ui |
| `OpenTabWindows.cpp` | 213 | 6 | 7 | 1 | 0 | Dock pane. |
| `ReopenFileWithPasswordDlg.cpp` | 119 | 3 +2 | 3 | 3 | 0 | Dialog -> .ui |
| `SaveFileWithPassWordDlg.cpp` | 114 | 3 +2 | 3 | 3 | 0 | Dialog -> .ui |
| `BracketOutLineDlg.cpp` | 277 | 3 +1 | 4 | 12 | 1 | Dialog -> .ui |
| `CodePageMFCDlg.cpp` | 148 | 4 | 4 | 3 | 0 | Encoding picker dialog. |
| `MisspelledReplaceWithDlg.cpp` | 108 | 2 +2 | 1 | 1 | 2 | Dialog -> .ui |
| `ProjectTempateCreatorDlg.cpp` | 198 | 2 +2 | 1 | 7 | 0 | Dialog -> .ui |
| `GeneralSettingDlg.cpp` | 430 | 1 +2 | 9 | 14 | 1 | Settings page. CMFCEditBrowseCtrl -> QLineEdit + browse button. |
| `InsertAfterWordInLineDlg.cpp` | 53 | 0 +3 | 1 | 1 | 0 | Dialog -> .ui |
| `SetDeleteFileByExtDlg.cpp` | 68 | 1 +2 | 1 | 1 | 0 | Dialog -> .ui |
| `ExplorerSettingDlg.cpp` | 182 | 0 +2 | 9 | 12 | 1 | Settings page. |
| `FilenameFilterDlg.cpp` | 53 | 1 +1 | 1 | 1 | 0 | Dialog -> .ui |
| `InsertFromPositionXDlg.cpp` | 53 | 0 +2 | 1 | 1 | 0 | Dialog -> .ui |
| `ProgrammingSettingDlg.cpp` | 216 | 0 +2 | 9 | 12 | 1 | Settings page. |
| `QuickGotoLine.cpp` | 68 | 2 | 1 | 7 | 0 | Inline editor popup. |
| `RemoveAfterBeforeWordDlg.cpp` | 48 | 0 +2 | 1 | 1 | 0 | Dialog -> .ui |
| `RemoveFromXToYDlg.cpp` | 48 | 0 +2 | 1 | 1 | 0 | Dialog -> .ui |
| `EditWithXDlg.cpp` | 40 | 0 +1 | 1 | 1 | 0 | Dialog -> .ui |
| `EditorSettingDlg.cpp` | 1,002 | 0 +1 | 6 | 11 | 1 | Settings page, 1,002 lines. |
| `InsertBetweenLines.cpp` | 52 | 0 +1 | 1 | 2 | 0 | Dialog -> .ui |
| `SetBookmarkPathDlg.cpp` | 45 | 0 +1 | 1 | 1 | 0 | Dialog -> .ui |
| `VinaTextSettingDlg.cpp` | 289 | 1 | 3 | 40 | 0 | Settings host; 30 CMFC* tokens. |
| `AppAboutDlg.cpp` | 31 | 0 | 1 | 4 | 0 | Must gain the Qt LGPLv3 attribution + version (D3). |
| `FileExplorerWindow.cpp` | 86 | 0 | 2 | 3 | 0 | Dock pane 11/11. |
| `GammaDlg.cpp` | 41 | 0 | 1 | 1 | 0 | Dialog -> .ui |
| `LearnPrograming.cpp` | 212 | 0 | 4 | 8 | 0 | CDockablePane + CMFCToolBar. |
| `ProjectManager.cpp` | 262 | 0 | 1 | 12 | 0 | CDockablePane + CMFCToolBar. |
| `TerminalWindow.cpp` | 62 | 0 | 2 | 0 | 0 | Dock pane shell; the pty backend is platform/ work. |
| `TextReferenceWindow.cpp` | 61 | 0 | 2 | 0 | 0 | Dock pane. |

### 3.4 `delete` — 35 files

| File | LOC | CS | Win32 | MFC | std | Note |
|---|---:|---:|---:|---:|---:|---|
| `WndResizer.cpp` | 2,611 | 23 +19 | 144 | 66 | 0 | 2,610-line fixed-pixel resize framework -> QLayout. Largest pure deletion. |
| `UndoRedoEditControl.cpp` | 772 | 16 +8 | 35 | 15 | 0 | CEdit undo stack -> QLineEdit/QUndoStack. |
| `EditorDoc.cpp` | 426 | 14 +3 | 5 | 1 | 1 | Doc/view plumbing; state moves into core/. |
| `ComboboxMultiLine.cpp` | 894 | 9 +5 | 146 | 26 | 0 | 893-line CComboBox subclass -> QComboBox. |
| `ImageDoc.cpp` | 212 | 9 +3 | 5 | 0 | 0 | Doc/view plumbing. |
| `CommandLine.cpp` | 222 | 6 +3 | 19 | 0 | 0 | CCommandLineInfo -> QCommandLineParser. |
| `ShellContextMenu.cpp` | 491 | 7 +2 | 10 | 30 | 0 | COM IContextMenu -> own QMenu (§5). |
| `FileBrowser.cpp` | 211 | 6 | 14 | 2 | 0 | CFileDialog subclass -> QFileDialog. |
| `VinaTextProgressBar.cpp` | 183 | 2 +3 | 0 | 1 | 0 | -> QProgressBar. One of the 4 non-PCH files. |
| `GifHandler.cpp` | 555 | 2 +2 | 14 | 2 | 4 | GDI+ Gdiplus::Image animation -> QMovie. |
| `TextProgressCtrl.cpp` | 909 | 2 +2 | 66 | 26 | 0 | CProgressCtrl subclass -> QProgressBar. |
| `TreeHelper.cpp` | 698 | 3 +1 | 45 | 11 | 3 | CTreeCtrl helpers -> QTreeView/QAbstractItemModel. |
| `BaseDoc.cpp` | 103 | 3 | 6 | 7 | 0 | CDocument base. |
| `CodePageMFC.cpp` | 87 | 1 +1 | 2 | 0 | 0 | Win32 codepage enumeration -> QStringConverter. |
| `MediaDoc.cpp` | 81 | 1 +1 | 2 | 0 | 0 | Doc/view plumbing. |
| `PdfDoc.cpp` | 81 | 1 +1 | 2 | 0 | 0 | Doc/view plumbing. |
| `WebDoc.cpp` | 74 | 1 +1 | 1 | 5 | 0 | Doc/view plumbing; see WebView. |
| `WebView.cpp` | 86 | 1 +1 | 2 | 5 | 0 | CHtmlView (IE ActiveX). QtWebEngine is unverified + Chromium-sized (§7) - dropping is on the table. |
| `ComboBoxExtList.cpp` | 183 | 1 | 41 | 8 | 0 | CListBox subclass -> QComboBox view. |
| `DialogBase.cpp` | 37 | 0 +1 | 1 | 4 | 0 | CDialogEx base -> QDialog. |
| `MultiDocTemplateEx.cpp` | 102 | 1 | 6 | 7 | 0 | CMultiDocTemplate. MFC doc/view has no Qt counterpart. |
| `SearchEditToolBar.cpp` | 71 | 1 | 4 | 0 | 0 | -> QToolBar + QLineEdit. |
| `ViewBase.cpp` | 71 | 0 +1 | 3 | 8 | 0 | CView base. |
| `stdafx.cpp` | 11 | 0 +1 | 0 | 0 | 0 | The PCH itself (§3a). Dies at the end of Phase 1. |
| `AppLookDlg.cpp` | 243 | 0 | 2 | 31 | 0 | Office2007/VS2008 skin picker. Removed by D7; replaced by a QSS light/dark picker (Risk 5). |
| `CWMPEventDispatch.cpp` | 484 | 0 | 2 | 5 | 0 | Windows Media Player ActiveX event sink. |
| `CWMPHost.cpp` | 238 | 0 | 22 | 4 | 0 | Windows Media Player ActiveX. |
| `ChildFrm.cpp` | 114 | 0 | 9 | 0 | 0 | CMDIChildWndEx. MDI dropped in favour of QTabWidget (§7). |
| `DockPaneBase.cpp` | 49 | 0 | 1 | 1 | 0 | CDockablePane base -> QDockWidget. One of the 4 non-PCH files. |
| `FileExplorerDoc.cpp` | 48 | 0 | 0 | 0 | 0 | Doc/view plumbing. |
| `HostDoc.cpp` | 68 | 0 | 2 | 0 | 0 | Doc/view plumbing. |
| `Hpslib.cpp` | 215 | 0 | 26 | 0 | 0 | Third-party TCHAR/GetLastError C utils, marked LGPL in-file - see the licence note below. Superseded by QString. |
| `MDIClientWnd.cpp` | 46 | 0 | 1 | 7 | 0 | MDI client area. No Qt analogue. |
| `ProgramStatusBarEx.cpp` | 252 | 0 | 2 | 3 | 0 | -> QStatusBar. One of the 4 non-PCH files. |
| `ScrollHelper.cpp` | 401 | 0 | 4 | 6 | 0 | -> QScrollArea. |

---

## 4. Phase 2 work order (`core/` extraction)

Ascending `CString` load. Each row is an independently assignable PR **targeting `port/cross-platform`**
(D4). The MFC build is the regression test for every line moved into `core/` (D5) — but on a
branch it is `port/cross-platform`'s MFC build, not the one users are running, so CI on `port/cross-platform` has to
be green on every one of these PRs or the signal is worthless.

### Wave 1 — ~~zero `CString`, move as-is~~ — did not survive contact

> ⚠️ **This wave was wrong.** It was built from `CString` counts, and **zero `CString` does
> not mean zero MFC coupling.** An attempt to actually move these seven files found that
> three are dead, two are misclassified, and the remaining two are blocked. Corrected below;
> the original list is kept so the mistake is visible rather than quietly rewritten.

| File | Was | Reality |
|---|---|---|
| `Observer.cpp` | Wave 1 | **Dead — never compiled.** Deleted, see §2 |
| `Subject.cpp` | Wave 1 | **Dead — never compiled.** Deleted, see §2 |
| `FixedBlockMemory.cpp` | Wave 1 | **Dead — never compiled.** Deleted, see §2 |
| `ComboboxRegexHelper.cpp` | Wave 1 | **`ui-rewrite`.** `PopulateRegexFields(CComboBox&)` — pure MFC UI |
| `UnicodeUtils.cpp` | Wave 1 | **`platform/`.** 8 Win32 codepage calls; needs a portable codec |
| `StringHelper.cpp` | Wave 1 | Genuine `core/` candidate, **blocked**: uses `AppSettingMgr.m_nPageAlignmentWidth`, plus `TCHAR`/`_T()` |
| `TextFormatConverter.cpp` | Wave 1 | Genuine `core/` candidate, **blocked**: needs `StringHelper::trim` and `AppUtils::SplitterStdString` |

**So Wave 1 is not a wave.** Progress since:

1. ✅ **`StringHelper.cpp`** — *split*, not moved. The portable majority is now
   `Core::CStringUtil` in `core/StringUtil.h`; `STDStringHelper` derives from it so no call
   site changed. Four members stayed behind: `Format` (MSVC `_vscwprintf`),
   `ExpandEnvironmentStrings` (Win32, unused), `to_lower(std::wstring)` (Win32
   `LCMapStringEx`), `find_caseinsensitive` (MSVC `_wcsnicmp`).
   `AppUtils::SplitterStdString` / `SplitterWStdString` also folded in as
   `Core::CStringUtil::Split`, with the originals kept as forwarders.
2. ⛔ **`TextFormatConverter.cpp`** — **also needs splitting, not moving.** A trial move
   compiled off-Windows and turned up four dependencies no grep had shown:

   | Site | Dependency |
   |---|---|
   | `base64_decode`, 1 line | `LOG_OUTPUT_MESSAGE_COLOR(_T(...), BasicColors::orange)` — logs into a dock pane. **UI.** |
   | `sha256_hash`, 1 line | `wsprintf(..., TEXT("%02x"), ...)` — Win32 |
   | 4 sites | `variadic_string_format`, a free template still in `src/StringHelper.h` |
   | header | missing `<sstream>`, previously supplied by `stdafx.h` |

   The first is the real blocker: `core/` cannot log into the UI. Either the function
   returns an error for the caller to report, or the logging moves out. Both change
   behaviour on the failure path, so it wants its own change — not a silent drop of a
   user-visible error message.

Everything else nominally in `core/` carries `CString` and belongs in Wave 2 or 3.

**Lesson for the remaining buckets — the check has four steps, not three.** Before moving any
file: is it in `src/VinaText.vcxproj` at all; what does its `#include` list drag in; do its
public signatures mention MFC types; **and does it actually compile off Windows?**

The fourth step is the one that earns its keep. A token grep for
`CString|TCHAR|DWORD|INTERNET` cleared `StringHelper`'s extracted block, and the compiler
then rejected it for `LCMapStringEx`, `_wcsnicmp` and a missing `<memory>`. The same grep
cleared `TextFormatConverter`, and the compiler found four more. **Neither `LCMapStringEx` nor
`wsprintf` nor a missing include matches any pattern you would think to write.** Every
`core/` candidate gets a trial compile before anyone estimates it.

**And one hazard created by the split-with-inheritance pattern itself:** any name declared on
both sides of the split is hidden by C++ member-name lookup. `to_lower` hit this — the base
overload silently stopped resolving, and neither the Windows build nor the tests noticed
because nothing calls it. Each split needs a `using Base::name;` for every shared name.
`AppUtil`, `RAIIUtils` and `GuiUtils` in §5 are queued for the same treatment.

### Wave 2 — light migration (1–30 sites)

| File | CS | LOC | Note |
|---|---:|---:|---|
| `LocalizationHandler.cpp` | 2 | 85 | Loads localization files; port to QTranslator or keep as-is. |
| `SpellChecker.cpp` | 2 | 279 | Dictionary lookup; strip the Editor.h/UI coupling on the way out. |
| `TemporarySettings.cpp` | 2 | 10 | 9 lines, trivial. |
| `Textfile.cpp` | 2 | 1,551 | Encoding detection (uchardet) + file I/O. Split: swap Win32 CreateFile/ReadFile for QFile, keep the codec logic. |
| `WebHandler.cpp` | 2 | 191 | curl-based HTTP. 19 std:: lines, zero Win32. |
| `RAIIUtils.cpp` | 3 | 62 | Split: CCriticalSectionLock/CMemoryGuard/CBenchmarkTest are core; CLockCtrlRedraw/CLockCtrlUpdate/CMultipleSelectionKeeper are editor-widget guards -> ui-qt/. |
| `LocalizationDatabase.cpp` | 4 | 183 | String table. |
| `EditorLexerDark.cpp` | 5 | 995 | Scintilla lexer/theme table. Phase 4; SCI_ API is identical on Qt. |
| `EditorLexerLight.cpp` | 5 | 995 | Scintilla lexer/theme table. Phase 4; SCI_ API is identical on Qt. |
| `UserCustomizeData.cpp` | 7 | 48 | std::fstream config reader. |
| `RecentCloseFileManager.cpp` | 8 | 61 | MRU list. |
| `TemplateCreator.cpp` | 9 | 145 | File-template generation. |
| `UserExtension.cpp` | 18 | 170 | User-defined tool commands. Decouple from EditorView/EditorDoc first. |
| `LexerParser.cpp` | 22 | 297 | **→ `core/Tokenizer` (#25).** Delimiter tokenizer, not language parsing. See §6c correction 3. |
| `AppSettings.cpp` | 25 | 414 | Config already file-based (issue #44). Maps onto QSettings. |
| `FindPathWorker.cpp` | 26 | 241 | Path search worker. Threading moves to platform/. |

### Wave 3 — heavy, do last

| File | CS | LOC | Note |
|---|---:|---:|---|
| `DiffEngine.cpp` | 32 | 324 | **Algorithm → `core/LineDiff` (#26); HTML renderer stays.** See §6c correction 4. |
| `FileUtil.cpp` | 33 | 406 | Already heavily std::-based (41 std:: lines). |
| `EditorDatabase.cpp` | 40 | 92 | **Stays in `ui-mfc/`** — duplicates `Core::SLanguageInfo`; 197 signature sites. See §6c correction 5. |
| `FindReplaceTextWorker.cpp` | 62 | 362 | Find/replace over Scintilla buffers. Threading -> platform/. |
| `FilePartition.cpp` | 67 | 530 | CArray subclass -> QVector. |
| `PathUtil.cpp` | 308 | 1,585 | THE Phase 2 boss file: 202 CString in .cpp + 106 in .h, plus 101 Win32 path-API tokens. Do last. |

`PathUtil` is the boss file the brief predicted — it is both the heaviest `CString` user in
`core/` and carries 101 Win32 path-API tokens. Do it last, when the `QString` idioms are
already settled across the other 28 files.

### Not in this table: the three static data tables — ✅ done

`modellanguages.h` (504), `EditorColorLight.h` (211), `EditorColorDark.h` (210) are
**headers with no `.cpp`**, so they don't appear above — but per §3b of the brief they are
925 `CString` declarations, 22% of the corpus figure, and the brief recommends converting
them to external data **before** Wave 1 for the cheapest possible reduction.

That work is done, and one of the three turned out not to need converting at all:

- **`modellanguages.h` was dead code — deleted, not converted.** No translation unit included
  it, it was absent from `src/VinaText.vcxproj`, its `ScinRules` namespace was referenced
  nowhere, and the two classes it defined 84 method bodies for (`CScintillaEditor`,
  `CIDEDatabase`) exist nowhere in the codebase. 3,510 lines and **504 `CString` — the single
  heaviest entry in the brief's table — were a phantom.**
- **`EditorColorLight.h` / `EditorColorDark.h`** duplicated an identical half: 42 languages of
  metadata and 41 keyword blobs, byte-for-byte the same in both, differing only in colour.
  Extracted to `Packages/data-packages/{languages,theme-light,theme-dark}.json`, with
  `tools/extract_language_data.py` as both the extractor and the round-trip verifier.

**Corpus after this work: 4,138 → 3,634.** The two theme headers are unchanged and still the
live source of truth, so their 421 `CString` remain; removing them means switching the MFC
frontend onto `core/LanguageData`, which changes what Windows compiles and is a separate
change. The §1 reconciliation above describes the tree **before** these changes, so it still
sums to 4,138.

---

## 5. Files that must be split, not moved

Six files carry both portable logic and MFC UI. Splitting them is the highest-leverage
Phase 2 work because it directly shrinks the dual-maintenance surface (Risk 2).

| File | LOC | CS | Split |
|---|---:|---:|---|
| `EditorView.cpp` | 9,860 | 532 | Command handlers, text transforms and file-op logic → `core/`. Only the message map, menu state and `CViewBase` plumbing stay UI. |
| `AppUtil.cpp` | 2,478 | 155 | `BasicColors` + generic helpers → `core/`. The `LOG_*_MESSAGE*` family routes into dock panes → `ui-qt/`. |
| `RAIIUtils.cpp` | 62 | 3 | `CCriticalSectionLock`, `CMemoryGuard`, `CBenchmarkTest` → `core/`. `CLockCtrlRedraw`, `CLockCtrlUpdate`, `CMultipleSelectionKeeper` are widget guards → `ui-qt/`. |
| `GuiUtils.cpp` | 240 | 9 | DPI/metrics/shell-open helpers → `platform/`. `CWnd` drawing helpers → `ui-qt/`. |
| `Textfile.cpp` | 1,551 | 2 | Encoding detection (uchardet) and codec logic → `core/`. `CreateFile`/`ReadFile` I/O → `QFile`. |
| `Editor.cpp` | 5,221 | 161 | `SCI_*` message logic survives nearly intact against `ScintillaEditBase` (D2); the `CWnd` hosting does not. |

---

## 6. Licence finding: `Hpslib.cpp`

`src/Hpslib.cpp` carries this header:

```
// Minimal, exportable version of HPSLIB
// Copyright (c) 2007, AlpineSoft http://www.alpinesoft.co.uk
// LGPL applies - i.e. if you use it, please give us a credit
```

This is third-party **LGPL** code inside an MIT repo, and it is not represented in
`license/` (which tracks Curl, Mozilla, PDFium, Qt, Scintilla, VinaText). It is 215 lines
of `TCHAR`/`GetLastError` string and block-memory helpers that `QString` replaces outright.

It is already classified `delete`. **Deleting it in Phase 2 also closes the licence gap** —
worth doing early and worth flagging to the owner regardless of the port, since it applies
to the shipping MFC build today.

**Three files depend on it, not one** — and one of them is `PathUtil.cpp`, the Phase 2 boss
file, so the deletion is coupled to the heaviest item in the `core/` work order:

| Caller | Includes | Symbols actually used |
|---|---|---|
| `Textfile.cpp` | `Hpsutils.h`, `Hpslib.hr` | `AtoA`, `LogError`, `LookupSystemError`, `alloc_block`, `copy_string`, `free_block`, `vFormatPString` |
| `PathUtil.cpp` | `Hpsutils.h` | `GetLastErrorString`, `free_block` |
| `Editor.cpp` | `Hpsutils.h` | `GetLastErrorString`, `free_block` |

`PathUtil.cpp` and `Editor.cpp` need only two symbols each and are the cheap ones to cut
first. `Textfile.cpp` is the real work: it uses seven, including the block-memory allocator,
so it needs `QFile`/`QString` equivalents before `Hpslib` can go.

---

## 6b. `Packages/` consolidation — what was reconciled

`Packages/` was duplicated under `bin/x64/Release/` and `bin/x64/Debug/`, maintained by
hand-copying. Both copies are now build outputs, written by a `PostBuildEvent` in
`src/VinaText.vcxproj` from a single source at the repository root.

The two copies had **diverged in exactly two of 61 files** (`diff -rq` over the whole tree,
not a sampled or heuristic comparison). **`Release` was taken as canonical** because it is the
shipping configuration. Recorded here rather than only in a commit message, because a dropped
edit that exists only in git history is the same silent-drift problem the consolidation was
meant to end.

| File | `Release` — kept | `Debug` — discarded |
|---|---|---|
| `extension-packages/user-extensions.dat` | CRLF + CR line endings | LF, plus one extra blank line at 41 |
| `language-packages/html-data.ee-package` | `- Please change to your **target web** browser to view HTML file (CHROME_BROWSER, FIREFOX_BROWSER, EDGE_BROWSER, DEFAULT_BROWSER)` | `- Please change to your **default** browser to view HTML file (…)` |

Neither affects behaviour: the first is whitespace, the second is a line of guidance text in a
config file's comment header. On the second, `Release`'s wording is also the better of the two
— the sentence lists four browser options including `DEFAULT_BROWSER`, so "target web browser"
describes the choice and "default browser" describes only one of the four.

If the `Debug` wording was the intentional later edit, restoring it is a one-line change to
`Packages/language-packages/html-data.ee-package`.

---

## 6c. Phase 2 cost is call-site churn, not `CString` count

The §3 tables rank `core/` candidates by `CString` occurrences. That is the wrong cost model,
and `EditorDatabase` is the clearest example.

| | |
|---|---|
| the file | 92 LOC, 40 `CString`, **includes only `stdafx.h`** — no other dependency at all |
| looks like | the easiest move in the bucket |
| actually | its API is 16 `CString` accessors — 8 setters, 8 getters — used at **~190 call sites across 15 files** |

The class is trivially *portable* and expensively *movable*. Nothing in the per-file triage
sees that, because the cost lives in the callers.

**The measure that matters is: how many call sites change if this type's signature changes.**

```bash
# for a candidate's public methods, count the sites that would have to move with it
grep -rhoE '(Set|Get)(Language[A-Za-z]*|CompilerPath|DebuggerPath)\s*\(' src/*.cpp src/*.h \
  | sort | uniq -c | sort -rn
```

**Two ways to pay it, and they are not equivalent:**

1. **Convert every call site.** Honest, and leaves no adapter behind — but it is a large
   mechanical diff through `EditorView.cpp` and both lexer files, verifiable only by the
   Windows build.
2. **Move the type to `core/` with a `std::wstring` API and leave a thin `CString` adapter in
   `src/`.** Call sites do not change. This is the shape used for `STDStringHelper` in PR #6 —
   and it carries that PR's hazard: any method name present on both sides hides the base
   overload set, silently, so each one needs a `using` declaration.

Neither is wrong. What would be wrong is estimating either from the `CString` column.

**Consequence for the work order in §4 — measured, and it corrects two of this section's
own claims.**

This section originally speculated that `PathUtil` *"may not be the worst by call-site churn"*.
It is the worst by the two columns that measure churn — but **not** by every measure, and the
first published version of this table said so wrongly. Both corrections are below the table.

| file | `#include`s of its header | `Owner::` references | `CString` | LOC |
|---|---:|---:|---:|---:|
| **`PathUtil`** | 21 | **353** | **308** | 1,587 |
| **`AppSettings`** | **46** | 0 | 25 | 414 |
| `FileUtil` | 11 | 2 | 33 | 406 |
| `EditorDatabase` | 7 | 0 | 40 | 92 |
| `StringHelper` | 6 | 30 | 0 | 478 |
| `FindReplaceTextWorker` | 3 | 15 | 62 | 362 |
| ~~`DiffEngine`~~ → `core/` (algorithm) | **1** | 0 | 32 | 323 |
| ~~`LexerParser`~~ → `core/` | **1** | 0 | 22 | 297 |
| `UserExtension` | **1** | 0 | 18 | 171 |
| `SpellChecker` | **1** | 0 | 2 | 280 |

The two columns measure different costs and **neither alone is sufficient**:

- **`#include`s** is the number for a class used through instances — `obj.Method()` carries no
  qualifier to count. `AppSettings` leads it at 46: a settings singleton reached through a
  macro, far more entangled than its 25 `CString` suggests.
- **`Owner::` references** is the number for a namespace or a static API. `PathUtil` leads it
  at 353, an order of magnitude clear of anything else.

**So `PathUtil` is not "worst by every measure".** It is worst by qualified references, by
`CString`, and by size; `AppSettings` is worst by header inclusion. They are entangled in
different ways and the §4 order should reflect that — `PathUtil` last because moving
`PathUtils::` touches 353 sites, `AppSettings` late because its macro reaches 46 files.

**Correction 1 — the first version of this table was not reproducible.** It published a single
"including files" column showing 46 for `PathUtil`, which the reproduction command below does
not produce. That figure was `#include`s **union** files containing `PathUtils::` — 21 ∪ 43 —
under a label that said only the first. It coincided exactly with `AppSettings`' 46, which
looks like a copy-paste and was not. A number a reader cannot reproduce from the stated command
is worse than no number, and that is precisely what §6c exists to warn about.

**Correction 2 — the LOC column disagreed with §3.1, for two reasons.** §3.1 was measured on
2026-07-26 with `wc -l`; the first version of this table used Python's `splitlines()`, which
differs by one on a file with no trailing newline. On top of that the files genuinely grew:
Phase 1 (#9–#14) added per-file includes, so `PathUtil.cpp` is 1,584 → 1,587, `SpellChecker.cpp`
278 → 280, and others +1 each. This table now uses `wc -l`, so it and §3.1 differ only by that
real growth.

**Revised §4 order:** the four single-includer files first — `DiffEngine`, `LexerParser`,
`UserExtension`, `SpellChecker` — then `FileUtil` and `EditorDatabase`, then `AppSettings` once
the settings macro is dealt with, then `PathUtil` last.

**Correction 3 — "`LexerParser` is the most valuable of the four: language and keyword
parsing" was wrong, and the file name is why.** It was picked first on that basis. Reading it
found no lexing and no parsing: `CLexingParser` was a generic character-delimited tokenizer,
and its only two callers split a `|`-separated file-extension list in
`CEditorCtrl::GetLexerNameFromExtension`. The 297 LOC were also not 297 LOC of work — of ten
methods, **three were reachable and seven had no caller in the tree**.

That makes the file name the fourth thing on this list that failed to predict move cost, after
build membership (§6c note 1), implementation coupling (note 2) and call-site churn (note 3).
The first three at least measured something. This one was inferred from a nine-character
identifier, and no measurement stood behind it — a triage row that says what a file *does*
should be read as a hypothesis until someone opens the file.

**What the move actually cost (#25):** `core/Tokenizer.{h,cpp}`, 3 methods, ~45 lines, and a
two-line change at each of the two call sites to convert `CString` → `std::wstring` at the
boundary. `src/LexerParser.{h,cpp}` deleted. The seven unreachable methods were **not** ported,
because both of the alternatives were bad — they carried two defects that only a port would
have had to make a decision about:

- `NextInt` / `NextLong` / `NextFloat` / `NextDouble` / `NextBool` parsed via
  `::sscanf_s((LPCSTR)(LPCTSTR)strReturn, ...)`. Under `_UNICODE` — how VinaText builds —
  `LPCTSTR` is `const wchar_t*`, so the cast reinterprets a UTF-16 buffer as narrow characters
  and every ASCII digit is followed by a zero byte, which `sscanf` reads as the terminator.
  Demonstrated: `NextInt(L"42")` → **4**, `NextDouble(L"3.5")` → **3**.
- `NextString(bRemoveQuotes = TRUE)` could not terminate. Its post-quote scan
  (`LexerParser.cpp:93-105`) advanced `m_nPosition` only on a delimiter match, so any other
  character spun the loop on the same index forever. Demonstrated: 1,000,000 iterations with no
  progress at position 4 of `"ab"xcd`.

Both were latent — nothing called these methods, so neither ever ran. Porting them would have
meant either reproducing broken behaviour or silently changing behaviour, and the third option
is the honest one: **the dead surface is deleted and the reason is recorded**, in
`core/Tokenizer.h` where the next reader will find it.

Equivalence for the surface that *is* live was established by differential test rather than by
inspection: `CLexingParser::Next` transcribed verbatim, run against `Core::CTokenizer` over
every string of length ≤ 6 from an alphabet of letters and delimiters, across six delimiter
sets — **117,186 pairs, zero mismatches**. `core/tests/TestTokenizer.cpp` then pins the
edge cases that look like oversights and are deliberately preserved, notably that a leading
delimiter yields a leading empty token but a trailing one yields no trailing empty token.

**Correction 4 — "`DiffEngine.cpp` — Diff algorithm. No Win32." was true and badly
incomplete, and reading it found a live hang.** Three things the row did not say:

1. **It is two programs in one class.** Roughly half of `CDiffEngine` is an HTML report
   generator — `Serialize`, `SetTitles`, `SetColorStyles`, `Escape` and seven `CString`
   colour members. Only the alignment algorithm is portable; the renderer is presentation
   and stays in `ui-mfc/`. So "move `DiffEngine`" was never a single action.
2. **The algorithm is written against a 529-LOC MFC type.** `CFilePartition` is `CArray`
   from `<afxtempl.h>`, file IO and option handling. But `Diff` only ever calls six of its
   methods, and just two of those read anything — `GetRawLine` and `GetLine`. That is the
   whole interface, so `core/` takes two `vector<wstring>` per side and `CFilePartition`
   never has to move at all.
3. **Some of it must not move.** The ignore-case option is applied with `CString::MakeLower`.
   Every plausible implementation of that (Win32 `CharLowerBuff`, or `_wcslwr_s` under the
   locale `CommandLine.cpp` installs via `_tsetlocale(LC_ALL, "")`) is Unicode- or
   locale-aware, while `Core::ToLower` is the classic locale and therefore ASCII-only by
   construction — see the note in `core/TextTransform.h`. Moving those 20 lines would
   silently change which lines compare equal in any file with non-ASCII text. So the
   frontend filters and hands the filtered lines in, which is what `SDiffSide::_Compare` is.

**And the hang.** Differential-testing the port against a verbatim transcription of the
original found that **`CDiffEngine::Diff` does not terminate on 8,484 of 132,496 small file
pairs — 6.4%**. Two three-line files are enough: `ABA` against `BBA` spins forever at
`(i=2, nf2CurrentLine=1)`.

The cause is a missing guard. The `bDeleted` block means "f1 lines `i`..`itmp-1` were
deleted", which presupposes `itmp > i`; the original never checks it. When `itmp <= i`,
`j = itmp - i` is `<= 0`, the loop body never runs, nothing is emitted, neither index moves,
and `continue` re-enters with identical state. It allocates nothing, so `Path Comparator`
pins a core at 100% rather than crashing — which is why it has presumably been reported as
"hangs sometimes", if at all.

**Unlike the two defects in #25, this one is live.** `CPathComparatorDlg::DoDiff` reaches it
on ordinary input. `core/LineDiff.cpp` adds the `(itmp > i)` guard, and because `ui-mfc/` now
calls `core/`, the shipping app is fixed by the extraction rather than by a separate patch.

The guard **cannot change any output that previously existed**: every state it excludes is a
state that did not terminate, so there was nothing to preserve. Verified, not argued —
124,012 pairs where the original terminates produce **0 mismatches**, and the port returns on
all 8,484 where it does not.

**So the pattern from #25 repeats with the opposite sign.** There the triage row's summary was
wrong and the defects were latent; here the summary was right as far as it went, and the
defect was live. What both have in common is that **no amount of counting `CString` would have
surfaced either.** Reading the file did.

`FileUtil` and `EditorDatabase` are next by the §4 order — and this time the estimate carries
an explicit caveat: **11 and 7 including files respectively, contents unread.**

**Correction 5 — the two columns above are both wrong, and so is the §4 wave.** Reading
`EditorDatabase` and `FileUtil` as promised produced no move at all. It produced a third
metric, and the third metric reorders everything.

**A type is reached in one of three ways, and each is invisible to the other two.**

| reached as | example | what the `#include` column sees | what the `Owner::` column sees |
|---|---|---:|---:|
| namespace-qualified call | `PathUtils::GetName()` | undercounts | correct |
| type name in a signature | `void f(CLanguageDatabase*)` | undercounts | **0** |
| macro | `AppSettingMgr.m_Field` | counts the include only | **0** |

So the honest measure is **occurrences of whatever identifier the call site actually writes** —
namespace, type name, or macro. Measured that way:

| file | reached via | **sites** | files | `#include`s | `Owner::` | `CString` | LOC |
|---|---|---:|---:|---:|---:|---:|---:|
| **`AppSettings`** | `AppSettingMgr` **macro** | **783** | **42** | 46 | 0 | 25 | 414 |
| **`PathUtil`** | `PathUtils::` | 353 | 43 | 20 | 353 | 308 | 1,587 |
| **`EditorDatabase`** | `CLanguageDatabase` type | **197** | 13 | 7 | **0** | 40 | 92 |
| `StringHelper` | mixed | 39 | 7 | 6 | 30 | 0 | 478 |
| `FindReplaceTextWorker` | types | 29 | 4 | 3 | 15 | 62 | 362 |
| `FileUtil` | types | 26 | 9 | 11 | 2 | 33 | 406 |
| `SpellChecker` | type | **1** | 1 | 1 | 0 | 2 | 280 |
| `UserExtension` | type | **1** | 1 | 1 | 0 | 18 | 171 |

(`AppSettings` also exposes `IS_LIGHT_THEME`, a further 53 sites across 17 files. `PathUtil`'s
includes are 20 rather than §6c's earlier 21 because #26 removed the unused one in
`DiffEngine.cpp`.)

**Three things follow, none of which the previous ranking permitted.**

**1. `AppSettings` is the most entangled file in the codebase — 783 sites, not 46.** It was
ranked mid-cost. It is more than twice `PathUtil`, which §6c called "an order of magnitude
clear of anything else" and scheduled last. That claim was about qualified references, and
`AppSettings` has none: every one of the 783 is written `AppSettingMgr`, a macro expanding to
`CAppSettings::GetInstance()`. **`AppSettings` is now the last file to move, and `PathUtil`
second-last.**

**2. `EditorDatabase` must not move at all.** `CLanguageDatabase` is eight `CString` fields with
setters and getters — and six of them are already filled from `Core::SLanguageInfo` by
`EditorLanguageData.cpp`'s `ApplyLanguageMetadata`. **The portable representation already
exists in `core/`; this class is the boundary conversion, just not named one.** Moving it would
edit 197 signatures across 13 files to arrive where the code already is. It stays in `ui-mfc/`
until the lexer init functions are rewritten in Phase 4, which touches those signatures anyway.

**3. Half of §4's "cheapest start" wave is in the wrong bucket.** The wave was `DiffEngine`,
`LexerParser`, `UserExtension`, `SpellChecker`, chosen because each had one including file. The
first two were right and are done (#25, #26). The other two are not `core/` candidates:

- **`SpellChecker` → `platform/`.** Its header includes `<spellcheck.h>` and holds
  `ISpellCheckerFactory*` and `ISpellChecker*` **as members**. That is the Windows COM Spell
  Checking API, not a dictionary this project owns. §3's note — *"strip the Editor.h/UI
  coupling on the way out"* — describes the wrong problem; the coupling to remove is COM, and
  macOS and Linux need a different backend entirely (`NSSpellChecker`, hunspell). It is a
  reimplementation, not a port.
- **`UserExtension` → `ui-rewrite`.** `LoadMenuUserExtensions(CMenu* pExtensionsMenu)` puts an
  MFC menu in the public API, and the `.cpp` includes `Resource.h`, `EditorDoc.h` and
  `EditorView.h`. §3 already said "decouple from EditorView/EditorDoc first", which is true and
  is most of the file.

Both have exactly **1** call site, so they still look cheapest by every column in the table —
including the new one. **Call-site count measures the cost of moving a file; it says nothing
about whether the file can move.** That needs the header read, which is what §7 has warned
about since the first draft and what these five corrections keep re-learning.

**Revised order, replacing the one above:** `FileUtil` (26 sites), `FindReplaceTextWorker` (29),
`StringHelper`'s remaining split (39), then `PathUtil` (353), then `AppSettings` (783) last.
`EditorDatabase`, `SpellChecker` and `UserExtension` leave the `core/` list.

**Since 2026-07-31 this order is a demand-driven backlog, not a schedule** — brief §4 D9:
Phase 3 runs in parallel and pulls these when the Qt frontend needs them. In particular,
`PathUtil` and `AppSettings` are expected to move **in slices**, not wholesale.

**"Sites" means occurrences, not lines.** `grep -o ... | wc -l` and `grep -c` are not the same
measurement and differ on real data here: `PathUtils::` is **353 occurrences on 349 lines**,
because four lines carry two calls each —

    src/FileExplorerCtrl.cpp:1294   src/FileExplorerCtrl.cpp:5746
    src/PathResultWindow.cpp:720    src/VinaTextApp.cpp:222

— and `AppSettingMgr` is **783 on 778**. Occurrences is the right unit for this table: the cost
of moving a type is the number of expressions that must be edited, and two on one line is two
edits. Every command below uses `-o`; substituting `-c` reproduces 349 and 778 instead.

Reproduce — the point is to count the identifier the CALL SITE writes, which is not always the
type's name:

```bash
# 1. namespace-qualified (PathUtil)
grep -rhoE '\bPathUtils\s*::' $(ls src/*.cpp src/*.h | grep -v '^src/PathUtil\.') | wc -l

# 2. type name in signatures (EditorDatabase) - the Owner:: command returns 0 here
grep -rhoE '\bCLanguageDatabase\b' $(ls src/*.cpp src/*.h | grep -v '^src/EditorDatabase\.') | wc -l

# 3. macro (AppSettings) - both other commands return 0; find the macro first
grep -nE '^\s*#define' src/AppSettings.h
grep -rhoE '\bAppSettingMgr\b' $(ls src/*.cpp src/*.h | grep -v '^src/AppSettings\.') | wc -l
```

Reproduce, per candidate:

```bash
# 1. headers that include it (the cost for an instance-based class)
grep -rl '#include "PathUtil.h"' src/ | grep -v '^src/PathUtil\.'   | wc -l

# 2. qualified references FROM OTHER FILES (the cost for a namespace or static
#    API). Excluding the file's own self-references matters: PathUtil.cpp and .h
#    account for 166 of the 519 total, so counting them inflates the number by a
#    third. Use the types the header DEFINES - `class X ... {`, not `class X;`
grep -rhoE '\bPathUtils\s*::' $(ls src/*.cpp src/*.h | grep -v '^src/PathUtil\.') | wc -l

# 3. LOC, matching §3.1's convention
wc -l src/PathUtil.cpp
```

---

## 6d. The first demand-driven pull: which lexer does a file get?

The first thing the Qt shell asked `core/` for under D9 was not on the §6c backlog at all. It
was *"the user opened `main.cpp` — now what?"*, and answering it needed data that lives in
neither of the files the backlog names.

**The spike was matching on the wrong field.** `ui-qt/main.cpp` compared the file's suffix
against `SLanguageInfo::_Extension`, the `extention` (sic) column extracted from
`EditorColorLight.h`. That column is a display label, not a mapping: it reads `r` for autoit,
`javascript` for javascript, `mk` for makefile. It happens to be right for `cpp`, `py` and
`json`, which is exactly why the spike looked like it worked.

Measured against the 112 real extensions, the spike's matching reached:

| | |
|---:|---|
| **14** | the right language *and* the lexer the MFC app uses |
| 13 | a language, but a different lexer — including `.r`, which selected **autoit**, because autoit's display label is `r` and it sorts first |
| 85 | nothing at all — `.h`, `.js`, `.htm`, `.md`, `.rb`, `.rs`, `.pl` … all plain text |

**A language is written three different ways between a file name and a styled document.**

| # | where | written | example |
|---|---|---|---|
| 1 | `EditorCommonDef.h` — `arrLangExtensions[i]` ↔ `arrLexerNames[i]` | a VinaText dispatch token | `json` → `"kix"` |
| 2 | `EditorLexerDark.cpp` — `LoadLexer`'s if-chain | an `Init_<x>_Editor` function | `"kix"` → `Init_json_Editor` |
| 3 | that function's two string literals | the **Lexilla** lexer name, and the **languages.json id** | `SetLexer("cpp")`, `ApplyLanguageMetadata(_, "json")` |

Only (3) is any use to a second frontend: `ui-qt/` needs the Lexilla name for `CreateLexer`
and the id for keywords and a theme style table. The token in (1) is an MFC dispatch detail,
so it is deliberately **not** carried into the data — it would be a fourth name for a thing
that already has three.

**What moved.** `languages.json` gained two fields per language, `extensions` and `lexer`,
generated from the C++ by `tools/extract_language_data.py` and checked back against it by
`--verify` on every CI run. `core/LanguageData` gained `FindByExtension`, `DetectForFileName`
and `ExtensionOf`. **`src/` is unchanged** — the MFC app still walks its own arrays, exactly
as the theme tables worked when they were extracted.

| | |
|---:|---|
| 41 | extension rows in `EditorCommonDef.h`, over 42 languages (`makefile` is selected by file name, never by extension) |
| 112 | file extensions, all distinct — no row shadows another |
| 28 | distinct Lexilla lexers behind them |
| 15 | languages whose lexer is **not** their own id — 12 of the 15 are lexed as C++; the other three are `flexlicense`→`python`, `html`→`hypertext`, `inno`→`asm` |
| 24,389 | file names run through both implementations, **0 disagreements** |

**`.json` is lexed as C++, and the port reproduces that.** `Init_json_Editor` calls
`SetLexer("cpp")` while colouring with `SCE_JSON_*` constants — two different style families,
so the theme's `json` table lands on whatever `SCE_C_*` styles happen to share those numbers.
Lexilla *does* ship a `json` lexer, and the vendored copy has it. Switching to it would change
what a `.json` file looks like in the shipping Windows app, so it is recorded here and left
alone; the correct-looking change is a decision, not a port detail. Same for `.iss`, which is
lexed as `asm` while Lexilla ships `inno`.

**One more, found while transcribing:** `Init_flexlicense_Editor` fills its autocomplete list
from `GetKeywords("resource")`, not `"flexlicense"` — a copy-paste from `Init_resource_Editor`
directly below it. `.lic` files offer RC keywords. It affects autocomplete only, nothing in
the alpha's scope, and is left for whoever touches those initialisers in Phase 4.

**Equivalence is tested, not argued.** `core/tests/TestLanguageLookup.cpp` carries its own
verbatim copy of `arrLangExtensions`, `arrLexerNames` and `LoadLexer`'s 42 branches, reads no
JSON, and folds case with `towlower` rather than `core/`'s ASCII table — so the two sides share
no data and no implementation. Both are run over every extension in every case permutation,
both file-name special cases and their near misses, and every string of length ≤ 5 over
`cpH.|1m`. The corpus and the test were mutation-checked: making the comparison
case-sensitive, dropping the `Makefile` rule, and leaving the dot on the extension each fail
it.

**Two quirks are preserved deliberately**, because both frontends have to agree about them:
`CMakeLists.txt` is matched case-**sensitively** and `Makefile` case-**insensitively**, which
is what `CEditorCtrl::DetectFileLexer` does. So `cmakelists.txt` is plain text and `MAKEFILE`
is a makefile.

**Not pulled, and worth stating.** `CUserCustomizeData::GetSyntaxHighlightUserData()` replaces
the whole built-in table when `Packages/data-packages/syntax-highlight-file-extension.dat` is
non-empty. `ui-qt/` does not read it yet. That is `UserCustomizeData.cpp`, a Wave 2 file, and
under D9 it waits until the shell needs it rather than being taken along for the ride.

Reproduce:

```bash
# extension rows in the C++ table
grep -cE '_T\("[^"]*"\),[[:space:]]*//' src/EditorCommonDef.h          # 41

# what the extraction produced
python3 -c "
import json
L=json.load(open('Packages/data-packages/languages.json'))['languages']
toks=[t for e in L for t in e['extensions'].split('|') if t]
print('languages:', len(L), '| with extensions:', sum(1 for e in L if e['extensions']))
print('extension tokens:', len(toks), '| distinct:', len(set(toks)))
d=[e for e in L if e['lexer'] and e['lexer'] != e['id']]
print('lexer != id:', len(d), '| of which cpp:', sum(1 for e in d if e['lexer']=='cpp'))
print('distinct lexers:', len({e['lexer'] for e in L if e['lexer']}))
"

# what the spike's old _Extension matching would have reached
python3 -c "
import json
L=json.load(open('Packages/data-packages/languages.json'))['languages']
by_label={}
for e in L: by_label.setdefault(e['extension'], e)
toks=[t for e in L for t in e['extensions'].split('|') if t]
ok=[t for t in toks if t in by_label and by_label[t]['lexer']==by_label[t]['id']]
bad=[t for t in toks if t in by_label and by_label[t]['lexer']!=by_label[t]['id']]
print('right lexer:', len(ok), '| wrong lexer:', len(bad), '| no match:', len(toks)-len(ok)-len(bad))
"

# the JSON still reproduces the C++
python3 tools/extract_language_data.py --verify

# the differential test, and the count it prints
cmake -S . -B build -G Ninja && cmake --build build --parallel
./build/TestLanguageLookup Packages/data-packages
```

---

## 6e. The second pull: weight, slant, and the table a language is actually coloured from

§6d extracted *which lexer* a file gets. This is *how its styles are drawn* — and it found a
live divergence the alpha shipped with.

**The visible gap.** The Windows build draws Python keywords bold, class and function names
bold-italic, and strings italic. The Qt alpha drew all of them at normal weight, because
`theme-*.json` carries colour and nothing else. Weight and slant are an if-chain inside each
`Init_<x>_Editor`, applied per style constant as the colour table is walked — **13 languages,
75 styles**.

**They belong to the language, not to a theme.** The 13 chains are byte-identical between
`EditorLexerLight.cpp` and `EditorLexerDark.cpp` once the namespace is normalised, so
`languages.json` gains a `styleAttributes` array rather than both theme files gaining a
duplicate — the same deduplication the original extraction made.

**Three things reading it found that no count would have.**

**1. The comparisons are numeric, and one initialiser depends on that.** `iItem == SCE_H_TAG`
compares integers. `Init_xml_Editor` tests `SCE_H_*` constants while its loop variable holds
values from a table written in `SCE_C_*` names — legitimate, because Scintilla's `xml` lexer
emits the H family. An extraction that matched constant *names* would have attributed nine
styles the running program never touches. The extractor resolves every name through
`SciLexer.h` and compares numbers.

**2. `xml` is coloured from `html`'s table — and `ui-qt/` was not.** `Init_xml_Editor` walks
`g_rgb_Syntax_html` (**111** styles), not `g_rgb_Syntax_xml` (**28**). The alpha looked the
table up by language id, so every `.xml` file in the Qt build was coloured from a table the
shipping app applies to nothing, with C-family style numbers against an H-family lexer.
`languages.json` gains `styleTable` — the same shape as §6d's `lexer` field, for the same
reason: **the name of a thing is not the name of the thing it uses.** `xml` is the only
language affected; the other 41 use their own.

**3. Two colour tables are dead.** `g_rgb_Syntax_xml` (28 styles) and `g_rgb_Syntax_python_2`
(16) are defined and walked by no initialiser. `xml`'s is dead precisely *because* of finding
2. They are data-only and left in place — deleting them changes what Windows compiles, so it
belongs with the Phase 4 lexer rewrite, not here.

Had the rule ever run against `xml`'s own table, **3 of its 9 constants** would have matched
anything at all; the other six are ≥ 56 and that table stops at 27. Against the table it
really walks, all nine fire.

**Also preserved: a rule that is switched off.** `Init_html_Editor` carries a commented-out
block that would make `SCE_H_ATTRIBUTE` bold and italic. The extractor strips comments before
parsing, so the rule stays off and `.html` attributes stay plain — as they are on Windows. A
parser that ignored comments would have found the `else */if` and mis-read the whole chain.

**Equivalence, again by differential test.** `core/tests/TestStyleAttributes.cpp` transcribes
all 13 chains from `EditorLexerDark.cpp` — structure included, because **two of them
(`python`, `r`) are not one if/else-if chain but two independent `if` chains**, so a style
matched by the first still falls through the second to its `else`. **1,238 styles compared, 0
disagreements.** The transcription reads no JSON and shares no code with the Python extractor.

Reproduce:

```bash
# 13 languages, 75 styles, and the JSON still reproduces the C++
python3 tools/extract_language_data.py --verify | grep 'style attributes'

# which language is coloured from another's table
python3 tools/extract_language_data.py 2>&1 | grep 'coloured from'

# table sizes, dead tables, and how much of the xml rule could ever have fired
python3 - <<'PY'
import sys, re; sys.path.insert(0, 'tools')
import extract_language_data as x
sce = x.parse_sce_constants(); st = x.parse_header(x.LIGHT_H)['styles']
src = x.strip_comments(x.read('src/EditorLexerDark.cpp'))
body = re.search(r'void\s+\w+::Init_xml_Editor\([^)]*\)\s*\n\{(.*?)\n\}', src, re.S).group(1)
names = re.findall(r'SCE_[A-Z0-9_]+', re.search(r'if\s*\((.*?)\)\s*\{', body, re.S).group(1))
own = {sce[s] for s, _ in st['xml']}; html = {sce[s] for s, _ in st['html']}
print('xml rule constants:', len(names))
print('  match xml\'s own table :', sum(1 for n in names if sce[n] in own))
print('  match the html table  :', sum(1 for n in names if sce[n] in html))
print('table sizes: xml=%d html=%d python_2=%d' % (len(st['xml']), len(st['html']), len(st['python_2'])))
PY

# the differential test
ctest --test-dir build -R core.StyleAttributes --output-on-failure
```

---

## 6f. Folding, and a third name space that had to be re-keyed

Phase 4's next item, and the third time the same lesson arrived in a new costume.

**What folding needed.** `CEditorCtrl::LoadEditorSettings` sets ten `SCI_SETPROPERTY`
fold flags, seven fold marker shapes, the margin colours, and the text a collapsed block
shows. All of it transcribes cleanly into `ui-qt/EditorWidget.cpp` except the last, which is
data.

**The marker shapes are the shipping default, not a choice.** The original branches four ways
on `AppSettingMgr.m_FolderMarginStyle`; `src/AppSettings.h:115` ships `STYLE_TREE_BOX`, so
that is the branch ported. The per-marker RGB literals inside it are **not** transcribed,
because the original overwrites every one of them two lines later with the theme's
`editorFolderForeColor` / `editorFolderBackColor`. Copying them would have been faithful to
the text and wrong about the behaviour.

**And the re-keying.** `SCI_SETDEFAULTFOLDDISPLAYTEXT` is chosen by an if-chain over the
**VinaText lexer token** (`src/Editor.cpp:193-207`) — §6d's name space (1), the one
deliberately not carried into the JSON. So the extractor reads that chain and re-keys it onto
language ids, which is the only name a second frontend has.

| marker | languages |
|---|---|
| `" { ... } "` | 12 — bash, c, cpp, cs, css, java, javascript, json, markdown, php, rust, typescript |
| `" < ... > "` | 2 — html, xml |
| `" --- "` | the other 28 |

**Keying on the Lexilla lexer name instead would be wrong, and would look right.** `go`,
`protobuf`, `autoit`, `resource` and `vcxproject` are all lexed as `cpp` and none of them is
in the chain's list — they fold with `" --- "`. That is now a named assertion in
`core/tests/TestLanguageLookup.cpp` rather than a comment, and mutation-checked: setting
`go`'s marker to the C++ one fails two checks.

**Three fields extracted, three different keys.** `lexer` is keyed by what Lexilla calls the
language, `styleTable` by which colour table the initialiser walks, `foldMarker` by the
VinaText dispatch token. Nothing in the file names would have told you they differ; each was
found by reading the code that consumes it.

**The same trap, a second time, and this one is not hypothetical.** The next view setting
ported — indentation guides — has the identical shape: Python gets `SC_IV_LOOKFORWARD` and
everything else `SC_IV_LOOKBOTH` (`src/Editor.cpp:209-215`), keyed by token. **`flexlicense`
is lexed by Lexilla's `python` lexer but its token is `FLEXlm`, so it takes `LOOKBOTH`.**
Keying on the lexer name would have silently changed how `.lic` files are drawn. `indentGuides`
is therefore the fourth extracted field, and the fourth with its own key.

Reproduce:

```bash
# the mapping, re-keyed from Editor.cpp's chain onto language ids
python3 - <<'PY'
import sys; sys.path.insert(0, 'tools')
import extract_language_data as x
d, _ = x.parse_lexer_dispatch()
fold, default = x.parse_fold_markers(d)
by = {}
for k, v in fold.items(): by.setdefault(v, []).append(k)
for v, ks in sorted(by.items()): print('%-11r %d: %s' % (v, len(ks), sorted(ks)))
print('%-11r %d (the default)' % (default, len(set(d) - set(fold))))
PY

ctest --test-dir build -R core.LanguageLookup --output-on-failure
```

---

## 6g. Brace matching, and a name that lies about what it is

Phase 4 again, and this time the trap is not a fifth name space — it is a **fifth
key inside a space already extracted**, plus a member whose name says the opposite
of what it does.

**What was ported.** `CEditorView`'s `SCN_UPDATEUI` case does three things
(`src/EditorView.cpp:6179-6190`); this is the two that are unconditional:
`CEditorCtrl::UpdateCaretLineVisible` (`src/Editor.cpp:4488`) and
`CEditorCtrl::DoBraceMatchHighlight` (`:1232`). The XML/HTML tag match on the same
event is language-gated and is its own change.

**The brace LOGIC is a clean transcription** — `SCI_BRACEMATCH` on the position
*before* the caret, which is where the caret sits after you type a brace, then
`SCI_BRACEHIGHLIGHT` and `SCI_SETHIGHLIGHTGUIDE` at that brace's column, or both
cleared on a miss. At position 0 it asks Scintilla about position −1;
`SplitVector::ValueAt` returns `T()` for a negative index, `BraceOpposite('\0')` is
`'\0'`, and `BraceMatch` returns −1, so the miss branch runs. That is load-bearing
rather than incidental — it is why the original never guards the subtraction.

**But the logic is only half of it, and the first attempt shipped only that half.**
The colours live 800 lines away in `LoadEditorSettings` (`src/Editor.cpp:447-456`),
not next to `DoBraceMatchHighlight`. Ported without them the feature is *entirely
correct and invisible*: `STYLE_BRACELIGHT` inherits `STYLE_DEFAULT`, so the matched
brace was drawn in `#FFFFFF` on a dark theme — the same colour as ordinary text.
Caught by running the editor and clicking, not by any check that existed at the time.

**And it colours a second thing, from neither end obviously.** `EditView.cxx:290`
draws the *highlighted* indent guide with `styles[StyleBraceLight].fore`. So
`SCI_SETHIGHLIGHTGUIDE` — the other half of `DoBraceMatchHighlight`, the one that
marks the matched block's whole depth — produced nothing a user could see either,
because the highlighted guide was white like every other guide. One missing pair of
`STYLESETFORE` calls made two ported behaviours invisible.

> This is the gap the derivation command in the session prompt warns about. It scans
> `sed -n '110,420p' src/Editor.cpp`, and **line 447 is outside that window**. A plan
> built from "port everything this greps" would have skipped it, and so would a plan
> built from reading `DoBraceMatchHighlight` alone. The self-test now asserts the
> style, so a correct-but-invisible port fails.

**Six of those ten calls are dead, and are deliberately not ported.**
`SCI_INDICSETSTYLE`, `SCI_INDICSETALPHA` and `SCI_INDICSETOUTLINEALPHA` take an
**indicator** number; the original passes `STYLE_BRACELIGHT` (34) and
`STYLE_BRACEBAD` (35), which are **style** numbers. `INDIC_MAX` is 35, so those are
valid indicator ids — the calls silently configure two indicators nothing ever draws
with. Scintilla only takes the indicator path for braces when
`SCI_BRACEHIGHLIGHTINDICATOR` turns it on; neither frontend calls it
(`ViewStyle.cxx:250` defaults it off, `Editor.cxx:8398` is the only setter). Only
`SCI_STYLESETFORE` and `SCI_STYLESETBOLD` do anything. The source comment there —
*"foreground and alpha maybe overridden by style settings"* — reads like the author
was unsure which mechanism applied.

**And the brace colours come from a third colour table.** Not the theme headers and
not the `IS_LIGHT_THEME` preset, but `BasicColors` in `src/AppUtil.h`, which has no
light and dark variant — so these two roles are the only ones identical in both
themes. `BasicColors::red` and the palette's `red` are both `#FF0000` *today*, which
is what makes resolving one through the other possible at all; `parse_brace_styles`
therefore **checks that equality rather than assuming it** and fails loudly if the
two tables drift. Same trap as reading the selection colour out of `editorTextColor`
because the numbers happen to match — caught this time because the first one taught
it.

**The selection half had two things in it that reading the row would not give you.**

**1. `_selectionTextColor` is a background.** It is passed to `SCI_SETSELBACK`
(`CEditorCtrl::SetSelectionTextColor`, `src/Editor.cpp:3154`). A frontend author who
trusted the name would set `SCI_SETSELFORE` and get unreadable selected text.

**2. It is the only editor colour with no single palette key.** The ten members of
`m_AppThemeColorSet` are filled by an `IS_LIGHT_THEME` preset
(`src/Editor.cpp:106-132`). Eight take the constant named after them, which is why
`ui-qt/` could resolve them by name and be right. Two do not:

| role | light | dark |
|---|---|---|
| `lineNumberColor` | `linenumber` | `linenumber` |
| **`selectionTextColor`** | **`black`** | **`white`** |

The second cannot be expressed as one key at all, so `theme.ResolveColor("...")` —
the shape every other colour used — has no correct argument. `theme-*.json` gains a
`roles` object (twelve entries — the ten above plus the two brace colours),
`core/CEditorTheme` gains `ResolveRole`, and the existing colour lookups in
`ui-qt/EditorWidget.cpp` move onto it: eight of them are no-ops today, and that is
the point — they were *assumptions* that happened to hold.

`roles` is deliberately **not** merged into `palette`. `palette` is a faithful mirror
of the header's `COLORREF` declarations and §7's count check asserts exactly that
(34/34); injecting a derived entry would break the one invariant that makes the
palette trustworthy.

**The role names are the C++ member names, misleading one included.** Renaming
`selectionTextColor` to something honest would break the round-trip that lets
`--verify` re-derive it, so the lie is *documented at both consumers*
(`core/LanguageData.h`, `ui-qt/EditorWidget.cpp`) instead of corrected in the data.

**What no test can catch here, stated because the alternative is implying otherwise.**
`selectionTextColor` and `editorTextColor` resolve to the *same two values* —
`#000000` light, `#FFFFFF` dark. A frontend reading the wrong one paints identical
pixels. Mutation-checked: swapping the role at the `ui-qt/` call site leaves all 266
self-test checks green. Seven other mutations each fail it: dropping the `updateUi`
connection, matching the brace at the caret instead of before it, never hiding the
caret line, resolving the selection colour by its own name, dropping the brace
styling, dropping its bold, and swapping the matched and unmatched colours. The correctness of
*which role* is a review property backed by `core/tests/TestLanguageData.cpp` pinning
the palette keys, not a runtime one; the self-test compares against `core/`'s own
resolution so that it becomes a real check the day the two values diverge.

**One thing that looks redundant and is not.** `UpdateCaretLineVisible` re-sets
`SCI_SETSELBACK` on every caret move where the selection is empty. Nothing else in
that function changes it — but `SearchForward` and `SearchBackward` paint the
selection **yellow at alpha 90** to flag a match (`src/Editor.cpp:1904`, `:1936`),
and this is what restores it once the user clicks away. It is deliberately *not*
restored on the else branch, so the yellow survives for as long as the match stays
selected. `ui-qt/`'s find bar does not paint that yellow yet, so today the reset
restores a colour nothing has changed; it is transcribed anyway, because an
"optimisation" that dropped it would be found by eye months later.

**And a note on the harness.** Scintilla does not send `SCN_UPDATEUI` from the
message that moved the caret — it records what changed and flushes from
`Editor::Paint` and `Editor::Idle` (`Editor.cxx:1893`, `:5296`). A headless test that
only sends `SCI_GOTOPOS` runs none of this and asserts against whatever the previous
check left behind. `viewport()->grab()` forces a synchronous `paintEvent` and works
under `QT_QPA_PLATFORM=offscreen`; that is what makes these checks real rather than
decorative.

Reproduce:

```bash
# the role table, and the two departures from name-equals-key
python3 tools/extract_language_data.py --verify | grep 'theme colour roles'
python3 - <<'PY'
import json
for t in ('light', 'dark'):
    d = json.load(open('Packages/data-packages/theme-%s.json' % t))
    r, p = d['roles'], d['palette']
    print(t, 'roles:', len(r),
          '| key != role name:', sorted(k for k, v in r.items() if k != v))
    for role in ('selectionTextColor', 'editorTextColor'):
        print('   %-19s -> %-10s %s' % (role, r[role], p[r[role]]))
PY

# the C++ the roles are derived from
sed -n '106,132p' src/Editor.cpp

# the two ported functions
sed -n '1232,1249p' src/Editor.cpp      # DoBraceMatchHighlight
sed -n '4488,4500p' src/Editor.cpp      # UpdateCaretLineVisible

# the brace styling, 800 lines away from the logic and outside the 110-420
# window the session's derivation command scans - 4 live calls of 10
sed -n '446,456p' src/Editor.cpp
# empty: neither frontend CALLS it, so the indicator path stays off. Matching
# the bare name instead would now hit this port's own explanatory comment.
grep -rnE '(DoCommand|Send)\(SCI_BRACEHIGHLIGHTINDICATOR' src/ ui-qt/

# BasicColors is a third colour table, and the brace colours are only
# resolvable through the palette while the two agree
grep -n 'const COLORREF \(red\|blue\) ' src/AppUtil.h
python3 -c "
import json
for t in ('light', 'dark'):
    p = json.load(open('Packages/data-packages/theme-%s.json' % t))['palette']
    print(t, 'red =', p['red'], ' blue =', p['blue'])"

# 266 checks, up from 164
cmake -S . -B qtbuild -G Ninja -DVINATEXT_BUILD_QT=ON && cmake --build qtbuild --parallel
python3 tools/make_selftest_fixtures.py qtbuild/fixtures
QT_QPA_PLATFORM=offscreen ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py

ctest --test-dir build -R core.LanguageData --output-on-failure
```

---

## 6h. Tag matching, and the fifth key — where every other key looks right

The third and last item of `SCN_UPDATEUI`: `DoXMLHTMLTagsHightlight`
(`src/Editor.cpp:1783`) plus `GetXmlHtmlTagsPosition` (`:1535`) and its four
search helpers — about 400 lines, transcribed into `ui-qt/TagMatcher.{h,cpp}`.

**Which languages get it is a fifth key, and it takes two hops to reach.**
`CEditorView` gates the call on `LANGUAGE_XML`, `LANGUAGE_HTML` and `LANGUAGE_PHP`
(`src/EditorView.cpp:6183-6190`) — the `VINATEXT_SUPPORTED_LANGUAGE` enum, which
none of the four already-extracted fields uses. Getting from there to a language id
goes enum → VinaText token (`DetectCurrentDocLanguage`'s if-chain) → id
(`parse_lexer_dispatch`).

> A trap inside the trap: that if-chain tests `m_czLexerFromFile`, which sounds
> like a Lexilla name and is not. It is `m_strLexerName`, filled by
> `GetLexerNameFromExtension` from `arrLexerNames` — the VinaText dispatch token.
> `phpscript` and `hypertext` are tokens; Lexilla has never heard of either.

**Every other available key gives a wrong answer, and two of them look right:**

| keyed on | result | wrong how |
|---|---|---|
| `lexer == "cpp"` | autoit, c, cpp, cs, go, java, javascript, json, **php**, protobuf, resource, typescript, vcxproject | **13 languages instead of 3.** php really is lexed as `cpp`, so php is correct and the other twelve are not |
| `styleTable == "html"` | html, xml | **drops php silently** |
| `foldMarker == " < ... > "` | html, xml | drops php, by a different route |
| `tagMatch` (this field) | **html, php, xml** | ✅ |

`html` and `xml` come out right under all four, which is exactly what makes the
wrong ones dangerous: the bug is one language wide and invisible in the two cases
anyone would check first.

**Five extracted fields, five different keys.** `lexer` by what Lexilla calls the
language, `styleTable` by which colour table the initialiser walks, `foldMarker`
and `indentGuides` by the VinaText dispatch token, `tagMatch` by the
`VINATEXT_SUPPORTED_LANGUAGE` enum. Nothing about any of their names predicts this.

**Absent means false.** Three of 42 languages carry `tagMatch`, so the other 39 omit
it rather than saying `false`. Unlike `foldMarker` there is no hidden default to
discover: a frontend that does not find the key does nothing, which is correct.

### What the transcription preserved

- **`>` inside an attribute value is data.** `<item note="a>b">` is valid XML, and
  every search skips a `>` whose lexer style is `SCE_H_DOUBLESTRING` or
  `SCE_H_SINGLESTRING`. Without that the open tag appears to end four characters
  early.
- **Nested same-name tags.** `<item><item>…</item></item>` resolves by counting the
  close tags between a candidate open tag and ours, and searching further out when
  the count is non-zero.
- **Self-closing tags** match themselves and report `_TagCloseStart == -1`.
- **A tag name may not be a prefix of a longer one.** `<TAGNAME2` must not satisfy a
  search for `<TAGNAME`, so the character after the name has to be `>` or whitespace.
- **The tag-name scan stops at `"` and `'`**, which is wrong for well-formed XML — a
  quote cannot appear in a name — but behaves better on the malformed XML people
  actually edit. The original says so in a comment; it is preserved deliberately.
- **Target and search flags are saved and restored**, because find/replace owns them.

### Not ported, and why

The attribute highlighting over `INDIC_TAGATTR` is **commented out in the original**
(`src/Editor.cpp:1820-1826`), along with `CEditorCtrl::GetAttributesPos`, the 90-line
state machine that feeds it. It draws nothing on Windows today, so it draws nothing
here. `INDIC_TAGATTR` is still styled and still cleared, both of which the original
also does.

### Two things the tests got wrong first

**1. A check that asserted behaviour no build has ever had.** The first version
asserted the highlight *vanishes* while text is selected. It does not: the MFC gates
the whole call, and the clearing lives *inside* it, so a highlight painted a moment
ago stays on screen while the user selects. The port is faithful and the test was
wrong — caught because the baseline failed. The check now asserts what the gate
actually does: the highlight **freezes**. Park the caret in one tag, select inside a
different one, and the highlight must still describe the first.

**2. A mutation harness that silently tested the previous binary.** Two mutations
"passed" — meaning they were not caught — and both had in fact failed to *compile*:
deleting the calls left a variable unused, which is an error under
`-Werror`. The harness sent build output to `/dev/null`, so it ran the stale
executable and reported a clean run. **A mutation that does not build is not a
mutation that passed.** The harness now fails loudly on a build error, and the two
mutations were rewritten into compiling forms — after which both are caught, by the
checks intended to catch them.

That second one generalises: mutation-checking verifies the *test*, and nothing was
verifying the *mutation*.

### The checks

Invariants computed from the document text, not a table of expected offsets — a
table copied from the matcher's own output agrees with it by construction. For every
`<` in the fixture, with the caret two characters past it:

1. every highlighted run starts at `<` or is a bare tail (`>` / `/>`);
2. the open tag's name and the close tag's name are the same string;
3. the pair encloses the caret's own tag;
4. at most three runs — more means stale highlights were never cleared;
5. the open tag ends at `>` with **balanced quotes** in between, which is what
   catches a search that walked into an attribute value.

Plus the negative: a language that must not tag-match paints nothing.

Mutations, each caught by the check named: gate on the lexer instead of `tagMatch`
(1), drop the empty-selection gate (freeze check), disable the attribute-string skip
(5), never clear the indicators (4 — 10 to 14 runs accumulate). On the extractor:
edit the JSON, drop PHP from the C++ guard, name an enum no token maps to. On
`core/`: drop php's `tagMatch`, give cpp one.

Reproduce:

```bash
# the set, and what each wrong key would have given
python3 - <<'PY'
import sys; sys.path.insert(0, 'tools')
import extract_language_data as x
d, _ = x.cached_dispatch()
f, _ = x.cached_fold_markers()
print('tagMatch          :', sorted(x.cached_tag_match()))
print('by lexer == cpp   :', sorted(k for k, v in d.items() if v.get('lexer') == 'cpp'))
print('by styleTable=html:', sorted(k for k, v in d.items() if v.get('styleTable') == 'html'))
print('by foldMarker < > :', sorted(k for k, v in f.items() if '<' in v))
PY

# the guard, and the chain it has to be read through
sed -n '6183,6190p' src/EditorView.cpp
sed -n '1129,1138p' src/EditorView.cpp

# the attribute highlighting that is commented out in the original
sed -n '1820,1826p' src/Editor.cpp

# 426 checks, up from 266
python3 tools/make_selftest_fixtures.py qtbuild/fixtures
QT_QPA_PLATFORM=offscreen ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml

ctest --test-dir build -R core.LanguageData --output-on-failure
```

---

## 6i. URL hotspots — the first pull that had to replace a Win32 call

`AppSettingMgr.m_bEnableUrlHighlight` ships **TRUE**, so every VinaText user has
underlined URLs and the Qt alpha had none. The scanner is
`src/StringHelper.cpp:87-345` — seven `*Url*` helpers driven by
`AppUtils::IsUrlHyperLink` (`src/AppUtil.cpp:565`), which has exactly **one**
caller, `CEditorCtrl::RenderHotSpotForUrlLinks` (`src/Editor.cpp:4419`).

Extracted to `core/UrlScanner.{h,cpp}` under D9. `src/` is unchanged and still uses
its own copy; `core/tests/TestUrlScanner.cpp` keeps the two in step.

### The Win32 call, and what replaced it

```cpp
bool r = InternetCrackUrl(&text[start], len, 0, &url)
      && StringHelper::isUrlSchemeSupported(url.nScheme);
```

`isUrlSchemeSupported` accepts `INTERNET_SCHEME_{FTP,HTTP,HTTPS,MAILTO,FILE}`. There
is no portable wininet, and adding a URL parser would put a dependency into the one
layer that has none (D6). So `CUrlScanner::IsSupportedScheme` matches the scheme
**text** against the same five names and drops the parse.

**The difference is one-directional, and that is the whole argument for it.** wininet
maps exactly those five spellings onto those five enum values, so anything Windows
accepts, this accepts. It can only differ by **accepting** a candidate wininet would
have rejected as malformed — never by rejecting one Windows underlines. For a
cosmetic underline that is the safe direction, and the candidate has already been
through `scanToUrlEnd`, which admits only `isUrlTextChar` characters.

It is a divergence all the same, it **cannot be verified off Windows**, and it is why
that function exists instead of being inlined.

### Bytes, not wide characters

The MFC converts the document to UTF-16, scans, then converts each segment's length
back with `WideCharToMultiByte` to reach a document offset. `core/` scans the UTF-8
bytes Scintilla already indexes, which removes the round trip.

That is equivalent, not merely close: every byte of a multi-byte UTF-8 sequence is
`>= 0x80`, and at `>= 0x80` all four character classifiers say the same thing they
say about a non-ASCII `wchar_t` — not a URL-text character's opposite, a scheme
delimiter, never a scheme start. So the two agree on every URL boundary and differ
only in the units they express it in.

### The number that meant nothing

The differential test's first form was an exhaustive sweep: every string of length
≤ 5 over an 11-character alphabet, **177,156 comparisons, 0 mismatches**. It looked
strong. It was worthless for the interesting half of the scanner, and mutation is
what showed it:

| mutation | free-form sweep | after the fix |
|---|---|---|
| trailing `.` no longer stripped | 0 mismatches | 1,925 |
| bracket count starts at 0 | 0 mismatches | 1,847 |
| quote parity inverted | 0 mismatches | 36 |
| fragment state never entered | 0 mismatches | 1,961 |
| query state never entered | 0 mismatches | 1,986 |
| quoted query values not recognised | 0 mismatches | 12 |

The cause is arithmetic. The shortest supported scheme spelling is `ftp:` at four
characters and a URL needs at least one more — **so no string of length ≤ 5 over that
alphabet is ever a URL.** Every one of those 177,156 comparisons exercised the reject
paths and nothing else.

The fix is a second sweep prefixed with `ftp:`, whose tail alphabet carries a path
separator, a query and fragment introducer, a query delimiter, both quotes, both
brackets, a full stop, a letter and a space: 16,105 strings of which **11,712 are
actually URLs**. That count is itself asserted, because a sweep that quietly stops
producing URLs is exactly the failure being fixed.

**A large number of comparisons is not coverage.** Nothing in the first sweep was
wrong; it just never reached the code under test, and only mutation could say so.

### And a mutation harness that tested the previous binary

Recorded in §6h and it happened again here: two mutations "passed" because deleting
a call left a variable unused, `-Werror` rejected the build, and the harness had sent
build output to `/dev/null`. Both failures are the same bug in the same place —
**a harness that hides the build is not running the mutation.**

### One dead call, transcribed anyway

`SCI_INDICSETFLAGS(INDIC_URL_HOTSPOT, SC_INDICFLAG_VALUEFORE)` makes the drawn colour
come from the per-range value (`Indicator.cxx:33`), so the
`SCI_INDICSETFORE(..., BasicColors::orange)` two lines earlier never reaches the
screen — URLs are drawn in the default text colour.

Unlike §6g's six dead brace calls this one is transcribed: those addressed the wrong
indicator entirely and could never matter, while this one addresses the right
indicator and would start mattering the day the flag changed.

### One unit of blue, in both frontends

`SCI_SETINDICATORVALUE` is the colour under `SC_INDICFLAG_VALUEFORE`, and
`DecorationList::SetCurrentValue` is `currentValue = value ? value : 1`
(`Decoration.cxx:191-193`) — a value of 0 becomes 1. The light theme's
`editorTextColor` is `RGB(0,0,0)` (`src/EditorColorLight.h:32`), i.e. Scintilla
colour 0, so on light **URLs are drawn in `RGB(0,0,1)`, not pure black**. Measured:
light `STYLE_DEFAULT` fore 0 → stored indicator value **1**; dark 16777215 → 16777215.

Found by the review bot, and left alone deliberately. `CEditorCtrl` does the
identical `SCI_SETINDICATORVALUE(SCI_STYLEGETFORE(STYLE_DEFAULT))` into the identical
vendored Scintilla (`src/Editor.cpp:4426-4428`), so **Windows has the same one unit
of blue**. Special-casing 0 would make `ui-qt/` differ from the shipping app to fix
something no eye can see. The comment at the call site was wrong and is now right;
the code is unchanged.

### When it runs

The original is called **once**, from `LoadEditorSettings` (`:409-412`) — not from any
notification. A URL typed after the file is open is not underlined until the editor
is re-styled. `ui-qt/` calls it from `ApplyTheme`, the same moment. Hooking
`SCN_MODIFIED` would be an improvement and a behaviour change; it is not this change.

Reproduce:

```bash
# the scanner, against a verbatim transcription of the original
ctest --test-dir build -R core.UrlScanner --output-on-failure
./build/TestUrlScanner        # prints both sweeps and how many were URLs

# the one caller, and the setting that ships TRUE
sed -n '4419,4429p' src/Editor.cpp
grep -n 'm_bEnableUrlHighlight' src/AppSettings.h src/AppSettings.cpp

# the Win32 call that had to be replaced
sed -n '578,590p' src/AppUtil.cpp

# 474 checks, up from 426
python3 tools/make_selftest_fixtures.py qtbuild/fixtures
QT_QPA_PLATFORM=offscreen ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml qtbuild/fixtures/urls.md
```

---

## 6j. Autocomplete, and what the nine missing messages actually were

The last item of Phase 4's editor parity, and the one that closes the derivation
this phase was steered by:

```bash
for m in $(sed -n '110,420p' src/Editor.cpp | grep -oE "SCI_[A-Z_]+" | sort -u); do
  grep -q "$m" ui-qt/EditorWidget.cpp || echo "missing: $m"
done
```

It printed **9**. Two of them were filed wrongly, and reading them was the only way
to find out:

| message | assumed | actually |
|---|---|---|
| 4 × `SCI_AUTOC*` | autocomplete | ✅ autocomplete |
| 3 × RGBA image | bookmark markers, Phase 5 | **the autocomplete list-box icon** — `IDR_AUTO_COMPLETE`, `src/Editor.cpp:371-377` |
| `SCI_MARKERENABLEHIGHLIGHT` | bookmark markers, Phase 5 | **folding** — `m_bEnableHightLightFolder`, `:341-348`. Missed by §6f |
| `SCI_SETFOLDFLAGS` | folding, missing | **folding, and correctly absent** |

**`SCI_SETFOLDFLAGS` is the interesting one.** The original calls it only when
`m_bDrawFoldingLineUnderLineStyle` is TRUE, and AppSettings ships it **FALSE**
(`src/AppSettings.cpp:19`) — so *not* calling it is the shipped behaviour, and its
appearance in the "missing" list was the grep counting a message the default build
never sends. The same shape as §6f's fold-marker branch, where copying the RGB
literals would have been faithful to the text and wrong about the behaviour.

**`SCI_MARKERENABLEHIGHLIGHT` is the opposite**: `m_bEnableHightLightFolder` ships
**TRUE**, so the fold marker containing the caret is highlighted on Windows and was
not here. A real gap left by §6f, fixed in this change — one line, and the
derivation is what surfaced it.

After this change the command prints **3**: the RGBA image calls, which need an icon
resource `ui-qt/` does not have. That is a resource question, not logic.

### The list

`CEditorView::GetAutoCompleteList` (`src/EditorView.cpp:5823`) draws from three
sources. Two are ported:

1. **The language's keywords** — the same blob `core/` already carries, split on a
   single space exactly as `AppUtils::SplitterCString` does.
2. **The words already in the document**, found with `EDITOR_REGEX_AUTO_COMPLETE_PATTERN`
   (`src/MacroDef.h:83`) under `SCFIND_WORDSTART | SCFIND_REGEXP | SCFIND_POSIX`,
   dropping `SCFIND_MATCHCASE` when ignore-case is on, as the original does.

The third is **not**, and it is not an omission: `m_AutoCompelteDataset` is an
English vocabulary list that is empty unless the user picks a menu item which loads
`Packages/translator-packages/english-words.ee-package` and pops a message box
(`:7927-7945`). A user-invoked extra, and `ui-qt/` has no menu to invoke it from.

**One quirk preserved:** document words are de-duplicated **case-sensitively**
(`CString::operator==`) even when the search that found them ignored case — so `Foo`
and `foo` are both offered.

### A latent defect, reproduced rather than fixed

The list is built keywords-first then document-words, and is therefore **not
sorted**. Scintilla's `AutoComplete::Select` binary-searches it
(`AutoComplete.cxx`), and the ordering defaults to `Ordering::PreSorted`
(`:53`) — which the original never changes. A binary search over an unsorted list
can miss a match that is present.

Measured on one case and it worked: list `continue,container`, typing `cont`
selected `continue`. That is one data point, not a proof, and the mechanism is
fragile.

Left alone deliberately. `SCI_AUTOCSETORDER(SC_ORDER_PERFORMSORT)` would fix it in
one line and would change the order the user sees, which the shipping app does not
do. Recorded here as a decision for whoever wants it, in the same way §6d recorded
`.json` being lexed as C++.

### The check that passed for the wrong reason

`m_bAutoCompleteIgnoreNumbers` ships TRUE, so an all-digit prefix contributes no
document words. The check for that — "a numeric prefix offers nothing" — passed
with the rule **removed**, because no word anywhere in the fixture corpus starts
with a digit. It was asserting something no code path could violate.

`1234` was added to the `crlf-bom.cpp` fixture for exactly this, and the mutation
then fails as intended. Same family as §6i's 177,156 comparisons that never reached
a URL: **an assertion that cannot fail is not an assertion**, and only mutation says
which ones those are.

Reproduce:

```bash
# 9 before this change, 3 after - and the 3 are the icon resource
for m in $(sed -n '110,420p' src/Editor.cpp | grep -oE "SCI_[A-Z_]+" | sort -u); do
  grep -q "$m" ui-qt/EditorWidget.cpp || echo "missing: $m"
done

# the two folding messages, and why only one of them was a gap
sed -n '341,348p' src/Editor.cpp                       # MARKERENABLEHIGHLIGHT
grep -n 'm_bEnableHightLightFolder' src/AppSettings.cpp        # TRUE  -> was missing
sed -n '187,190p' src/Editor.cpp                       # SETFOLDFLAGS
grep -n 'm_bDrawFoldingLineUnderLineStyle' src/AppSettings.cpp # FALSE -> correctly absent

# the RGBA calls are the autocomplete icon, not a bookmark marker
sed -n '370,378p' src/Editor.cpp

# the dataset that is deliberately not ported
sed -n '7927,7945p' src/EditorView.cpp

# 536 checks, up from 474
python3 tools/make_selftest_fixtures.py qtbuild/fixtures
QT_QPA_PLATFORM=offscreen ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml qtbuild/fixtures/urls.md
```

---

## 6k. Phase 5's first dock pane — the framework, not the pane

Phase 4 is done. This is the **first slice of Phase 5 and deliberately only that**:
the `QDockWidget` framework, show/hide, save/restore geometry, and one pane's
content. The other eight panes and all thirty dialogs wait until this shape has
been looked at.

**Re-derived, because the brief's number is stale.** `ls src/*Dlg.cpp | wc -l` is
**30**, not the "~40" in §5 of the brief, and all 30 are in `src/VinaText.vcxproj`
— unlike the dock panes, where §2 found two that were never compiled. All nine
surviving panes are built.

### Why MessageWindow first

| pane | LOC | |
|---|---:|---|
| `FileExplorerWindow` | 85 | a shell for `FileExplorerCtrl`, 6,217 lines of `ui-rewrite` |
| `SearchAndReplaceWindow` | 124 | |
| `OpenTabWindows` | 214 | |
| **`MessageWindow`** | **582** | ← |
| `BreakpointWindow` / `BookmarkWindow` | 831 / 832 | need Phase 5's marker work |
| `PathResultWindow` / `SearchResultWindow` | 1,168 / 1,175 | need find-in-files |
| `BuildWindow` | 1,885 | needs the compiler layer |

`MessageWindow` is the simplest one that is **useful the moment it exists**.
`SearchResultWindow` was the other suggestion and would have landed as an empty
pane, because `ui-qt/` has no find-in-files to fill it. Meanwhile `ui-qt/` reported
file errors in a `QMessageBox` plus a status-bar message that expires after five
seconds — there was nowhere to look and see what had happened.

### 582 lines become about 120

The pane's entire public surface is two methods: `AddLogMessage(text, colour)` and
`ClearAll()`. Everything else in that file is scaffolding that Qt supplies:

| MFC | Qt |
|---|---|
| `CDockPaneBase : CDockablePane` | `QDockWidget` |
| `CMessagePaneDlg : CDialogEx` hosted inside it | *gone* — the dock holds the widget directly |
| `CRichEditCtrlEX : CRichEditCtrl` | `QPlainTextEdit` + a `QTextCharFormat` |
| `DoDataExchange`, `OnSize`, `OnMoving`, `OnInitDialog` | *gone* — layouts |
| a hand-rolled checkable menu item | `QDockWidget::toggleViewAction()` |

**A `QTextCursor` with a character format, not `appendHtml()`.** Log lines carry
file paths and compiler output, which contain `<` and `&` as data. Going through
HTML would mean escaping them, and would silently mangle any line that was not.

**Two quirks preserved.** An empty message is ignored. And the original tests
`str.Find('\n') != -1` — a newline *anywhere*, not at the end — before deciding
whether to append one, so a multi-line message that does not end in a newline gets
none and the next message continues its last line. The callers all pass single
lines; "fixing" it would change where the breaks fall for any caller that does not.

### Geometry: a deliberate divergence in mechanism

`QMainWindow::saveState`/`restoreState` plus `saveGeometry`/`restoreGeometry`,
stored in `QSettings`. **Not** `AppSettings`: the MFC persists docking through
`CDockingManager` into the registry, which has no portable counterpart and is not a
file this port could read. Same behaviour — the pane comes back where it was left —
by a different mechanism, which is the right trade here and worth naming as one.

`CMessagePane` sets an `objectName`, and that is load-bearing rather than tidy:
`saveState` keys docks by it, and a mutation removing it fails both persistence
checks.

### The self-test had to start showing the window

The dock checks failed on first run, and the cause was the harness, not the pane:
**`RunSelfTest` never showed the window.** A `QDockWidget`'s visibility is only real
once its parent window is — `setVisible(true)` on a child of a hidden window leaves
`isVisible()` false and `isHidden()` unchanged. Measured:

```
PANEPROBE afterHide  hidden=0 visible=0 checked=1 winVisible=0
```

So the checks were asserting against a state no user could ever be in. `show()` is
now the first thing `RunSelfTest` does — harmless under
`QT_QPA_PLATFORM=offscreen`, and what `RenderScreenshots` already did.

That is the third harness defect this phase, after §6h's silent build failures and
§6i's sweep that never reached its own subject. All three had the same shape: the
test ran, reported success, and was measuring something other than what it claimed.

### Checks

Content in order and in the right colour; an empty message ignored; a message
ending in a newline not given a second; `ClearAll`; show and hide through the same
action the View menu uses, both ways, with the menu item's checked state following;
and the layout round-tripping through the real `Save`/`RestoreDockState` —
saved-hidden restores hidden **and** saved-visible restores visible, so the check
cannot pass on a restore that simply hides everything.

**And the isolation has to happen before the window exists.** `CMainWindow`'s
constructor restores the layout from `QSettings`, so scoping a `QTemporaryDir`
inside `RunSelfTest` is already too late: a `--selftest` would inherit whatever an
earlier interactive session saved, and fail on a pane the user had merely closed.
Found by the review bot and reproduced — seeding a hidden-pane layout makes the
next, otherwise untouched self-test fail three checks, and that failure persists
across runs because the state is on disk. `main.cpp` now points `QSettings` at a
scratch directory for the whole process whenever a headless mode is set, which also
means `--selftest` and `--screenshot` can no longer write to a real install's
settings at all.

Mutations caught: colour ignored, empty message not ignored, newline always
appended, state never saved, state never restored, `objectName` removed.

**Self-test: 536 → 553 checks.**

Reproduce:

```bash
# the counts, re-derived
ls src/*Dlg.cpp | wc -l                                    # 30, not ~40
for f in src/*Dlg.cpp; do grep -q "$(basename $f)" src/VinaText.vcxproj \
  || echo "not built: $f"; done                            # silent: all 30 are

# the pane's whole public surface
sed -n '87,105p' src/MessageWindow.h

# 553 checks, up from 536
QT_QPA_PLATFORM=offscreen ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml qtbuild/fixtures/urls.md
```

---

## 6l. Goto — a dialog that was never a dialog

**The record skipped five PRs.** §6k ends at 553 checks and the suite is now at
610; #41 (About), #42 (Replace), #43 (the macOS shortcut fix), #44 (AppSettings
read) and #45 (Preferences) added 57 checks between them and wrote no section
here. Their reasoning is in their commit messages only. That is worth knowing
before reading this as a continuous record, and worth not repeating.

### The design question answered itself

The task was "decide deliberately whether this is a dialog at all". It is not,
and not by preference — **`CGotoDlg` is not a dialog in the MFC either.**

```
IDD_POS DIALOGEX 0, 0, 227, 185
STYLE DS_SETFONT | WS_CHILD | WS_SYSMENU        <- WS_CHILD
```

`m_GotoDlg.Create(IDD_POS, &m_CTabCtrl)` (`src/SearchAndReplaceDlg.cpp:341`)
makes it **tab 2 of `CSearchAndReplaceWindowDlg`'s tab control**, beside Find,
Replace and Bracket Outline. `OnOK` and `OnCancel` are overridden to empty
bodies (`src/GotoDlg.h:41-42`), so it has no accept and no cancel; its Escape
handler hands focus back to the editor rather than closing anything. There is
nothing there to make modal.

Two consequences the brief does not draw out:

- `ui-qt/` already ships tabs 0 and 1 of that same control as `CFindBar`, so a
  goto bar in the same place **reproduces the MFC's own grouping** rather than
  merely following a modern convention.
- The dialog D10 *keeps* is hosted by a dock pane D10 *defers*
  (`SearchAndReplaceWindow` is named in the deferred six). A faithful port would
  mean standing up a deferred pane to hold a kept dialog.

Not a third mode of `CFindBar`, though. That widget is one widget in two modes
**because** find and replace share the pattern, the options and the match count;
goto shares none of them.

### The tab carries six operations, not two

`IDD_POS` has 4 edits and 6 buttons. The prompt for this work described it as
"go to line, and the go to offset mode the same dialog carries" — that is two of
the six.

| MFC control | what it calls | ported as |
|---|---|---|
| `IDC_LINE` + GO | `CEditorCtrl::GotoLine` | goto bar, line field |
| `IDC_POSITION` + GO | `GotoPosition` | goto bar, offset field |
| `ID_EDITOR_GOTO_POINT_X`/`_Y` + GO | `GotoPointXY` | **not ported** |
| `..._LINE_GO_PREVIOUS_PARAH` | `SCI_PARAUP` | Search menu, Ctrl+[ |
| `..._LINE_GO_NEXT_PARAH` | `SCI_PARADOWN` | Search menu, Ctrl+] |
| `..._LINE_GO_TO_CARET` | `SetLineCenterDisplay(GetCurrentLine())` | Search menu |

**This split is the MFC's own, not a judgement call.** `src/VinaText.rc:402-411`
is a **`POPUP "Goto..."` menu** carrying exactly these operations, in this
order, with separators in exactly these two places:

```
POPUP "Goto..."
BEGIN
    MENUITEM "Goto Line\tCtrl+G",           ID_OPTIONS_GOTOLINE
    MENUITEM "Goto Position",               ID_OPTIONS_GOTOPOS
    MENUITEM SEPARATOR
    MENUITEM "Goto Next Paragraph\tCtrl+]", ID_OPTIONS_GOTO_NEXT_PARAGRAPH
    MENUITEM "Goto Previous Paragraph\tCtrl+[", ID_OPTIONS_GOTO_PREV_PARAGRAPH
    MENUITEM SEPARATOR
    MENUITEM "Goto To Caret\tMiddle Mouse", ID_EDIT_SCROLL_TO_CARET
END
```

The tab and this menu are two front ends onto the same six handlers, and the
menu is the one that maps onto an editor. `ui-qt/`'s Search menu reproduces it
item for item and separator for separator.

**Note what that menu does not carry: Goto Point.** The MFC does not consider it
a command worth exposing anywhere but the tab, which settles the one operation
not ported here. `GotoPointXY` is `SCI_POSITIONFROMPOINT`, which takes
**client-area pixel coordinates** — so where the caret lands depends on the
scroll offset and the window size at the instant GO is pressed. It is a
debugging probe rather than navigation. `grep -rn GotoPointXY src/*.cpp src/*.h`
returns three hits: the declaration, the definition, and a **single** call site,
which is this tab.

**Goto To Caret gets no keyboard shortcut here, because it has none there
either** — its binding is the middle mouse button. A menu item is the entry
point that survives having no third button.

### Two asymmetries transcribed rather than tidied

`GotoLine` guards `lLine < 0`, expands folds, jumps, then centres.
`GotoPosition` guards nothing, expands folds, and jumps **without centring**.
Both are preserved:

- **The guard is `< 0`, not `< 1`, and that is load-bearing.** An empty edit
  through `_ttoi` is `0`, so an empty box reaches `SCI_GOTOLINE` with `-1` and
  Scintilla clamps to the first line. Pressing GO on an empty box goes to the top
  of the document, and tightening the guard to `< 1` to look tidier would make it
  do nothing instead. A check covers exactly this.
- **The centring is one line out.** `GotoLine` passes the 1-based
  `GetCurrentLine()` to `SetLineCenterDisplay`, which indexes document lines from
  0. Reproduced, for the same reason §6j reproduced the unsorted autocomplete
  list: the MFC has the same defect and two frontends scrolling differently is
  worse than both scrolling one line low. The check encodes the off-by-one on
  purpose, so "fixing" it fails here first.

### Two bars at once, which the MFC cannot do

The find bar and the goto bar are independent strips and can both be open. The
MFC makes them mutually exclusive only because they are pages of one tab
control. Hiding a search the user has set up because they also want to jump to a
line would lose state for no reason. Both are in the screenshot.

### Ctrl+G on macOS, a claim that was wrong, and the third instance of the Cmd+H bug

`src/VinaText.rc`'s accelerator table binds `ID_OPTIONS_GOTOLINE` to Ctrl+G, and
Qt maps `Qt::CTRL` to Command. The reasoning written down first was: *Cmd+G is
already Find Next on macOS, so Ctrl+G would collide, and the duplicate-shortcut
check from the Replace/Cmd+H PR catches it.*

**That was asserted and then tested, and it was false.** Binding goto to Ctrl+G
produced no failure at all. The reason turned out to be a defect rather than a
quirk:

```
QKeySequence::keyBindings(FindNext)     -> [F3, Ctrl+G]
QKeySequence::keyBindings(FindPrevious) -> [Shift+F3, Ctrl+Shift+G]
```

`addAction(text, QKeySequence::FindNext, ...)` takes the **first** binding only.
So Find Next shipped answering to **F3 and nothing else**, while Cmd+G — the
macOS convention, listed by Qt itself — did nothing. On a Mac laptop F3 needs Fn
held down, so the binding that worked was the awkward one and the natural one was
dead. Nothing collided with Ctrl+G because Cmd+G was not bound to anything.

**That is the third instance of one bug**, after Replace on Cmd+H (reserved by
the OS) and Exit on `Qt::Key_Exit` (a key no Mac keyboard has). A shortcut that
*resolves* is not a shortcut that *arrives*.

**And the check could not have caught it**, because it read `shortcut()` — the
primary — and never `shortcuts()`. Every secondary binding in the app was
invisible to the check written to police bindings. Both fixed — `setShortcuts` on
Find Next and Find Previous, and the check now iterates every sequence an action
answers to. Only *then* does the original claim become true, demonstrated rather
than asserted: binding goto to Ctrl+G gives
`shortcut: ⌘G is bound twice, most recently by '&Go to Line...'`.

**And then CI failed on Linux, with that same new check.** Qt lists Ctrl+G for
Find Next on Linux and Windows too, so installing every binding took the key away
from Go to Line — where `src/VinaText.rc`'s accelerator and every editor on those
platforms puts it. Two corrections followed:

- **Ownership is decided once, before either action is bound.** `gotoKey` is
  Cmd+L on macOS and Ctrl+G elsewhere, and Find Next's bindings are Qt's list
  **with `gotoKey` filtered out** rather than an `#ifdef` — so the two cannot
  both be assigned it on any platform, including ones nobody has thought about.
  On macOS the filter removes nothing.
- **The check states the real invariant.** "Every binding is installed" was
  wrong, and Linux was right to fail it. The rule is that **no binding Qt lists
  is left doing nothing** — each is either installed on that action or claimed
  by another. That is precisely what the shipped defect violated: Cmd+G was
  neither.

**One thing the local sweep cannot cover, stated rather than left looking like
coverage.** On macOS `gotoKey` is Cmd+L, which is not in Find Next's binding
list, so the filter is **inert** here and no local mutation of it can change a
result. It is exercised on Linux. Verified by hand by forcing the non-macOS
branch locally: with the filter the suite passes; without it the duplicate check
reports `⌘G is bound twice`.

**Confirmed by a person on macOS, 2026-08-04: Cmd+L opens the goto bar and Cmd+G
now works for Find Next**, where it previously did nothing. That confirmation is
the evidence for this section, not the checks — the harness can prove a key is
*bound* and cannot prove it *arrives*, which is exactly how the first two
instances got through. **Every new menu shortcut still needs this step**; nothing
here makes it automatable.

### Two review findings, both real, both about a field that lies

Both premises reproduced, and both were worth fixing for reasons a little
different from the ones given.

**`QString::toInt` overflows to zero.** Typing `99999999999` — or anything past
`2147483647` — gave `0`, which this port has deliberately made mean *the top of
the document*. So a user asking for a line far past the end silently landed at
the **opposite end**, indistinguishable from an empty box. Measured:

```
""            -> 0 (ok=0)
"999999"      -> 999999 (ok=1)
"99999999999" -> 0 (ok=0)
"2147483648"  -> 0 (ok=0)
```

The suggested fix was capping the validator. Capping refuses keystrokes
silently, and would be the second time this port let a widget's range rewrite
what the user typed. `CGotoBar::ParseTarget` instead returns `INT_MAX` for a
non-empty run of digits that will not convert, so Scintilla clamps it to the end
— which is **already** what a merely-large-but-representable number does. The
two now agree. Empty still means 0, so the `< 0` guard behaviour above is
untouched.

**The offset box went stale on a tab switch.** Only the labels were refreshed, so
the field kept showing the *previous* document's caret offset. The MFC has the
same staleness — `CGotoDlg::ClearAll` is reachable only from
`CMainFrame::OnCleanUpAllWindows`, not from a document switch — so this is a
deliberate divergence, and the reason is that this port gave the field a
**readout** role: it opens on the caret position. A readout showing another
document's number is a field that lies, and a byte offset means nothing outside
the document it was measured in.

The fix is a rule rather than a patch. `SyncToDocument` fills **everything
derived from the document** — both ranges and the offset — and is called from
both `Activate` and the tab-change handler; having it in two places is how the
two drifted apart. The line field is deliberately untouched, because nothing ever
fills it from the document, so typing in it survives a tab switch.

### The harness broke again, in a new way

Two of the four defects this session were in the verification machinery, keeping
§6i–§6k's pattern intact:

1. **A mutation harness whose restore step left the mutation in the binary.**
   `cp` for the backup and `mv` to restore gives the restored file the
   *backup's* mtime, which is older than the object built from the mutated
   source — so ninja reported "no work to do" and never rebuilt it. Mutations
   accumulated across cases, and the "clean" self-test afterwards was running a
   mutated binary. Same family as §6i's harness that re-ran a stale binary,
   reached by a different route. Fixed with `touch` after restore, plus a final
   clean run the harness now asserts on rather than prints.
2. **A mapping check that measured an identity.** `SetFirstVisibleLine` converts
   document lines to visible lines through `SCI_VISIBLEFROMDOCLINE`. With nothing
   folded the two spaces are equal, so deleting the conversion changed no result
   and the mutation went **uncaught** — every other check ran on a fully expanded
   document. Two wrong attempts at fixing it, both worth recording:
   - **Word wrap does not do it.** `SCI_VISIBLEFROMDOCLINE` counts lines hidden
     by *folds*; wrap rows are display rows it does not touch. The wrapped
     version asserted `182 != 182` and failed its own guard.
   - **`SCI_FOLDALL` does not do it either.** Contracting every fold in a
     521-line file leaves **12 visible lines** — fewer than the 39 that fit on
     screen — so nothing can scroll and first-visible is pinned at 0. Measured,
     after that version also failed its own guard.

   Folding **one** block is what works. `ScrollToCaret` is the only public path
   that can meet hidden lines at all, because both goto paths expand folds
   before they scroll.

   Both guards firing rather than passing is the point: they were written as
   `Require(found, ...)` precisely so that a check with nothing to check says so.

### Checks

Line and offset land exactly; `GotoLine` is 1-based; it centres and
`GotoPosition` does not (asserted relationally, so it holds whatever the
offscreen viewport turns out to be); an empty box goes to the top; a negative is
refused; jumping into a collapsed fold expands it; Scroll to Caret moves the view
and not the caret; scrolling counts visible and not document lines; the two
paragraph commands move in opposite directions; the bar's readout carries the
current document's line count and follows a tab switch; the offset box opens on
the caret; the menu action opens the bar; the close path hides it.

**14 mutations, 14 caught** — including the one that was missed on the first
sweep, and the two covering the review fixes. The filter case is not among them,
for the reason given above. The centring check mirrors the implementation's own arithmetic, so it is a
change-detector for that formula rather than an independent oracle; the
asymmetry check is the independent part.

**Self-test: 610 → 656 checks on defaults, 614 → 660 configured.**

Reproduce:

```bash
# it is a child window, and a tab page, not a dialog
sed -n '1406,1408p' src/VinaText.rc                  # STYLE ... WS_CHILD
grep -n 'm_GotoDlg.Create' src/SearchAndReplaceDlg.cpp
sed -n '41,42p' src/GotoDlg.h                        # OnOK/OnCancel, empty

# 4 edits and 6 buttons - six operations, not two
sed -n '1406,1424p' src/VinaText.rc | grep -c EDITTEXT       # 4
sed -n '1406,1424p' src/VinaText.rc | grep -c DEFPUSHBUTTON  # 6

# the MFC's own Goto menu - the grouping ui-qt/ reproduces, and the operation
# it leaves out
sed -n '402,411p' src/VinaText.rc

# that operation has one CALL site (the other two hits declare and define it)
grep -rn 'GotoPointXY' src/*.cpp src/*.h

# Ctrl+G, which is why macOS could not have it
awk '/ACCELERATORS/,/^END/' src/VinaText.rc | grep ID_OPTIONS_GOTOLINE

# 292 lines become 208 across two files
wc -l src/GotoDlg.cpp ui-qt/GotoBar.cpp ui-qt/GotoBar.h

# 656 checks, up from 610
QT_QPA_PLATFORM=offscreen perl -e 'alarm 300; exec @ARGV or die "exec failed: $!"' -- \
  ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml qtbuild/fixtures/urls.md
```

---

## 6m. The encoding picker, and two operations that must not be confused

### The premise this was started from was wrong

The brief for this work said the MFC *conflates* reinterpreting bytes with
converting text, and asked which of the two `CCodePageMFCDlg` performs. **It
performs both, and the MFC does not conflate them at all.** One dialog class,
an `m_bReopen` flag its callers set, and two entirely separate paths:

| `m_bReopen` | Caller | OK button | Does |
|---|---|---|---|
| `TRUE` | `CVinaTextApp::OnFileOpenAsEncoding` | **"Reopen File"** | `SetEncodingFromUser` → `OnReLoadDocument` — **reinterpret** |
| `FALSE` | `CEditorDoc::OnFileSaveAsEncoding` | **"Save File"** | `SetSaveEncoding` → `DoSaveDocument` — **convert** |

The button caption is the only thing on screen distinguishing them at the moment
of committing, so it is reproduced verbatim rather than tidied into one generic
OK. `ui-qt/` keeps the same shape: one `CEncodingDialog`, an `EMode`, two menus.

**And the menu layout is the MFC's own** (`src/VinaText.rc:306-331`): a
`POPUP "Reopen With Encoding"` directly under Open File and a
`POPUP "Save As Encoding"` under the save group, each with the same six fixed
encodings, a separator, and `"Code Page Table..."`. Reproduced item for item —
the same thing §6l found for Goto, and the second time the `.rc`'s own menus
turned out to be the right guide to what an editor needs.

### Two things in the MFC worth knowing, neither of them a shipping bug

- **`m_bReopen` is never initialised.** Both callers set it, so it is latent —
  but a third caller that forgot would choose between reinterpret and convert
  from stack garbage, which is the exact silent-data-loss failure.
- **Five `OnUpdate*` handlers gate the save-encoding family on
  `IsReadOnlyEditor()`** — inverted, since a read-only document is the one you
  *cannot* save. The obvious reading is "Save As Encoding is unreachable on
  Windows". **That reading is wrong**: `CEditorDoc`'s message map has 10
  `ON_COMMAND` entries and **zero** `ON_UPDATE_COMMAND_UI`, so not one of those
  handlers is ever wired. Dead code. Checked before reporting it, because the
  wrong version of that sentence would have been alarming and false.

### 805 encodings, from a module already linked

`QStringConverter` offers **9** encodings, all Unicode plus Latin-1 — and no
Vietnamese codepage, in a Vietnamese editor. `QTextCodec` offers **805**
including `windows-1258`, and Qt5Compat is **already** a dependency of
`vinatext-qt` because Scintilla's own Qt binding uses `QTextCodec` in
`PlatQt.cpp` and `ScintillaQt.cpp`. So the full list costs no new dependency.

**The two paths are chosen in exactly one place each direction**
(`SetSaveEncoding` for identity, `EncodeForSave`/`DecodeBytes` for the bytes),
and a name **both** libraries know goes to `QStringConverter`. That is
deliberate: those are the encodings whose byte-for-byte behaviour this port
already has round-trip fixtures for, and routing them through the compatibility
module instead would quietly change which bytes a UTF-8 save produces.

The list dialog gets a filter box. 805 rows without one is unusable — the MFC
ships exactly that.

### What mutation testing found, which reading the code did not

Every encoding check passed on the first run. Five of the first eight mutations
were **MISSED**, and two of the holes were defects rather than weak checks:

1. **The menus were not covered at all.** Every check drove
   `CEditorWidget` directly, so *swapping the two submenus* — the single worst
   mistake available here — failed nothing. A check now goes through
   `ApplyEncoding`, which is what the menus call.
2. **`m_bHasBom = false` on the codec path was untested AND wrong.** It was
   written as "the invariant". The invariant is actually enforced in
   `EncodeForSave`, which passes `QTextCodec::IgnoreHeader` and never consults
   `m_bHasBom` — so the line bought nothing, while it **permanently destroyed**
   the byte-order mark for a document that went UTF-8 → codepage → UTF-8, since
   the builtin branch has nothing to restore it from. Removed; a check now
   asserts the mark comes back.
3. **A name check could not tell the two paths apart.** `GetEncodingName()`
   returns `"UTF-8"` whichever library handled it, so asserting on it passed
   with every name forced onto the codec path. The observable difference is the
   BOM, and the check is written through that instead.
4. **"some error" is not "the right error".** The untitled-document check
   asserted only that reinterpreting failed. Without the `IsUntitled` guard it
   still fails — `QFile("")` cannot open — so the check passed with the guard
   removed. It now asserts the message.
5. **A guard that could never fire.** `SelectedEncoding` tested
   `selected.first()->isHidden()`. Measured: `QTreeWidget` **clears** the
   selection when the current row is hidden, so `selectedItems()` is already
   empty and the clause was dead code dressed as a guard. Removed, and the
   self-test now asserts *Qt's* behaviour, which is what the dialog relies on.

**Two mutations remain MISSED, and both are recorded as uncovered rather than
left looking checked:**

- **`IgnoreHeader` on the encode path.** Unreachable: every codec that would
  emit a mark is a Unicode one, and all of those route to `QStringConverter`.
  Removing it changes no result today. Defensive, and said so in the code.
- **A hidden row staying selected.** That is Qt's behaviour, not this code's, so
  no mutation of this repository can produce it.

### The eighth instance, found in review

`ui-qt/`'s fixed menu table read `{ "ANSI", "System" }` — the **enumerator's**
name. `QStringConverter` calls that encoding **`"Locale"`**:

```
nameForEncoding(System)   = 'Locale'
encodingForName("System") -> none    | QTextCodec::codecForName("System") -> NULL
encodingForName("Locale") -> Locale
```

So the eighth instance of *the name of a thing is not the name of the thing it
uses* went in exactly like the previous seven: the string looked right. Two
distinct defects fell out of it, and **the review found the milder one**:

- **Reported:** the picker offers `"Locale"` (via `nameForEncoding`), that
  resolves, and `GetEncodingLabel()`'s switch had no `System` case — so it fell
  into `default: "UTF-8"` and the status bar named a *different encoding from
  the one about to be written*.
- **Not reported, and worse:** the **ANSI menu item was entirely broken.** It
  passed the literal `"System"`, which resolves in neither library, so choosing
  it failed outright. That is the second time in this session the real defect
  was worse than the one described.

Both fixed at the source rather than at the symptom: **the table now holds enum
values and the name is derived** with `nameForEncoding`, which makes a
non-resolving entry structurally impossible rather than merely detected. And
`GetEncodingLabel`'s `default:` no longer guesses — it falls back to
`nameForEncoding`, because guessing "UTF-8" is precisely how the `System` case
hid.

**And the checks that should have caught it did not exist.** Three encodings
were exercised by hand out of the 805 the picker offers and the six the menus
hard-code. Offering an encoding is a promise that choosing it works, so that is
now the check: **every** name in `AvailableEncodings()` must be accepted and
must not be labelled `UTF-8` unless it *resolves* to UTF-8, and **every** fixed
menu item must name an encoding that resolves — read off the action's own
`data()`, so a check can ask an item what it will actually apply.

One correction on the way: comparing the *requested* name against `"UTF-8"`
flagged 13 encodings that are simply **aliases** of it (`ibm-1208`, `utf8`, …)
and are labelled correctly. The comparison is against the **resolved** encoding
instead — a check that cries wolf on correct behaviour gets deleted, not obeyed.

### A third round, and the finding sat on top of a worse one again

The review then found that when a chosen codec is **unavailable at save time**,
`EncodeForSave` fell through to the builtin encoding while `GetEncodingLabel()`
went on naming the codec — the same silent mislabelling as the `System` case.
True. The suggested fix was to clear `m_CodecName` so the label matched.

**That fix is too weak, and reading the function to apply it found something
worse.** Relabelling only makes the mislabelling honest *after the fact*: the
file still contains an encoding nobody chose. And `SaveFile` opened with
`QIODevice::Truncate` **before** calling `EncodeForSave` — so the file was
emptied before the bytes existed. Nothing could fail in between when that was
written; the moment the encoder could, a refused save would have left a
**zero-byte file where the document had been.**

Both fixed:

- **The save refuses.** `EncodeForSave` returns `std::optional` and yields
  nothing when the named codec is gone. Refusing is the only outcome that cannot
  lose information; the user can pick another encoding.
- **The encode happens before the open.** A save that cannot produce bytes must
  not have destroyed the old ones getting there.

**And this one is covered rather than documented as unreachable.**
`SetSaveEncoding` refuses names that do not resolve, so the branch has no
natural route — a test seam (`SetCodecNameForTest`, following the existing
`OnCharAddedForTest` precedent) reaches it, which turned a third
"uncovered, said so" branch into two real checks. Both mutations reproduce the
defects:

```
gone codec falls through   -> a save REFUSES when the chosen codec is gone ... FAIL
truncate before encoding   -> the file on disk is UNTOUCHED ... FAIL
```

### A fourth round: a BOM belongs to one encoding, not to "the bytes"

`ReloadWithEncoding` re-derived the byte-order mark with
`encodingForData(raw).has_value()` — which asks only *"do these bytes start with
a mark anybody would recognise"*. So reinterpreting a UTF-8-with-BOM file left
the document claiming a mark whatever it was now being read as. Two
consequences, and **measuring them split the finding in two**:

| reinterpreted as | what happens | harm |
|---|---|---|
| Latin-1 | label reads **"Latin-1 BOM"** | label only — measured 10 bytes in, 10 out, because Latin-1 **ignores** `WriteBom`, and those `EF BB BF` are three ordinary characters now |
| UTF-16LE | save writes **`FF FE`** | real: bytes injected that the file never had |

The review called the byte half a *"could also cause"*. It is an actual — and
the *other* half, which reads like the obvious one, turns out to be
label-only. **A byte assertion on the Latin-1 case would have failed on correct
behaviour**, so the check deliberately asserts only the label there and moves
the byte assertion to the encoding where the flag is honoured.

The fix is the one suggested: compare against the **resolved** encoding rather
than asking whether any mark exists. Both checks were written *before* it, so
their failure is the evidence the defect was real, and reverting to
`has_value()` reproduces both.

**14 mutations, 12 caught, 2 impossible.**

**Self-test: 656 → 747 checks on defaults, 660 → 751 configured.**

Reproduce:

```bash
# the mode flag, its two callers, and the captions that distinguish them
grep -n "m_bReopen" src/CodePageMFCDlg.cpp src/CodePageMFCDlg.h
grep -rn "SetDlgModeReopen" src/*.cpp

# the MFC's own File menu - the layout ui-qt/ reproduces
sed -n '306,331p' src/VinaText.rc

# the inverted enable handlers, and the reason they never run
awk '/BEGIN_MESSAGE_MAP\(CEditorDoc/,/END_MESSAGE_MAP/' src/EditorDoc.cpp \
  | grep -c "ON_UPDATE_COMMAND_UI"      # 0 - so none of them is ever wired
awk '/BEGIN_MESSAGE_MAP\(CEditorDoc/,/END_MESSAGE_MAP/' src/EditorDoc.cpp \
  | grep -c "ON_COMMAND"                # 10
grep -cE "^void CEditorDoc::On[Uu]pdateFileSave" src/EditorDoc.cpp   # 5 dead handlers

# 9 versus 805, and why the compatibility module is worth using
#   QStringConverter: UTF-8/16/32, Latin-1, Locale  -> no Vietnamese codepage
#   QTextCodec:       805 names, windows-1258 among them
grep -rn "QTextCodec" thirdparty/scintilla/qt/ScintillaEditBase/*.cpp | head -3   # already a dependency

# 747 checks, up from 656
QT_QPA_PLATFORM=offscreen perl -e 'alarm 300; exec @ARGV or die "exec failed: $!"' -- \
  ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml qtbuild/fixtures/urls.md
```

---

## 6n. The window manager — and there are eight dock panes, not nine

### The brief is wrong about what this is

`OpenTabWindows` is listed in the brief's §3d among **"9 dock panes"**, and D10
keeps it as one of three. It is not a dock pane:

```
class COpenTabWindows : public CDlgBase          // src/OpenTabWindows.h:14

void CMainFrame::OnWindowManager()               // src/MainFrm.cpp:692
{
    COpenTabWindows dlg;
    dlg.DoModal();                               // <- modal dialog
}
```

**Eight** classes derive from `CDockPaneBase`, and this is not one of them:
`CBookmarkWindow`, `CBreakpointWindow`, `CBuildPane`, `CFileExplorerWindow`,
`CMessagePane`, `CPathResultWindow`, `CSearchAndReplaceWindow`,
`CSearchResultWindow`.

So the corrections are:

- **§3d's "9 dock panes" is 8.** The ninth name is a dialog.
- **D10's "3 dock panes: MessageWindow, OpenTabWindows, BookmarkWindow"** is
  really **two panes and one dialog** — and with `MessagePane` done (§6k), the
  only dock pane left in D10's kept set is `BookmarkWindow`.
- **D10's "6 of the 9 dock panes" deferred is 6 of 8.**

Same family as "~40 dialogs is 30" and "15 menu actions was 14": a plausible
number that nobody re-derived. This one cost nothing because it was checked
before the code was written — the task description said "the second dock pane"
and the framework from §6k turned out to be entirely irrelevant.

**The pattern this actually follows is the dialog one** — `CAboutDialog`,
`CPreferencesDialog`, `CEncodingDialog`. No `QDockWidget`, no `toggleViewAction`,
no `SaveDockState`.

### What it does, and the one entry point it did not have

Two columns (File Name, Full Path), four operations — Activate, Save,
Close Tab(s), Copy Full Path — double-click to activate, Ctrl+A to select all,
Ctrl+Shift+C to copy the path, and a title carrying the count.

**In the MFC it has no menu item at all.** `grep -c "MENUITEM.*ID_CURRENT_WINDOWS"`
is **0**; it is reachable only from two toolbar buttons (`src/VinaText.rc:179`,
`:241`) and the Ctrl+Shift+W accelerator (`:1374`). `ui-qt/` has no toolbar, so
it gets a View menu entry — a feature whose only route is one key combination is
one platform quirk away from not existing, which this port has now learned three
times.

Two deliberate departures, both small:

- **A modified document is marked** with the asterisk the tab bar already uses.
  The MFC offers a Save button with no way of telling whether it would do
  anything.
- **Selected rows are returned highest-first.** The MFC closes documents *by
  path*, so index invalidation cannot arise there; `ui-qt/` closes by tab index,
  where it very much can.

### Checks

One row per tab and in tab order; the title carries the count; an untitled
document reads `N/A` while a saved one shows its path (with both kinds asserted
to be present, so a hard-coded answer cannot pass); Copy Full Path copies the
path and copies **nothing** for a document that has none; Activate switches to a
row that was **not** already current; and selected rows come back descending.

Three of the checks had to be fixed before they meant anything, all found the
same way.

1. **The descending-order check selected a single row**, and a one-element list
   is sorted both ways, so reversing the comparator went straight through it.
   Three rows out of order now.
2. **The checks re-implemented the handlers instead of exercising them.** The
   Activate check connected a throwaway lambda of its own and moved the tab
   widget directly, so the handlers in `OnWindowManager` were never covered —
   the dialog is modal, so a self-test cannot reach them through `exec()`.
   Raised in review, and it is the same hole §6m found in the encoding menus.
   `ConnectWindowList` is now the single wiring point that `OnWindowManager`
   and the self-test share, and Close — previously untested altogether — is
   covered through it too.

**And two standard includes were missing.** `std::sort` and `std::greater` are
used with neither `<algorithm>` nor `<functional>` included; it compiles only
because libc++ and libstdc++ pull them in through Qt headers, and MSVC's STL is
markedly less forgiving. Worth stating precisely: **the Qt CI matrix is
`[ubuntu-latest, macos-latest]` with no Windows job**, so no CI job could ever
have caught it — it would have surfaced the first time `ui-qt/` was built with
MSVC. Raised in review.

**And Save had two problems, both raised in review.** It was the one handler
with no coverage at all — and it **switched the active tab as a side effect**,
because the first version called `OnSave`, which works on whatever is current.
`COpenTabWindows` saves the document it looked up and never touches the active
view; matching it is one `qobject_cast`. The exception is a document that has
never been saved, which needs a path: Save As is a modal prompt *about* that
document, so it is brought to the front first rather than asking the user to
name a file they cannot see.

The check covers both, on a **scratch file in a temporary directory** — never a
fixture, because a check that writes to the corpus it reads from has bitten this
port before (§6k).

**10 mutations, 10 caught.**

**Self-test: 747 → 776 checks on defaults, 751 → 780 configured.** Screenshot: `vinatext-windows.png`,
rendered separately because the dialog is modal and cannot appear in the main
one.

Reproduce:

```bash
# it is a dialog, not a dock pane
grep -n "class COpenTabWindows" src/OpenTabWindows.h
sed -n '692,696p' src/MainFrm.cpp

# and there are EIGHT dock panes, not the brief's nine
grep -rhE "^class C\w+ : public CDockPaneBase" src/*.h | sort
grep -rhE "^class C\w+ : public CDockPaneBase" src/*.h | wc -l    # 8

# no menu item in the MFC - toolbar and accelerator only
grep -c "MENUITEM.*ID_CURRENT_WINDOWS" src/VinaText.rc            # 0
grep -n "ID_CURRENT_WINDOWS" src/VinaText.rc                      # 179, 241, 1374

# 214 lines become 292 across two files
wc -l src/OpenTabWindows.cpp ui-qt/WindowListDialog.cpp ui-qt/WindowListDialog.h
```

---

## 6o. Bookmarks — a marker number is not a marker mask

The last dock pane D10 keeps, after §6n corrected the inventory. §6k's framework
carried it with nothing new required: a `QDockWidget`, an `objectName`
`saveState` keys on, a `toggleViewAction`, geometry through the existing
`SaveDockState`.

**The cheap part was the marker.** `SC_MARK_BOOKMARK` is a built-in Scintilla
shape, so no RGBA image and no icon resource — which is why this cost less than
§6j's three remaining `SCI_REGISTERRGBAIMAGE` calls implied. Margin 1 carried
width 0 and a "bookmarks/breakpoints: Phase 5" note; it now has width, a mask
admitting only the bookmark, and click-to-toggle.

### Four defects in the MFC's bookmark code, and none reproduced

Scintilla's own `Scintilla.iface` draws the line these all cross:

```
MarkerAdd(line, int markerNumber)       <- a NUMBER
MarkerDelete(line, int markerNumber)    <- a NUMBER
MarkerDeleteAll(int markerNumber)       <- a NUMBER
MarkerGet(line)                         <- returns a MASK
MarkerNext(lineStart, int markerMask)   <- a MASK
```

`src/Editor.cpp` passes the marker number to all five.

| where | what it does | consequence |
|---|---|---|
| `IsLineHasBookMark` | `nMarker == 8 \|\| nMarker == 9` | `SCI_MARKERGET` returns *every* marker on the line. A bookmark sharing a line with a **disabled** breakpoint is mask 10 and reports as **no bookmark**. Its own comment says "check mask for markerbit 0"; the bookmark is bit 3. |
| `HasBookmarks` | `SCI_MARKERNEXT(0, SC_SETMARGINTYPE_MAKER)` | that constant is **1** — the *margin's* number used where a marker mask belongs. Mask 1 is marker 0, the enabled breakpoint. The function is named for bookmarks and answers about breakpoints. |
| `FindNextBreakPoint` / `FindPreviousBreakPoint` | same constant as a mask | navigates **enabled breakpoints only**, never disabled ones and never bookmarks. |
| `OnOptionsFindNextBookmark` / `...PrevBookmark` | call `FindNextBreakPoint` / `FindPreviousBreakPoint` | **Find Next Bookmark moves between breakpoints.** Not a subtle confusion — the wrong function outright. |

**None of these is reproduced, and the reason is not taste.** `ui-qt/` has no
breakpoints — D10 defers the debugger — so transcribing them faithfully would
give: navigation that moves to nothing, a `HasBookmarks` that is always false,
and a Clear All that is permanently disabled. The feature would not work at all.
That is the line this port has drawn before: reproduce quirks that are merely
different (§6l's off-by-one centring, §6j's unsorted list), fix what is
outright broken (§6j's zero-width-match hang).

The mask defects are unobservable in `ui-qt/` *today* for the same reason — with
no breakpoints, a line's mask is only ever 0 or 8. The self-test reaches the
state anyway by setting marker 1 directly, so the check that distinguishes a
mask test from an equality **can** fail.

### The markers are the truth

`src/BookmarkWindow.cpp` keeps a parallel `std::vector<BOOKMARK_LINE_DATA>`,
appending on add and removing on delete. **Scintilla moves its markers as the
document is edited, and nothing updates those stored line numbers.** Insert a
line above a bookmark on Windows and the pane still names the old one.

The Qt pane is rebuilt from `SCI_MARKERNEXT` across every open document, so it
cannot drift. A check inserts a line and asserts all three bookmarks moved down
by one, in the markers *and* in the pane.

### Two smaller departures

- **No `PathFileExists` guard.** `CEditorView::OnOptionsAddBookmark` refuses to
  bookmark a document that is not on disk, so an unsaved buffer cannot be marked
  at all. Nothing about a marker needs a file, and the pane shows the display
  name for exactly that case.
- **Navigation wraps, both ways.** The MFC's dead-ends at the last marker.
  Checking only one direction left the other's wrap untested and a mutation
  removing `NextBookmark`'s fallback went straight through — both are asserted
  now.

### What the checks caught in their own right

- **The fold-margin click check failed**, correctly. It computed its x as
  "margin 0 width + 8", which only landed in the fold margin while margin 1 had
  **zero width**. Giving the symbol margin width moved the layout and the click
  started landing in the wrong margin. It was the only thing that noticed. The x
  is computed from the actual widths now.
- **`setChecked` does not show a dock.** Qt connects `toggleViewAction` through
  `QAction::triggered`, not `toggled`, so `setChecked` moved the tick and left
  the pane hidden — measured: checked `0 → 1`, hidden stayed `1`. §6k's
  message-pane check already used `trigger()`; this one did not, and failed
  until it did.

**10 mutations, 10 caught**, including both mask defects re-introduced as
mutations to prove the checks distinguish them.

**Self-test: 776 → 820 checks on defaults, 780 → 824 configured.** The markers
and the pane are both in the main screenshot.

Reproduce:

```bash
# what Scintilla says each call takes
grep -nE "Marker(Add|Delete|DeleteAll|Get|Next)=" thirdparty/scintilla/include/Scintilla.iface

# the four defects
sed -n '3345,3364p' src/Editor.cpp          # IsLineHasBookMark, HasBookmarks
sed -n '3216,3232p' src/Editor.cpp          # Find*BreakPoint, mask = margin number
sed -n '3606,3616p' src/EditorView.cpp     # the two BOOKMARK handlers, calling
                                           # the two BREAKPOINT functions

# the constant that is a margin number, used as a marker mask
grep -n "SC_SETMARGINTYPE_MAKER\|SC_MARKER_BOOKMARK" src/EditorCommonDef.h

# 832 lines become 153 across two files
wc -l src/BookmarkWindow.cpp ui-qt/BookmarkPane.cpp ui-qt/BookmarkPane.h

# 820 checks, up from 776
QT_QPA_PLATFORM=offscreen perl -e 'alarm 300; exec @ARGV or die "exec failed: $!"' -- \
  ./qtbuild/ui-qt/vinatext-qt --selftest \
  core/LanguageData.cpp tools/extract_language_data.py \
  qtbuild/fixtures/crlf-bom.cpp qtbuild/fixtures/utf16.py \
  qtbuild/fixtures/latin1.md qtbuild/fixtures/no-trailing-newline.py \
  qtbuild/fixtures/tags.xml qtbuild/fixtures/urls.md
```

---

## 7. How to reproduce these numbers

```bash
# CString per file (.cpp + paired .h), descending
for f in src/*.cpp; do
  h="${f%.cpp}.h"
  n=$(cat "$f" "$h" 2>/dev/null | grep -oE '\bCString\b' | wc -l | tr -d ' ')
  printf '%-6s %s\n' "$n" "$f"
done | sort -rn

# PCH dependency — note the -i, all 137 match
grep -il 'include.*stdafx\.h' src/*.cpp | wc -l

# Win32/COM API surface — separates core from platform
grep -oE '\b(HWND|HANDLE|HKEY|TCHAR|DWORD|HRESULT|CreateFile|CreateProcess|ShellExecute|CoCreateInstance|IShell\w*)\b' src/*.cpp \
  | cut -d: -f1 | sort | uniq -c | sort -rn

# MFC/ATL class surface — separates ui-rewrite from delete
grep -oE '\bCMFC\w+|\bC(Wnd|Dialog|DialogEx|View|Document|FrameWnd|ListCtrl|TreeCtrl)\b' src/*.cpp \
  | cut -d: -f1 | sort | uniq -c | sort -rn
```

Bucket assignment is a judgement call per file, driven by the class each `.cpp` derives from
(`src/*.h` base classes), its Win32/COM density, and whether Qt ships a native replacement.
It is not mechanically derivable from the counts alone — treat the table as a proposal to
review, not an oracle.

