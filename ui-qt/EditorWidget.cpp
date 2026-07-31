/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "EditorWidget.h"

// Order matters: Lexilla.h uses Scintilla::ILexer5 without declaring it, so
// ILexer.h has to come first. Neither header is self-contained.
#include <Scintilla.h>
#include <SciLexer.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>

namespace
{
	// Scintilla wants 0x00BBGGRR, which is COLORREF ordering - the same convention
	// ui-mfc/ already relies on.
	sptr_t ToScintillaColour(const Core::SColor& c)
	{
		return static_cast<sptr_t>((c._Blue << 16) | (c._Green << 8) | c._Red);
	}

	// The indicator number used to mark find matches. Scintilla reserves 8..31 for
	// containers; 8 is the first of those.
	const int FIND_INDICATOR = 8;

	// Matches SC_DEFAUFT_TAB_WIDTH in src/EditorCommonDef.h (sic).
	const int DEFAULT_TAB_WIDTH = 4;

	int g_nNextUntitled = 1;
}

CEditorWidget::CEditorWidget(const CEditorData& data, QWidget* pParent)
	: ScintillaEditBase(pParent)
	, m_Data(data)
	, m_nUntitledNumber(g_nNextUntitled++)
{
	// Everything handed to and read from the document is UTF-8. Decoding happens
	// at the file boundary in LoadFile/SaveFile, which is the only place that
	// knows what the file on disk actually was.
	Send(SCI_SETCODEPAGE, SC_CP_UTF8);
	Send(SCI_SETTABWIDTH, DEFAULT_TAB_WIDTH);
	Send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
	Send(SCI_SETMARGINWIDTHN, 1, 0);		// no symbol margin in the alpha
	Send(SCI_SETMARGINWIDTHN, 2, 0);		// no folding margin either
	Send(SCI_SETCARETLINEVISIBLE, 1);
	Send(SCI_SETINDICATORCURRENT, FIND_INDICATOR);
	Send(SCI_INDICSETSTYLE, FIND_INDICATOR, INDIC_ROUNDBOX);
	Send(SCI_INDICSETALPHA, FIND_INDICATOR, 80);

	// The margin is sized for the line count, so it has to follow it. Without
	// this, a document that grows past 999 lines clips its own line numbers until
	// something else re-styles the editor.
	connect(this, &ScintillaEditBase::linesAdded,
		this, [this](Scintilla::Position) { UpdateLineNumberMargin(); });
}

QString CEditorWidget::GetDisplayName() const
{
	if (m_strFilePath.isEmpty())
	{
		return tr("Untitled %1").arg(m_nUntitledNumber);
	}
	return QFileInfo(m_strFilePath).fileName();
}

bool CEditorWidget::IsModified() const
{
	return Send(SCI_GETMODIFY) != 0;
}

//////////////////////////////////////////////////////////////////////////
// File IO

bool CEditorWidget::LoadFile(const QString& strPath, QString& strErrorOut)
{
	QFile file(strPath);
	if (!file.open(QIODevice::ReadOnly))
	{
		strErrorOut = tr("Cannot open %1: %2").arg(strPath, file.errorString());
		return false;
	}
	const QByteArray raw = file.readAll();
	if (file.error() != QFile::NoError)
	{
		strErrorOut = tr("Cannot read %1: %2").arg(strPath, file.errorString());
		return false;
	}

	// A byte-order mark is the only encoding evidence trusted here. Full detection
	// is uchardet in src/Textfile.cpp, a Wave 2 file that has not been pulled -
	// see doc/PORTING.md 6d for why nothing is pulled before the shell needs it.
	// Without a BOM: UTF-8 if the bytes decode as UTF-8, Latin-1 otherwise, which
	// is lossless for arbitrary bytes and so cannot corrupt a file it misreads.
	const std::optional<QStringConverter::Encoding> detected =
		QStringConverter::encodingForData(raw);
	m_bHasBom = detected.has_value();
	m_Encoding = detected.value_or(QStringConverter::Utf8);

	QStringDecoder decoder(m_Encoding);
	QString strText = decoder.decode(raw);
	if (decoder.hasError())
	{
		m_Encoding = QStringConverter::Latin1;
		m_bHasBom = false;
		strText = QStringDecoder(m_Encoding).decode(raw);
	}

	const QByteArray utf8 = strText.toUtf8();
	Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(utf8.constData()));
	Send(SCI_EMPTYUNDOBUFFER);
	Send(SCI_SETSAVEPOINT);
	Send(SCI_GOTOPOS, 0);
	DetectEol(utf8);

	m_strFilePath = strPath;
	m_pLanguage = m_Data.DetectLanguage(QFileInfo(strPath).fileName());
	ApplyTheme(m_Theme);
	return true;
}

