/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Exercises core/TextTransform.h, the boost/algorithm/string replacements.
//
// These assert the behaviour that Boost had at the call sites being replaced,
// including the parts that are easy to "improve" by accident:
//
//   - the default locale is the classic one, so casing is ASCII-only. A
//     replacement that lowercases Latin-1 or Greek would be a silent behaviour
//     change in EditorView.cpp's hash_tab.find(wordCase) lookups.
//   - Trim classifies with std::isspace(c, loc), not iswspace.
//   - Join on an empty vector yields an empty string, not a separator.
//
// Build and run (one line):
//   c++ -std=c++11 -I core core/tests/TestTextTransform.cpp -o testtexttransform && ./testtexttransform

#include "TextTransform.h"

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
			std::cout << "  FAIL: " << strWhat << "\n        actual   = [" << actual
				<< "]\n        expected = [" << expected << "]\n";
		}
	}
}

int main()
{
	// --- narrow casing ---
	{
		std::string s = "MiXeD CaSe 123";
		CheckEqual(Core::ToLower(s), "mixed case 123", "ToLower, narrow");

		std::string u = "MiXeD CaSe 123";
		CheckEqual(Core::ToUpper(u), "MIXED CASE 123", "ToUpper, narrow");

		std::string already = "lower";
		CheckEqual(Core::ToLower(already), "lower", "ToLower leaves lowercase alone");

		std::string empty = "";
		CheckEqual(Core::ToLower(empty), "", "ToLower on an empty string");
	}

	// --- wide casing: the type every replaced call site actually uses ---
	{
		std::wstring w = L"MiXeD";
		Check(Core::ToLower(w) == L"mixed", "ToLower, wide");

		std::wstring wu = L"MiXeD";
		Check(Core::ToUpper(wu) == L"MIXED", "ToUpper, wide");
	}

	// --- the classic locale is ASCII-only, and that is deliberate ---
	//
	// Boost did not fold these either. If a future change makes them fold, the
	// hash_tab.find(wordCase) lookups in EditorView.cpp start matching different
	// words - so this asserts the limitation rather than papering over it.
	{
		std::wstring accented = L"ÉÈ";          // É È
		std::wstring copy = accented;
		Core::ToLower(copy);
		Check(copy == accented,
			"ToLower does NOT fold Latin-1 under the classic locale (matches Boost)");

		std::wstring turkish = L"I";
		std::wstring t = turkish;
		Core::ToLower(t);
		Check(t == L"i", "ToLower maps ASCII I to i, with no Turkish special-casing");
	}

	// --- trim ---
	{
		std::wstring s = L"  hello  ";
		Check(Core::Trim(s) == L"hello", "Trim strips both ends");

		std::wstring tabs = L"\t\n hello \r\n";
		Check(Core::Trim(tabs) == L"hello", "Trim handles tabs, newlines and CR");

		std::wstring inner = L"  a b  ";
		Check(Core::Trim(inner) == L"a b", "Trim preserves interior whitespace");

		std::wstring blank = L"   ";
		Check(Core::Trim(blank).empty(), "an all-whitespace string trims to empty");

		std::wstring none = L"hello";
		Check(Core::Trim(none) == L"hello", "Trim leaves a clean string alone");

		std::wstring empty = L"";
		Check(Core::Trim(empty).empty(), "Trim on an empty string is safe");
	}

	// --- join ---
	{
		std::vector<std::wstring> three;
		three.push_back(L"a"); three.push_back(L"b"); three.push_back(L"c");
		Check(Core::Join(three, std::wstring(L", ")) == L"a, b, c", "Join with a separator");

		std::vector<std::wstring> one;
		one.push_back(L"solo");
		Check(Core::Join(one, std::wstring(L", ")) == L"solo",
			"Join of one element adds no separator");

		std::vector<std::wstring> none;
		Check(Core::Join(none, std::wstring(L", ")).empty(),
			"Join of an empty vector is empty, not a stray separator");

		std::vector<std::string> narrow;
		narrow.push_back("x"); narrow.push_back("y");
		CheckEqual(Core::Join(narrow, std::string("-")), "x-y", "Join, narrow");
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
