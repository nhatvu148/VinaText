/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Differential test: does core/ pick the same lexer the MFC app picks?
//
// ui-qt/ chooses a lexer from languages.json via Core::CLanguageTable; ui-mfc/
// chooses one by walking EditorLanguageDef::arrLangExtensions and dispatching
// through EditorLexer{Light,Dark}.cpp's LoadLexer. Two frontends highlighting the
// same file differently is a silent, plausible-looking bug, so the two paths are
// run against each other here rather than reasoned about.
//
// The ORIGINAL side below is a transcription of three functions in src/, with
// their own verbatim copy of the C++ tables:
//
//   CEditorCtrl::DetectFileLexer              src/Editor.cpp:2515
//   CEditorCtrl::GetLexerNameFromExtension    src/Editor.cpp:3054
//   EditorLexerDark::LoadLexer                src/EditorLexerDark.cpp:10
//
// It reads NO JSON. That is the point: if the extraction into languages.json were
// wrong, core/ and this file would disagree. tools/extract_language_data.py
// --verify checks the same data the other way round, against the C++ itself.
//
// Two things are deliberately not transcribed, and are out of scope for both
// sides:
//   - the user override in CUserCustomizeData::GetSyntaxHighlightUserData().
//     When Packages/data-packages/syntax-highlight-file-extension.dat is
//     non-empty it replaces the built-in table wholesale. ui-qt/ does not read
//     it yet; when it does, this test grows a second corpus.
//   - CString::CompareNoCase follows the C runtime locale (CommandLine.cpp calls
//     _tsetlocale(LC_ALL, "")). The corpus below is ASCII, where every plausible
//     implementation agrees; see the note on EqualsNoCaseAscii in LanguageData.cpp.
//
// Build and run (one line):
//   c++ -std=c++11 -I include -I core core/LanguageData.cpp core/Tokenizer.cpp core/tests/TestLanguageLookup.cpp -o testlanglookup && ./testlanglookup Packages/data-packages

#include "LanguageData.h"
#include "Tokenizer.h"

