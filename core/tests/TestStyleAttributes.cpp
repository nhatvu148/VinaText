/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Differential test: does languages.json render the same styles bold and italic
// as the MFC lexer initialisers?
//
// Weight and slant live in neither colour header. They are an if-chain inside
// each of 13 Init_<x>_Editor functions, applied per style constant as the colour
// table is walked, and the JSON carried only colour until this change - so
// ui-qt/ drew Python keywords at normal weight where Windows draws them bold.
//
// The ORIGINAL side below transcribes those 13 chains from
// src/EditorLexerDark.cpp, verbatim, structure included. It reads no JSON and
// shares nothing with the Python extractor, so a mistake in either shows up here
// as a disagreement.
//
// The structure is the part worth transcribing carefully. Two of the thirteen -
// python and r - are NOT one if/else-if chain but two independent `if` chains,
// so a style matched by the first still falls through the second to its `else`.
// Anything that assumed a single chain would produce the same answer for most
// languages and the wrong one for those two.
//
// The style names each language walks come from theme-dark.json, which is the
// same table (g_rgb_Syntax_<lang>) the MFC loop iterates. Numeric values are not
// checked here: tools/extract_language_data.py --verify resolves them against
// Scintilla's own SciLexer.h, which is a stronger check than repeating them.
//
// Build and run (one line):
//   c++ -std=c++11 -I include -I core core/LanguageData.cpp core/tests/TestStyleAttributes.cpp -o teststyleattrs && ./teststyleattrs Packages/data-packages

#include "LanguageData.h"

#include <algorithm>
#include <iostream>
#include <set>
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

	typedef std::set<std::string> TAttributes;

	// `iItem == A || iItem == B || ...`
	bool Is(const std::string& strStyle, const char* const* names, size_t nCount)
	{
		for (size_t i = 0; i < nCount; ++i)
		{
			if (strStyle == names[i])
			{
				return true;
			}
		}
		return false;
	}

