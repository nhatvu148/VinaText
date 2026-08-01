/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Loads the editor language and theme tables from Packages/data-packages/*.json.
//
// This is core/ code (see doc/QT-PORT-BRIEF.md D5): UI-free and portable, with no
// MFC, no Win32 and no Qt dependency. The intent is that it links into both
// frontends.
//
// Both frontends read this now: src/EditorLanguageData.cpp for the MFC build's
// metadata and keywords, and ui-qt/EditorWidget.cpp for everything. The colour
// tables in src/EditorColor{Light,Dark}.h are still the live source of truth for
// the MFC build's colours, so tools/extract_language_data.py --verify keeps the
// JSON and the C++ in step. core/tests/ exercises this code on Linux and macOS,
// where the MFC application cannot be built at all.
//
// Deliberately std::string rather than CString or QString - the payload is ASCII
// lexer configuration, so there is nothing to gain from UTF-16 here, and staying
// on std::string keeps the header free of both toolkits.

#pragma once

#include <map>
#include <string>
#include <vector>

namespace Core
{
	// One entry of a theme's per-language style table: a Scintilla style constant
	// mapped to a named colour drawn from the theme palette.
	struct SStyleMapping
	{
		std::string	_Style;		// symbolic name, e.g. "SCE_P_COMMENTLINE"
		int			_Value = 0;	// resolved numeric value from SciLexer.h
		std::string	_Color;		// palette key, e.g. "comment"
	};

	// Bold/italic/underline for one Scintilla style. Deliberately NOT part of a
	// theme: the MFC lexer initialisers apply the same attributes in the light and
	// the dark build, so this belongs to the language.
	struct SStyleAttribute
	{
		std::string	_Style;				// symbolic name, e.g. "SCE_P_WORD"
		int			_Value = 0;			// resolved numeric value from SciLexer.h
		bool		_Bold = false;
		bool		_Italic = false;
		bool		_Underline = false;
	};

	struct SLanguageInfo
	{
		std::string	_Id;
		std::string	_Name;
		// A display label, NOT the file-extension mapping - it says "r" for autoit
		// and "javascript" for javascript. Match on _Extensions instead.
		std::string	_Extension;
		// Every file extension that selects this language, '|'-separated and
		// without dots: "cpp|cxx|h|hh|hpp|hxx|cc".
		std::string	_Extensions;
		// The Lexilla lexer name to pass to CreateLexer. Frequently NOT _Id: the
		// shipping app lexes .java, .js, .cs and .json with the "cpp" lexer.
		std::string	_LexerName;
		// Indentation-guide mode: "lookforward" or "lookboth". Python gets the
		// first - guides that stop at a blank line suit a language with no closing
		// brace. flexlicense is lexed by python and does NOT: the rule keys on the
		// VinaText token, not the lexer.
		std::string	_IndentGuides;
		// What a collapsed fold shows: " { ... } ", " < ... > " or " --- ". Keyed
		// by language, NOT by lexer: go, protobuf, autoit, resource and vcxproject
		// are all lexed as cpp and all fold with " --- ".
		std::string	_FoldMarker;
		// Which theme style table colours this language. Usually _Id, but xml is
		// coloured from html's table - Scintilla's xml lexer emits the SCE_H_*
		// family, so xml's own SCE_C_* table is never applied to anything.
		std::string	_StyleTable;
		// Whether the caret's enclosing XML/HTML tag pair is highlighted. True for
		// exactly html, php and xml, and keyed by NONE of the fields above:
		// CEditorView gates it on the VINATEXT_SUPPORTED_LANGUAGE enum. php is
		// lexed as "cpp", so keying on _LexerName would drag in twelve languages;
		// keying on _StyleTable or _FoldMarker gives {html, xml} and silently
		// drops php. See doc/PORTING.md 6h.
		bool		_TagMatch = false;
		std::string	_CommentLine;
		std::string	_CommentStart;
		std::string	_CommentEnd;
		std::string	_Keywords;
		// Only the styles that carry an attribute; most languages have none.
		std::vector<SStyleAttribute> _StyleAttributes;
	};

	struct SColor
	{
		int _Red = 0;
		int _Green = 0;
		int _Blue = 0;
	};

	// Language metadata shared by every theme (Packages/data-packages/languages.json).
	class CLanguageTable final
	{
	public:
		bool LoadFromFile(const std::string& strPath, std::string& strError);
		bool LoadFromString(const std::string& strJson, std::string& strError);

		const std::vector<SLanguageInfo>& GetLanguages() const { return m_Languages; }
		// Returns nullptr when the language is unknown.
		const SLanguageInfo* FindById(const std::string& strId) const;
		const std::string& GetPlainTextLabel() const { return m_PlainTextLabel; }

		// Which language a file extension selects - "cpp", not ".cpp", not "a.cpp".
		// Case-insensitive over ASCII. Returns nullptr for plain text, which is not
		// a language: it has no metadata, no keywords and no lexer.
		//
		// First match wins, in file order, exactly as CEditorCtrl::
		// GetLexerNameFromExtension walks arrLangExtensions.
		const SLanguageInfo* FindByExtension(const std::string& strExtension) const;

		// Which language a FILE NAME selects - "main.cpp" or "Makefile", never a
		// path. Path splitting stays in the frontend: QFileInfo::fileName() on the
		// Qt side, PathUtils::GetFilenameFromPath on the MFC side. core/ does not
		// own path syntax yet (PathUtil is the last Phase 2 item), and nothing here
		// needs it to.
		//
		// Transcribes CEditorCtrl::DetectFileLexer, quirks included: two file names
		// beat any extension, and the two comparisons do not agree about case.
		const SLanguageInfo* DetectForFileName(const std::string& strFileName) const;

		// The extension of a file name: everything after its last '.', or empty.
		// "main.tar.gz" -> "gz"; "Makefile" -> ""; ".gitignore" -> "gitignore".
		static std::string ExtensionOf(const std::string& strFileName);

	private:
		std::vector<SLanguageInfo>	m_Languages;
		std::string					m_PlainTextLabel;
	};

	// One theme: a named colour palette plus per-language style tables
	// (Packages/data-packages/theme-light.json, theme-dark.json).
	class CEditorTheme final
	{
	public:
		bool LoadFromFile(const std::string& strPath, std::string& strError);
		bool LoadFromString(const std::string& strJson, std::string& strError);

		const std::string& GetName() const { return m_Name; }
		const std::map<std::string, SColor>& GetPalette() const { return m_Palette; }
		// Role name -> palette key, from CEditorCtrl::InitilizeSetting's
		// IS_LIGHT_THEME preset. See ResolveRole.
		const std::map<std::string, std::string>& GetRoles() const { return m_Roles; }

		// Returns nullptr when the theme has no table for that language.
		const std::vector<SStyleMapping>* FindStyles(const std::string& strLanguageId) const;
		// Resolves a palette key. Returns false when the key is not defined.
		bool ResolveColor(const std::string& strKey, SColor& color) const;

		// Resolves one of the editor's colour ROLES - the indirection the MFC
		// frontend performs when it fills m_AppThemeColorSet. Prefer this to
		// ResolveColor for anything CEditorCtrl reads out of that struct, because
		// a role's palette key is not reliably its own name:
		//
		//   lineNumberColor    -> "linenumber"       (both themes)
		//   selectionTextColor -> "black" on light, "white" on dark
		//
		// The second cannot be expressed as a single palette key at all, which is
		// why resolving these by name happens to work for eight roles and quietly
		// does not for those two.
		//
		// NAMES ARE THE C++ MEMBER NAMES, including one that lies:
		// `selectionTextColor` is passed to SCI_SETSELBACK, so it is the selection
		// BACKGROUND. It is kept verbatim so every value traces back to an
		// identifier that can be grepped for in src/.
		//
		// Returns false when the role is unknown or names an undefined key.
		bool ResolveRole(const std::string& strRole, SColor& color) const;

	private:
		std::string										m_Name;
		std::map<std::string, SColor>					m_Palette;
		std::map<std::string, std::string>				m_Roles;
		std::map<std::string, std::vector<SStyleMapping>> m_Styles;
	};

	// Reads a whole file into memory. Returns false and sets strError on failure.
	bool ReadFileToString(const std::string& strPath, std::string& strOut, std::string& strError);
}