bool CEditorWidget::SaveFile(const QString& strPath, QString& strErrorOut)
{
	const sptr_t nLength = Send(SCI_GETLENGTH);
	QByteArray utf8(static_cast<int>(nLength) + 1, '\0');
	Send(SCI_GETTEXT, static_cast<uptr_t>(nLength) + 1,
		reinterpret_cast<sptr_t>(utf8.data()));
	utf8.truncate(static_cast<int>(nLength));

	QFile file(strPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		strErrorOut = tr("Cannot write %1: %2").arg(strPath, file.errorString());
		return false;
	}
	const QByteArray encoded = EncodeForSave(QString::fromUtf8(utf8));
	if (file.write(encoded) != encoded.size() || !file.flush())
	{
		strErrorOut = tr("Cannot write %1: %2").arg(strPath, file.errorString());
		return false;
	}
	file.close();

	Send(SCI_SETSAVEPOINT);

	// Saving under a new name can change the language, so re-detect rather than
	// keep whatever the old name implied - "Save As untitled.py" must highlight.
	if (m_strFilePath != strPath)
	{
		m_strFilePath = strPath;
		m_pLanguage = m_Data.DetectLanguage(QFileInfo(strPath).fileName());
		ApplyTheme(m_Theme);
	}
	return true;
}

QByteArray CEditorWidget::EncodeForSave(const QString& strText) const
{
	QStringEncoder encoder(m_Encoding, m_bHasBom
		? QStringConverter::Flag::WriteBom
		: QStringConverter::Flag::Default);
	return encoder.encode(strText);
}

void CEditorWidget::DetectEol(const QByteArray& utf8)
{
	// The first line ending wins, which is what Scintilla's own EOL mode means:
	// the ending given to lines the user adds. Existing lines are untouched either
	// way, so a mixed file stays mixed rather than being silently rewritten.
	const int nIndex = utf8.indexOf('\n');
	int nMode = SC_EOL_LF;
	if (nIndex < 0)
	{
		nMode = utf8.contains('\r') ? SC_EOL_CR : SC_EOL_LF;
	}
	else if (nIndex > 0 && utf8.at(nIndex - 1) == '\r')
	{
		nMode = SC_EOL_CRLF;
	}
	Send(SCI_SETEOLMODE, static_cast<uptr_t>(nMode));
}

//////////////////////////////////////////////////////////////////////////
// Styling

void CEditorWidget::ApplyTheme(EEditorTheme theme)
{
	m_Theme = theme;
	const Core::CEditorTheme& colours = m_Data.GetTheme(theme);
	ApplyEditorStyles(colours);
	ApplyLanguageStyles(colours);
	UpdateLineNumberMargin();
}

void CEditorWidget::ApplyEditorStyles(const Core::CEditorTheme& theme)
{
	// Mirrors CEditorCtrl::LoadEditorSettings (src/Editor.cpp:135-175): set
	// STYLE_DEFAULT, broadcast it with SCI_STYLECLEARALL, then everything else on
	// top. Doing it in any other order silently discards the later calls.
	Core::SColor fore, back;
	const bool bHaveFore = theme.ResolveColor("editorTextColor", fore);
	const bool bHaveBack = theme.ResolveColor("editorBackground", back);
	if (!bHaveFore || !bHaveBack)
	{
		// SColor default-constructs to black, so a missing key would paint black
		// on black - invisible text, and very hard to attribute to a data file.
		qWarning("theme %s: missing %s", theme.GetName().c_str(),
			!bHaveFore ? "editorTextColor" : "editorBackground");
		return;
	}

	const QFont fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	const QByteArray family = fixed.family().toUtf8();
	Send(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(family.constData()));
	Send(SCI_STYLESETSIZE, STYLE_DEFAULT, fixed.pointSize() > 0 ? fixed.pointSize() : 11);
	Send(SCI_STYLESETFORE, STYLE_DEFAULT, ToScintillaColour(fore));
	Send(SCI_STYLESETBACK, STYLE_DEFAULT, ToScintillaColour(back));
	Send(SCI_STYLECLEARALL);

	Core::SColor colour;
	if (theme.ResolveColor("editorCaretColor", colour))
	{
		Send(SCI_SETCARETFORE, ToScintillaColour(colour));
	}
	// The caret line uses the text colour at low alpha, which is what
	// src/Editor.cpp:405 does. The palette's "currentline" entry is a Monokai
	// colour the lexer tables reference; it is not the caret-line background, and
	// using it here would invent a difference from the Windows build.
	Send(SCI_SETCARETLINEBACK, ToScintillaColour(fore));
	Send(SCI_SETCARETLINEBACKALPHA, 100);
	Send(SCI_SETCARETWIDTH, 2);
	if (theme.ResolveColor("editorIndicatorColor", colour))
	{
		Send(SCI_INDICSETFORE, FIND_INDICATOR, ToScintillaColour(colour));
	}

	Core::SColor margin, lineNumber;
	if (theme.ResolveColor("editorMarginBarColor", margin)
		&& theme.ResolveColor("linenumber", lineNumber))
	{
		Send(SCI_STYLESETFORE, STYLE_LINENUMBER, ToScintillaColour(lineNumber));
		Send(SCI_STYLESETBACK, STYLE_LINENUMBER, ToScintillaColour(margin));
	}
}

