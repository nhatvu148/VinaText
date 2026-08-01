/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Differential test for core/UrlScanner against a verbatim transcription of the
// original - src/StringHelper.cpp's seven `*Url*` helpers, and the walk in
// AppUtils::IsUrlHyperLink that drives them (src/AppUtil.cpp:565).
//
// The transcription below is deliberately UGLY: `TCHAR*` becomes `wchar_t*`, the
// out-parameters stay out-parameters, the enums keep their one-letter names, and
// the loops keep their original shape. It shares no code with the port, so a
// transposed comparison or an off-by-one in either one shows up as a mismatch
// rather than cancelling out.
//
// WHAT THIS DOES NOT COVER, stated because the gap is real: the original calls
// InternetCrackUrl, and there is no portable wininet. Both sides here use the
// same scheme-text check, so this proves the transcription of the PURE logic and
// says nothing about the Win32 call the port replaces. See the note on
// CUrlScanner::IsSupportedScheme for what that replacement does and why the
// difference can only go one way.
//
// Build and run (one line):
//   c++ -std=c++11 -I core core/UrlScanner.cpp core/tests/TestUrlScanner.cpp -o testurl && ./testurl

#include "UrlScanner.h"

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

	//////////////////////////////////////////////////////////////////////////
	// Verbatim transcription of src/StringHelper.cpp:87-345
	//////////////////////////////////////////////////////////////////////////

	bool isUrlSchemeStartChar(wchar_t const c)
	{
		return ((c >= 'A') && (c <= 'Z'))
			|| ((c >= 'a') && (c <= 'z'));
	}

	bool isUrlSchemeDelimiter(wchar_t const c)
	{
		return   !(((c >= '0') && (c <= '9'))
			|| ((c >= 'A') && (c <= 'Z'))
			|| ((c >= 'a') && (c <= 'z'))
			|| (c == '_'));
	}

	bool isUrlTextChar(wchar_t const c)
	{
		if (c <= ' ') return false;
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

	bool isUrlQueryDelimiter(wchar_t const c)
	{
		switch (c)
		{
		case '&':
		case '+':
		case '=':
		case ';':
			return true;
		}
		return false;
	}

	bool scanToUrlStart(wchar_t *text, int textLen, int start, int* distance, int* schemeLength)
	{
		int p = start;
		int p0 = 0;
		enum { sUnknown, sScheme } s = sUnknown;
		while (p < textLen)
		{
			switch (s)
			{
			case sUnknown:
				if (isUrlSchemeStartChar(text[p]) && ((p == 0) || isUrlSchemeDelimiter(text[p - 1])))
				{
					p0 = p;
					s = sScheme;
				}
				break;

			case sScheme:
				if (text[p] == ':')
				{
					*distance = p0 - start;
					*schemeLength = p - p0 + 1;
					return true;
				}
				if (!isUrlSchemeStartChar(text[p]))
					s = sUnknown;
				break;
			}
			p++;
		}
		*schemeLength = 0;
		*distance = p - start;
		return false;
	}

	void scanToUrlEnd(wchar_t *text, int textLen, int start, int* distance)
	{
		int p = start;
		wchar_t q = 0;
		enum { sHostAndPath, sQuery, sQueryAfterDelimiter, sQueryQuotes, sQueryAfterQuotes, sFragment } s = sHostAndPath;
		while (p < textLen)
		{
			switch (s)
			{
			case sHostAndPath:
				if (text[p] == '?')
					s = sQuery;
				else if (text[p] == '#')
					s = sFragment;
				else if (!isUrlTextChar(text[p]))
				{
					*distance = p - start;
					return;
				}
				break;

			case sQuery:
				if (text[p] == '#')
					s = sFragment;
				else if (isUrlQueryDelimiter(text[p]))
					s = sQueryAfterDelimiter;
				else if (!isUrlTextChar(text[p]))
				{
					*distance = p - start;
					return;
				}
				break;

			case sQueryAfterDelimiter:
				if ((text[p] == '\'') || (text[p] == '"'))
				{
					q = text[p];
					s = sQueryQuotes;
				}
				else if (text[p] == '{')
				{
					q = '}';
					s = sQueryQuotes;
				}
				else if (isUrlTextChar(text[p]))
					s = sQuery;
				else
				{
					*distance = p - start;
					return;
				}
				break;

			case sQueryQuotes:
				if (text[p] < ' ')
				{
					*distance = p - start;
					return;
				}
				if (text[p] == q)
					s = sQueryAfterQuotes;
				break;

			case sQueryAfterQuotes:
				if (isUrlQueryDelimiter(text[p]))
					s = sQueryAfterDelimiter;
				else
				{
					*distance = p - start;
					return;
				}
				break;

			case sFragment:
				if (!isUrlTextChar(text[p]))
				{
					*distance = p - start;
					return;
				}
				break;
			}
			p++;
		}
		*distance = p - start;
	}

	bool removeUnwantedTrailingCharFromUrl(wchar_t const *text, int* length)
	{
		int l = *length - 1;
		if (l <= 0) return false;
		{ // remove unwanted single characters
			const wchar_t *singleChars = L".,:;?!#";
			for (int i = 0; singleChars[i]; i++)
				if (text[l] == singleChars[i])
				{
					*length = l;
					return true;
				}
		}
		{ // remove unwanted closing parenthesis
			const wchar_t *closingParenthesis = L")]>";
			const wchar_t *openingParenthesis = L"([<";
			for (int i = 0; closingParenthesis[i]; i++)
				if (text[l] == closingParenthesis[i])
				{
					int count = 1;
					for (int j = l - 1; j >= 0; j--)
					{
						if (text[j] == closingParenthesis[i])
							count++;
						if (text[j] == openingParenthesis[i])
							count--;
					}
					if (count == 0)
						return false;
					*length = l;
					return true;
				}
		}
		{ // remove unwanted quotes
			const wchar_t *quotes = L"\"'`";
			for (int i = 0; quotes[i]; i++)
			{
				if (text[l] == quotes[i])
				{
					int count = 0;
					for (int j = l - 1; j >= 0; j--)
						if (text[j] == quotes[i])
							count++;

					if (count & 1)
						return false;
					*length = l;
					return true;
				}
			}
		}
		return false;
	}

	// The five schemes isUrlSchemeSupported accepts, matched on TEXT. Stands in
	// for InternetCrackUrl + isUrlSchemeSupported on both sides of this test -
	// see the header comment for why that part is not differentially covered.
	bool isSchemeSupportedByText(const wchar_t* text, int schemeLen)
	{
		if (schemeLen <= 0) return false;
		std::wstring name(text, text + (schemeLen - 1));
		for (size_t i = 0; i < name.size(); ++i)
			if (name[i] >= 'A' && name[i] <= 'Z') name[i] = static_cast<wchar_t>(name[i] - 'A' + 'a');
		return name == L"ftp" || name == L"http" || name == L"https"
			|| name == L"mailto" || name == L"file";
	}

	// AppUtils::IsUrlHyperLink (src/AppUtil.cpp:565), verbatim apart from the
	// InternetCrackUrl call noted above.
	bool IsUrlHyperLink(wchar_t* text, int textLen, int start, int* segmentLen)
	{
		int dist = 0, schemeLen = 0;
		if (scanToUrlStart(text, textLen, start, &dist, &schemeLen))
		{
			if (dist)
			{
				*segmentLen = dist;
				return false;
			}
			int len = 0;
			scanToUrlEnd(text, textLen, start + schemeLen, &len);
			if (len)
			{
				len += schemeLen;
				bool r = isSchemeSupportedByText(&text[start], schemeLen);
				if (r)
				{
					while (removeUnwantedTrailingCharFromUrl(&text[start], &len));
					*segmentLen = len;
					return true;
				}
			}
			len = 1;
			int lMax = textLen - start;
			while (isUrlSchemeStartChar(text[start + len]) && (len < lMax)) len++;
			*segmentLen = len;
			return false;
		}
		*segmentLen = dist;
		return false;
	}

	// The MFC's walk over the whole buffer (CEditorCtrl::RenderHotSpotForUrlLinks,
	// src/Editor.cpp:4440-4460), reduced to the URL ranges it fills.
	std::vector<std::pair<int, int> > OriginalUrls(const std::wstring& strText)
	{
		// NUL-terminated, as the MFC's buffer always is: IsUrlHyperLink's last
		// loop reads text[start + len] BEFORE testing len < lMax, so at the very
		// end of the buffer it reads one past textLen and relies on finding the
		// terminator there.
		std::vector<wchar_t> buffer(strText.begin(), strText.end());
		buffer.push_back(L'\0');

		std::vector<std::pair<int, int> > urls;
		const int textLen = static_cast<int>(strText.size());
		int startWide = 0;
		int lenWide = 0;
		while (true)
		{
			const bool bUrl = IsUrlHyperLink(&buffer[0], textLen, startWide, &lenWide);
			if (lenWide <= 0) break;
			if (bUrl) urls.push_back(std::make_pair(startWide, lenWide));
			startWide += lenWide;
			if (startWide >= textLen) break;
		}
		return urls;
	}

	//////////////////////////////////////////////////////////////////////////

	std::vector<std::pair<int, int> > PortUrls(const std::string& strText)
	{
		std::vector<Core::SUrlMatch> matches;
		Core::CUrlScanner::FindAll(strText, matches);
		std::vector<std::pair<int, int> > urls;
		for (size_t i = 0; i < matches.size(); ++i)
		{
			urls.push_back(std::make_pair(static_cast<int>(matches[i]._Start),
				static_cast<int>(matches[i]._Length)));
		}
		return urls;
	}

	std::string Describe(const std::vector<std::pair<int, int> >& urls)
	{
		std::string out;
		for (size_t i = 0; i < urls.size(); ++i)
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "[%d,%d]", urls[i].first, urls[i].second);
			out += buf;
		}
		return out.empty() ? "(none)" : out;
	}
}

