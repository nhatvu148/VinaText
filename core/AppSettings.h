/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Reads the editor settings the Qt frontend consults, from the very file the
// MFC application writes.
//
// A READER, NOT A MIGRATION. `CAppSettings` in src/ is the most entangled type
// in the codebase - 783 call sites across 42 files (doc/PORTING.md 6c
// correction 5) - and none of them move. `src/` is untouched. This class parses
// `vinatext-app-settings.json`, which `CAppSettings::SaveSettingData` already
// produces as ordinary picojson, so the two frontends read one file and a
// setting changed in either is seen by both.
//
// Demand-driven per brief D9: only the settings `ui-qt/` actually consults are
// here. Before this, ten of them were transcribed into `ui-qt/EditorWidget.cpp`
// as literals with a comment naming the member they came from - which works
// until someone changes one.
//
// THE KEY IS NOT THE MEMBER NAME, and that is not a hypothetical:
// `m_nLongLineMaximum` is stored under `"LongLineColumnLimitation"`. Every key
// below was read out of `SaveSettingData`, not inferred from a member name -
// the seventh time in this port that the name of a thing turned out not to be
// the name of the thing it uses.
//
// The file may legitimately be absent: a fresh install has never saved, and a
// macOS or Linux user may never have run the MFC application at all. That is
// not an error, and every setting keeps its shipped default.

#pragma once

#include <string>

namespace Core
{
	class CAppSettings final
	{
	public:
		// Returns false only on a file that exists and cannot be parsed. A file
		// that is simply absent leaves every default in place and returns true,
		// because "no settings yet" is the normal state, not a failure.
		bool LoadFromFile(const std::string& strPath, std::string& strError);
		bool LoadFromString(const std::string& strJson, std::string& strError);

		// Writes back ONLY the keys this class owns, leaving every other key in
		// the file exactly as it was.
		//
		// THAT PRESERVATION IS THE WHOLE POINT. CAppSettings::SaveSettingData
		// writes 84 keys; this class understands 10. A save that serialised
		// just its own would silently discard the other 74 - compiler paths,
		// the recent-file list, the spell-check language - the first time a
		// macOS user changed a checkbox in a file their Windows install shares.
		//
		// Fails if the containing directory does not exist: creating it is a
		// platform question and core/ takes no platform dependency, so the
		// frontend does that (ui-qt/ uses QDir::mkpath) before calling.
		bool SaveToFile(const std::string& strPath, std::string& strError) const;

		// Setters, for a settings UI. Only the keys above are settable, for the
		// same reason only they are read.
		void SetEnableUrlHighlight(bool b) { m_bEnableUrlHighlight = b; }
		void SetEnableAutoComplete(bool b) { m_bEnableAutoComplete = b; }
		void SetAutoCompleteIgnoreCase(bool b) { m_bAutoCompleteIgnoreCase = b; }
		void SetAutoCompleteIgnoreNumbers(bool b) { m_bAutoCompleteIgnoreNumbers = b; }
		void SetDrawCaretLineFrame(bool b) { m_bDrawCaretLineFrame = b; }
		void SetDrawFoldingLineUnderLineStyle(bool b) { m_bDrawFoldingLineUnderLineStyle = b; }
		void SetEnableHighlightFolder(bool b) { m_bEnableHightLightFolder = b; }
		void SetUseFolderMarginClassic(bool b) { m_bUseFolderMarginClassic = b; }
		void SetFolderMarginStyle(int n) { m_nFolderMarginStyle = n; }
		void SetLongLineColumnLimit(int n) { m_nLongLineMaximum = n; }

		// The eight the editor now honours. Every KEY below was read out of
		// CAppSettings::SaveSettingData rather than guessed from the member
		// name, and three of them differ - "EditorTabWidth" for
		// m_nEditorIndentationWidth, "UseCustomEditorTabSettings" for
		// m_bUseUserIndentationSettings, and "EditorFontIsStalic" for italic,
		// which is a typo in the original. Guessing any of them would write a
		// key the WINDOWS build does not read, in a file both frontends share.
		void SetEditorFontName(const std::string& str) { m_strEditorFontName = str; }
		void SetEditorFontPointSize(int n) { m_nEditorFontPointSize = n; }
		void SetEditorTabWidth(int n) { m_nEditorTabWidth = n; }
		void SetUseCustomTabSettings(bool b) { m_bUseCustomTabSettings = b; }
		void SetProcessIndentationTab(bool b) { m_bProcessIndentationTab = b; }
		void SetEditorZoomFactor(int n) { m_nEditorZoomFactor = n; }
		void SetEnableCaretBlink(bool b) { m_bEnableCaretBlink = b; }
		void SetEnableMultipleCursor(bool b) { m_bEnableMultipleCursor = b; }
		void SetDefaultFileEol(int n) { m_nDefaultFileEol = n; }
		void SetAutoAddNewLineAtEof(bool b) { m_bAutoAddNewLineAtEof = b; }

