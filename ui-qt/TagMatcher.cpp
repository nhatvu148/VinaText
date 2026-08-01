/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "TagMatcher.h"

#include <Scintilla.h>
#include <SciLexer.h>

namespace TagMatch
{
	namespace
	{
		// AppUtils::IsXMLWhitespace (src/AppUtil.cpp:2155). Deliberately only
		// these four - XML's definition of whitespace, not the C library's, which
		// would also accept vertical tab and form feed.
		bool IsXmlWhitespace(int ch)
		{
			return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
		}

		int StyleAt(const SDocument& doc, long long nPosition)
		{
			return static_cast<int>(doc.Send(SCI_GETSTYLEAT,
				static_cast<unsigned long long>(nPosition)));
		}

		int CharAt(const SDocument& doc, long long nPosition)
		{
			return static_cast<int>(doc.Send(SCI_GETCHARAT,
				static_cast<unsigned long long>(nPosition)));
		}

		// A '<' or '>' inside an attribute value is data, not markup:
		// <tag attrib="a>b"> is valid XML. Every search below has to skip those,
		// and the lexer's style is what tells them apart.
		bool IsInsideAttributeString(int nStyle)
		{
			return nStyle == SCE_H_DOUBLESTRING || nStyle == SCE_H_SINGLESTRING;
		}

		// CEditorCtrl::FindTextXmlHTMLResult (src/Editor.cpp:1251). Searching
		// backwards is expressed as end < start, which SCI_FINDTEXT supports
		// directly - so the same helper serves both directions.
		SResult FindText(const SDocument& doc, const char* szText,
			long long nStart, long long nEnd)
		{
			Sci_TextToFind search{};
			search.lpstrText = const_cast<char*>(szText);
			search.chrg.cpMin = static_cast<Sci_PositionCR>(nStart);
			search.chrg.cpMax = static_cast<Sci_PositionCR>(nEnd);

			SResult result;
			if (doc.Send(SCI_FINDTEXT, 0, reinterpret_cast<long long>(&search)) == -1)
			{
				result._Success = false;
			}
			else
			{
				result._Success = true;
				result._Start = search.chrgText.cpMin;
				result._End = search.chrgText.cpMax;
			}
			return result;
		}

		// CEditorCtrl::FindCloseXmlHTMLAngle (src/Editor.cpp:1332). The next '>'
		// that is not inside an attribute value, or -1.
		long long FindCloseAngle(const SDocument& doc, long long nStart, long long nEnd)
		{
			// Only searches forwards, so the original swaps the bounds rather
			// than refusing. Preserved: callers rely on passing them either way.
			if (nStart > nEnd)
			{
				const long long nTemp = nEnd;
				nEnd = nStart;
				nStart = nTemp;
			}

			long long nResult = -1;
			bool bValidClose = false;
			SResult closeAngle;
			do
			{
				bValidClose = false;
				closeAngle = FindText(doc, ">", nStart, nEnd);
				if (closeAngle._Success)
				{
					if (!IsInsideAttributeString(StyleAt(doc, closeAngle._Start)))
					{
						nResult = closeAngle._Start;
						bValidClose = true;
					}
					else
					{
						nStart = closeAngle._End;
					}
				}
			} while (closeAngle._Success && !bValidClose);
			return nResult;
		}

		// CEditorCtrl::FindOpenXmlHTMLTag (src/Editor.cpp:1272). Looks for
		// "<tagName" that is genuinely this tag rather than a longer name: the
		// character after it must be '>' or whitespace, or "<TAGNAME2" matches
		// a search for "<TAGNAME".
		SResult FindOpenTag(const SDocument& doc, const std::string& strTagName,
			long long nStart, long long nEnd)
		{
			const std::string strSearch = "<" + strTagName;
			const bool bForward = (nStart < nEnd);
			long long nSearchStart = nStart;

			SResult openTagFound;
			SResult result;
			do
			{
				result = FindText(doc, strSearch.c_str(), nSearchStart, nEnd);
				if (result._Success)
				{
					const int nNextChar = CharAt(doc, result._End);
					const int nStyle = StyleAt(doc, result._Start);
					if (nStyle != SCE_H_CDATA && !IsInsideAttributeString(nStyle))
					{
						if (nNextChar == '>')
						{
							// The common case: <TAGNAME> with no attributes.
							openTagFound._End = result._End;
							openTagFound._Success = true;
						}
						else if (IsXmlWhitespace(nNextChar))
						{
							const long long nCloseAngle = FindCloseAngle(doc, result._End,
								bForward ? nEnd : nStart);
							// A '/' before the '>' makes it self-closing, so it is
							// not an open tag that anything closes.
							if (nCloseAngle != -1 && CharAt(doc, nCloseAngle - 1) != '/')
							{
								openTagFound._End = nCloseAngle;
								openTagFound._Success = true;
							}
						}
					}
				}
				nSearchStart = bForward ? result._End + 1 : result._Start - 1;
			} while (result._Success && !openTagFound._Success);

			// Set unconditionally in the original, including on failure - and it
			// is read only when _Success, so the stale value is never used.
			openTagFound._Start = result._Start;
			return openTagFound;
		}

