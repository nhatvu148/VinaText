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

