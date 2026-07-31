/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// DiffEngine ////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////
// Original version from S.Rodriguez - Feb 2003
//////////////////////////////////////////////////////////////////////
//
//

#include "stdafx.h"
#include "FilePartition.h"
#include "DiffEngine.h"
#include "LineDiff.h"		// core/ - owns the alignment algorithm

#include <vector>

// The algorithm itself now lives in core/LineDiff.{h,cpp} so ui-qt/ can use it
// too. What is left here is the HTML report renderer, which is presentation and
// stays with the MFC frontend, plus the adapter below.
//
// core/ is dependency-free and speaks std::wstring, so conversion happens at
// this boundary (brief D6). Note that option filtering - ignore case, ignore
// indentation - deliberately does NOT move: CFilePartition::GetLine applies it
// with CString::MakeLower, whose Unicode behaviour core/ cannot reproduce
// without changing which lines compare equal. See the note in core/LineDiff.h.

namespace
{
	// core/ and FilePartition.h declare the same four statuses in the same order;
	// this maps rather than casts so that reordering either one is a compile
	// error instead of a silently mislabelled diff.
	LineStatus ToLineStatus(Core::ELineStatus eStatus)
	{
		switch (eStatus)
		{
		case Core::LineChanged:	return Changed;
		case Core::LineAdded:	return Added;
		case Core::LineDeleted:	return Deleted;
		case Core::LineNormal:
		default:				return Normal;
		}
	}

	// _Raw is what the report shows, _Compare is what matching uses. That is
	// exactly the existing GetRawLine / GetLine split, so no filtering logic
	// is duplicated here.
	void FillSide(CFilePartition& partition, Core::SDiffSide& side)
	{
		const long nbLines = partition.GetNBLines();
		side._Raw.reserve(static_cast<size_t>(nbLines));
		side._Compare.reserve(static_cast<size_t>(nbLines));
		for (long i = 0; i < nbLines; i++)
		{
			side._Raw.push_back(std::wstring(partition.GetRawLine(i).GetString()));
			side._Compare.push_back(std::wstring(partition.GetLine(i).GetString()));
		}
	}

	void EmitSide(const std::vector<Core::SDiffLine>& lines, CFilePartition& out)
	{
		for (size_t i = 0; i < lines.size(); i++)
		{
			// AddString takes a non-const reference, so this needs an lvalue.
			CString strLine(lines[i]._Text.c_str());
			out.AddString(strLine, ToLineStatus(lines[i]._Status));
		}
	}
}

CDiffEngine::CDiffEngine()
{
	m_szColorText		= _T("#888888"); // default colors
	m_szColorBackground = _T("white");
	m_szColorChanged	= _T("#FFFFBB");
	m_szColorAdded		= _T("#BBFFBB");
	m_szColorDeleted	= _T("#FFBBBB");
	m_szFooter = _T("<br><font size='-2' color='#BBBBBB'></font>");
}

// compare f1 (old version) with f2 (new version)
// and build two new copies of those file objects with status on a line by line basis
//
BOOL CDiffEngine::Diff(	/*in*/CFilePartition &f1, /*in*/CFilePartition &f2,
						/*out*/CFilePartition &f1_bis, /*out*/CFilePartition &f2_bis)
{
	f1_bis.SetName( f1.GetName() );
	f2_bis.SetName( f2.GetName() );

	Core::SDiffSide left, right;
	FillSide(f1, left);
	FillSide(f2, right);

	std::vector<Core::SDiffLine> leftOut, rightOut;
	if ( !Core::DiffLines(left, right, leftOut, rightOut) )
		return FALSE;

	EmitSide(leftOut, f1_bis);
	EmitSide(rightOut, f2_bis);

	return TRUE;
}

void CDiffEngine::SetTitles(CString &szHeader, CString &szFooter)
{
	m_szHeader = szHeader;
	m_szFooter = szFooter;
}

void CDiffEngine::SetColorStyles(CString &szText, CString &szBackground, CString &szChanged, CString &szAdded, CString &szDeleted)
{
	m_szColorText = szText;
	m_szColorBackground = szBackground;
	m_szColorChanged = szChanged;
	m_szColorAdded = szAdded;
	m_szColorDeleted = szDeleted;
}

