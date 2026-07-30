/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "Tokenizer.h"

namespace Core
{
	CTokenizer::CTokenizer(const std::wstring& strText, const std::wstring& strDelimiters)
		: m_Text(strText)
		, m_Delimiters(strDelimiters)
		, m_nPosition(0)
	{
	}

	bool CTokenizer::HasMoreTokens() const
	{
		// CLexingParser wrote this as (m_nPosition != length). The two agree for
		// every state Next() can produce, since Next() stops at the end and never
		// steps past it - but '<' cannot run away if that ever stops being true.
		return m_nPosition < m_Text.size();
	}

	std::wstring CTokenizer::Next()
	{
		std::wstring strToken;
		while (m_nPosition < m_Text.size())
		{
			const wchar_t chCurrent = m_Text[m_nPosition];

			// find() over the delimiter set is the original's inner loop over
			// every delimiter character, with the same result.
			if (m_Delimiters.find(chCurrent) != std::wstring::npos)
			{
				++m_nPosition;		// consume the delimiter
				return strToken;	// may be empty, and that is intended
			}

			strToken += chCurrent;
			++m_nPosition;
		}
		return strToken;
	}
}
