/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Pins core/AppSettings to the MFC's file format and to its shipped defaults.
//
// The defaults are duplicated - once in src/AppSettings.h as member
// initialisers, once in core/AppSettings.h - because core/ cannot include an
// MFC header. A value written down twice drifts, so this test READS
// src/AppSettings.h and compares, which is the same guard
// tools/extract_language_data.py --verify provides for the language data.
//
// Run with the repo root as the argument, so the header can be found:
//   ./TestAppSettings /path/to/VinaText

#include "AppSettings.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <iterator>
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

	// The declared initialiser for a member, straight out of src/AppSettings.h.
	// Deliberately crude string matching rather than anything clever: the point
	// is to read what a human reading that header would read.
	std::string DeclaredDefault(const std::string& strHeader, const std::string& strMember)
	{
		const std::string strNeedle = " " + strMember + " = ";
		const size_t nAt = strHeader.find(strNeedle);
		if (nAt == std::string::npos)
		{
			return "<member not found>";
		}
		const size_t nFrom = nAt + strNeedle.size();
		const size_t nEnd = strHeader.find(';', nFrom);
		if (nEnd == std::string::npos)
		{
			return "<no semicolon>";
		}
		std::string strValue = strHeader.substr(nFrom, nEnd - nFrom);
		while (!strValue.empty() && (strValue.back() == ' ' || strValue.back() == '\t'))
		{
			strValue.pop_back();
		}
		return strValue;
	}
}

