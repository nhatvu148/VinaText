/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "UrlScanner.h"

#include <vector>

namespace Core
{
	namespace
	{
		// The four character classifiers, from src/StringHelper.cpp:87-129.
		// Unsigned char throughout: `char` is signed on every platform this
		// builds for, so a UTF-8 continuation byte would compare as negative and
		// take the wrong branch of every range test below.

		bool IsSchemeStartChar(unsigned char c)
		{
			return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
		}

		// What may appear immediately BEFORE a scheme. Anything that is not
		// alphanumeric or '_', so that "nothttp://x" does not start a URL at
		// "http".
		bool IsSchemeDelimiter(unsigned char c)
		{
			return !(((c >= '0') && (c <= '9'))
				|| ((c >= 'A') && (c <= 'Z'))
				|| ((c >= 'a') && (c <= 'z'))
				|| (c == '_'));
		}

		bool IsUrlTextChar(unsigned char c)
		{
			if (c <= ' ')
			{
				return false;
			}
			switch (c)
			{
			case '"':
			case '#':
			case '\'':
			case '<':
			case '>':
			case '{':
			case '}':
			case '?':
			case 0x7f:
				return false;
			}
			return true;
		}

		bool IsQueryDelimiter(unsigned char c)
		{
			return c == '&' || c == '+' || c == '=' || c == ';';
		}

		unsigned char At(const char* szText, size_t nIndex)
		{
			return static_cast<unsigned char>(szText[nIndex]);
		}

		// StringHelper::scanToUrlStart. Finds the next "scheme:" and reports how
		// far away it is; on failure reports the distance to the end of the text.
		bool ScanToUrlStart(const char* szText, size_t nLength, size_t nStart,
			size_t& nDistance, size_t& nSchemeLength)
		{
			size_t p = nStart;
			size_t p0 = 0;
			enum { sUnknown, sScheme } s = sUnknown;
			while (p < nLength)
			{
				switch (s)
				{
				case sUnknown:
					if (IsSchemeStartChar(At(szText, p))
						&& ((p == 0) || IsSchemeDelimiter(At(szText, p - 1))))
					{
						p0 = p;
						s = sScheme;
					}
					break;

				case sScheme:
					if (At(szText, p) == ':')
					{
						nDistance = p0 - nStart;
						nSchemeLength = p - p0 + 1;
						return true;
					}
					if (!IsSchemeStartChar(At(szText, p)))
					{
						s = sUnknown;
					}
					break;
				}
				++p;
			}
			nSchemeLength = 0;
			nDistance = p - nStart;
			return false;
		}

		// StringHelper::scanToUrlEnd. Coarsely parses host-and-path, query and
		// fragment. The query pattern is deliberately loose - the objective is to
		// let through most of the real world's queries and reject what is
		// certainly not one, not to diagnose malformed URLs. The original's
		// example of what passes: ?abc;def;fgh="i j k"&'l m n'+opq
		void ScanToUrlEnd(const char* szText, size_t nLength, size_t nStart, size_t& nDistance)
		{
			size_t p = nStart;
			unsigned char q = 0;
			enum { sHostAndPath, sQuery, sQueryAfterDelimiter, sQueryQuotes,
				sQueryAfterQuotes, sFragment } s = sHostAndPath;
			while (p < nLength)
			{
				const unsigned char c = At(szText, p);
				switch (s)
				{
				case sHostAndPath:
					if (c == '?')			{ s = sQuery; }
					else if (c == '#')		{ s = sFragment; }
					else if (!IsUrlTextChar(c))
					{
						nDistance = p - nStart;
						return;
					}
					break;

				case sQuery:
					if (c == '#')					{ s = sFragment; }
					else if (IsQueryDelimiter(c))	{ s = sQueryAfterDelimiter; }
					else if (!IsUrlTextChar(c))
					{
						nDistance = p - nStart;
						return;
					}
					break;

				case sQueryAfterDelimiter:
					if ((c == '\'') || (c == '"'))
					{
						q = c;
						s = sQueryQuotes;
					}
					else if (c == '{')
					{
						q = '}';
						s = sQueryQuotes;
					}
					else if (IsUrlTextChar(c))
					{
						s = sQuery;
					}
					else
					{
						nDistance = p - nStart;
						return;
					}
					break;

				case sQueryQuotes:
					if (c < ' ')
					{
						nDistance = p - nStart;
						return;
					}
					if (c == q)
					{
						s = sQueryAfterQuotes;
					}
					break;

				case sQueryAfterQuotes:
					if (IsQueryDelimiter(c))
					{
						s = sQueryAfterDelimiter;
					}
					else
					{
						nDistance = p - nStart;
						return;
					}
					break;

				case sFragment:
					if (!IsUrlTextChar(c))
					{
						nDistance = p - nStart;
						return;
					}
					break;
				}
				++p;
			}
			nDistance = p - nStart;
		}