void CEditorWidget::ApplyLanguageStyles(const Core::CEditorTheme& theme)
{
	if (m_pLanguage == nullptr)
	{
		Send(SCI_SETILEXER, 0, 0);				// plain text
		Send(SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(""));
		return;
	}

	// NOT m_pLanguage->_Id: the shipping app lexes .java, .js, .cs and .json with
	// Lexilla's C++ lexer, and the theme's style tables are written against
	// whatever lexer it picked. See doc/PORTING.md 6d.
	//
	// One CreateLexer call, not two - each allocates an ILexer5 and the document
	// only takes ownership of the one handed to SCI_SETILEXER.
	void* pLexer = CreateLexer(m_pLanguage->_LexerName.c_str());
	if (pLexer == nullptr)
	{
		qWarning("no Lexilla lexer named '%s' for language '%s'",
			m_pLanguage->_LexerName.c_str(), m_pLanguage->_Id.c_str());
		Send(SCI_SETILEXER, 0, 0);
		return;
	}
	Send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(pLexer));
	Send(SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(m_pLanguage->_Keywords.c_str()));

	const std::vector<Core::SStyleMapping>* pStyles = theme.FindStyles(m_pLanguage->_Id);
	if (pStyles != nullptr)
	{
		for (const Core::SStyleMapping& style : *pStyles)
		{
			Core::SColor colour;
			if (theme.ResolveColor(style._Color, colour))
			{
				Send(SCI_STYLESETFORE, style._Value, ToScintillaColour(colour));
			}
		}
	}
	Send(SCI_COLOURISE, 0, -1);
}

void CEditorWidget::UpdateLineNumberMargin()
{
	// Wide enough for the document's real line count plus one digit of headroom,
	// measured in the margin's own style rather than guessed at in pixels.
	const sptr_t nLines = Send(SCI_GETLINECOUNT);
	QByteArray sample("_9");
	for (sptr_t n = nLines; n >= 10; n /= 10)
	{
		sample.append('9');
	}
	const sptr_t nWidth = Send(SCI_TEXTWIDTH, STYLE_LINENUMBER,
		reinterpret_cast<sptr_t>(sample.constData()));
	Send(SCI_SETMARGINWIDTHN, 0, nWidth);
}

//////////////////////////////////////////////////////////////////////////
// Status bar

QString CEditorWidget::GetLanguageLabel() const
{
	if (m_pLanguage == nullptr)
	{
		return m_Data.GetPlainTextLabel();
	}
	return QString::fromStdString(m_pLanguage->_Name);
}

QString CEditorWidget::GetEncodingLabel() const
{
	const char* szName = "UTF-8";
	switch (m_Encoding)
	{
	case QStringConverter::Utf8:		szName = "UTF-8"; break;
	case QStringConverter::Utf16LE:		szName = "UTF-16 LE"; break;
	case QStringConverter::Utf16BE:		szName = "UTF-16 BE"; break;
	case QStringConverter::Utf32LE:		szName = "UTF-32 LE"; break;
	case QStringConverter::Utf32BE:		szName = "UTF-32 BE"; break;
	case QStringConverter::Latin1:		szName = "Latin-1"; break;
	default:							szName = "UTF-8"; break;
	}
	const QString strName = QString::fromLatin1(szName);
	return m_bHasBom ? strName + QStringLiteral(" BOM") : strName;
}

QString CEditorWidget::GetEolLabel() const
{
	switch (Send(SCI_GETEOLMODE))
	{
	case SC_EOL_CRLF:	return QStringLiteral("CRLF");
	case SC_EOL_CR:		return QStringLiteral("CR");
	default:			return QStringLiteral("LF");
	}
}

int CEditorWidget::GetCaretLine() const
{
	const sptr_t nPos = Send(SCI_GETCURRENTPOS);
	return static_cast<int>(Send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(nPos))) + 1;
}