		// CEditorCtrl::FindCloseXmlHTMLTag (src/Editor.cpp:1373). "</tagName"
		// followed by '>', optionally with whitespace between - anything else
		// means a different tag whose name merely starts the same way.
		SResult FindCloseTag(const SDocument& doc, const std::string& strTagName,
			long long nStart, long long nEnd)
		{
			const std::string strSearch = "</" + strTagName;
			const bool bForward = (nStart < nEnd);
			long long nSearchStart = nStart;

			SResult closeTagFound;
			SResult result;
			bool bValidCloseTag = false;
			do
			{
				bValidCloseTag = false;
				result = FindText(doc, strSearch.c_str(), nSearchStart, nEnd);
				if (result._Success)
				{
					int nNextChar = CharAt(doc, result._End);
					const int nStyle = StyleAt(doc, result._Start);

					// Advanced before the checks below, not after - the original
					// sets up the next iteration here so that every `continue`
					// path further down still makes progress.
					nSearchStart = bForward ? result._End + 1 : result._Start - 1;

					if (nStyle != SCE_H_CDATA && !IsInsideAttributeString(nStyle))
					{
						if (nNextChar == '>')
						{
							bValidCloseTag = true;
							closeTagFound._Start = result._Start;
							closeTagFound._End = result._End;
							closeTagFound._Success = true;
						}
						else if (IsXmlWhitespace(nNextChar))
						{
							// </TAGNAME   > is valid; </TAGNAMEX> is not.
							long long nWhitespacePoint = result._End;
							do
							{
								++nWhitespacePoint;
								nNextChar = CharAt(doc, nWhitespacePoint);
							} while (IsXmlWhitespace(nNextChar));

							if (nNextChar == '>')
							{
								bValidCloseTag = true;
								closeTagFound._Start = result._Start;
								closeTagFound._End = nWhitespacePoint;
								closeTagFound._Success = true;
							}
						}
					}
				}
			} while (result._Success && !bValidCloseTag);
			return closeTagFound;
		}

		// The tag name starting at nPosition. Stops at whitespace, '/', '>' and
		// the two quotes. Matching on quotes is wrong for well-formed XML - a
		// quote cannot appear in a name - but it makes the highlighter behave
		// better on the malformed XML people actually edit, which is why the
		// original does it and why it is preserved.
		std::string ReadTagName(const SDocument& doc, long long nPosition, long long nDocLength)
		{
			std::string strTagName;
			int nChar = CharAt(doc, nPosition);
			while (nPosition < nDocLength && !IsXmlWhitespace(nChar) && nChar != '/'
				&& nChar != '>' && nChar != '\"' && nChar != '\'')
			{
				strTagName.push_back(static_cast<char>(nChar));
				++nPosition;
				nChar = CharAt(doc, nPosition);
			}
			return strTagName;
		}

		// The caret is inside a close tag: </TAG|NAME>. Walk backwards for the
		// open tag, counting the close tags in between so that nested pairs of
		// the same name resolve to the right partner.
		bool MatchFromCloseTag(const SDocument& doc, const SResult& openFound,
			long long nCaret, STagPositions& positions)
		{
			positions._TagCloseStart = openFound._Start;
			const long long nDocLength = doc.Send(SCI_GETLENGTH);

			const SResult endCloseTag = FindText(doc, ">", nCaret, nDocLength);
			if (endCloseTag._Success)
			{
				positions._TagCloseEnd = endCloseTag._End;
			}

			// + 2 to step over "</".
			const std::string strTagName = ReadTagName(doc, openFound._Start + 2, nDocLength);
			if (strTagName.empty())
			{
				return false;
			}

			// Search back for "<TAGNAME". Each one found may itself already be
			// closed by a "</TAGNAME" lying between it and us, in which case it
			// is not our partner and the search continues further back.
			long long nCurrentEndPoint = positions._TagCloseStart;
			int nOpenTagsRemaining = 1;
			bool bTagFound = false;
			SResult nextOpenTag;
			do
			{
				nextOpenTag = FindOpenTag(doc, strTagName, nCurrentEndPoint, 0);
				if (nextOpenTag._Success)
				{
					--nOpenTagsRemaining;

					long long nCurrentStartPosition = nextOpenTag._End;
					int nCloseTagsFound = 0;
					const bool bForwardSearch = (nCurrentStartPosition < nCurrentEndPoint);
					SResult inBetweenCloseTag;
					do
					{
						inBetweenCloseTag = FindCloseTag(doc, strTagName,
							nCurrentStartPosition, nCurrentEndPoint);
						if (inBetweenCloseTag._Success)
						{
							++nCloseTagsFound;
							nCurrentStartPosition = bForwardSearch
								? inBetweenCloseTag._End
								: inBetweenCloseTag._Start - 1;
						}
					} while (inBetweenCloseTag._Success);

					if (nCloseTagsFound == 0 && nOpenTagsRemaining == 0)
					{
						positions._TagOpenStart = nextOpenTag._Start;
						positions._TagOpenEnd = nextOpenTag._End + 1;
						// + 1 to account for '<'.
						positions._TagNameEnd = nextOpenTag._Start
							+ static_cast<long long>(strTagName.size()) + 1;
						bTagFound = true;
					}
					else
					{
						nOpenTagsRemaining += nCloseTagsFound;
						nCurrentEndPoint = nextOpenTag._Start;
					}
				}
			} while (!bTagFound && nOpenTagsRemaining > 0 && nextOpenTag._Success);
			return bTagFound;
		}