int main(int argc, char** argv)
{
	const std::string strRoot = (argc > 1) ? argv[1] : ".";
	std::cout << "core/AppSettings against src/AppSettings.h\n";

	//----------------------------------------------------------------------
	// 1. The shipped defaults must match the MFC header, member by member.
	//    This is the check that stops the two copies drifting.
	//----------------------------------------------------------------------
	{
		std::ifstream in((strRoot + "/src/AppSettings.h").c_str());
		if (!in.good())
		{
			std::cout << "FATAL: cannot read " << strRoot << "/src/AppSettings.h\n";
			return 1;
		}
		std::ostringstream buffer;
		buffer << in.rdbuf();
		const std::string strHeader = buffer.str();

		Core::CAppSettings defaults;

		struct SBoolCase { const char* _Member; bool _Ours; };
		const SBoolCase aBools[] = {
			{ "m_bEnableUrlHighlight",           defaults.EnableUrlHighlight() },
			{ "m_bDrawFoldingLineUnderLineStyle", defaults.DrawFoldingLineUnderLineStyle() },
			{ "m_bDrawCaretLineFrame",           defaults.DrawCaretLineFrame() },
			{ "m_bEnableHightLightFolder",       defaults.EnableHighlightFolder() },
			{ "m_bEnableAutoComplete",           defaults.EnableAutoComplete() },
			{ "m_bAutoCompleteIgnoreNumbers",    defaults.AutoCompleteIgnoreNumbers() },
			{ "m_bAutoCompleteIgnoreCase",       defaults.AutoCompleteIgnoreCase() },
			{ "m_bUseFolderMarginClassic",       defaults.UseFolderMarginClassic() },
		};
		for (size_t i = 0; i < sizeof(aBools) / sizeof(aBools[0]); ++i)
		{
			const std::string strDeclared = DeclaredDefault(strHeader, aBools[i]._Member);
			const std::string strOurs = aBools[i]._Ours ? "TRUE" : "FALSE";
			Check(strDeclared == strOurs, std::string(aBools[i]._Member) + ": src/ says "
				+ strDeclared + ", core/ says " + strOurs);
		}

		// STYLE_TREE_BOX is the fourth value of FOLDER_MARGIN_STYPE
		// (src/EnumDef.h:233-239), so 3. Checked by NAME against the header and
		// by VALUE against the enum, because either alone would miss a
		// reordering of that enum.
		Check(DeclaredDefault(strHeader, "m_FolderMarginStyle")
			== "FOLDER_MARGIN_STYPE::STYLE_TREE_BOX",
			"m_FolderMarginStyle default is STYLE_TREE_BOX");
		{
			std::ifstream enums((strRoot + "/src/EnumDef.h").c_str());
			std::ostringstream enumBuffer;
			enumBuffer << enums.rdbuf();
			const std::string strEnums = enumBuffer.str();
			const size_t nAt = strEnums.find("enum FOLDER_MARGIN_STYPE");
			const size_t nArrow = strEnums.find("STYLE_ARROW", nAt);
			const size_t nBox = strEnums.find("STYLE_TREE_BOX", nAt);
			// Counting commas between the first entry and ours gives its value,
			// since only the first is given an explicit = 0.
			int nIndex = 0;
			for (size_t p = nArrow; p < nBox && p != std::string::npos; ++p)
			{
				if (strEnums[p] == ',') { ++nIndex; }
			}
			Check(nIndex == defaults.FolderMarginStyle(),
				"STYLE_TREE_BOX is index " + std::to_string(nIndex)
					+ ", core/ says " + std::to_string(defaults.FolderMarginStyle()));
		}

		Check(DeclaredDefault(strHeader, "m_nLongLineMaximum")
			== std::to_string(defaults.LongLineColumnLimit()),
			"m_nLongLineMaximum default matches");
	}

	//----------------------------------------------------------------------
	// 2. The file format, in the shape JSonWriter actually produces: every
	//    setting nested under the root name.
	//----------------------------------------------------------------------
	{
		const std::string strJson =
			"{ \"VinaText Setting\": {"
			"  \"EnableUrlHighlight\": false,"
			"  \"EnableAutoComplete\": false,"
			"  \"AutoCompleteIgnoreCase\": false,"
			"  \"UseFolderMarginClassic\": true,"
			"  \"FolderMarginStyle\": 1,"
			"  \"LongLineColumnLimitation\": 120"
			"} }";
		Core::CAppSettings settings;
		std::string strError;
		Check(settings.LoadFromString(strJson, strError), "a well-formed file loads: " + strError);
		Check(settings.WasLoaded(), "WasLoaded is true after a real load");
		Check(!settings.EnableUrlHighlight(), "EnableUrlHighlight read as false");
		Check(!settings.EnableAutoComplete(), "EnableAutoComplete read as false");
		Check(!settings.AutoCompleteIgnoreCase(), "AutoCompleteIgnoreCase read as false");
		Check(settings.UseFolderMarginClassic(), "UseFolderMarginClassic read as true");
		Check(settings.FolderMarginStyle() == 1, "FolderMarginStyle read as 1");
		Check(settings.LongLineColumnLimit() == 120, "LongLineColumnLimitation read as 120");
		// Absent keys keep their defaults rather than becoming false or zero.
		Check(settings.DrawCaretLineFrame(), "an absent key keeps its default");
		Check(settings.AutoCompleteIgnoreNumbers(), "and so does another");
	}

	//----------------------------------------------------------------------
	// 3. The key really is LongLineColumnLimitation. Spelling it after the
	//    member - LongLineMaximum - parses cleanly, finds nothing, and keeps
	//    the default: a wrong answer that leaves no trace. Asserted so that
	//    the day someone "tidies" the key, this says so.
	//----------------------------------------------------------------------
	{
		Core::CAppSettings settings;
		std::string strError;
		settings.LoadFromString(
			"{ \"VinaText Setting\": { \"LongLineMaximum\": 999 } }", strError);
		Check(settings.LongLineColumnLimit() == 80,
			"the member name is NOT the key - LongLineMaximum is ignored");
	}

	//----------------------------------------------------------------------
	// 4. Malformed and absent input.
	//----------------------------------------------------------------------
	{
		Core::CAppSettings settings;
		std::string strError;

		Check(!settings.LoadFromString("not json", strError), "garbage is rejected");
		Check(!settings.LoadFromString("[1,2,3]", strError), "a non-object root is rejected");
		Check(!settings.LoadFromString("{ \"Other\": {} }", strError),
			"a file without the root object is rejected");

		// A key of the wrong type is ignored, not read as false. A corrupt entry
		// silently disabling a feature looks exactly like the user disabling it.
		Core::CAppSettings typed;
		Check(typed.LoadFromString(
			"{ \"VinaText Setting\": { \"EnableUrlHighlight\": \"yes\","
			" \"LongLineColumnLimitation\": \"120\" } }", strError),
			"a wrongly-typed value does not fail the load");
		Check(typed.EnableUrlHighlight(), "a wrongly-typed bool keeps its default");
		Check(typed.LongLineColumnLimit() == 80, "a wrongly-typed int keeps its default");

		// An ABSENT file is not an error: a fresh install has never saved, and a
		// macOS or Linux user may never have run the MFC application.
		Core::CAppSettings missing;
		Check(missing.LoadFromFile(strRoot + "/no-such-settings-file.json", strError),
			"an absent settings file is not an error");
		Check(!missing.WasLoaded(), "and WasLoaded reports that nothing was read");
		Check(missing.EnableUrlHighlight() && missing.LongLineColumnLimit() == 80,
			"every default survives an absent file");
	}

	//----------------------------------------------------------------------
	// 5. Saving preserves every key this class does not understand.
	//    CAppSettings::SaveSettingData writes 84 keys; this class knows 10. A
	//    save that serialised only its own would discard the other 74 the first
	//    time a macOS user ticked a checkbox in a file their Windows install
	//    shares. This is the check that says it does not.
	//----------------------------------------------------------------------
	{
		const std::string strPath = strRoot + "/core-appsettings-roundtrip.json";
		{
			std::ofstream seed(strPath.c_str(), std::ios::binary);
			seed << "{\n \"VinaText Setting\": {\n"
				"  \"EnableUrlHighlight\": true,\n"
				"  \"CompilerPathCPP\": \"/usr/bin/g++\",\n"
				"  \"LanguageSpellCheck\": \"en-GB\",\n"
				"  \"FilePreviewSizeLimit\": 4096,\n"
				"  \"SomeFutureSetting\": {\"nested\": [1, 2, 3]}\n"
				" },\n \"AnotherRoot\": { \"x\": 1 }\n}";
		}

		Core::CAppSettings settings;
		std::string strError;
		Check(settings.LoadFromFile(strPath, strError), "seeded file loads: " + strError);
		settings.SetEnableUrlHighlight(false);
		settings.SetLongLineColumnLimit(42);
		Check(settings.SaveToFile(strPath, strError), "saves: " + strError);

		// Re-read the raw text: the four keys core/ has never heard of, and the
		// sibling root object, must all still be there.
		std::ifstream back(strPath.c_str(), std::ios::binary);
		const std::string strAfter((std::istreambuf_iterator<char>(back)),
			std::istreambuf_iterator<char>());
		back.close();

		// Compared after stripping backslashes: picojson serialises '/' as "\/",
		// which is legal JSON and which the MFC's own JSonWriter produces too,
		// since both use the same picojson. So the VALUE survives even though
		// these bytes differ from the seed, and comparing raw text here would
		// fail on an escaping convention rather than on any data loss. Learned
		// by writing the raw comparison first and watching it fail on
		// "/usr/bin/g++".
		std::string strFlat = strAfter;
		strFlat.erase(std::remove(strFlat.begin(), strFlat.end(), '\\'), strFlat.end());

		const char* aPreserved[] = { "CompilerPathCPP", "/usr/bin/g++",
			"LanguageSpellCheck", "en-GB", "FilePreviewSizeLimit",
			"SomeFutureSetting", "nested", "AnotherRoot" };
		for (size_t i = 0; i < sizeof(aPreserved) / sizeof(aPreserved[0]); ++i)
		{
			Check(strFlat.find(aPreserved[i]) != std::string::npos,
				std::string("saving preserves an unknown key: ") + aPreserved[i]);
		}

		// And what we did change round-trips.
		Core::CAppSettings reread;
		Check(reread.LoadFromFile(strPath, strError), "the saved file re-loads");
		Check(!reread.EnableUrlHighlight(), "the changed bool round-trips");
		Check(reread.LongLineColumnLimit() == 42, "the changed int round-trips");
		Check(reread.DrawCaretLineFrame(), "an untouched setting keeps its value");

		// Saving twice must produce identical bytes. Without that the two
		// frontends would rewrite each other's file on every save, and a shared
		// settings file would churn forever.
		Core::CAppSettings again;
		Check(again.LoadFromFile(strPath, strError), "re-load for the idempotence check");
		Check(again.SaveToFile(strPath, strError), "second save");
		std::ifstream twice(strPath.c_str(), std::ios::binary);
		const std::string strTwice((std::istreambuf_iterator<char>(twice)),
			std::istreambuf_iterator<char>());
		twice.close();
		Check(strTwice == strAfter, "saving twice is byte-identical");

		// An unparseable file is REFUSED, not overwritten: far more likely to be
		// someone's settings plus a typo than something safe to replace.
		{
			std::ofstream broken(strPath.c_str(), std::ios::binary);
			broken << "{ this is not json";
		}
		Core::CAppSettings refuses;
		Check(!refuses.SaveToFile(strPath, strError),
			"an unparseable settings file is not overwritten");
		std::ifstream check(strPath.c_str(), std::ios::binary);
		const std::string strStill((std::istreambuf_iterator<char>(check)),
			std::istreambuf_iterator<char>());
		Check(strStill == "{ this is not json", "and its contents survive");
		check.close();
		std::remove(strPath.c_str());
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
