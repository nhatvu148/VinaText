/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// MFC-side bridge to core/LanguageData.
//
// The editor's per-language metadata (display name, extension, comment
// delimiters) and keyword blobs used to live as static CString/const char*
// tables duplicated across EditorColorLight.h and EditorColorDark.h. They now
// come from Packages/data-packages/languages.json via core/CLanguageTable.
//
// This header exists so EditorLexerLight.cpp / EditorLexerDark.cpp can keep
// their existing shape - it converts std::string to CString and hides the
// one-time load. core/ itself stays free of MFC.
//
// The per-theme colour tables (g_rgb_Syntax_*) are deliberately NOT covered
// here; they remain in the two colour headers because the style loops around
// them carry per-language bold/italic logic, not just data.

#pragma once

class CLanguageDatabase;

namespace EditorLanguageData
{
	// Loads languages.json on first use. Safe to call before the main frame
	// exists. Returns false if the data file could not be read or parsed, in
	// which case the accessors below return empty values.
	bool EnsureLoaded();

	// Applies name / extension / comment delimiters for szLanguageId. No-op if
	// the language is unknown. szLanguageId matches the "id" field in
	// languages.json - historically the g_str_<id>_* variable suffix.
	void ApplyLanguageMetadata(CLanguageDatabase* pDatabase, const char* szLanguageId);

	// Keyword blob for szLanguageId, or "" when the language is unknown or
	// declares no keywords. Never returns NULL: several languages (markdown,
	// xml) legitimately have an empty blob and the editor expects "" there,
	// exactly as the old g_<id>_KeyWords tables provided.
	//
	// The returned pointer is owned by the loaded table and stays valid for the
	// lifetime of the process.
	const char* GetKeywords(const char* szLanguageId);
}
