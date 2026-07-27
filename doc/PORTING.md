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
> **125 `.cpp` / 3,165 `CString`**.

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
| `EditorDatabase.cpp` | 92 | 16 +24 | 0 | 0 | 0 | Open-document registry. No Win32; most sites are in the header. |
| `FileUtil.cpp` | 406 | 19 +14 | 7 | 0 | 41 | Already heavily std::-based (41 std:: lines). |
| `DiffEngine.cpp` | 324 | 20 +12 | 6 | 0 | 0 | Diff algorithm. No Win32. |
| `FindPathWorker.cpp` | 241 | 16 +10 | 8 | 0 | 6 | Path search worker. Threading moves to platform/. |
| `AppSettings.cpp` | 414 | 8 +17 | 6 | 0 | 0 | Config already file-based (issue #44). Maps onto QSettings. |
| `LexerParser.cpp` | 297 | 13 +9 | 14 | 0 | 0 | Language/keyword parsing. Feeds Scintilla lexers. |
| `UserExtension.cpp` | 170 | 12 +6 | 2 | 2 | 0 | User-defined tool commands. Decouple from EditorView/EditorDoc first. |
| `TemplateCreator.cpp` | 145 | 7 +2 | 2 | 1 | 0 | File-template generation. |
| `RecentCloseFileManager.cpp` | 61 | 4 +4 | 2 | 0 | 1 | MRU list. |
| `UserCustomizeData.cpp` | 48 | 5 +2 | 0 | 0 | 5 | std::fstream config reader. |
| `EditorLexerDark.cpp` | 995 | 4 +1 | 0 | 0 | 0 | Scintilla lexer/theme table. Phase 4; SCI_ API is identical on Qt. |
| `EditorLexerLight.cpp` | 995 | 4 +1 | 0 | 0 | 0 | Scintilla lexer/theme table. Phase 4; SCI_ API is identical on Qt. |
| `LocalizationDatabase.cpp` | 183 | 2 +2 | 16 | 0 | 1 | String table. |
| `RAIIUtils.cpp` | 62 | 1 +2 | 0 | 0 | 0 | Split: CCriticalSectionLock/CMemoryGuard/CBenchmarkTest are core; CLockCtrlRedraw/CLockCtrlUpdate/CMultipleSelectionKeeper are editor-widget guards -> ui-qt/. |
| `LocalizationHandler.cpp` | 85 | 2 | 3 | 3 | 1 | Loads localization files; port to QTranslator or keep as-is. |
| `SpellChecker.cpp` | 279 | 1 +1 | 11 | 1 | 15 | Dictionary lookup; strip the Editor.h/UI coupling on the way out. |
| `TemporarySettings.cpp` | 10 | 0 +2 | 0 | 0 | 0 | 9 lines, trivial. |
| `Textfile.cpp` | 1,551 | 1 +1 | 130 | 0 | 0 | Encoding detection (uchardet) + file I/O. Split: swap Win32 CreateFile/ReadFile for QFile, keep the codec logic. |
| `WebHandler.cpp` | 191 | 2 | 0 | 0 | 19 | curl-based HTTP. 19 std:: lines, zero Win32. |
| `ComboboxRegexHelper.cpp` | 156 | 0 | 0 | 3 | 0 | **MISCLASSIFIED — belongs in `ui-rewrite`.** Its API is `PopulateRegexFields(CComboBox&)`; it is the combobox binding, not a data table. Zero `CString` hid pure MFC UI. |
| `FixedBlockMemory.cpp` | 58 | 0 | 0 | 0 | 0 | Custom allocator. No Win32, no PCH. |
| `Observer.cpp` | 26 | 0 | 2 | 0 | 0 | Observer pattern base. |
| `StringHelper.cpp` | 477 | 0 | 13 | 0 | 28 | Zero CString, 28 std:: lines. Pure string ops. |
| `Subject.cpp` | 58 | 0 | 2 | 0 | 1 | Observer pattern base. |
| `TextFormatConverter.cpp` | 563 | 0 | 0 | 0 | 86 | Zero CString, 86 std:: lines. Uses boost::algorithm. |
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

**So Wave 1 is not a wave.** The real cheapest start, in order:

1. **`StringHelper.cpp`** (477 LOC) — parameterise the single `AppSettingMgr` lookup so the
   caller passes the page width, then move. This unblocks the next item.
2. **`TextFormatConverter.cpp`** (563 LOC) — follows `StringHelper`; also needs
   `AppUtils::SplitterStdString`, a generic helper that belongs in `core/` anyway and is part
   of the `AppUtil` split already listed in §5.

Everything else nominally in `core/` carries `CString` and belongs in Wave 2 or 3.

**Lesson for the remaining buckets.** Before moving any file, check three things the counts do
not capture: is it in `src/VinaText.vcxproj` at all; what does its `#include` list actually
drag in; and do its public signatures mention MFC types. All three failures above would have
been caught by that check, and it costs about a minute per file.

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
| `LexerParser.cpp` | 22 | 297 | Language/keyword parsing. Feeds Scintilla lexers. |
| `AppSettings.cpp` | 25 | 414 | Config already file-based (issue #44). Maps onto QSettings. |
| `FindPathWorker.cpp` | 26 | 241 | Path search worker. Threading moves to platform/. |

### Wave 3 — heavy, do last

| File | CS | LOC | Note |
|---|---:|---:|---|
| `DiffEngine.cpp` | 32 | 324 | Diff algorithm. No Win32. |
| `FileUtil.cpp` | 33 | 406 | Already heavily std::-based (41 std:: lines). |
| `EditorDatabase.cpp` | 40 | 92 | Open-document registry. No Win32; most sites are in the header. |
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
`license/` (which tracks Boost, Curl, Mozilla, PDFium, Scintilla, VinaText). It is 215 lines
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