#define IS(style, ...) \
	([&]() -> bool { \
		const char* const names[] = { __VA_ARGS__ }; \
		return Is(style, names, sizeof(names) / sizeof(names[0])); \
	}())

	//----------------------------------------------------------------------
	// The 13 initialisers, transcribed
	//----------------------------------------------------------------------

	TAttributes OriginalAttributes(const std::string& strLanguage, const std::string& s)
	{
		TAttributes out;

		if (strLanguage == "batch")
		{
			if (IS(s, "SCE_C_COMMENTLINE", "SCE_C_COMMENTDOC", "SCE_C_WORD2")) { out.insert("bold"); }
			else if (IS(s, "SCE_C_COMMENT")) { out.insert("italic"); }
		}
		else if (strLanguage == "css")
		{
			if (IS(s, "SCE_CSS_TAG", "SCE_CSS_PSEUDOCLASS", "SCE_CSS_OPERATOR", "SCE_CSS_IMPORTANT"))
			{
				out.insert("bold");
			}
			else if (IS(s, "SCE_CSS_CLASS")) { out.insert("italic"); out.insert("bold"); }
			else if (IS(s, "SCE_CSS_IDENTIFIER2")) { out.insert("bold"); }
		}
		else if (strLanguage == "flexlicense")
		{
			if (IS(s, "SCE_P_WORD", "SCE_P_WORD2", "SCE_P_OPERATOR")) { out.insert("bold"); }
			else if (IS(s, "SCE_P_STRING", "SCE_P_CHARACTER")) { out.insert("italic"); }
		}
		else if (strLanguage == "freebasic" || strLanguage == "vb")
		{
			// Byte-identical chains in the two initialisers.
			if (IS(s, "SCE_C_COMMENTDOC")) { out.insert("bold"); }
			else if (IS(s, "SCE_C_NUMBER")) { out.insert("italic"); }
		}
		else if (strLanguage == "html")
		{
			// The disabled rule above this one in the source would have made
			// SCE_H_ATTRIBUTE bold+italic. It is commented out, so it does not.
			if (IS(s, "SCE_H_TAG", "SCE_H_ENTITY", "SCE_HB_DEFAULT", "SCE_HJA_DEFAULT",
					"SCE_HBA_IDENTIFIER", "SCE_HB_IDENTIFIER", "SCE_HPHP_OPERATOR",
					"SCE_HPHP_DEFAULT", "SCE_H_OTHER", "SCE_H_XMLSTART"))
			{
				out.insert("bold");
			}
		}
		else if (strLanguage == "xml")
		{
			// The same list as html, minus SCE_H_XMLSTART.
			if (IS(s, "SCE_H_TAG", "SCE_H_ENTITY", "SCE_HB_DEFAULT", "SCE_HJA_DEFAULT",
					"SCE_HBA_IDENTIFIER", "SCE_HB_IDENTIFIER", "SCE_HPHP_OPERATOR",
					"SCE_HPHP_DEFAULT", "SCE_H_OTHER"))
			{
				out.insert("bold");
			}
		}
		else if (strLanguage == "json")
		{
			if (IS(s, "SCE_JSON_DEFAULT", "SCE_JSON_ESCAPESEQUENCE", "SCE_JSON_COMPACTIRI",
					"SCE_JSON_PROPERTYNAME", "SCE_JSON_OPERATOR", "SCE_JSON_KEYWORD"))
			{
				out.insert("bold");
			}
		}
		else if (strLanguage == "markdown")
		{
			if (IS(s, "SCE_MARKDOWN_PRECHAR", "SCE_MARKDOWN_BLOCKQUOTE", "SCE_MARKDOWN_CODE",
					"SCE_MARKDOWN_CODE2", "SCE_MARKDOWN_CODEBK"))
			{
				out.insert("italic");
			}
			else if (IS(s, "SCE_MARKDOWN_STRIKEOUT", "SCE_MARKDOWN_LINK")) { out.insert("bold"); }
		}
		else if (strLanguage == "pascal")
		{
			if (IS(s, "SCE_PAS_WORD")) { out.insert("bold"); }
			else if (IS(s, "SCE_PAS_STRING")) { out.insert("italic"); }
			else if (IS(s, "SCE_PAS_PREPROCESSOR", "SCE_PAS_PREPROCESSOR2")) { out.insert("bold"); }
		}
		else if (strLanguage == "powershell")
		{
			if (IS(s, "SCE_C_WORD", "SCE_C_PREPROCESSOR", "SCE_C_OPERATOR", "SCE_C_UUID",
					"SCE_C_NUMBER"))
			{
				out.insert("bold");
			}
			else if (IS(s, "SCE_C_COMMENT", "SCE_C_COMMENTLINE", "SCE_C_COMMENTDOC",
					"SCE_C_WORD2"))
			{
				out.insert("italic");
			}
		}
		else if (strLanguage == "python")
		{
			// TWO chains. The second `if` is not an `else if`, so SCE_P_WORD takes
			// the first chain's bold and then falls to the second chain's else.
			if (IS(s, "SCE_P_WORD", "SCE_P_WORD2", "SCE_P_OPERATOR")) { out.insert("bold"); }

			if (IS(s, "SCE_P_CLASSNAME", "SCE_P_DEFNAME"))
			{
				out.insert("bold");
				out.insert("italic");
			}
			else if (IS(s, "SCE_P_STRING", "SCE_P_CHARACTER")) { out.insert("italic"); }
		}
		else if (strLanguage == "r")
		{
			// Two chains again.
			if (IS(s, "SCE_R_KWORD", "SCE_R_BASEKWORD")) { out.insert("bold"); }

			if (IS(s, "SCE_R_STRING", "SCE_R_STRING2")) { out.insert("italic"); }
		}
		return out;
	}

	// The 13 languages whose initialisers carry attribute logic at all. Every
	// other language must carry none, and that is asserted below - a language
	// that grew attributes in the C++ without reaching the JSON is exactly the
	// drift this test exists to catch.
	const char* const ATTRIBUTE_LANGUAGES[] = {
		"batch", "css", "flexlicense", "freebasic", "html", "json", "markdown",
		"pascal", "powershell", "python", "r", "vb", "xml",
	};

	TAttributes ToSet(const Core::SStyleAttribute& attribute)
	{
		TAttributes out;
		if (attribute._Bold) { out.insert("bold"); }
		if (attribute._Italic) { out.insert("italic"); }
		if (attribute._Underline) { out.insert("underline"); }
		return out;
	}

	std::string Describe(const TAttributes& attributes)
	{
		if (attributes.empty())
		{
			return "(none)";
		}
		std::string out;
		for (TAttributes::const_iterator it = attributes.begin(); it != attributes.end(); ++it)
		{
			out += (out.empty() ? "" : "+") + *it;
		}
		return out;
	}
}