int main()
{
	std::cout << "core/UrlScanner vs a verbatim transcription of the original\n";

	//----------------------------------------------------------------------
	// 1. Exhaustive over a small alphabet. ASCII only, so a byte offset and a
	//    wide-character offset are the same number and the two sides are
	//    directly comparable.
	//
	//    The alphabet carries one scheme letter, the ':' that ends a scheme, a
	//    '/' , a query and fragment introducer, a query delimiter, a quote, a
	//    bracket and a space - so every state of scanToUrlEnd and every branch
	//    of removeUnwantedTrailingCharFromUrl is reachable.
	//----------------------------------------------------------------------
	{
		const std::string alphabet = "h:/?#&\")t. ";
		long long nCompared = 0;
		int nMismatches = 0;
		std::string sample;
		for (size_t nLen = 0; nLen <= 5; ++nLen)
		{
			std::vector<size_t> index(nLen, 0);
			while (true)
			{
				sample.assign(nLen, ' ');
				for (size_t i = 0; i < nLen; ++i)
				{
					sample[i] = alphabet[index[i]];
				}
				const std::wstring wide(sample.begin(), sample.end());
				const std::vector<std::pair<int, int> > wanted = OriginalUrls(wide);
				const std::vector<std::pair<int, int> > got = PortUrls(sample);
				++nCompared;
				if (wanted != got && nMismatches < 5)
				{
					++nMismatches;
					std::cout << "  FAIL: " << sample << " -> original "
						<< Describe(wanted) << ", port " << Describe(got) << "\n";
				}
				else if (wanted != got)
				{
					++nMismatches;
				}

				size_t nDigit = nLen;
				while (nDigit-- > 0)
				{
					if (++index[nDigit] < alphabet.size()) break;
					index[nDigit] = 0;
				}
				if (nDigit == static_cast<size_t>(-1) || nLen == 0) break;
			}
		}
		Check(nMismatches == 0, "exhaustive length <= 5 over 11 characters");
		std::cout << "  free-form: " << nCompared << " strings compared, "
			<< nMismatches << " mismatch(es)\n";
	}

	//----------------------------------------------------------------------
	// 1b. The same, but PREFIXED with a supported scheme.
	//
	//     Sweep 1 alone is worth much less than its count suggests, and this
	//     was found by mutation rather than by reading: breaking the trailing
	//     '.' strip, dropping "ftp" from the scheme list, and inverting the
	//     bracket count all left it reporting 0 mismatches over 177,156
	//     strings. The reason is arithmetic - the shortest supported scheme
	//     spelling is "ftp:" at 4 characters and a URL needs at least one more,
	//     so NO string of length <= 5 over that alphabet is ever a URL. Every
	//     one of those comparisons exercised the reject paths only.
	//
	//     "ftp:" is prepended here so the tail sweep lands in scanToUrlEnd and
	//     removeUnwantedTrailingCharFromUrl, which is where the interesting
	//     behaviour lives. The tail alphabet carries a path separator, a query
	//     and fragment introducer, a query delimiter, both quote characters,
	//     both brackets, a full stop, a letter and a space.
	//----------------------------------------------------------------------
	{
		const std::string alphabet = "/?#&\"')(.a ";
		long long nCompared = 0;
		int nMismatches = 0;
		int nWereUrls = 0;
		for (size_t nLen = 0; nLen <= 4; ++nLen)
		{
			std::vector<size_t> index(nLen, 0);
			while (true)
			{
				std::string sample = "ftp:";
				for (size_t i = 0; i < nLen; ++i)
				{
					sample += alphabet[index[i]];
				}
				const std::wstring wide(sample.begin(), sample.end());
				const std::vector<std::pair<int, int> > wanted = OriginalUrls(wide);
				const std::vector<std::pair<int, int> > got = PortUrls(sample);
				++nCompared;
				if (!wanted.empty())
				{
					++nWereUrls;
				}
				if (wanted != got)
				{
					if (nMismatches < 5)
					{
						std::cout << "  FAIL: \"" << sample << "\" -> original "
							<< Describe(wanted) << ", port " << Describe(got) << "\n";
					}
					++nMismatches;
				}

				size_t nDigit = nLen;
				while (nDigit-- > 0)
				{
					if (++index[nDigit] < alphabet.size()) break;
					index[nDigit] = 0;
				}
				if (nDigit == static_cast<size_t>(-1) || nLen == 0) break;
			}
		}
		Check(nMismatches == 0, "exhaustive \"ftp:\" + tail of length <= 4");
		// Without this the sweep could go back to proving nothing and nobody
		// would notice - which is exactly what happened to sweep 1.
		Check(nWereUrls > 1000, "the prefixed sweep actually produces URLs");
		std::cout << "  ftp:-prefixed: " << nCompared << " strings compared, "
			<< nWereUrls << " were URLs, " << nMismatches << " mismatch(es)\n";
	}

	//----------------------------------------------------------------------
	// 2. Real URLs and the cases the original's comments call out. These are
	//    also compared against the transcription, so they test both sides.
	//----------------------------------------------------------------------
	{
		const char* aCases[] = {
			"see http://example.com/page for details",
			"http://example.com.",							// trailing '.' stripped
			"(see http://example.com/x)",					// unmatched ')' stripped
			"http://example.com/a_(b)",						// matched ')' kept
			"\"http://example.com/q\"",						// closing quote stripped
			"mailto:someone@example.com",
			"ftp://files.example.com/pub",
			"file:///etc/hosts",
			"https://example.com/s?a=1&b='c d'+e",			// the documented query shape
			"nothttp://example.com",						// scheme not at a delimiter
			"gopher://example.com",							// unsupported scheme
			"telnet:x http://a.b/",							// unsupported then supported
			"http://a.b/#frag",
			"http://",
			"http:",
			"a:b:c:",
			"",
			"http://example.com/x)y",
			"::::",
			"HTTP://EXAMPLE.COM/X",							// scheme case-insensitive
		};
		for (size_t i = 0; i < sizeof(aCases) / sizeof(aCases[0]); ++i)
		{
			const std::string sample(aCases[i]);
			const std::wstring wide(sample.begin(), sample.end());
			const std::vector<std::pair<int, int> > wanted = OriginalUrls(wide);
			const std::vector<std::pair<int, int> > got = PortUrls(sample);
			Check(wanted == got, std::string("case \"") + sample + "\": original "
				+ Describe(wanted) + " vs port " + Describe(got));
		}
	}

	//----------------------------------------------------------------------
	// 3. Non-ASCII. The two sides cannot be compared by offset here - one counts
	//    bytes and the other counts wide characters - so these assert the BYTE
	//    ranges directly, which is what Scintilla indicators need.
	//
	//    The claim being tested is the one in UrlScanner.h: every byte of a
	//    multi-byte UTF-8 sequence is >= 0x80, and at >= 0x80 the classifiers
	//    agree with what they say about a non-ASCII wchar_t - so the two find
	//    the same URLs, only expressed in different units.
	//----------------------------------------------------------------------
	{
		// "xin chào http://example.com/á rest" - the accented characters are
		// two bytes each, before and inside the URL.
		const std::string strText = "xin ch\xc3\xa0o http://example.com/\xc3\xa1 rest";
		std::vector<Core::SUrlMatch> matches;
		Core::CUrlScanner::FindAll(strText, matches);
		Check(matches.size() == 1, "one URL found in text with multi-byte characters");
		if (matches.size() == 1)
		{
			const std::string found = strText.substr(matches[0]._Start, matches[0]._Length);
			Check(found == "http://example.com/\xc3\xa1",
				"the URL's byte range covers the whole multi-byte tail, got \"" + found + "\"");
			// And the offset is a byte offset, not a character offset: "xin chào "
			// is 10 bytes and 9 characters.
			Check(matches[0]._Start == 10,
				"the start is a BYTE offset (10), not a character offset (9)");
		}

		// A multi-byte character immediately before a scheme must act as a
		// delimiter, exactly as a non-ASCII wchar_t does.
		std::vector<Core::SUrlMatch> after;
		Core::CUrlScanner::FindAll("\xc3\xa9http://example.com", after);
		Check(after.size() == 1 && after[0]._Start == 2,
			"a multi-byte character delimits a scheme, so the URL still starts");
	}

	//----------------------------------------------------------------------
	// 4. The walk terminates on every input. NextSegment returning true with a
	//    zero-length segment would hang the caller, and the caller is a loop
	//    over a whole document.
	//----------------------------------------------------------------------
	{
		const char* aInputs[] = { "", ":", "h", "h:", "::", "hh:hh:", " ", "?#&" };
		bool bAllPositive = true;
		for (size_t i = 0; i < sizeof(aInputs) / sizeof(aInputs[0]); ++i)
		{
			const std::string sample(aInputs[i]);
			size_t nStart = 0, nSegment = 0;
			bool bIsUrl = false;
			int nGuard = 0;
			while (Core::CUrlScanner::NextSegment(sample.c_str(), sample.size(), nStart,
				nSegment, bIsUrl))
			{
				if (nSegment == 0)
				{
					bAllPositive = false;
					break;
				}
				nStart += nSegment;
				if (++nGuard > 1000)
				{
					bAllPositive = false;
					break;
				}
			}
		}
		Check(bAllPositive, "every segment has non-zero length, so the walk terminates");
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
