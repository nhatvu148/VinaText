/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Drop-in replacements for the four boost/algorithm/string functions this
// codebase uses. core/ code: UI-free and portable, no MFC, no Win32, no Qt.
//
// WHY THESE ARE NOT "EQUIVALENTS"
//
// These are constructed to perform the *same operation* as Boost, not merely a
// similar one. boost::to_lower is defined as
//
//     std::transform(begin, end, out, [loc](Ch c) { return std::tolower(c, loc); })
//
// with loc defaulting to std::locale() - the C++ global locale. Each function
// below is that same transform against that same default-constructed locale, so
// the result is identical by construction rather than by argument. That matters
// because the alternative - reaching for towlower or an ASCII table - silently
// changes behaviour for any character outside A-Z.
//
// WHAT THE LOCALE ACTUALLY IS HERE
//
// std::locale() is the C++ global locale, which is the classic locale unless
// std::locale::global() is called. Nothing in this codebase calls it.
// CommandLine.cpp does call _tsetlocale(LC_ALL, ""), but that sets the C runtime
// locale and does not move the C++ global one. So these operations are
// effectively ASCII-only today - and, crucially, they were already ASCII-only
// under Boost. Preserving that is the point.
//
// Two call sites feed the result into hash_tab.find(wordCase) in EditorView.cpp,
// so a change in casing behaviour would change which words are found. Keeping
// the transform identical keeps that lookup identical.

#pragma once

#include <algorithm>
#include <locale>
#include <string>
#include <vector>

namespace Core
{
	// Lowercases in place. Same operation as boost::to_lower.
	template <class TString>
	TString& ToLower(TString& s)
	{
		const std::locale loc;
		std::transform(s.begin(), s.end(), s.begin(),
			[&loc](typename TString::value_type c)
			{ return std::tolower<typename TString::value_type>(c, loc); });
		return s;
	}

	// Uppercases in place. Same operation as boost::to_upper.
	template <class TString>
	TString& ToUpper(TString& s)
	{
		const std::locale loc;
		std::transform(s.begin(), s.end(), s.begin(),
			[&loc](typename TString::value_type c)
			{ return std::toupper<typename TString::value_type>(c, loc); });
		return s;
	}

	// Trims locale-defined whitespace from both ends, in place. Same operation as
	// boost::trim, which classifies with std::isspace(c, loc) - NOT the same as
	// Core::CStringUtil::trim, which uses iswspace and can differ outside ASCII.
	// Kept separate for exactly that reason.
	template <class TString>
	TString& Trim(TString& s)
	{
		typedef typename TString::value_type TChar;
		const std::locale loc;
		const auto isSpace = [&loc](TChar c) { return std::isspace<TChar>(c, loc); };

		auto itFirst = std::find_if(s.begin(), s.end(),
			[&isSpace](TChar c) { return !isSpace(c); });
		auto itLast = std::find_if(s.rbegin(), s.rend(),
			[&isSpace](TChar c) { return !isSpace(c); }).base();

		if (itFirst >= itLast)
		{
			s.clear();
		}
		else
		{
			s = TString(itFirst, itLast);
		}
		return s;
	}

	// Concatenates with a separator between elements. Same result as
	// boost::algorithm::join, including the empty-input case, which yields an
	// empty string rather than a stray separator.
	template <class TString>
	TString Join(const std::vector<TString>& fields, const TString& strSeparator)
	{
		TString out;
		for (size_t i = 0; i < fields.size(); ++i)
		{
			if (i != 0)
			{
				out += strSeparator;
			}
			out += fields[i];
		}
		return out;
	}
}