		// StringHelper::removeUnwantedTrailingCharFromUrl. Strips ONE unwanted
		// trailing character, so the caller loops until it returns false.
		//
		// "(see http://x/y)" must not swallow the ')', but "http://x/y_(z)" must
		// keep it - hence the counting rather than a plain strip.
		bool RemoveUnwantedTrailingChar(const char* szText, size_t& nLength)
		{
			if (nLength <= 1)
			{
				return false;
			}
			const size_t l = nLength - 1;

			// Punctuation that ends a sentence rather than a URL.
			const char* szSingle = ".,:;?!#";
			for (size_t i = 0; szSingle[i] != '\0'; ++i)
			{
				if (At(szText, l) == static_cast<unsigned char>(szSingle[i]))
				{
					nLength = l;
					return true;
				}
			}

			// A closing bracket is kept only if it is matched inside the URL.
			const char* szClosing = ")]>";
			const char* szOpening = "([<";
			for (size_t i = 0; szClosing[i] != '\0'; ++i)
			{
				if (At(szText, l) == static_cast<unsigned char>(szClosing[i]))
				{
					int nCount = 1;
					for (size_t j = l; j-- > 0; )
					{
						if (At(szText, j) == static_cast<unsigned char>(szClosing[i]))
						{
							++nCount;
						}
						if (At(szText, j) == static_cast<unsigned char>(szOpening[i]))
						{
							--nCount;
						}
					}
					if (nCount == 0)
					{
						return false;
					}
					nLength = l;
					return true;
				}
			}

			// A quote is kept only if an odd number precede it, i.e. it closes one.
			const char* szQuotes = "\"'`";
			for (size_t i = 0; szQuotes[i] != '\0'; ++i)
			{
				if (At(szText, l) == static_cast<unsigned char>(szQuotes[i]))
				{
					int nCount = 0;
					for (size_t j = l; j-- > 0; )
					{
						if (At(szText, j) == static_cast<unsigned char>(szQuotes[i]))
						{
							++nCount;
						}
					}
					if (nCount & 1)
					{
						return false;
					}
					nLength = l;
					return true;
				}
			}
			return false;
		}
	}