		// The caret is inside an open tag: <TAG|NAME attrib="v">. Either it is
		// self-closing and matches itself, or walk forwards for the close tag.
		bool MatchFromOpenTag(const SDocument& doc, const SResult& openFound,
			STagPositions& positions)
		{
			const long long nDocLength = doc.Send(SCI_GETLENGTH);
			positions._TagOpenStart = openFound._Start;

			// + 1 to step over '<'.
			const std::string strTagName = ReadTagName(doc, openFound._Start + 1, nDocLength);
			if (strTagName.empty())
			{
				return false;
			}
			positions._TagNameEnd = openFound._Start
				+ static_cast<long long>(strTagName.size()) + 1;

			const long long nCloseAngle = FindCloseAngle(doc,
				openFound._Start + 1 + static_cast<long long>(strTagName.size()), nDocLength);
			if (nCloseAngle == -1)
			{
				return false;
			}
			positions._TagOpenEnd = nCloseAngle + 1;

			if (CharAt(doc, nCloseAngle - 1) == '/')
			{
				// Self-closing: it is its own match, and there is no close tag.
				positions._TagCloseEnd = -1;
				positions._TagCloseStart = -1;
				return true;
			}

			long long nCurrentStartPosition = positions._TagOpenEnd;
			int nCloseTagsRemaining = 1;
			bool bTagFound = false;
			SResult nextCloseTag;
			do
			{
				nextCloseTag = FindCloseTag(doc, strTagName, nCurrentStartPosition, nDocLength);
				if (nextCloseTag._Success)
				{
					--nCloseTagsRemaining;

					long long nCurrentEndPosition = nextCloseTag._Start;
					long long nOpenTagsFound = 0;
					SResult inBetweenOpenTag;
					do
					{
						inBetweenOpenTag = FindOpenTag(doc, strTagName,
							nCurrentStartPosition, nCurrentEndPosition);
						if (inBetweenOpenTag._Success)
						{
							++nOpenTagsFound;
							nCurrentStartPosition = inBetweenOpenTag._End;
						}
					} while (inBetweenOpenTag._Success);

					if (nOpenTagsFound == 0 && nCloseTagsRemaining == 0)
					{
						positions._TagCloseStart = nextCloseTag._Start;
						positions._TagCloseEnd = nextCloseTag._End + 1;
						bTagFound = true;
					}
					else
					{
						nCloseTagsRemaining += static_cast<int>(nOpenTagsFound);
						nCurrentStartPosition = nextCloseTag._End;
					}
				}
			} while (!bTagFound && nCloseTagsRemaining > 0 && nextCloseTag._Success);
			return bTagFound;
		}
	}

	bool FindEnclosingTag(const SDocument& doc, STagPositions& positions)
	{
		const long long nCaret = doc.Send(SCI_GETCURRENTPOS);
		long long nSearchStartPoint = nCaret;
		int nStyleAt = 0;

		// Back to the previous '<', skipping any that is inside an attribute
		// value rather than opening a tag.
		SResult openFound;
		do
		{
			openFound = FindText(doc, "<", nSearchStartPoint, 0);
			nStyleAt = StyleAt(doc, openFound._Start);
			nSearchStartPoint = openFound._Start - 1;
		} while (openFound._Success && IsInsideAttributeString(nStyleAt)
			&& nSearchStartPoint > 0);

		if (!openFound._Success || nStyleAt == SCE_H_CDATA)
		{
			return false;
		}

		// If a '>' sits between that '<' and the caret then the caret is not in
		// the tag at all - it is in the text after it.
		SResult closeFound;
		nSearchStartPoint = openFound._Start;
		do
		{
			closeFound = FindText(doc, ">", nSearchStartPoint, nCaret);
			nStyleAt = StyleAt(doc, closeFound._Start);
			nSearchStartPoint = closeFound._End;
		} while (closeFound._Success && IsInsideAttributeString(nStyleAt)
			&& nSearchStartPoint <= nCaret);

		if (closeFound._Success)
		{
			return false;
		}

		return (CharAt(doc, openFound._Start + 1) == '/')
			? MatchFromCloseTag(doc, openFound, nCaret, positions)
			: MatchFromOpenTag(doc, openFound, positions);
	}
}