#include <cwctype>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	int g_Failures = 0;
	int g_Checks = 0;

	void Check(bool bCondition, const std::string& strWhat)
	{
		++g_Checks;
		if (!bCondition)
		{
			++g_Failures;
			std::cout << "  FAIL: " << strWhat << "\n";
		}
	}

	//----------------------------------------------------------------------
	// The original, transcribed
	//----------------------------------------------------------------------

	// src/EditorCommonDef.h, EditorLanguageDef::arrLangExtensions - verbatim,
	// in order. The trailing 0 terminator is the loop's stop condition there and
	// is the end of this array here.
	const wchar_t* const ORIGINAL_EXTENSIONS[] =
	{
		L"py|pyw",
		L"cpp|cxx|h|hh|hpp|hxx|cc",
		L"ada|ads|adb",
		L"asm",
		L"iss",
		L"sh",
		L"bat|cmd|nt",
		L"c",
		L"cmake",
		L"cs",
		L"css",
		L"erl|hrl",
		L"f|for|f90|f95|f77",
		L"htm|html|shtml|htt|cfm|tpl|hta",
		L"java",
		L"js|jsx",
		L"ts|tsx",
		L"lua",
		L"m",
		L"pas|inc|pp",
		L"pl|pm|cgi|pod",
		L"php|php3|php4|php5|phps|phpt|phtml",
		L"ps1|psm1",
		L"rb",
		L"rs",
		L"sql|spec|body|sps|spb|sf|sp",
		L"tcl",
		L"vb|vbs|frm|cls|ctl|pag|dsr|dob",
		L"v|sv|vh|svh",
		L"vhd|vhdl",
		L"xml|gcl|xsl|svg|xul|xsd|dtd|xslt|axl",
		L"json",
		L"go",
		L"md|markdown|rmd",
		L"proto",
		L"r",
		L"lic",
		L"rc",
		L"au3",
		L"bas",
		L"vcxproj|filters|user|vcproj|csproj|csxproj|vbproj|dbproj|sln",
	};

	// src/EditorCommonDef.h, EditorLanguageDef::arrLexerNames - verbatim, in
	// order. One entry longer than the array above: "plaintext" is never reached
	// through it, because the loop stops at arrLangExtensions' terminator.
	const char* const ORIGINAL_LEXER_TOKENS[] =
	{
		"python", "cpp", "ada", "asm", "inno", "bash", "batch", "c", "cmake", "cs",
		"css", "erlang", "fortran", "hypertext", "java", "javascript", "typescript",
		"lua", "matlab", "pascal", "perl", "phpscript", "powershell", "ruby", "rust",
		"sql", "tcl", "vb", "verilog", "vhdl", "xml", "kix", "golang", "markdown",
		"protobuf", "r", "FLEXlm", "Resource", "autoit", "freebasic", "vcxproj",
		"plaintext",
	};

	// EditorLexerDark::LoadLexer's if-chain, plus the two string literals each
	// Init_<x>_Editor function names: SetLexer("...") and
	// ApplyLanguageMetadata(pDatabase, "..."). EditorLexerLight.cpp is identical -
	// the extractor asserts that, and refuses to run if the two ever drift.
	struct SDispatchRow
	{
		const char* _Token;			// what GetLexerNameFromExtension returns
		const char* _Lexer;			// SetLexer(...)         - the Lexilla lexer
		const char* _LanguageId;	// ApplyLanguageMetadata - the languages.json id
	};

	const SDispatchRow ORIGINAL_DISPATCH[] =
	{
		{ "ada",		"ada",			"ada" },
		{ "asm",		"asm",			"asm" },
		{ "inno",		"asm",			"inno" },
		{ "bash",		"bash",			"bash" },
		{ "batch",		"batch",		"batch" },
		{ "cmake",		"cmake",		"cmake" },
		{ "makefile",	"makefile",		"makefile" },
		{ "cpp",		"cpp",			"cpp" },
		{ "c",			"cpp",			"c" },
		{ "css",		"css",			"css" },
		{ "erlang",		"erlang",		"erlang" },
		{ "fortran",	"fortran",		"fortran" },
		{ "hypertext",	"hypertext",	"html" },
		{ "lua",		"lua",			"lua" },
		{ "matlab",		"matlab",		"matlab" },
		{ "pascal",		"pascal",		"pascal" },
		{ "perl",		"perl",			"perl" },
		{ "phpscript",	"cpp",			"php" },
		{ "powershell",	"powershell",	"powershell" },
		{ "python",		"python",		"python" },
		{ "ruby",		"ruby",			"ruby" },
		{ "rust",		"rust",			"rust" },
		{ "golang",		"cpp",			"go" },
		{ "sql",		"sql",			"sql" },
		{ "tcl",		"tcl",			"tcl" },
		{ "vb",			"vb",			"vb" },
		{ "verilog",	"verilog",		"verilog" },
		{ "vhdl",		"vhdl",			"vhdl" },
		{ "xml",		"xml",			"xml" },
		{ "kix",		"cpp",			"json" },
		{ "java",		"cpp",			"java" },
		{ "javascript",	"cpp",			"javascript" },
		{ "typescript",	"cpp",			"typescript" },
		{ "cs",			"cpp",			"cs" },
		{ "markdown",	"markdown",		"markdown" },
		{ "protobuf",	"cpp",			"protobuf" },
		{ "r",			"r",			"r" },
		{ "FLEXlm",		"python",		"flexlicense" },
		{ "Resource",	"cpp",			"resource" },
		{ "autoit",		"cpp",			"autoit" },
		{ "freebasic",	"freebasic",	"freebasic" },
		{ "vcxproj",	"cpp",			"vcxproject" },
	};

	// What the editor ends up in when nothing matches: LoadLexer's final else
	// calls Init_text_Editor, which is SetLexer("plaintext") and no metadata.
	const char* const PLAIN_TEXT_LEXER = "plaintext";
	const char* const NO_LANGUAGE = "";

	// CString::CompareNoCase(...) == 0, implemented per-character with towlower
	// rather than with core/'s ASCII table, so the two sides of this test do not
	// share a case-folding implementation.
	bool OriginalCompareNoCaseEqual(const std::wstring& strLeft, const std::wstring& strRight)
	{
		if (strLeft.size() != strRight.size())
		{
			return false;
		}
		for (std::wstring::size_type i = 0; i < strLeft.size(); ++i)
		{
			if (std::towlower(static_cast<std::wint_t>(strLeft[i]))
				!= std::towlower(static_cast<std::wint_t>(strRight[i])))
			{
				return false;
			}
		}
		return true;
	}

	// CEditorCtrl::GetLexerNameFromExtension, src/Editor.cpp:3054, built-in branch.
	std::string OriginalGetLexerNameFromExtension(const std::wstring& strExtension)
	{
		const size_t nRows = sizeof(ORIGINAL_EXTENSIONS) / sizeof(ORIGINAL_EXTENSIONS[0]);
		for (size_t lexerIndex = 0; lexerIndex < nRows; ++lexerIndex)
		{
			Core::CTokenizer parser(ORIGINAL_EXTENSIONS[lexerIndex], L"|");
			while (parser.HasMoreTokens())
			{
				if (OriginalCompareNoCaseEqual(parser.Next(), strExtension))
				{
					return ORIGINAL_LEXER_TOKENS[lexerIndex];
				}
			}
		}
		return "plaintext";			// LEXER_PLAIN_TEXT
	}

	// PathUtils::GetFileExtention, src/PathUtil.cpp:418 - _tsplitpath_s takes the
	// last '.' of the final path component, and .Mid(1) drops the period. The
	// corpus feeds file names, never paths, so the directory split does not arise.
	std::wstring OriginalGetFileExtention(const std::wstring& strFileName)
	{
		const std::wstring::size_type nDot = strFileName.rfind(L'.');
		if (nDot == std::wstring::npos)
		{
			return std::wstring();
		}
		return strFileName.substr(nDot + 1);
	}

	// CEditorCtrl::DetectFileLexer, src/Editor.cpp:2515, followed by LoadLexer's
	// dispatch. Returns the pair the editor actually acts on.
	void OriginalDetect(const std::wstring& strFileName,
		std::string& strLexerOut, std::string& strLanguageIdOut)
	{
		std::string strToken;
		if (strFileName == L"CMakeLists.txt")
		{
			strToken = "cmake";
		}
		else if (OriginalCompareNoCaseEqual(strFileName, L"Makefile"))
		{
			strToken = "makefile";
		}
		else
		{
			strToken = OriginalGetLexerNameFromExtension(OriginalGetFileExtention(strFileName));
		}

		const size_t nRows = sizeof(ORIGINAL_DISPATCH) / sizeof(ORIGINAL_DISPATCH[0]);
		for (size_t i = 0; i < nRows; ++i)
		{
			if (strToken == ORIGINAL_DISPATCH[i]._Token)
			{
				strLexerOut = ORIGINAL_DISPATCH[i]._Lexer;
				strLanguageIdOut = ORIGINAL_DISPATCH[i]._LanguageId;
				return;
			}
		}
		strLexerOut = PLAIN_TEXT_LEXER;
		strLanguageIdOut = NO_LANGUAGE;
	}

	//----------------------------------------------------------------------
	// The corpus
	//----------------------------------------------------------------------

	std::wstring Widen(const std::string& s)
	{
		return std::wstring(s.begin(), s.end());		// ASCII corpus only
	}

	std::string Narrow(const std::wstring& s)
	{
		return std::string(s.begin(), s.end());
	}

	// Every case permutation of a short ASCII string, capped so a long token does
	// not explode: beyond 6 characters only all-lower, all-upper and Title are
	// produced, which is where the interesting disagreements would be anyway.
	std::vector<std::string> CasePermutations(const std::string& str)
	{
		std::vector<std::string> out;
		if (str.size() <= 6)
		{
			const size_t nCombinations = static_cast<size_t>(1) << str.size();
			for (size_t mask = 0; mask < nCombinations; ++mask)
			{
				std::string candidate = str;
				for (size_t i = 0; i < str.size(); ++i)
				{
					if ((mask >> i) & 1)
					{
						if (candidate[i] >= 'a' && candidate[i] <= 'z')
						{
							candidate[i] = static_cast<char>(candidate[i] - 'a' + 'A');
						}
					}
				}
				out.push_back(candidate);
			}
			return out;
		}
		std::string lower = str, upper = str, title = str;
		for (size_t i = 0; i < str.size(); ++i)
		{
			if (upper[i] >= 'a' && upper[i] <= 'z')
			{
				upper[i] = static_cast<char>(upper[i] - 'a' + 'A');
			}
		}
		if (!title.empty() && title[0] >= 'a' && title[0] <= 'z')
		{
			title[0] = static_cast<char>(title[0] - 'a' + 'A');
		}
		out.push_back(lower);
		out.push_back(upper);
		out.push_back(title);
		return out;
	}

	void CollectExtensionTokens(std::vector<std::string>& out)
	{
		const size_t nRows = sizeof(ORIGINAL_EXTENSIONS) / sizeof(ORIGINAL_EXTENSIONS[0]);
		for (size_t i = 0; i < nRows; ++i)
		{
			const std::wstring strRow = ORIGINAL_EXTENSIONS[i];
			std::wstring::size_type nStart = 0;
			while (nStart <= strRow.size())
			{
				std::wstring::size_type nEnd = strRow.find(L'|', nStart);
				if (nEnd == std::wstring::npos)
				{
					nEnd = strRow.size();
				}
				if (nEnd > nStart)
				{
					out.push_back(Narrow(strRow.substr(nStart, nEnd - nStart)));
				}
				nStart = nEnd + 1;
			}
		}
	}

	// Every string of length <= nMaxLength over the alphabet. Small alphabet, but
	// chosen so that it generates dots, pipes, digits, both cases and the initial
	// letters of real extensions - which is where the parsing edge cases live.
	void GenerateExhaustive(const std::string& strAlphabet, size_t nMaxLength,
		std::vector<std::string>& out)
	{
		std::vector<std::string> current(1, std::string());
		out.push_back(std::string());
		for (size_t length = 1; length <= nMaxLength; ++length)
		{
			std::vector<std::string> next;
			for (size_t i = 0; i < current.size(); ++i)
			{
				for (size_t c = 0; c < strAlphabet.size(); ++c)
				{
					next.push_back(current[i] + strAlphabet[c]);
				}
			}
			out.insert(out.end(), next.begin(), next.end());
			current.swap(next);
		}
	}
}

