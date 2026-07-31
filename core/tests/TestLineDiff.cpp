/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Pins core/LineDiff.{h,cpp} to CDiffEngine::Diff's behaviour.
//
// Equivalence was established separately, by differential test against a verbatim
// transcription of the original over 124,012 file pairs (see the PR). What this
// file adds is the readable version: the specific outputs a maintainer needs to
// see to know what the algorithm does, plus the regression for the
// non-termination the port fixes.
//
// Build and run (one line):
//   c++ -std=c++11 -I core core/LineDiff.cpp core/tests/TestLineDiff.cpp -o testlinediff && ./testlinediff

#include "LineDiff.h"

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

	// One character per line, so a whole file is one readable literal.
	std::vector<std::wstring> Lines(const wchar_t* szChars)
	{
		std::vector<std::wstring> lines;
		for (const wchar_t* p = szChars; *p != L'\0'; ++p)
		{
			lines.push_back(std::wstring(1, *p));
		}
		return lines;
	}

	Core::SDiffSide Side(const wchar_t* szChars)
	{
		Core::SDiffSide side;
		side._Raw = Lines(szChars);
		side._Compare = side._Raw;
		return side;
	}

	// Renders one output side as "text:status" pairs, blanks shown as '.',
	// so an expectation is a single readable string.
	std::string Render(const std::vector<Core::SDiffLine>& out)
	{
		static const char* szNames[] = { "n", "c", "a", "d" };
		std::string strResult;
		for (size_t i = 0; i < out.size(); ++i)
		{
			if (i != 0)
			{
				strResult += " ";
			}
			strResult += out[i]._Text.empty()
				? std::string(".")
				: std::string(out[i]._Text.begin(), out[i]._Text.end());
			strResult += szNames[out[i]._Status];
		}
		return strResult;
	}

	void CheckDiff(const wchar_t* szF1, const wchar_t* szF2,
		const std::string& strExpect1, const std::string& strExpect2,
		const std::string& strWhat)
	{
		++g_Checks;
		std::vector<Core::SDiffLine> out1, out2;
		if (!Core::DiffLines(Side(szF1), Side(szF2), out1, out2))
		{
			++g_Failures;
			std::cout << "  FAIL: " << strWhat << " (Diff returned false)\n";
			return;
		}
		const std::string strActual1 = Render(out1);
		const std::string strActual2 = Render(out2);
		if (strActual1 == strExpect1 && strActual2 == strExpect2)
		{
			return;
		}
		++g_Failures;
		std::cout << "  FAIL: " << strWhat
			<< "\n        left  actual   = [" << strActual1 << "]"
			<< "\n        left  expected = [" << strExpect1 << "]"
			<< "\n        right actual   = [" << strActual2 << "]"
			<< "\n        right expected = [" << strExpect2 << "]\n";
	}
}

int main()
{
	// --- the four things a diff is for ---
	{
		CheckDiff(L"ABC", L"ABC", "An Bn Cn", "An Bn Cn", "identical files are all Normal");
		CheckDiff(L"AC", L"ABC", "An .n Cn", "An Ba Cn", "an inserted line is Added on the right");
		CheckDiff(L"ABC", L"AC", "An Bd Cn", "An .n Cn", "a removed line is Deleted on the left");
		CheckDiff(L"ABC", L"AXC", "An Bc Cn", "An Xc Cn", "a substituted line is Changed on both");
	}

	// --- the asymmetry, which is deliberate ---
	//
	// A blank inserted opposite an Added line is Normal, not Added. The status
	// belongs to the line that exists; the padding is just padding. The HTML
	// renderer in ui-mfc/ relies on this to colour only one side of a row.
	{
		std::vector<Core::SDiffLine> out1, out2;
		Core::DiffLines(Side(L"AC"), Side(L"ABC"), out1, out2);
		Check(out1.size() == out2.size(), "both sides are always the same length");
		Check(out1[1]._Status == Core::LineNormal && out1[1]._Text.empty(),
			"the pad opposite an Added line is a blank with status Normal");
		Check(out2[1]._Status == Core::LineAdded, "and the real line carries Added");
	}

	// --- empty inputs, including the original's special case ---
	//
	// An empty left file marks every right-hand line Normal rather than Added.
	// That is the `if (nbf1Lines==0)` early return in the original, and it
	// disagrees with what the general path would produce - preserved as-is.
	{
		CheckDiff(L"", L"AB", ".n .n", "An Bn",
			"against an empty left file, lines are Normal - NOT Added");
		CheckDiff(L"AB", L"", "Ad Bd", ".n .n", "against an empty right file, lines are Deleted");
		CheckDiff(L"", L"", "", "", "two empty files produce no rows");
	}

	// --- regression: the non-termination this port fixes ---
	//
	// CDiffEngine::Diff spins forever on these two three-line files, at
	// (i=2, nf2CurrentLine=1), emitting nothing and allocating nothing. See the
	// (itmp > i) guard in DiffEngine.cpp. If this test hangs rather than fails,
	// that guard has been removed.
	{
		CheckDiff(L"ABA", L"BBA", "Ad Bn .n An", ".n Bn Ba An",
			"ABA vs BBA terminates (the original does not)");

		// The same shape at greater length, to catch a fix that only handles
		// the three-line case.
		CheckDiff(L"ABAB", L"BBAB", "Ad Bn .n An Bn", ".n Bn Ba An Bn",
			"ABAB vs BBAB terminates too");
	}

	// --- _Compare drives matching, _Raw drives output ---
	//
	// This is how ignore-case and ignore-indentation reach the algorithm without
	// core/ knowing anything about them: the frontend filters into _Compare and
	// leaves _Raw as the text to display.
	{
		Core::SDiffSide left, right;
		left._Raw = Lines(L"AB");
		left._Compare = Lines(L"ab");		// filtered: lowercased
		right._Raw = Lines(L"aB");
		right._Compare = Lines(L"ab");

		std::vector<Core::SDiffLine> out1, out2;
		Check(Core::DiffLines(left, right, out1, out2), "a filtered comparison runs");
		Check(Render(out1) == "An Bn" && Render(out2) == "an Bn",
			"lines equal only after filtering are Normal, and each side shows its OWN raw text");

		// Without the filtering, the same files differ.
		CheckDiff(L"AB", L"aB", "Ac Bn", "ac Bn",
			"and unfiltered, those same lines are Changed");
	}

	// --- malformed input is rejected rather than read out of bounds ---
	{
		Core::SDiffSide broken;
		broken._Raw = Lines(L"ABC");
		broken._Compare = Lines(L"AB");		// one short

		std::vector<Core::SDiffLine> out1, out2;
		Check(!Core::DiffLines(broken, Side(L"ABC"), out1, out2),
			"a side whose _Raw and _Compare disagree in length is rejected");
		Check(!Core::DiffLines(Side(L"ABC"), broken, out1, out2),
			"on either side");
	}

	// --- repeated lines, where a naive forward search goes wrong ---
	{
		CheckDiff(L"AAA", L"AAA", "An An An", "An An An", "a file of identical lines matches itself");
		CheckDiff(L"AAA", L"AA", "An An Ad", "An An .n", "and one shorter is a delete at the end");
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