	bool CUrlScanner::IsSupportedScheme(const char* szText, size_t nSchemeLength)
	{
		// THIS REPLACES A WIN32 CALL, and the replacement is not provably
		// identical - so what it does instead is stated exactly.
		//
		// The original is:
		//     InternetCrackUrl(&text[start], len, 0, &url)
		//         && isUrlSchemeSupported(url.nScheme)
		// i.e. wininet parses the candidate, and the parsed scheme must be one of
		// INTERNET_SCHEME_{FTP,HTTP,HTTPS,MAILTO,FILE}. There is no portable
		// wininet, and pulling in a URL parser to replace it would add a
		// dependency to a layer that has none (brief D6).
		//
		// So this matches the scheme TEXT against the same five names and drops
		// the parse. The consequence is one-directional and worth being precise
		// about: wininet maps exactly these five spellings onto those five enum
		// values, so anything Windows accepts this accepts too. It can only
		// differ by ACCEPTING a candidate wininet would have rejected as
		// malformed - never by rejecting one Windows underlines.
		//
		// That is the safe direction for a cosmetic underline, and the candidate
		// reaching here has already been through ScanToUrlEnd, which admits only
		// IsUrlTextChar characters. It is a divergence all the same, and it is
		// the reason this function exists rather than being inlined.
		if (nSchemeLength == 0)
		{
			return false;
		}
		// nSchemeLength counts the ':' that terminates the scheme.
		const size_t nNameLength = nSchemeLength - 1;
		static const char* const SUPPORTED[] = { "ftp", "http", "https", "mailto", "file" };
		for (size_t i = 0; i < sizeof(SUPPORTED) / sizeof(SUPPORTED[0]); ++i)
		{
			const char* szWanted = SUPPORTED[i];
			size_t nWanted = 0;
			while (szWanted[nWanted] != '\0')
			{
				++nWanted;
			}
			if (nWanted != nNameLength)
			{
				continue;
			}
			bool bSame = true;
			for (size_t c = 0; c < nNameLength && bSame; ++c)
			{
				unsigned char got = At(szText, c);
				if (got >= 'A' && got <= 'Z')
				{
					got = static_cast<unsigned char>(got - 'A' + 'a');
				}
				bSame = (got == static_cast<unsigned char>(szWanted[c]));
			}
			if (bSame)
			{
				return true;
			}
		}
		return false;
	}

	bool CUrlScanner::NextSegment(const char* szText, size_t nLength, size_t nStart,
		size_t& nSegmentLength, bool& bIsUrl)
	{
		bIsUrl = false;
		nSegmentLength = 0;
		if (nStart >= nLength)
		{
			return false;
		}

		size_t nDistance = 0;
		size_t nSchemeLength = 0;
		if (!ScanToUrlStart(szText, nLength, nStart, nDistance, nSchemeLength))
		{
			// No scheme anywhere ahead: the rest of the text is one non-URL run.
			nSegmentLength = nDistance;
			return nSegmentLength > 0;
		}

		if (nDistance != 0)
		{
			// Text before the candidate. Reported on its own so the caller can
			// clear any highlight over it.
			nSegmentLength = nDistance;
			return true;
		}

		size_t nUrlLength = 0;
		ScanToUrlEnd(szText, nLength, nStart + nSchemeLength, nUrlLength);
		if (nUrlLength != 0)
		{
			nUrlLength += nSchemeLength;
			if (IsSupportedScheme(szText + nStart, nSchemeLength))
			{
				while (RemoveUnwantedTrailingChar(szText + nStart, nUrlLength))
				{
					// Strips one character per call, by design.
				}
				nSegmentLength = nUrlLength;
				bIsUrl = true;
				return true;
			}
		}

		// A scheme-shaped run that is not a URL - "foo:" or an unsupported
		// scheme. Skip its leading letters so the next call cannot rediscover the
		// same candidate and spin.
		//
		// The original reads text[start + len] with len starting at 1 and tests
		// the bound afterwards, so at the very end of the buffer it reads one
		// past it. Ordered the other way round here: same result for every input
		// that is not at the boundary, and no read past the end at the boundary.
		size_t nSkip = 1;
		while (nStart + nSkip < nLength && IsSchemeStartChar(At(szText, nStart + nSkip)))
		{
			++nSkip;
		}
		nSegmentLength = nSkip;
		return true;
	}

	void CUrlScanner::FindAll(const std::string& strText, std::vector<SUrlMatch>& matches)
	{
		matches.clear();
		size_t nStart = 0;
		size_t nSegmentLength = 0;
		bool bIsUrl = false;
		while (CUrlScanner::NextSegment(strText.c_str(), strText.size(), nStart,
			nSegmentLength, bIsUrl))
		{
			if (bIsUrl)
			{
				SUrlMatch match;
				match._Start = nStart;
				match._Length = nSegmentLength;
				matches.push_back(match);
			}
			nStart += nSegmentLength;
		}
	}
}
