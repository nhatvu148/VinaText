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
		bool	m_bAutoCompleteIgnoreNumbers = true;		// :83
		bool	m_bAutoCompleteIgnoreCase = true;		// :84
		bool	m_bUseFolderMarginClassic = false;		// :95
		int		m_nFolderMarginStyle = 3;			// :115 STYLE_TREE_BOX
		int		m_nLongLineMaximum = 80;			// :154
	};
}
