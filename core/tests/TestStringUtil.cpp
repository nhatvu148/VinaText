/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Exercises core/StringUtil.h away from Windows.
//
// The point is as much the compile as the assertions: StringUtil.h was carved out of
// src/StringHelper.h, which carried no #includes at all and leaned entirely on
// src/stdafx.h. If anything in it still depends on the god-header - a Win32 type, a
// CRT extension, an include it never declared - this file will not build on
// macOS/Linux, which is exactly the signal Phase 1 needs.
//
// Build and run (one line - a trailing backslash inside a // comment trips -Wcomment):
//   c++ -std=c++11 -I core core/tests/TestStringUtil.cpp -o teststringutil && ./teststringutil

#include "StringUtil.h"

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

	void CheckEqual(const std::string& actual, const std::string& expected,
		const std::string& strWhat)
	{
		++g_Checks;
		if (actual != expected)
		{
			++g_Failures;
			std::cout << "  FAIL: " << strWhat << "\n"
				<< "        actual   = [" << actual << "]\n"
				<< "        expected = [" << expected << "]\n";
		}
	}
}

int main()
{
	typedef Core::CStringUtil SU;

	// --- narrow trim family ---
	{
		std::string s = "  hello  ";
		CheckEqual(SU::trim(s), "hello", "trim strips both ends");

		std::string l = "  hello";
		CheckEqual(SU::ltrim(l), "hello", "ltrim strips leading");

		std::string r = "hello  ";
		CheckEqual(SU::rtrim(r), "hello", "rtrim strips trailing");

		std::string none = "hello";
		CheckEqual(SU::trim(none), "hello", "trim leaves a clean string alone");

		std::string all = "   ";
		CheckEqual(SU::trim(all), "", "an all-space string trims to empty");

		std::string empty = "";
		CheckEqual(SU::trim(empty), "", "empty string is safe");

		std::string inner = "  a b  ";
		CheckEqual(SU::trim(inner), "a b", "interior whitespace is preserved");
	}

	// --- trim with an explicit character set ---
	{
		std::string s = "xxhelloxx";
		CheckEqual(SU::trim(s, std::string("x")), "hello", "trim with a char set");

		std::string c = "--hello--";
		CheckEqual(SU::trim(c, static_cast<wint_t>('-')), "hello", "trim with a single char");
	}

	// --- wide overloads resolve and behave ---
	{
		std::wstring w = L"  wide  ";
		Check(SU::trim(w) == L"wide", "wide trim");

		std::wstring wl = L"\t\twide";
		Check(SU::ltrim(wl) == L"wide", "wide ltrim handles tabs");
	}

	// Deliberately not tested here: find_caseinsensitive (MSVC _wcsnicmp) and the wide
	// to_lower (Win32 LCMapStringEx) stayed in src/StringHelper.h. The narrow to_lower
	// is portable but carries [[deprecated]], and this file builds with -Werror.

	// --- the Trim* templates ---
	{
		std::string s = "xxyyhelloyyxx";
		SU::TrimLeading(s, std::string("xy"));
		CheckEqual(s, "helloyyxx", "TrimLeading removes any leading char in the set");

		std::string t = "xxyyhelloyyxx";
		SU::TrimTrailing(t, std::string("xy"));
		CheckEqual(t, "xxyyhello", "TrimTrailing removes any trailing char in the set");

		std::string u = "xxyyhelloyyxx";
		SU::TrimLeadingAndTrailing(u, std::string("xy"));
		CheckEqual(u, "hello", "TrimLeadingAndTrailing does both");
	}

	// --- Split, including the quirk that must not be "fixed" ---
	{
		const std::vector<std::string> one = SU::Split("a b c", " ");
		Check(one.size() == 3 && one[0] == "a" && one[1] == "b" && one[2] == "c",
			"single-character delimiter splits cleanly");

		// A multi-character delimiter advances by one char, so every token after the
		// first keeps the delimiter's tail. CWebHandler::ResultParser relies on this -
		// see the comment on Split. Pinned here so a well-meaning correction fails
		// loudly instead of silently breaking the translate feature.
		const std::vector<std::string> two = SU::Split("x[\"alpha[\"beta", "[\"");
		Check(two.size() == 3, "multi-char delimiter yields 3 tokens");
		if (two.size() == 3)
		{
			CheckEqual(two[0], "x", "multi-char: first token is clean");
			CheckEqual(two[1], "\"alpha", "multi-char: token keeps the stray quote (INTENTIONAL)");
			CheckEqual(two[2], "\"beta", "multi-char: last token keeps the stray quote (INTENTIONAL)");
		}

		const std::vector<std::string> none = SU::Split("abc", ",");
		Check(none.size() == 1 && none[0] == "abc", "no delimiter yields the whole string");

		const std::vector<std::wstring> wide = SU::Split(std::wstring(L"a;b"), std::wstring(L";"));
		Check(wide.size() == 2 && wide[0] == L"a" && wide[1] == L"b", "wide Split");
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
