/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Verifies core/LanguageData against the real shipped data files.
//
// This runs on macOS/Linux/Windows with any C++11 compiler - no MFC, no Qt, no
// test framework. It exists because the MFC application cannot be built outside
// Windows, so this is the only executable check available on a developer machine
// until the CI matrix lands in Phase 0.
//
// Build and run (one line - no trailing backslash, which would splice this
// comment into the next and trip -Wcomment under GCC):
//   c++ -std=c++11 -I include -I core core/LanguageData.cpp core/tests/TestLanguageData.cpp -o testlangdata && ./testlangdata

#include "LanguageData.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

	template <typename T>
	void CheckEqual(const T& actual, const T& expected, const std::string& strWhat)
	{
		++g_Checks;
		if (!(actual == expected))
		{
			++g_Failures;
			std::cout << "  FAIL: " << strWhat << "\n"
				<< "        actual   = " << actual << "\n"
				<< "        expected = " << expected << "\n";
		}
	}
}

int main(int argc, char** argv)
{
	const std::string strDataDir = (argc > 1) ? argv[1] : "Packages/data-packages";
	std::string strError;

	//----------------------------------------------------------------------
	// languages.json
	//----------------------------------------------------------------------
	Core::CLanguageTable languages;
	if (!languages.LoadFromFile(strDataDir + "/languages.json", strError))
	{
		std::cout << "FATAL: " << strError << "\n";
		return 1;
	}

	CheckEqual<size_t>(languages.GetLanguages().size(), 42, "language count");
	CheckEqual<std::string>(languages.GetPlainTextLabel(), "plain text", "plain text label");

	const Core::SLanguageInfo* pPython = languages.FindById("python");
	Check(pPython != nullptr, "python is present");
	if (pPython != nullptr)
	{
		CheckEqual<std::string>(pPython->_Extension, "py", "python extension");
		CheckEqual<std::string>(pPython->_CommentLine, "#", "python comment line");
		Check(pPython->_CommentStart.empty(), "python has no block comment start");
		Check(pPython->_Keywords.find("lambda") != std::string::npos,
			"python keywords contain 'lambda'");
	}

	const Core::SLanguageInfo* pCpp = languages.FindById("cpp");
	Check(pCpp != nullptr, "cpp is present");
	if (pCpp != nullptr)
	{
		CheckEqual<std::string>(pCpp->_CommentLine, "//", "cpp comment line");
		CheckEqual<std::string>(pCpp->_CommentStart, "/*", "cpp comment start");
		CheckEqual<std::string>(pCpp->_CommentEnd, "*/", "cpp comment end");
	}

	//----------------------------------------------------------------------
	// tagMatch. Exactly three languages, and the point of the field is that no
	// other key selects that set - so the wrong answers are asserted too, by
	// naming a language each wrong key would have included or dropped.
	//----------------------------------------------------------------------
	{
		std::vector<std::string> matching;
		for (size_t i = 0; i < languages.GetLanguages().size(); ++i)
		{
			if (languages.GetLanguages()[i]._TagMatch)
			{
				matching.push_back(languages.GetLanguages()[i]._Id);
			}
		}
		CheckEqual<size_t>(matching.size(), 3, "exactly three languages tag-match");
		for (size_t i = 0; i < matching.size(); ++i)
		{
			Check(matching[i] == "html" || matching[i] == "php" || matching[i] == "xml",
				"tag-matching language is html, php or xml, got " + matching[i]);
		}

		// php is lexed as "cpp". Keying tag matching on the lexer would have
		// dragged in every other cpp-lexed language; these three are the canaries.
		const Core::SLanguageInfo* pPhp = languages.FindById("php");
		Check(pPhp != nullptr && pPhp->_TagMatch, "php tag-matches");
		Check(pPhp != nullptr && pPhp->_LexerName == "cpp",
			"php is lexed as cpp, which is why the lexer is the wrong key");
		const char* aNotTagged[] = { "cpp", "json", "javascript", "typescript", "go" };
		for (size_t i = 0; i < sizeof(aNotTagged) / sizeof(aNotTagged[0]); ++i)
		{
			const Core::SLanguageInfo* pOther = languages.FindById(aNotTagged[i]);
			Check(pOther != nullptr && pOther->_LexerName == "cpp" && !pOther->_TagMatch,
				std::string(aNotTagged[i]) + " is lexed as cpp and does NOT tag-match");
		}

		// html and xml share a style table and a fold marker; php shares neither.
		// Either would therefore have silently dropped php.
		const Core::SLanguageInfo* pHtml = languages.FindById("html");
		const Core::SLanguageInfo* pXml = languages.FindById("xml");
		Check(pHtml != nullptr && pXml != nullptr && pPhp != nullptr
			&& pHtml->_StyleTable == pXml->_StyleTable && pPhp->_StyleTable != pHtml->_StyleTable,
			"styleTable groups html with xml but not php - the wrong key");
		Check(pHtml != nullptr && pXml != nullptr && pPhp != nullptr
			&& pHtml->_FoldMarker == pXml->_FoldMarker && pPhp->_FoldMarker != pHtml->_FoldMarker,
			"foldMarker groups html with xml but not php - also the wrong key");
	}

	Check(languages.FindById("no_such_language") == nullptr, "unknown language returns nullptr");

	// Known data quirks, asserted so they stay visible rather than becoming folklore.
	const Core::SLanguageInfo* pVcxproject = languages.FindById("vcxproject");
	Check(pVcxproject != nullptr && pVcxproject->_Keywords.empty(),
		"vcxproject has no keyword blob (matches the C++ table)");

	size_t nWithKeywords = 0;
	for (size_t i = 0; i < languages.GetLanguages().size(); ++i)
	{
		if (!languages.GetLanguages()[i]._Keywords.empty())
		{
			++nWithKeywords;
		}
	}
	CheckEqual<size_t>(nWithKeywords, 39, "languages carrying a non-empty keyword blob");

	//----------------------------------------------------------------------
	// theme-light.json / theme-dark.json
	//----------------------------------------------------------------------
	const char* aThemes[] = { "light", "dark" };
	for (int i = 0; i < 2; ++i)
	{
		const std::string strName = aThemes[i];
		Core::CEditorTheme theme;
		if (!theme.LoadFromFile(strDataDir + "/theme-" + strName + ".json", strError))
		{
			std::cout << "FATAL: " << strError << "\n";
			return 1;
		}

		CheckEqual<std::string>(theme.GetName(), strName, strName + ": name");
		CheckEqual<size_t>(theme.GetPalette().size(), 34, strName + ": palette size");

		// Pin a couple of concrete values. tools/extract_language_data.py --verify is
		// the authority on JSON-vs-C++ equivalence, but these catch a stray edit to the
		// data files without needing the C++ headers around.
		Core::SColor probe;
		Check(theme.ResolveColor("black", probe) && probe._Red == 0
			&& probe._Green == 0 && probe._Blue == 0, strName + ": black is #000000");
		// editorBackground is what the editor paints behind the text. It is not a
		// per-language style, so the original extraction missed it entirely and the
		// Qt frontend rendered a dark theme on a white background until it was added.
		Core::SColor background;
		Check(theme.ResolveColor("editorBackground", background),
			strName + ": editorBackground is present");
		if (strName == "light")
		{
			Check(background._Red == 255 && background._Green == 255 && background._Blue == 255,
				"light: editorBackground is #FFFFFF");
		}
		else
		{
			Check(background._Red == 39 && background._Green == 40 && background._Blue == 34,
				"dark: editorBackground is #272822 (monokai)");
		}

		if (strName == "light")
		{
			Check(theme.ResolveColor("comment", probe) && probe._Red == 10
				&& probe._Green == 103 && probe._Blue == 4, "light: comment is #0A6704");
		}
		else
		{
			Check(theme.ResolveColor("editorTextColor", probe) && probe._Red == 255
				&& probe._Green == 255 && probe._Blue == 255, "dark: text is #FFFFFF");
		}

		//------------------------------------------------------------------
		// Colour roles. Eight of the ten take the palette key named after them,
		// which is why resolving a role by its own name looked correct. The two
		// that do not are pinned here by value, because they are the whole reason
		// the indirection exists.
		//------------------------------------------------------------------
		// Ten from the IS_LIGHT_THEME preset, two from the brace styling below it.
		CheckEqual<size_t>(theme.GetRoles().size(), 12, strName + ": role count");

		// The brace colours come from BasicColors (src/AppUtil.h), a table with no
		// light and dark variant - so unlike every other role these are the same
		// in both themes. The extractor guards that BasicColors and the palette
		// still agree; this pins the values a reader can see on screen.
		Core::SColor braceLight, braceBad;
		Check(theme.ResolveRole("braceLightColor", braceLight)
			&& braceLight._Red == 255 && braceLight._Green == 0 && braceLight._Blue == 0,
			strName + ": a matched brace is red");
		Check(theme.ResolveRole("braceBadColor", braceBad)
			&& braceBad._Red == 0 && braceBad._Green == 0 && braceBad._Blue == 255,
			strName + ": an unmatched brace is blue");

		Core::SColor role;
		// The one whose key differs BETWEEN the themes. Nothing a frontend can
		// resolve by name gets this right in both builds.
		Check(theme.ResolveRole("selectionTextColor", role),
			strName + ": selectionTextColor resolves");
		if (strName == "light")
		{
			CheckEqual<std::string>(theme.GetRoles().at("selectionTextColor"), "black",
				"light: selectionTextColor takes the palette key 'black'");
			Check(role._Red == 0 && role._Green == 0 && role._Blue == 0,
				"light: selectionTextColor is #000000");
		}
		else
		{
			CheckEqual<std::string>(theme.GetRoles().at("selectionTextColor"), "white",
				"dark: selectionTextColor takes the palette key 'white'");
			Check(role._Red == 255 && role._Green == 255 && role._Blue == 255,
				"dark: selectionTextColor is #FFFFFF");
		}

		// The one whose key differs from its own NAME, in both themes.
		CheckEqual<std::string>(theme.GetRoles().at("lineNumberColor"), "linenumber",
			strName + ": lineNumberColor takes the palette key 'linenumber'");
		Core::SColor lineNumber;
		Check(theme.ResolveRole("lineNumberColor", lineNumber)
			&& theme.ResolveColor("linenumber", probe)
			&& lineNumber._Red == probe._Red && lineNumber._Green == probe._Green
			&& lineNumber._Blue == probe._Blue,
			strName + ": lineNumberColor resolves to the linenumber entry");
		Check(!theme.ResolveColor("lineNumberColor", probe),
			strName + ": there is no palette key called 'lineNumberColor'");

		// Every role must resolve; the loader rejects a dangling key, so reaching
		// here means it held - assert it rather than assume it.
		for (std::map<std::string, std::string>::const_iterator it = theme.GetRoles().begin();
			it != theme.GetRoles().end(); ++it)
		{
			Check(theme.ResolveRole(it->first, role),
				strName + ": role " + it->first + " resolves key " + it->second);
		}
		Check(!theme.ResolveRole("noSuchRole", role),
			strName + ": an unknown role does not resolve");

		// python_2 has a style table but no language metadata - preserved deliberately.
		// (theme-level checks continue below; the tagMatch checks are on the
		// language table and live further down.)
		Check(theme.FindStyles("python_2") != nullptr,
			strName + ": python_2 style table is preserved");

		const std::vector<Core::SStyleMapping>* pStyles = theme.FindStyles("python");
		Check(pStyles != nullptr, strName + ": python style table");
		if (pStyles != nullptr && !pStyles->empty())
		{
			CheckEqual<std::string>((*pStyles)[0]._Style, "SCE_P_DEFAULT",
				strName + ": first python style symbol");
			CheckEqual<int>((*pStyles)[0]._Value, 0, strName + ": SCE_P_DEFAULT value");
		}

		// Every style in every language must resolve against the palette. The loader
		// already rejects a dangling reference, so reaching here means it held - but
		// assert it explicitly so the guarantee is tested, not merely assumed.
		size_t nStyles = 0;
		Core::SColor color;
		const char* aLangs[] = { "python", "cpp", "java", "rust", "python_2" };
		for (size_t l = 0; l < sizeof(aLangs) / sizeof(aLangs[0]); ++l)
		{
			const std::vector<Core::SStyleMapping>* p = theme.FindStyles(aLangs[l]);
			if (p == nullptr)
			{
				continue;
			}
			for (size_t s = 0; s < p->size(); ++s)
			{
				++nStyles;
				Check(theme.ResolveColor((*p)[s]._Color, color),
					strName + ": " + aLangs[l] + " style " + (*p)[s]._Style
					+ " resolves colour " + (*p)[s]._Color);
			}
		}
		Check(nStyles > 0, strName + ": sampled style mappings is non-empty");
	}

	//----------------------------------------------------------------------
	// malformed input must fail loudly, not silently
	//----------------------------------------------------------------------
	{
		Core::CEditorTheme bad;
		Check(!bad.LoadFromString("{ \"name\": \"x\" }", strError),
			"theme without a palette is rejected");

		// These carry "roles" so that they still fail for the reason each one is
		// named after. Without it they would be rejected for the missing object
		// instead, and would keep passing while testing nothing.
		Check(!bad.LoadFromString(
			"{\"name\":\"x\",\"palette\":{\"a\":\"#000000\"},\"roles\":{},"
			"\"languages\":{\"z\":[{\"style\":\"SCE_X\",\"value\":1,\"color\":\"missing\"}]}}",
			strError), "style referencing an undefined colour is rejected");

		Check(!bad.LoadFromString(
			"{\"name\":\"x\",\"palette\":{\"a\":\"red\"},\"roles\":{},\"languages\":{}}",
			strError), "non-hex palette entry is rejected");

		// A theme that loads at all, to show the three rejections above are about
		// their own defect and not about the shape of the fixture.
		Check(bad.LoadFromString(
			"{\"name\":\"x\",\"palette\":{\"a\":\"#010203\"},\"roles\":{\"r\":\"a\"},"
			"\"languages\":{}}", strError), "a minimal well-formed theme loads");

		Check(!bad.LoadFromString(
			"{\"name\":\"x\",\"palette\":{\"a\":\"#000000\"},\"languages\":{}}",
			strError), "theme without a roles object is rejected");

		Check(!bad.LoadFromString(
			"{\"name\":\"x\",\"palette\":{\"a\":\"#000000\"},"
			"\"roles\":{\"r\":\"missing\"},\"languages\":{}}",
			strError), "role referencing an undefined colour is rejected");

		Core::CLanguageTable badLang;
		Check(!badLang.LoadFromString("not json at all", strError), "garbage input is rejected");
		Check(!badLang.LoadFromString("{}", strError), "document without languages is rejected");
	}

	std::cout << "\n" << (g_Checks - g_Failures) << "/" << g_Checks << " checks passed\n";
	if (g_Failures != 0)
	{
		std::cout << g_Failures << " FAILED\n";
		return 1;
	}
	std::cout << "OK\n";
	return 0;
}
