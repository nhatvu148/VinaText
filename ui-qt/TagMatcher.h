/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Finds the XML/HTML tag pair enclosing the caret.
//
// A transcription of CEditorCtrl::GetXmlHtmlTagsPosition and its four search
// helpers (src/Editor.cpp:1251-1783). Kept in its own file rather than folded
// into CEditorWidget because it is ~250 lines that talk to nothing but Scintilla
// messages, and because that makes it testable against a document without a
// window.
//
// NOT in core/: it is written entirely in Scintilla messages and SCE_H_* style
// constants, and core/ takes no third-party dependency (brief D6).
//
// The algorithm is unchanged from the original, including its documented
// awkward cases - nested same-name tags, self-closing tags, a '>' inside an
// attribute value, and CDATA. The comments explaining those are the original's.

#pragma once

#include <string>

namespace TagMatch
{
	// A half-open [_Start, _End) range in the document, or _Success == false.
	struct SResult
	{
		long long	_Start = 0;
		long long	_End = 0;
		bool		_Success = false;
	};

	// The four positions the highlighter paints. _TagCloseStart and _TagCloseEnd
	// are -1 for a self-closing tag, which has no separate close.
	struct STagPositions
	{
		long long	_TagOpenStart = 0;
		long long	_TagNameEnd = 0;
		long long	_TagOpenEnd = 0;
		long long	_TagCloseStart = 0;
		long long	_TagCloseEnd = 0;
	};

	// Everything here needs is the ability to send a message to one document.
	// A plain function pointer plus context rather than std::function: this runs
	// on every caret movement in an XML file, and the search loops send several
	// messages per character examined.
	struct SDocument
	{
		typedef long long (*FnSend)(void* pContext, unsigned int nMessage,
			unsigned long long wParam, long long lParam);

		FnSend	_Send = nullptr;
		void*	_Context = nullptr;

		long long Send(unsigned int nMessage, unsigned long long wParam = 0,
			long long lParam = 0) const
		{
			return _Send(_Context, nMessage, wParam, lParam);
		}
	};

	// Fills positions and returns true when the caret sits inside a tag whose
	// partner can be found.
	//
	// On false, positions may still have been PARTIALLY written - the close-tag
	// path records where the close tag was before it goes looking for the open
	// one, and can then fail. The original behaves the same way and its caller
	// reads the struct only when this returns true; do the same.
	bool FindEnclosingTag(const SDocument& document, STagPositions& positions);
}