CString CDiffEngine::Serialize(	/*in*/CFilePartition &f1, 
								/*in*/CFilePartition &f2)
{

	// eval amount of differences between the two files
	int nAdded_f1, nChanged_f1, nDeleted_f1;
	f1.HowManyChanges(/*out*/nAdded_f1, /*out*/nChanged_f1, /*out*/nDeleted_f1);
	int nAdded_f2, nChanged_f2, nDeleted_f2;
	f2.HowManyChanges(/*out*/nAdded_f2, /*out*/nChanged_f2, /*out*/nDeleted_f2);

	int nTotal = nAdded_f1 + nDeleted_f1 + nChanged_f1 + nAdded_f2 + nDeleted_f2;

	if (nTotal==0)
		m_szHeader += _T("<font size=-1><b>Compare Result: 2 paths are identical.</font><br>");
	else
	{
		TCHAR szTmp[128];
		wsprintf(szTmp, _T("<font size=-1><b>Compare Result: there are %d change(s) between 2 paths.</font><br>"), nTotal);
		m_szHeader += szTmp;
	}

	// write html header
	CString s = _T("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n") \
		_T("<!-- diff html gen -->\r\n") \
		_T("<HTML>\r\n") \
		_T("<HEAD>\r\n") \
		_T("<TITLE> File Diff </TITLE>\r\n") \
		_T("<style type='text/css'>\r\n") \
		_T("<!--\r\n") \
		_T(".N { background-color:white; }\r\n") \
		_T(".C { background-color:") + m_szColorChanged + _T("; }\r\n") \
		_T(".A { background-color:") + m_szColorAdded + _T("; }\r\n") \
		_T(".D { background-color:") + m_szColorDeleted + _T("; }\r\n") \
		_T("-->\r\n") \
		_T("</style>\r\n") \
		_T("</HEAD>\r\n") \
		_T("\r\n") \
		_T("<BODY BGCOLOR='#FFFFFF'>\r\n") \
		_T("\r\n") + m_szHeader + \
		_T("<table border=0 bgcolor=0 cellpadding=1 cellspacing=1 width=100%><tr><td>\r\n") \
		_T("<table width=100% bgcolor=white border=0 cellpadding=0 cellspacing=0>\r\n") \
		_T("<tr bgColor='#EEEEEE' style='color:0'><td width=50%>Left Side</td><td width=50%>Right Side") \
		_T("&nbsp;&nbsp;&nbsp;(<b style='background-color:") + m_szColorChanged + _T(";width:20'>&nbsp;</b>changed&nbsp;&nbsp;") \
		_T("<b style='background-color:") + m_szColorAdded + _T(";width:20'>&nbsp;</b>added&nbsp;&nbsp;") \
		_T("<b style='background-color:") + m_szColorDeleted + _T(";width:20'>&nbsp;</b>deleted)&nbsp;&nbsp;") \
		_T("<FORM ACTION='' style='display:inline'><SELECT id='fontoptions' ") \
		_T("onchange='maintable.style.fontSize=this.options[this.selectedIndex].value'>") \
		_T("<option value='6pt'>6pt<option value='7pt'>7pt<option value='8pt'>8pt<option value='9pt' selected>9pt</SELECT> ") \
		_T("</FORM></td></tr>\r\n") \
		_T("<tr bgColor='#EEEEEE' style='color:0'><td width=50%><code>") + f1.GetName() + _T("</code></td><td width=50%><code>") + f2.GetName() + _T("</code></td></tr>") \
		_T("</table>\r\n") \
		_T("</td></tr>\r\n") \
		_T("</table>\r\n") \
		_T("\r\n") \
		_T("<br>\r\n") \
		_T("\r\n") ;

	long nbLines = f1.GetNBLines();
	if (nbLines==0)
	{
		s += _T("<br>empty files");
	}
	else
	{
		s += _T("<table border=0 bgcolor=0 cellpadding=1 cellspacing=1 width=100%><tr><td>") \
			_T("<table id='maintable' width=100% bgcolor='") + m_szColorBackground + _T("' cellpadding=0 cellspacing=0 border=0 style='color:")
			+ m_szColorText + _T(";font-family: Courier New, Helvetica, sans-serif; font-size: 8pt'>\r\n");
	}

	
	CString arrStatus[4] = {
		_T(""),
		_T(" class='C'"),
		_T(" class='A'"),
		_T(" class='D'") };

	CString sc;

	TCHAR szLine[16];

	// write content
	//
	for (long i=0; i<nbLines; i++)
	{
		wsprintf(szLine, _T("<b>%d</b>"), i);
		sc += _T("<tr><td width=50%") + CString(arrStatus[ f1.GetStatusLine(i) ]) + _T(">") +
			  CString(szLine) + 
			  _T(" ") +
			  Escape(f1.GetRawLine(i)) + _T("</td>");
		sc += _T("<td width=50%") + CString(arrStatus[ f2.GetStatusLine(i) ]) + _T(">") +
			  CString(szLine) + 
			  _T(" ") +
			  Escape(f2.GetRawLine(i)) + _T("</td></tr>");
		
	} // for i

	s += sc;

	if (nbLines>0)
		s += _T("</table></td></tr></table>\r\n");

	// write html footer
	s += m_szFooter + _T("</BODY>\r\n") \
		_T("</HTML>\r\n");

	return s;
}

CString CDiffEngine::Escape(CString &s) // a helper aimed to make sure tag symbols are passed as content
{
	CString o;
	long nSize = s.GetLength();
	if (nSize==0) return CString("&nbsp;");

	TCHAR c;
	BOOL bIndentation = TRUE;

	for (long i=0; i<nSize; i++)
	{
		c = s.GetAt(i);
		if (bIndentation && (c == _T(' ') || c == _T('\t')))
		{
			if (c == _T(' '))
				o += _T("&nbsp;");
			else
				o += _T("&nbsp;&nbsp;&nbsp;&nbsp;");
			continue;
		}
		bIndentation = FALSE;

		if (c == _T('<'))
			o += _T("&lt;");
		else if (c == _T('>'))
			o += _T("&gt;");
		else if (c == _T('&'))
			o += _T("&amp;");
		else
			o += c;
	}
	return o;
}