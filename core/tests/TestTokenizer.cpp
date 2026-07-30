/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Pins core/Tokenizer.{h,cpp} to CLexingParser::Next's exact behaviour.
//
// The interesting cases are the ones that look like oversights in the original.
// They are asserted rather than tidied up, because CEditorCtrl::GetLexerNameFromExtension
// compares every token - including the empty ones - against a file extension, so
// "improving" the empty-token handling would change which extensions match.
//
// Build and run (one line):
//   c++ -std=c++11 -I core core/Tokenizer.cpp core/tests/TestTokenizer.cpp -o testtokenizer && ./testtokenizer

#include "Tokenizer.h"

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

	// Drains a tokenizer the way the call site does: while (HasMoreTokens()) Next().
	std::vector<std::wstring> Drain(const std::wstring& strText, const std::wstring& strDelims)
	{
		std::vector<std::wstring> tokens;
		Core::CTokenizer tokenizer(strText, strDelims);
		while (tokenizer.HasMoreTokens())
		{
			tokens.push_back(tokenizer.Next());
		}
		return tokens;
	}

	void CheckTokens(const std::wstring& strText, const std::wstring& strDelims,
		const std::vector<std::wstring>& expected, const std::string& strWhat)
	{
		++g_Checks;
		const std::vector<std::wstring> actual = Drain(strText, strDelims);
		if (actual == expected)
		{
			return;
		}
		++g_Failures;
		std::cout << "  FAIL: " << strWhat << "\n        actual   = " << actual.size()
			<< " token(s):";
		for (size_t i = 0; i < actual.size(); ++i)
		{
			std::cout << " [" << std::string(actual[i].begin(), actual[i].end()) << "]";
		}
		std::cout << "\n        expected = " << expected.size() << " token(s):";
		for (size_t i = 0; i < expected.size(); ++i)
		{
			std::cout << " [" << std::string(expected[i].begin(), expected[i].end()) << "]";
		}
		std::cout << "\n";
	}

	std::vector<std::wstring> List(const wchar_t* a = 0, const wchar_t* b = 0,
		const wchar_t* c = 0, const wchar_t* d = 0)
	{
		std::vector<std::wstring> v;
		if (a) v.push_back(a);
		if (b) v.push_back(b);
		if (c) v.push_back(c);
		if (d) v.push_back(d);
		return v;
	}
}

int main()
{
	// --- the ordinary case ---
	{
		CheckTokens(L"a|b|c", L"|", List(L"a", L"b", L"c"), "splits on a single delimiter");
		CheckTokens(L"solo", L"|", List(L"solo"), "a string with no delimiter is one token");
	}

	// --- empty tokens: preserved deliberately, see the header ---
	{
		std::vector<std::wstring> withEmpty;
		withEmpty.push_back(L"a"); withEmpty.push_back(L""); withEmpty.push_back(L"b");
		CheckTokens(L"a||b", L"|", withEmpty, "adjacent delimiters yield an empty token");

		std::vector<std::wstring> leading;
		leading.push_back(L""); leading.push_back(L"a");
		CheckTokens(L"|a", L"|", leading, "a leading delimiter yields a leading empty token");

		// The asymmetry with the leading case is real. Consuming the trailing '|'
		// leaves the position at the end, so HasMoreTokens is false and the loop
		// never asks for the token that would have been empty.
		CheckTokens(L"a|", L"|", List(L"a"), "a TRAILING delimiter yields no trailing empty token");
	}

	// --- degenerate inputs ---
	{
		CheckTokens(L"", L"|", List(), "an empty string yields no tokens at all");
		CheckTokens(L"a|b", L"", List(L"a|b"), "an empty delimiter set returns the whole string");
		CheckTokens(L"|", L"|", List(L""), "a lone delimiter yields exactly one empty token");
	}

	// --- a delimiter SET, not a delimiter string ---
	{
		std::vector<std::wstring> mixed;
		mixed.push_back(L"a"); mixed.push_back(L"b"); mixed.push_back(L"c");
		CheckTokens(L"a b,c", L" ,", mixed, "any character in the set splits");
	}

	// --- non-ASCII passes through untouched ---
	{
		std::vector<std::wstring> wide;
		wide.push_back(L"éè"); wide.push_back(L"中文");
		CheckTokens(L"éè|中文", L"|", wide,
			"non-ASCII characters are copied through, not classified");
	}

	// --- the real call site ---
	//
	// This is the longest entry of EditorCommonDef.h's arrLangExtensions, drained
	// the way CEditorCtrl::GetLexerNameFromExtension drains it.
	{
		const std::wstring strMsBuild =
			L"vcxproj|filters|user|vcproj|csproj|csxproj|vbproj|dbproj|sln";
		const std::vector<std::wstring> tokens = Drain(strMsBuild, L"|");
		Check(tokens.size() == 9, "the MSBuild extension list yields 9 tokens");
		Check(!tokens.empty() && tokens.front() == L"vcxproj", "first token is vcxproj");
		Check(!tokens.empty() && tokens.back() == L"sln", "last token is sln");

		bool bFoundCsproj = false;
		for (size_t i = 0; i < tokens.size(); ++i)
		{
			if (tokens[i] == L"csproj") bFoundCsproj = true;
		}
		Check(bFoundCsproj, "a mid-list extension is reachable");

		// A single-extension entry, which is what most rows of the table are.
		CheckTokens(L"py", L"|", List(L"py"), "a one-extension row yields one token");
	}

	// --- termination, the property the original's unused quote path lacked ---
	{
		std::wstring strAdversarial;
		for (int i = 0; i < 500; ++i)
		{
			strAdversarial += L"|";
		}
		Core::CTokenizer tokenizer(strAdversarial, L"|");
		int nDrained = 0;
		while (tokenizer.HasMoreTokens() && nDrained < 100000)
		{
			tokenizer.Next();
			++nDrained;
		}
		Check(nDrained == 500, "500 consecutive delimiters drain in exactly 500 steps");
		Check(!tokenizer.HasMoreTokens(), "and leave the tokenizer exhausted");
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
