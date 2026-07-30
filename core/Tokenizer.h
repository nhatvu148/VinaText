/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Splits a string on any of a set of delimiter characters.
//
// core/ code: UI-free and portable, no MFC, no Win32, no Qt.
//
// WHAT THIS REPLACES
//
// src/LexerParser.cpp's CLexingParser. Despite the file name it was not a lexer
// and did no language parsing - it was a generic character-delimited tokenizer,
// and its only two call sites split a '|'-separated file-extension list in
// CEditorCtrl::GetLexerNameFromExtension.
//
// WHY THE SURFACE IS SMALLER THAN THE ORIGINAL
//
// CLexingParser exposed ten methods. Three were reachable - the two-argument
// constructor, HasMoreTokens and Next - and the other seven had no caller
// anywhere in the tree. The unreachable ones are not ported, because porting
// them would have meant choosing between reproducing two defects and silently
// changing behaviour nobody depended on:
//
//   1. NextInt/NextLong/NextFloat/NextDouble/NextBool all parsed via
//      ::sscanf_s((LPCSTR)(LPCTSTR)strReturn, ...). Under _UNICODE - which is
//      how VinaText builds - LPCTSTR is const wchar_t*, so the cast reinterprets
//      a UTF-16 buffer as narrow characters. Every ASCII digit is followed by a
//      zero byte, which is a NUL terminator to sscanf, so parsing stopped after
//      the first character: NextInt(L"42") returned 4 and NextDouble(L"3.5")
//      returned 3.
//   2. NextString(bRemoveQuotes = TRUE) could not terminate. Its post-quote
//      scan (LexerParser.cpp:93-105) advanced m_nPosition only when the current
//      character matched a delimiter, so any other character spun the loop
//      forever on the same index.
//
// Both were latent, not live: nothing called these methods. They are recorded
// here rather than in a commit message because the reason this class is three
// methods instead of ten is not otherwise recoverable.
//
// BEHAVIOUR PRESERVED FROM CLexingParser::Next
//
// Exactly, including the parts that look like oversights:
//   - a delimiter run yields empty tokens ("a||b" -> "a", "", "b")
//   - a leading delimiter yields a leading empty token ("|a" -> "", "a")
//   - a TRAILING delimiter yields no trailing empty token ("a|" -> "a"), because
//     consuming it leaves the position at the end and HasMoreTokens goes false
//   - an empty delimiter set returns the whole string as one token
// core/tests/TestTokenizer.cpp pins all four.

#pragma once

#include <string>

namespace Core
{
	class CTokenizer final
	{
	public:
		CTokenizer(const std::wstring& strText, const std::wstring& strDelimiters);

		// True while any input remains. Note that this is a statement about the
		// INPUT, not about tokens: after consuming the '|' of L"a|", the position
		// is at the end and this is false, so no trailing empty token appears.
		bool HasMoreTokens() const;

		// Returns the next token and consumes the delimiter that ended it. The
		// token is empty when two delimiters are adjacent.
		std::wstring Next();

	private:
		std::wstring m_Text;
		std::wstring m_Delimiters;
		std::wstring::size_type m_nPosition;
	};
}