int CEditorWidget::GetCaretColumn() const
{
	const sptr_t nPos = Send(SCI_GETCURRENTPOS);
	return static_cast<int>(Send(SCI_GETCOLUMN, static_cast<uptr_t>(nPos))) + 1;
}

int CEditorWidget::GetSelectedCharacterCount() const
{
	// Character count, not byte count: a selection over non-ASCII text would
	// otherwise report a number the user cannot relate to anything on screen.
	const sptr_t nStart = Send(SCI_GETSELECTIONSTART);
	const sptr_t nEnd = Send(SCI_GETSELECTIONEND);
	if (nEnd <= nStart)
	{
		return 0;
	}
	return static_cast<int>(Send(SCI_COUNTCHARACTERS, static_cast<uptr_t>(nStart), nEnd));
}

//////////////////////////////////////////////////////////////////////////
// Find

namespace
{
	// The same flags FindDlg.cpp builds (src/FindDlg.cpp:758-762) - plain
	// SCFIND_REGEXP, so the alpha's regex dialect is the one Windows users have.
	int ToSearchFlags(const CEditorWidget::SFindOptions& options)
	{
		int nFlags = 0;
		if (options._MatchCase) { nFlags |= SCFIND_MATCHCASE; }
		if (options._WholeWord) { nFlags |= SCFIND_WHOLEWORD; }
		if (options._Regex)     { nFlags |= SCFIND_REGEXP; }
		return nFlags;
	}
}

bool CEditorWidget::FindNext(const QString& strPattern, const SFindOptions& options)
{
	if (strPattern.isEmpty())
	{
		return false;
	}
	const QByteArray pattern = strPattern.toUtf8();
	const sptr_t nDocEnd = Send(SCI_GETLENGTH);
	Send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(ToSearchFlags(options)));

	// Search from the current selection's far edge so that repeated presses walk
	// through the document instead of finding the same match again, then wrap once.
	const sptr_t nFrom = options._Backward ? Send(SCI_GETSELECTIONSTART)
										   : Send(SCI_GETSELECTIONEND);
	const sptr_t ranges[2][2] = {
		{ nFrom, options._Backward ? 0 : nDocEnd },
		{ options._Backward ? nDocEnd : 0, nFrom },		// the wrap
	};

	for (const auto& range : ranges)
	{
		Send(SCI_SETTARGETSTART, static_cast<uptr_t>(range[0]), 0);
		Send(SCI_SETTARGETEND, static_cast<uptr_t>(range[1]), 0);
		const sptr_t nFound = Send(SCI_SEARCHINTARGET,
			static_cast<uptr_t>(pattern.size()), reinterpret_cast<sptr_t>(pattern.constData()));
		if (nFound >= 0)
		{
			Send(SCI_SETSEL, static_cast<uptr_t>(Send(SCI_GETTARGETSTART)),
				Send(SCI_GETTARGETEND));
			Send(SCI_SCROLLCARET);
			return true;
		}
	}
	return false;
}

int CEditorWidget::HighlightMatches(const QString& strPattern, const SFindOptions& options)
{
	ClearHighlight();
	if (strPattern.isEmpty())
	{
		return 0;
	}
	const QByteArray pattern = strPattern.toUtf8();
	const sptr_t nDocEnd = Send(SCI_GETLENGTH);
	Send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(ToSearchFlags(options)));
	Send(SCI_SETINDICATORCURRENT, FIND_INDICATOR);

	int nCount = 0;
	sptr_t nStart = 0;
	while (nStart <= nDocEnd)
	{
		Send(SCI_SETTARGETSTART, static_cast<uptr_t>(nStart), 0);
		Send(SCI_SETTARGETEND, static_cast<uptr_t>(nDocEnd), 0);
		if (Send(SCI_SEARCHINTARGET, static_cast<uptr_t>(pattern.size()),
				reinterpret_cast<sptr_t>(pattern.constData())) < 0)
		{
			break;
		}
		const sptr_t nMatchStart = Send(SCI_GETTARGETSTART);
		const sptr_t nMatchEnd = Send(SCI_GETTARGETEND);
		Send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(nMatchStart), nMatchEnd - nMatchStart);
		++nCount;
		// A zero-length match is possible under a regex like "^" or "\b". Stepping
		// past it is the difference between reporting the line count and hanging.
		nStart = (nMatchEnd > nMatchStart) ? nMatchEnd : nMatchStart + 1;
	}
	return nCount;
}

void CEditorWidget::ClearHighlight()
{
	Send(SCI_SETINDICATORCURRENT, FIND_INDICATOR);
	Send(SCI_INDICATORCLEARRANGE, 0, Send(SCI_GETLENGTH));
}