int main(int argc, char** argv)
{
	const std::string strDataDir = (argc > 1) ? argv[1] : "Packages/data-packages";

	Core::CLanguageTable languages;
	std::string strError;
	if (!languages.LoadFromFile(strDataDir + "/languages.json", strError))
	{
		std::cout << "FATAL: " << strError << "\n";
		return 1;
	}

	//----------------------------------------------------------------------
	// The transcription must itself be internally consistent, or the whole
	// comparison is against a straw man.
	//----------------------------------------------------------------------
	const size_t nExtensionRows = sizeof(ORIGINAL_EXTENSIONS) / sizeof(ORIGINAL_EXTENSIONS[0]);
	const size_t nTokens = sizeof(ORIGINAL_LEXER_TOKENS) / sizeof(ORIGINAL_LEXER_TOKENS[0]);
	const size_t nDispatchRows = sizeof(ORIGINAL_DISPATCH) / sizeof(ORIGINAL_DISPATCH[0]);
	Check(nExtensionRows == 41, "transcribed 41 extension rows");
	Check(nTokens == nExtensionRows + 1, "arrLexerNames is one longer than arrLangExtensions");
	Check(nDispatchRows == 42, "transcribed 42 LoadLexer branches");
	for (size_t i = 0; i < nExtensionRows; ++i)
	{
		bool bReachable = false;
		for (size_t j = 0; j < nDispatchRows; ++j)
		{
			bReachable = bReachable
				|| std::string(ORIGINAL_LEXER_TOKENS[i]) == ORIGINAL_DISPATCH[j]._Token;
		}
		Check(bReachable, std::string("token reaches a LoadLexer branch: ")
			+ ORIGINAL_LEXER_TOKENS[i]);
	}

	//----------------------------------------------------------------------
	// Build the corpus
	//----------------------------------------------------------------------
	std::vector<std::string> tokens;
	CollectExtensionTokens(tokens);
	Check(tokens.size() == 112, "112 extension tokens in the transcribed table");

	std::vector<std::string> names;

	// 1. Every real extension, in every case permutation, as "file.<ext>" - and
	//    bare, since a file may legitimately be named "cpp".
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		const std::vector<std::string> variants = CasePermutations(tokens[i]);
		for (size_t v = 0; v < variants.size(); ++v)
		{
			names.push_back("file." + variants[v]);
			names.push_back(variants[v]);
			names.push_back("." + variants[v]);
			names.push_back("archive.tar." + variants[v]);
		}
	}

	// 2. The two file names that beat every extension, cased every way, plus the
	//    near-misses that must NOT trigger them.
	const char* const kSpecials[] = { "Makefile", "CMakeLists.txt" };
	for (size_t i = 0; i < sizeof(kSpecials) / sizeof(kSpecials[0]); ++i)
	{
		const std::vector<std::string> variants = CasePermutations(kSpecials[i]);
		names.insert(names.end(), variants.begin(), variants.end());
	}
	const char* const kNearMisses[] = {
		"Makefile.am", "Makefile.in", "makefile2", "MyMakefile", "Make file",
		"CMakeLists.txt.bak", "CMakeLists", "CMakeLists.txt ", " CMakeLists.txt",
		"CMakeCache.txt", "notes.txt", "", ".", "..", "...", "|", ".|", "a|b",
		"file.", "file..", ".cpp.", "no-extension-here", "UPPER.ZZZ",
	};
	for (size_t i = 0; i < sizeof(kNearMisses) / sizeof(kNearMisses[0]); ++i)
	{
		names.push_back(kNearMisses[i]);
	}

	// 3. Exhaustive short strings - this is what finds the parsing edge cases
	//    nobody thinks to write down.
	GenerateExhaustive("cpH.|1m", 5, names);

	//----------------------------------------------------------------------
	// Run both implementations over all of it
	//----------------------------------------------------------------------
	size_t nCompared = 0;
	size_t nMismatches = 0;
	size_t nNonPlainText = 0;
	for (size_t i = 0; i < names.size(); ++i)
	{
		std::string strWantLexer, strWantId;
		OriginalDetect(Widen(names[i]), strWantLexer, strWantId);

		const Core::SLanguageInfo* pInfo = languages.DetectForFileName(names[i]);
		const std::string strGotLexer = pInfo ? pInfo->_LexerName : PLAIN_TEXT_LEXER;
		const std::string strGotId = pInfo ? pInfo->_Id : NO_LANGUAGE;

		++nCompared;
		if (strWantId != NO_LANGUAGE)
		{
			++nNonPlainText;
		}
		if (strGotLexer != strWantLexer || strGotId != strWantId)
		{
			++nMismatches;
			if (nMismatches <= 10)
			{
				std::cout << "  FAIL: " << names[i] << ": core says ("
					<< strGotLexer << ", " << strGotId << "), the MFC original says ("
					<< strWantLexer << ", " << strWantId << ")\n";
			}
		}
	}
	++g_Checks;
	if (nMismatches != 0)
	{
		++g_Failures;
		std::cout << "  FAIL: " << nMismatches << " of " << nCompared
			<< " file names disagree\n";
	}

	// A corpus that never reaches a real language would pass this test while
	// proving nothing at all.
	Check(nNonPlainText > 1000, "the corpus actually selects languages, not just plain text");
	Check(nCompared > 20000, "the corpus is large enough to be worth running");

	//----------------------------------------------------------------------
	// Cases worth naming, so a regression says what broke rather than just how
	// many things broke.
	//----------------------------------------------------------------------
	struct SNamedCase { const char* _FileName; const char* _Id; const char* _Lexer; };
	const SNamedCase kNamed[] = {
		{ "main.cpp",			"cpp",			"cpp" },
		{ "main.CPP",			"cpp",			"cpp" },
		{ "header.h",			"cpp",			"cpp" },
		{ "script.py",			"python",		"python" },
		{ "page.html",			"html",			"hypertext" },
		{ "app.js",				"javascript",	"cpp" },
		// Reproduced deliberately: .json gets Lexilla's C++ lexer in the shipping
		// app, while the theme's "json" table is written in SCE_JSON_* constants.
		// See doc/PORTING.md 6d - the port preserves it rather than diverging.
		{ "package.json",		"json",			"cpp" },
		{ "setup.iss",			"inno",			"asm" },
		{ "Makefile",			"makefile",		"makefile" },
		{ "makefile",			"makefile",		"makefile" },
		{ "MAKEFILE",			"makefile",		"makefile" },
		{ "CMakeLists.txt",		"cmake",		"cmake" },
		{ "build.cmake",		"cmake",		"cmake" },
		{ "notes.txt",			"",				"plaintext" },
		{ "README",				"",				"plaintext" },
	};
	for (size_t i = 0; i < sizeof(kNamed) / sizeof(kNamed[0]); ++i)
	{
		const Core::SLanguageInfo* pInfo = languages.DetectForFileName(kNamed[i]._FileName);
		const std::string strId = pInfo ? pInfo->_Id : "";
		const std::string strLexer = pInfo ? pInfo->_LexerName : PLAIN_TEXT_LEXER;
		Check(strId == kNamed[i]._Id && strLexer == kNamed[i]._Lexer,
			std::string(kNamed[i]._FileName) + " -> (" + kNamed[i]._Id + ", "
				+ kNamed[i]._Lexer + "), got (" + strId + ", " + strLexer + ")");
	}

	// cmakelists.txt lowercase is NOT the CMake special case - the original
	// compares that one with operator==, not CompareNoCase. It still resolves to
	// plain text rather than to cmake, and both sides must agree about that.
	{
		const Core::SLanguageInfo* pInfo = languages.DetectForFileName("cmakelists.txt");
		Check(pInfo == nullptr, "cmakelists.txt (lower case) is not the cmake special case");
	}

	// ExtensionOf is documented behaviour, not an implementation detail: the Qt
	// frontend relies on it agreeing with QFileInfo::suffix().
	Check(Core::CLanguageTable::ExtensionOf("main.cpp") == "cpp", "ExtensionOf: main.cpp");
	Check(Core::CLanguageTable::ExtensionOf("a.tar.gz") == "gz", "ExtensionOf: a.tar.gz");
	Check(Core::CLanguageTable::ExtensionOf("Makefile").empty(), "ExtensionOf: Makefile");
	Check(Core::CLanguageTable::ExtensionOf(".gitignore") == "gitignore", "ExtensionOf: dotfile");
	Check(Core::CLanguageTable::ExtensionOf("trailing.").empty(), "ExtensionOf: trailing dot");
	Check(Core::CLanguageTable::ExtensionOf("").empty(), "ExtensionOf: empty");

	std::cout << (g_Failures == 0 ? "PASS" : "FAIL") << ": TestLanguageLookup - "
		<< g_Checks << " checks, " << nCompared << " file names compared against the "
		<< "transcribed MFC original (" << nNonPlainText << " selecting a language), "
		<< g_Failures << " failure(s)\n";
	return g_Failures == 0 ? 0 : 1;
}