		// True once a settings file has actually been read, so a frontend can
		// tell "the user chose the defaults" from "there was nothing to read".
		bool WasLoaded() const { return m_bLoaded; }

		// The slice ui-qt/ consults. Defaults are the shipped ones from
		// src/AppSettings.h; core/tests/TestAppSettings.cpp reads that header
		// and fails if any of them drift apart.
		bool EnableUrlHighlight() const { return m_bEnableUrlHighlight; }
		bool EnableAutoComplete() const { return m_bEnableAutoComplete; }
		bool AutoCompleteIgnoreCase() const { return m_bAutoCompleteIgnoreCase; }
		bool AutoCompleteIgnoreNumbers() const { return m_bAutoCompleteIgnoreNumbers; }
		bool DrawCaretLineFrame() const { return m_bDrawCaretLineFrame; }
		bool DrawFoldingLineUnderLineStyle() const { return m_bDrawFoldingLineUnderLineStyle; }
		bool EnableHighlightFolder() const { return m_bEnableHightLightFolder; }
		bool UseFolderMarginClassic() const { return m_bUseFolderMarginClassic; }
		// FOLDER_MARGIN_STYPE: 0 arrow, 1 plus/minus, 2 tree circle, 3 tree box.
		int FolderMarginStyle() const { return m_nFolderMarginStyle; }
		int LongLineColumnLimit() const { return m_nLongLineMaximum; }

		const std::string& EditorFontName() const { return m_strEditorFontName; }
		int EditorFontPointSize() const { return m_nEditorFontPointSize; }
		int EditorTabWidth() const { return m_nEditorTabWidth; }
		bool UseCustomTabSettings() const { return m_bUseCustomTabSettings; }
		bool ProcessIndentationTab() const { return m_bProcessIndentationTab; }
		int EditorZoomFactor() const { return m_nEditorZoomFactor; }
		bool EnableCaretBlink() const { return m_bEnableCaretBlink; }
		bool EnableMultipleCursor() const { return m_bEnableMultipleCursor; }
		// 0 = CRLF, 1 = CR, 2 = LF, matching Scintilla's SC_EOL_* - which
		// core/ cannot include, so the numbers are spelled out here and the
		// frontend maps them.
		int DefaultFileEol() const { return m_nDefaultFileEol; }
		bool AutoAddNewLineAtEof() const { return m_bAutoAddNewLineAtEof; }

		// The object every setting lives under in that file - JSonWriter nests
		// everything below its root name (src/FileUtil.cpp, JSonWriter::SaveFile).
		static const char* RootName() { return "VinaText Setting"; }
		// The file name, without a directory: where AppData lives is a platform
		// question and core/ takes no platform dependency. ui-qt/ resolves the
		// directory with QStandardPaths and passes a full path in.
		static const char* FileName() { return "vinatext-app-settings.json"; }

	private:
		bool	m_bLoaded = false;

		// Shipped defaults, from src/AppSettings.h. Kept in the same order as
		// the header so the two are easy to compare by eye as well as by test.
		bool	m_bEnableUrlHighlight = true;			// :69
		bool	m_bDrawFoldingLineUnderLineStyle = false;	// :74
		bool	m_bDrawCaretLineFrame = true;			// :76
		bool	m_bEnableHightLightFolder = true;		// :77  (sic - "Hight")
		bool	m_bEnableAutoComplete = true;			// :81

		// Defaults taken from src/AppSettings.h, not chosen here - a default
		// that disagrees with the Windows one is a silent behaviour change for
		// anyone whose settings file predates this.
		std::string	m_strEditorFontName = "Courier New";	// :80
		int		m_nEditorFontPointSize = 12;			// :81
		int		m_nEditorTabWidth = 4;					// :173, SC_DEFAUFT_TAB_WIDTH
		bool	m_bUseCustomTabSettings = false;		// :112
		bool	m_bProcessIndentationTab = true;		// :82
		int		m_nEditorZoomFactor = 0;				// :162
		bool	m_bEnableCaretBlink = false;			// :96
		bool	m_bEnableMultipleCursor = true;			// :97  (ui-qt had this OFF)
		int		m_nDefaultFileEol = 0;					// :150, SC_EOL_CRLF
		bool	m_bAutoAddNewLineAtEof = false;			// :110
		bool	m_bAutoCompleteIgnoreNumbers = true;		// :83
		bool	m_bAutoCompleteIgnoreCase = true;		// :84
		bool	m_bUseFolderMarginClassic = false;		// :95
		int		m_nFolderMarginStyle = 3;			// :115 STYLE_TREE_BOX
		int		m_nLongLineMaximum = 80;			// :154
	};
}
