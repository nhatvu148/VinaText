/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Finds the URLs in a run of text, so a frontend can underline them.
//
// Extracted from src/StringHelper.cpp's seven `*Url*` helpers and the
// AppUtils::IsUrlHyperLink that drives them (src/AppUtil.cpp:565). Demand-driven
// per brief D9: ui-qt/ needs it for AppSettingMgr.m_bEnableUrlHighlight, which
// ships TRUE. src/ is unchanged and still uses its own copy; the differential
// test in core/tests/TestUrlScanner.cpp is what keeps the two in step.
//
// BYTES, NOT WIDE CHARACTERS. The MFC converts the document to UTF-16, scans it,
// and converts each segment's length back to reach a document offset. This works
// on UTF-8 directly, which is what Scintilla indexes anyway - and it is
// equivalent, not merely close: every byte of a multi-byte UTF-8 sequence is
// >= 0x80, and at >= 0x80 all four character classifiers below agree with what
// they say about a non-ASCII wchar_t. So the two agree on every URL boundary,
// and only the offsets they are expressed in differ. TestUrlScanner pins that
// for ASCII exhaustively and for non-ASCII by case.

#pragma once

#include <string>
#include <vector>

namespace Core
{
	// One URL found in the text: a half-open [_Start, _Start + _Length) byte range.
	struct SUrlMatch
	{
		size_t	_Start = 0;
		size_t	_Length = 0;
	};

	class CUrlScanner final
	{
	public:
		// Advances through the text one segment at a time, exactly as the MFC's
		// IsUrlHyperLink does. Returns false once nStart has reached the end.
		//
		// On true, the segment [nStart, nStart + nSegmentLength) has been
		// classified: bIsUrl says whether it is one. The caller advances nStart by
		// nSegmentLength and calls again - a URL segment is underlined and a
		// non-URL segment is not, which is how the original clears stale
		// highlights as it goes.
		//
		// Segments always have non-zero length while this returns true, so a
		// caller that advances by nSegmentLength always terminates.
		static bool NextSegment(const char* szText, size_t nLength, size_t nStart,
			size_t& nSegmentLength, bool& bIsUrl);

		// Every URL in the text, for callers that would rather have the list than
		// drive the walk. Built on NextSegment.
		static void FindAll(const std::string& strText, std::vector<SUrlMatch>& matches);

		// The five schemes the MFC accepts (StringHelper::isUrlSchemeSupported):
		// ftp, http, https, mailto, file. Compared without case, ASCII only.
		//
		// This REPLACES a Win32 call - see the note in the .cpp.
		static bool IsSupportedScheme(const char* szText, size_t nSchemeLength);
	};
}