int main(int argc, char** argv)
{
	const std::string strDataDir = (argc > 1) ? argv[1] : "Packages/data-packages";

	Core::CLanguageTable languages;
	Core::CEditorTheme theme;
	std::string strError;
	if (!languages.LoadFromFile(strDataDir + "/languages.json", strError)
		|| !theme.LoadFromFile(strDataDir + "/theme-dark.json", strError))
	{
		std::cout << "FATAL: " << strError << "\n";
		return 1;
	}

	const size_t nAttributeLanguages =
		sizeof(ATTRIBUTE_LANGUAGES) / sizeof(ATTRIBUTE_LANGUAGES[0]);
	std::set<std::string> expectedLanguages(ATTRIBUTE_LANGUAGES,
		ATTRIBUTE_LANGUAGES + nAttributeLanguages);

	size_t nCompared = 0;
	size_t nWithAttributes = 0;

	for (const Core::SLanguageInfo& info : languages.GetLanguages())
	{
		// What the MFC loop walks for this language - which is NOT always the
		// table named after it. Init_xml_Editor walks html's, so xml's own
		// 28-entry SCE_C_* table is applied to nothing at all.
		const std::vector<Core::SStyleMapping>* pStyles = theme.FindStyles(info._StyleTable);
		if (pStyles == nullptr)
		{
			Check(info._StyleAttributes.empty(),
				info._Id + " has style attributes but no style table at all");
			Check(false, info._Id + ": style table " + info._StyleTable + " does not exist");
			continue;
		}

		for (const Core::SStyleMapping& style : *pStyles)
		{
			const TAttributes expected = OriginalAttributes(info._Id, style._Style);

			TAttributes actual;
			for (const Core::SStyleAttribute& attribute : info._StyleAttributes)
			{
				if (attribute._Style == style._Style)
				{
					actual = ToSet(attribute);
					break;
				}
			}

			++nCompared;
			if (!expected.empty())
			{
				++nWithAttributes;
			}
			if (actual != expected)
			{
				++g_Failures;
				++g_Checks;
				std::cout << "  FAIL: " << info._Id << "/" << style._Style
					<< ": languages.json says " << Describe(actual)
					<< ", the MFC original says " << Describe(expected) << "\n";
			}
		}

		// Every attribute the JSON carries must belong to a style the language
		// actually has, or it can never be applied to anything.
		for (const Core::SStyleAttribute& attribute : info._StyleAttributes)
		{
			bool bKnown = false;
			for (const Core::SStyleMapping& style : *pStyles)
			{
				bKnown = bKnown || style._Style == attribute._Style;
			}
			Check(bKnown, info._Id + ": style attribute for " + attribute._Style
				+ ", which is not in that language's style table");
		}

		const bool bHasAttributes = !info._StyleAttributes.empty();
		const bool bShouldHave = expectedLanguages.count(info._Id) != 0;
		Check(bHasAttributes == bShouldHave,
			info._Id + (bShouldHave ? " should carry style attributes and does not"
									: " carries style attributes and should not"));
	}

	// A corpus that never reached a styled language would pass while proving
	// nothing, and the count is the cheapest guard against that.
	Check(nCompared > 500, "compared a meaningful number of styles");
	Check(nWithAttributes == 75, "75 styles carry an attribute");

	// xml is the one language coloured from another's table, and getting that
	// wrong is invisible until someone opens an .xml file and compares two
	// screenshots. Pin it.
	{
		const Core::SLanguageInfo* pXml = languages.FindById("xml");
		Check(pXml != nullptr && pXml->_StyleTable == "html",
			"xml is coloured from html's style table");
		const Core::SLanguageInfo* pHtml = languages.FindById("html");
		Check(pHtml != nullptr && pHtml->_StyleTable == "html", "html uses its own table");
		const Core::SLanguageInfo* pPython = languages.FindById("python");
		Check(pPython != nullptr && pPython->_StyleTable == "python", "python uses its own");
	}

	// Named cases, so a regression says what broke. These are read off the
	// source by hand, not derived from anything above.
	struct SNamed { const char* _Language; const char* _Style; bool _Bold; bool _Italic; };
	const SNamed kNamed[] = {
		{ "python", "SCE_P_WORD",      true,  false },	// keywords are bold
		{ "python", "SCE_P_CLASSNAME", true,  true  },	// class names bold + italic
		{ "python", "SCE_P_STRING",    false, true  },	// strings italic
		{ "python", "SCE_P_COMMENTLINE", false, false },// comments plain
		{ "css",    "SCE_CSS_CLASS",   true,  true  },
		{ "r",      "SCE_R_KWORD",     true,  false },	// two-chain: bold only
		{ "r",      "SCE_R_STRING",    false, true  },	// two-chain: italic only
		{ "html",   "SCE_H_TAG",       true,  false },
	};
	for (size_t i = 0; i < sizeof(kNamed) / sizeof(kNamed[0]); ++i)
	{
		const Core::SLanguageInfo* pInfo = languages.FindById(kNamed[i]._Language);
		bool bBold = false, bItalic = false;
		if (pInfo != nullptr)
		{
			for (const Core::SStyleAttribute& attribute : pInfo->_StyleAttributes)
			{
				if (attribute._Style == kNamed[i]._Style)
				{
					bBold = attribute._Bold;
					bItalic = attribute._Italic;
				}
			}
		}
		Check(bBold == kNamed[i]._Bold && bItalic == kNamed[i]._Italic,
			std::string(kNamed[i]._Language) + "/" + kNamed[i]._Style + ": bold/italic");
	}

	// SCE_H_ATTRIBUTE's rule is commented out in the source. If someone
	// uncomments it, the extraction must follow rather than this test hiding it.
	{
		const Core::SLanguageInfo* pInfo = languages.FindById("html");
		bool bFound = false;
		if (pInfo != nullptr)
		{
			for (const Core::SStyleAttribute& attribute : pInfo->_StyleAttributes)
			{
				bFound = bFound || attribute._Style == "SCE_H_ATTRIBUTE";
			}
		}
		Check(!bFound, "html/SCE_H_ATTRIBUTE stays plain - its rule is commented out");
	}

	//----------------------------------------------------------------------
	// Malformed input. The data is generated, so the realistic corruption is a
	// hand edit - and the failure that matters is the QUIET one: a style that
	// keeps its bold, passes every other check, and simply never renders italic.
	//----------------------------------------------------------------------
	{
		const std::string strHead =
			"{\"plainTextLabel\":\"plain text\",\"languages\":[{"
			"\"id\":\"probe\",\"name\":\"probe\",\"extension\":\"p\","
			"\"extensions\":\"p\",\"lexer\":\"cpp\",\"commentLine\":\"//\","
			"\"commentStart\":\"\",\"commentEnd\":\"\",\"keywords\":\"\","
			"\"styleAttributes\":[";
		const std::string strTail = "]}]}";

		struct SCase { const char* _Entry; bool _ShouldLoad; const char* _What; };
		const SCase kCases[] = {
			{ "{\"style\":\"S\",\"value\":1,\"bold\":true}", true,
			  "a well-formed attribute loads" },
			// The two the fold used to swallow, both alongside a valid "bold".
			{ "{\"style\":\"S\",\"value\":1,\"bold\":true,\"italic\":\"yes\"}", false,
			  "a wrong-typed value is rejected, not folded to false" },
			{ "{\"style\":\"S\",\"value\":1,\"bold\":true,\"Italic\":true}", false,
			  "a mistyped key is rejected, not ignored" },
			{ "{\"style\":\"S\",\"value\":1,\"bold\":false}", false,
			  "an attribute that sets nothing is rejected" },
			{ "{\"style\":\"S\",\"bold\":true}", false,
			  "an attribute with no numeric value is rejected" },
		};
		for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i)
		{
			Core::CLanguageTable probe;
			std::string strProbeError;
			const bool bLoaded = probe.LoadFromString(
				strHead + kCases[i]._Entry + strTail, strProbeError);
			Check(bLoaded == kCases[i]._ShouldLoad,
				std::string(kCases[i]._What) + " (error: " + strProbeError + ")");
		}
	}

	std::cout << (g_Failures == 0 ? "PASS" : "FAIL") << ": TestStyleAttributes - "
		<< g_Checks << " checks, " << nCompared << " styles compared against the "
		<< "transcribed MFC original (" << nWithAttributes << " carrying an attribute), "
		<< g_Failures << " failure(s)\n";
	return g_Failures == 0 ? 0 : 1;
}
