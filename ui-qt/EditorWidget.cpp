/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "EditorWidget.h"

#include "TagMatcher.h"

#include <UrlScanner.h>

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

	// src/EditorCommonDef.h:48-52, which numbers them from INDIC_CONTAINER (8).
	// Kept at the same numbers as the MFC so the two frontends stay comparable
	// when reading a document's indicator state.
	const int INDIC_TAGMATCH = 10;
	const int INDIC_TAGATTR = 11;
	const int INDIC_URL_HOTSPOT = 14;

	// AppSettingMgr.m_bEnableUrlHighlight, which ships TRUE (src/AppSettings.h:69
	// and AppSettings.cpp:14). Settings are AppSettings, the last file in the
	// Phase 2 backlog at 783 call sites, so the shipped default is transcribed
	// here rather than read - the same as every other setting in this file.
	const bool ENABLE_URL_HIGHLIGHT = true;

	// The autocomplete settings, all shipped defaults (src/AppSettings.h:81-84
	// and AppSettings.cpp:24-27). Same reasoning as ENABLE_URL_HIGHLIGHT.
	const bool ENABLE_AUTOCOMPLETE = true;
	const bool AUTOCOMPLETE_IGNORE_CASE = true;
	const bool AUTOCOMPLETE_IGNORE_NUMBERS = true;
	// src/EditorCommonDef.h:30-31.
	const char AUTOCOMPLETE_TYPE_SEPARATOR = '?';
	const char AUTOCOMPLETE_WORD_SEPARATOR = '$';
	// AppUtils::IsCStringAllDigits, which GetMatchedWordsOnFile gates on.
	bool IsAllDigits(const QString& strText)
	{
		if (strText.isEmpty())
		{
			return false;
		}
		for (const QChar& c : strText)
		{
			if (c < QLatin1Char('0') || c > QLatin1Char('9'))
			{
				return false;
			}
		}
		return true;
	}

	// Margin numbers, matching src/EditorCommonDef.h's SC_SETMARGINTYPE_*: line
	// numbers, symbols, folding, left to right.
	const int MARGIN_LINE_NUMBERS = 0;
	const int MARGIN_SYMBOLS = 1;
	const int MARGIN_FOLDING = 2;
	// VINATEXT_MARGINWIDTH in src/EditorCommonDef.h (defined twice there,
	// identically, at lines 42 and 43).
	const int FOLD_MARGIN_WIDTH = 16;

	// Scintilla.h publishes SC_CURSORNORMAL/ARROW/WAIT/REVERSEARROW but not the
	// hand, even though Platform.h's Cursor enum has it at 8 (invalid, text,
	// arrow, up, wait, horizontal, vertical, reverseArrow, hand) and PlatQt maps
	// it to Qt::PointingHandCursor. src/EditorCommonDef.h:41 names the same 8;
	// this is that constant, not a magic number.
	const int CURSOR_HAND = 8;

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
	Send(SCI_SETMARGINTYPEN, MARGIN_LINE_NUMBERS, SC_MARGIN_NUMBER);
	Send(SCI_SETMARGINWIDTHN, MARGIN_SYMBOLS, 0);	// bookmarks/breakpoints: Phase 5

	// Folding. The margin is only given width for languages that have a lexer -
	// CEditorCtrl does the same (src/Editor.cpp:354), and an always-visible empty
	// fold margin on a plain-text file is just wasted space.
	Send(SCI_SETMARGINTYPEN, MARGIN_FOLDING, SC_MARGIN_SYMBOL);
	Send(SCI_SETMARGINMASKN, MARGIN_FOLDING, SC_MASK_FOLDERS);
	Send(SCI_SETMARGINSENSITIVEN, MARGIN_FOLDING, 1);
	Send(SCI_SETMARGINWIDTHN, MARGIN_FOLDING, 0);
	// SC_AUTOMATICFOLD_CLICK makes Scintilla handle fold-margin clicks itself, and
	// there is deliberately no marginClicked handler here: Editor::NotifyMarginClick
	// returns as soon as it has folded (Editor.cxx:2675-2695), BEFORE building the
	// notification at 2697, so any handler would be dead code. Verified by A/B with
	// a synthetic click - 0 invocations with this flag, 1 without it, the fold
	// toggling either way. Its built-in handling is also richer than a plain
	// toggle: shift expands children, ctrl toggles them, shift+ctrl folds all.
	Send(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_SHOW
		| SC_AUTOMATICFOLD_CHANGE);
	Send(SCI_SETCARETLINEVISIBLE, 1);
	Send(SCI_SETCARETLINEVISIBLEALWAYS, 1);
	// AppSettings ships m_bDrawCaretLineFrame TRUE (src/AppSettings.h:76).
	Send(SCI_SETCARETLINEFRAME, 1);

	// The horizontal scrollbar sizes itself to the widest line seen, and the view
	// may scroll past the last line - both as src/Editor.cpp:380-384.
	Send(SCI_SETSCROLLWIDTH, 1);
	Send(SCI_SETSCROLLWIDTHTRACKING, 1);
	Send(SCI_SETENDATLASTLINE, 0);

	// Wrapping starts off and shows a marker at the end of a wrapped line; the
	// View menu toggles it, exactly as CEditorCtrl::EnableTextWrappingMode does.
	Send(SCI_SETWRAPMODE, SC_WRAP_NONE);
	Send(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_END);

	// The long-line marker's column is set here and the MODE only by the toggle,
	// so it is invisible until asked for - the same two-step the MFC uses
	// (src/Editor.cpp:417 sets the column, :3711 turns the mode on).
	Send(SCI_SETEDGECOLUMN, 80);		// AppSettingMgr.m_nLongLineMaximum
	Send(SCI_SETEDGEMODE, EDGE_NONE);

	// A hand cursor over every margin (src/Editor.cpp:357-359).
	Send(SCI_SETMARGINCURSORN, MARGIN_LINE_NUMBERS, CURSOR_HAND);
	Send(SCI_SETMARGINCURSORN, MARGIN_SYMBOLS, CURSOR_HAND);
	Send(SCI_SETMARGINCURSORN, MARGIN_FOLDING, CURSOR_HAND);

	// NOT ported: SCI_USEPOPUP(0) at src/Editor.cpp:415. The MFC frontend
	// disables Scintilla's context menu because it supplies its own; ui-qt/ does
	// not have one yet, so copying that call would leave right-click doing
	// nothing at all. Being faithful here would be a worse editor. Revisit when
	// ui-qt/ grows a context menu.
	Send(SCI_SETINDICATORCURRENT, FIND_INDICATOR);
	Send(SCI_INDICSETSTYLE, FIND_INDICATOR, INDIC_ROUNDBOX);
	Send(SCI_INDICSETALPHA, FIND_INDICATOR, 80);

	// The margin is sized for the line count, so it has to follow it. Without
	// this, a document that grows past 999 lines clips its own line numbers until
	// something else re-styles the editor.
	connect(this, &ScintillaEditBase::linesAdded,
		this, [this](Scintilla::Position) { UpdateLineNumberMargin(); });
	connect(this, &ScintillaEditBase::updateUi, this, &CEditorWidget::OnUpdateUi);
	connect(this, &ScintillaEditBase::charAdded, this, &CEditorWidget::OnCharAdded);

	// Autocomplete options (src/Editor.cpp:361-368). The list is built and shown
	// by OnCharAdded; these only describe how Scintilla should read and size it.
	if (AUTOCOMPLETE_IGNORE_CASE)
	{
		Send(SCI_AUTOCSETIGNORECASE, 1);
	}
	Send(SCI_AUTOCSETSEPARATOR, static_cast<uptr_t>(AUTOCOMPLETE_WORD_SEPARATOR));
	Send(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(AUTOCOMPLETE_TYPE_SEPARATOR));
	Send(SCI_AUTOCSETMAXWIDTH, 100);

	// The fold-marker highlight, which AppSettings ships TRUE
	// (src/AppSettings.h:77) and src/Editor.cpp:341-348 applies. Missed by the
	// folding change - see doc/PORTING.md 6j.
	//
	// Its sibling there, SCI_SETFOLDFLAGS, is deliberately still absent: the
	// original only calls it when m_bDrawFoldingLineUnderLineStyle is TRUE and
	// AppSettings ships it FALSE (src/AppSettings.cpp:19), so not calling it IS
	// the shipped behaviour.
	Send(SCI_MARKERENABLEHIGHLIGHT, 1);
}

//////////////////////////////////////////////////////////////////////////
// Caret-driven painting

// CEditorView handles SCN_UPDATEUI by calling UpdateCaretLineVisible() and then
// DoBraceMatchHighlight() (src/EditorView.cpp:6182-6183), in that order. Both are
// unconditional, so neither is filtered on `updated` here either - the MFC's own
// comment on that case is that speed matters more than tidiness, and adding a
// filter the original does not have would be a behaviour change dressed as an
// optimisation.
void CEditorWidget::OnUpdateUi(Scintilla::Update /*updated*/)
{
	UpdateSelectionPainting();
	UpdateBraceMatch();
	UpdateTagMatch();
}

//////////////////////////////////////////////////////////////////////////
// Autocomplete

QStringList CEditorWidget::GetAutoCompleteList(const QString& strPrefix) const
{
	// CEditorView::GetAutoCompleteList (src/EditorView.cpp:5823) draws from three
	// sources; two of them are here.
	//
	// NOT the third: m_AutoCompelteDataset is an English vocabulary list, and it
	// is empty unless the user picks "add autocomplete english dataset" from a
	// menu, which loads Packages/translator-packages/english-words.ee-package and
	// pops a message box (src/EditorView.cpp:7927-7945). It is a user-invoked
	// extra, not part of the default path, and ui-qt/ has no menu to invoke it
	// from yet.
	QStringList words;
	if (strPrefix.isEmpty())
	{
		return words;
	}

	// AppSettings ships m_bAutoCompleteIgnoreCase TRUE (src/AppSettings.h:84).
	const Qt::CaseSensitivity sensitivity = AUTOCOMPLETE_IGNORE_CASE
		? Qt::CaseInsensitive : Qt::CaseSensitive;

	// 1. The language's keywords, which core/ already carries - the same blob
	//    CLanguageDatabase::GetLanguageKeyWords hands the MFC, split the same way
	//    (AppUtils::SplitterCString on a single space).
	if (m_pLanguage != nullptr)
	{
		const QString strKeywords = QString::fromStdString(m_pLanguage->_Keywords);
		const QList<QStringView> keywords = QStringView(strKeywords).split(QLatin1Char(' '),
			Qt::SkipEmptyParts);
		for (const QStringView& keyword : keywords)
		{
			if (keyword.startsWith(strPrefix, sensitivity))
			{
				words.append(keyword.toString());
			}
		}
	}

	// 2. Words already in the document. CEditorView::GetMatchedWordsOnFile runs a
	//    POSIX regex over the whole buffer, anchored at a word start, and keeps
	//    each distinct match. The pattern is EDITOR_REGEX_AUTO_COMPLETE_PATTERN
	//    from src/MacroDef.h:83, verbatim.
	if (!(AUTOCOMPLETE_IGNORE_NUMBERS && IsAllDigits(strPrefix)))
	{
		const QByteArray pattern = ("\\<" + strPrefix
			+ QStringLiteral("[^ \\t\\n\\r.,;:\"(){}=<>'+!\\[\\]]+")).toUtf8();

		// The MFC drops SCFIND_MATCHCASE when ignore-case is on and keeps the
		// other three flags either way.
		int nFlags = SCFIND_WORDSTART | SCFIND_REGEXP | SCFIND_POSIX;
		if (!AUTOCOMPLETE_IGNORE_CASE)
		{
			nFlags |= SCFIND_MATCHCASE;
		}

		const sptr_t nDocLength = Send(SCI_GETLENGTH);
		Send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(nFlags));
		sptr_t nFrom = 0;
		while (nFrom < nDocLength)
		{
			Send(SCI_SETTARGETSTART, static_cast<uptr_t>(nFrom), 0);
			Send(SCI_SETTARGETEND, static_cast<uptr_t>(nDocLength), 0);
			if (Send(SCI_SEARCHINTARGET, static_cast<uptr_t>(pattern.size()),
					reinterpret_cast<sptr_t>(pattern.constData())) < 0)
			{
				break;
			}
			const sptr_t nWordStart = Send(SCI_GETTARGETSTART);
			const sptr_t nWordEnd = Send(SCI_GETTARGETEND);
			if (nWordEnd <= nWordStart)
			{
				break;			// a zero-width match would not advance
			}
			// The original skips anything >= 256 bytes rather than truncating it.
			if (nWordEnd - nWordStart < 256)
			{
				QByteArray word(static_cast<int>(nWordEnd - nWordStart) + 1, '\0');
				Sci_TextRange range{};
				range.chrg.cpMin = static_cast<Sci_PositionCR>(nWordStart);
				range.chrg.cpMax = static_cast<Sci_PositionCR>(nWordEnd);
				range.lpstrText = word.data();
				Send(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&range));
				const QString strWord = QString::fromUtf8(word.constData());
				// Distinct, and CASE-SENSITIVELY so: the original compares with
				// CString::operator==, which is case-sensitive even when the
				// SEARCH ignored case. So "Foo" and "foo" both appear.
				if (!words.contains(strWord))
				{
					words.append(strWord);
				}
			}
			nFrom = nWordEnd;
		}
	}
	return words;
}

void CEditorWidget::OnCharAdded(int nChar)
{
	// The gate CEditorView applies (src/EditorView.cpp:6163): a letter or an
	// underscore, autocomplete enabled, and not the large-file mode ui-qt/ does
	// not have. isalpha() is the C library's and therefore locale-dependent; this
	// is the ASCII range it means here, spelled out rather than inherited.
	if (!ENABLE_AUTOCOMPLETE)
	{
		return;
	}
	const bool bIsLetter = (nChar >= 'a' && nChar <= 'z') || (nChar >= 'A' && nChar <= 'Z');
	if (!bIsLetter && nChar != '_')
	{
		return;
	}

	// CEditorCtrl::GetRecentAddedText: the word so far, from its start to the
	// caret. Empty when the caret is at a word start, which cannot happen for a
	// letter that was just typed but is checked anyway, as the original does.
	const sptr_t nCaret = Send(SCI_GETCURRENTPOS);
	const sptr_t nWordStart = Send(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(nCaret), 1);
	if (nCaret <= nWordStart)
	{
		return;
	}
	QByteArray prefix(static_cast<int>(nCaret - nWordStart) + 1, '\0');
	Sci_TextRange range{};
	range.chrg.cpMin = static_cast<Sci_PositionCR>(nWordStart);
	range.chrg.cpMax = static_cast<Sci_PositionCR>(nCaret);
	range.lpstrText = prefix.data();
	Send(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&range));

	const QStringList words = GetAutoCompleteList(QString::fromUtf8(prefix.constData()));
	if (words.isEmpty())
	{
		return;
	}

	// "word?0$word?0$...word?0" - '?' introduces the image index and '$'
	// separates entries, matching SCI_AUTOCSETTYPESEPARATOR and
	// SCI_AUTOCSETSEPARATOR below. Image 0 is deliberately not registered here:
	// the original registers IDR_AUTO_COMPLETE, a Windows .ico loaded through
	// GuiUtils::LoadIconWithSize, and ui-qt/ has no equivalent resource yet.
	// Scintilla draws nothing for an unregistered type, so the suffix is kept -
	// it costs nothing and the day an icon is registered it simply works.
	QString strList;
	for (int i = 0; i < words.size(); ++i)
	{
		if (i != 0)
		{
			strList += QLatin1Char(AUTOCOMPLETE_WORD_SEPARATOR);
		}
		strList += words.at(i);
		strList += QLatin1Char(AUTOCOMPLETE_TYPE_SEPARATOR);
		strList += QLatin1Char('0');
	}
	const QByteArray list = strList.toUtf8();
	Send(SCI_AUTOCSHOW, static_cast<uptr_t>(nCaret - nWordStart),
		reinterpret_cast<sptr_t>(list.constData()));
}

void CEditorWidget::RenderUrlHotspots()
{
	// Transcribes CEditorCtrl::RenderHotSpotForUrlLinks (src/Editor.cpp:4419).
	//
	// WHEN it runs is part of the port: the original is called once, from
	// LoadEditorSettings (:409-412), not from any notification - so a URL typed
	// after the file is open is not underlined until the editor is re-styled.
	// Called from ApplyTheme here, which is the same moment. Hooking
	// SCN_MODIFIED would be an improvement and a behaviour change; it is not
	// this change.
	//
	// NO UTF-16 ROUND TRIP. The original converts the document to wide
	// characters, scans, and converts each segment's length back with
	// WideCharToMultiByte to get a document offset. core/UrlScanner works on the
	// UTF-8 bytes Scintilla already indexes; see the note in core/UrlScanner.h
	// for why that finds the same URLs.
	if (!ENABLE_URL_HIGHLIGHT)
	{
		return;
	}

	const sptr_t nLength = Send(SCI_GETLENGTH);
	if (nLength <= 0)
	{
		return;
	}
	QByteArray text(static_cast<int>(nLength) + 1, '\0');
	Send(SCI_GETTEXT, static_cast<uptr_t>(nLength) + 1,
		reinterpret_cast<sptr_t>(text.data()));
	text.truncate(static_cast<int>(nLength));

	Send(SCI_SETINDICATORCURRENT, INDIC_URL_HOTSPOT);
	// SC_INDICFLAG_VALUEFORE makes the colour come from the per-range VALUE set
	// below, so this has to be re-established here rather than only at styling
	// time: anything else that fills an indicator moves SCI_SETINDICATORCURRENT.
	//
	// ALMOST the default text colour, and the exception is the original's, not
	// this port's. DecorationList::SetCurrentValue is
	// `currentValue = value ? value : 1` (Decoration.cxx:191-193), so a value of
	// 0 becomes 1. The light theme's editorTextColor is RGB(0,0,0)
	// (src/EditorColorLight.h:32), i.e. Scintilla colour 0 - so on light, URLs
	// are drawn in RGB(0,0,1) rather than pure black.
	//
	// Left alone deliberately. CEditorCtrl does the identical
	// SCI_SETINDICATORVALUE(SCI_STYLEGETFORE(STYLE_DEFAULT)) into the identical
	// vendored Scintilla (src/Editor.cpp:4426-4428), so Windows has the same one
	// unit of blue. Special-casing 0 here would make ui-qt/ differ from the
	// shipping app to fix something no eye can see.
	Send(SCI_SETINDICATORVALUE, Send(SCI_STYLEGETFORE, STYLE_DEFAULT));

	// Walk the whole document, marking URL segments and clearing the rest. The
	// clearing is what removes a highlight over text that used to be a URL, and
	// it is why the original reports non-URL segments at all.
	size_t nStart = 0;
	size_t nSegment = 0;
	bool bIsUrl = false;
	while (Core::CUrlScanner::NextSegment(text.constData(), static_cast<size_t>(nLength),
		nStart, nSegment, bIsUrl))
	{
		if (bIsUrl)
		{
			Send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(nStart),
				static_cast<sptr_t>(nSegment));
		}
		else
		{
			Send(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(nStart),
				static_cast<sptr_t>(nSegment));
		}
		nStart += nSegment;
	}
}

void CEditorWidget::UpdateTagMatch()
{
	// Both gates are CEditorView's (src/EditorView.cpp:6183-6190): the language,
	// and an empty selection. The second matters because the tag highlight is an
	// indicator drawn under the text, and running it while the user is selecting
	// would fight the selection for the same pixels.
	//
	// _TagMatch, not a test on the lexer or the style table: php is lexed as
	// "cpp", so a lexer test would tag-match twelve languages that must not, and
	// _StyleTable or _FoldMarker would give {html, xml} and silently drop php.
	// See doc/PORTING.md 6h.
	if (m_pLanguage == nullptr || !m_pLanguage->_TagMatch)
	{
		return;
	}
	if (Send(SCI_GETSELECTIONEMPTY) == 0)
	{
		return;
	}

	// Clear both indicators over the whole document first. The original does the
	// same, unconditionally and before deciding whether there is anything to
	// draw, so that a tag highlighted a moment ago does not survive the caret
	// leaving it.
	const sptr_t nLength = Send(SCI_GETLENGTH);
	for (int nIndicator : { INDIC_TAGMATCH, INDIC_TAGATTR })
	{
		Send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(nIndicator));
		Send(SCI_INDICATORCLEARRANGE, 0, nLength);
	}

	// The search moves the target and the search flags, which find/replace also
	// owns - so they are saved and restored, exactly as the original does.
	const sptr_t nOriginalTargetStart = Send(SCI_GETTARGETSTART);
	const sptr_t nOriginalTargetEnd = Send(SCI_GETTARGETEND);
	const sptr_t nOriginalSearchFlags = Send(SCI_GETSEARCHFLAGS);

	TagMatch::SDocument document;
	document._Context = const_cast<CEditorWidget*>(this);
	document._Send = [](void* pContext, unsigned int nMessage,
		unsigned long long wParam, long long lParam) -> long long
	{
		return static_cast<CEditorWidget*>(pContext)->Send(nMessage,
			static_cast<uptr_t>(wParam), static_cast<sptr_t>(lParam));
	};

	TagMatch::STagPositions positions;
	if (TagMatch::FindEnclosingTag(document, positions))
	{
		Send(SCI_SETINDICATORCURRENT, INDIC_TAGMATCH);

		// 2 for "/>" on a self-closing tag, 1 for ">" when there is a close tag
		// to pair with - the original derives it from whether the close was found.
		int nOpenTagTailLength = 2;
		if (positions._TagCloseStart != -1 && positions._TagCloseEnd != -1)
		{
			Send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(positions._TagCloseStart),
				positions._TagCloseEnd - positions._TagCloseStart);
			nOpenTagTailLength = 1;
		}

		// The open tag is underlined in two pieces - its name, and its tail -
		// deliberately leaving the attributes in between unmarked.
		Send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(positions._TagOpenStart),
			positions._TagNameEnd - positions._TagOpenStart);
		Send(SCI_INDICATORFILLRANGE,
			static_cast<uptr_t>(positions._TagOpenEnd - nOpenTagTailLength),
			nOpenTagTailLength);

		// NOT ported: the attribute highlighting over INDIC_TAGATTR. It is
		// commented out in the original (src/Editor.cpp:1820-1826) along with
		// CEditorCtrl::GetAttributesPos, the 90-line state machine that feeds it,
		// so it draws nothing on Windows today. INDIC_TAGATTR is still styled and
		// still cleared above, both of which the original also does.

		// A tag pair spanning lines highlights the indent guide between them, the
		// same way brace matching does - but only when guides are on, because the
		// guide is what the highlight is drawn on.
		if (Send(SCI_GETINDENTATIONGUIDES) != 0)
		{
			const sptr_t nColumnAtCaret = Send(SCI_GETCOLUMN,
				static_cast<uptr_t>(positions._TagOpenStart));
			const sptr_t nColumnOpposite = Send(SCI_GETCOLUMN,
				static_cast<uptr_t>(positions._TagCloseStart));
			const sptr_t nLineAtCaret = Send(SCI_LINEFROMPOSITION,
				static_cast<uptr_t>(positions._TagOpenStart));
			const sptr_t nLineOpposite = Send(SCI_LINEFROMPOSITION,
				static_cast<uptr_t>(positions._TagCloseStart));

			if (positions._TagCloseStart != -1 && nLineAtCaret != nLineOpposite)
			{
				Send(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(positions._TagOpenStart),
					positions._TagCloseEnd - 1);
				Send(SCI_SETHIGHLIGHTGUIDE, static_cast<uptr_t>(
					(nColumnAtCaret < nColumnOpposite) ? nColumnAtCaret : nColumnOpposite));
			}
		}
	}

	Send(SCI_SETTARGETSTART, static_cast<uptr_t>(nOriginalTargetStart));
	Send(SCI_SETTARGETEND, static_cast<uptr_t>(nOriginalTargetEnd));
	Send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(nOriginalSearchFlags));
}

void CEditorWidget::UpdateBraceMatch()
{
	// The brace BEFORE the caret, which is where it sits after you type one.
	// At position 0 this asks about -1; Scintilla reads that as no character,
	// finds no opposite and returns -1, so the else branch clears the highlight -
	// the same path the MFC takes for the same reason.
	const sptr_t nCaret = Send(SCI_GETCURRENTPOS);
	const sptr_t nBrace = nCaret - 1;
	const sptr_t nMatch = Send(SCI_BRACEMATCH, static_cast<uptr_t>(nBrace), 0);
	if (nMatch >= 0)
	{
		Send(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(nBrace), nMatch);
		// The indent guide at the brace's own column is drawn highlighted, so a
		// matched block is legible down its whole depth rather than only at its
		// two ends.
		Send(SCI_SETHIGHLIGHTGUIDE,
			static_cast<uptr_t>(Send(SCI_GETCOLUMN, static_cast<uptr_t>(nBrace))));
	}
	else
	{
		Send(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(-1), -1);
		Send(SCI_SETHIGHLIGHTGUIDE, 0);
	}
}

void CEditorWidget::UpdateSelectionPainting()
{
	// Two things move together here, and only one of them is obvious.
	//
	// The caret line band is drawn only when there is nothing selected. With a
	// selection it would sit under the selection and fight it, so the MFC turns
	// it off (src/Editor.cpp:4488-4500).
	//
	// The selection colour is RE-SET on the empty branch, which looks redundant
	// and is not: CEditorCtrl::SearchForward and SearchBackward paint the
	// selection yellow at alpha 90 to flag a match (src/Editor.cpp:1904, :1936),
	// and this is what puts it back once the user clicks away. It is deliberately
	// NOT restored on the else branch, so the yellow survives for as long as the
	// match stays selected.
	//
	// ui-qt/'s find bar does not paint that yellow yet, so today this restores a
	// colour nothing has changed. Transcribed anyway: the day find does grow it,
	// the reset has to already be here, and an "optimisation" that removed it
	// would be found by eye long after the fact.
	if (Send(SCI_GETSELECTIONEMPTY) != 0 && Send(SCI_GETSELECTIONS) == 1)
	{
		if (m_bHaveSelectionBack)
		{
			Send(SCI_SETSELBACK, 1, ToScintillaColour(m_SelectionBack));
			Send(SCI_SETSELALPHA, 60);
		}
		Send(SCI_SETCARETLINEVISIBLE, 1);
	}
	else
	{
		Send(SCI_SETCARETLINEVISIBLE, 0);
	}
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
	ApplyFoldMargin(colours);
	UpdateLineNumberMargin();
	// Last, because it reads STYLE_DEFAULT's foreground - which
	// ApplyEditorStyles has just set - as the colour to draw URLs in.
	RenderUrlHotspots();
}

void CEditorWidget::ApplyEditorStyles(const Core::CEditorTheme& theme)
{
	// Mirrors CEditorCtrl::LoadEditorSettings (src/Editor.cpp:135-175): set
	// STYLE_DEFAULT, broadcast it with SCI_STYLECLEARALL, then everything else on
	// top. Doing it in any other order silently discards the later calls.
	// ResolveRole, not ResolveColor, for everything CEditorCtrl reads out of
	// m_AppThemeColorSet: the palette key a role takes is not reliably the role's
	// own name. Eight of the ten happen to coincide, which is why resolving by
	// name looked right; lineNumberColor takes "linenumber", and
	// selectionTextColor takes "black" on light and "white" on dark. See
	// core/LanguageData.h and doc/PORTING.md 6g.
	//
	// editorBackground stays on ResolveColor deliberately - it is a palette key
	// with no role, because the MFC gets the editor's background from the style
	// table rather than from m_AppThemeColorSet.
	Core::SColor fore, back;
	const bool bHaveFore = theme.ResolveRole("editorTextColor", fore);
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

	// The selection background, and the initial application of it. The MFC does
	// this once at the end of LoadEditorSettings (src/Editor.cpp:419, via
	// SetSelectionTextColor) and thereafter only from UpdateCaretLineVisible.
	m_bHaveSelectionBack = theme.ResolveRole("selectionTextColor", m_SelectionBack);
	if (m_bHaveSelectionBack)
	{
		// SELBACK, not SELFORE, despite the name of the role: the C++ member is
		// called _selectionTextColor and is passed to SCI_SETSELBACK.
		Send(SCI_SETSELBACK, 1, ToScintillaColour(m_SelectionBack));
		Send(SCI_SETSELALPHA, 60);
	}
	else
	{
		qWarning("theme %s: no selectionTextColor role", theme.GetName().c_str());
	}

	// How a matched and an unmatched brace are drawn (src/Editor.cpp:447-456).
	// This is the styling half of brace matching; the logic is UpdateBraceMatch.
	// Without it the highlight falls back to Scintilla's default and is very
	// nearly invisible against either theme.
	//
	// It also colours the HIGHLIGHTED INDENT GUIDE, which is not obvious from
	// either end: EditView.cxx:290 draws that guide with
	// styles[StyleBraceLight].fore. So SCI_SETHIGHLIGHTGUIDE in UpdateBraceMatch
	// does nothing a user can see until this runs - before it, the highlighted
	// guide was white, the same as every other guide.
	//
	// Four of the ten calls there are ported. The other six are
	// SCI_INDICSETSTYLE / INDICSETALPHA / INDICSETOUTLINEALPHA, which take an
	// INDICATOR number - and the original passes STYLE_BRACELIGHT (34) and
	// STYLE_BRACEBAD (35), which are STYLE numbers. INDIC_MAX is 35, so those are
	// valid indicator ids and the calls configure two indicators nothing ever
	// draws with. Scintilla only takes the indicator path for braces when
	// SCI_BRACEHIGHLIGHTINDICATOR turns it on, and neither frontend calls it. The
	// source comment there - "foreground and alpha maybe overridden by style
	// settings" - reads like the author was unsure which mechanism applied.
	Core::SColor brace;
	static const struct { const char* _Role; int _Style; } BRACE_STYLES[] = {
		{ "braceLightColor", STYLE_BRACELIGHT },
		{ "braceBadColor",   STYLE_BRACEBAD },
	};
	for (const auto& entry : BRACE_STYLES)
	{
		if (theme.ResolveRole(entry._Role, brace))
		{
			Send(SCI_STYLESETFORE, entry._Style, ToScintillaColour(brace));
		}
		// Unconditional in the original - no branch, no theme dependence.
		Send(SCI_STYLESETBOLD, entry._Style, 1);
	}

	// The tag-match indicators (src/Editor.cpp:462-469). SCI_INDICSETUNDER draws
	// them beneath the text rather than over it, which is what makes a dashed
	// underline readable on a styled tag name.
	Core::SColor tagMatch;
	if (theme.ResolveRole("editorTagMatchColor", tagMatch))
	{
		for (int nIndicator : { INDIC_TAGMATCH, INDIC_TAGATTR })
		{
			Send(SCI_INDICSETFORE, nIndicator, ToScintillaColour(tagMatch));
			Send(SCI_INDICSETSTYLE, nIndicator, INDIC_DASH);
			Send(SCI_INDICSETALPHA, nIndicator, 50);
			Send(SCI_INDICSETUNDER, nIndicator, 1);
		}
	}

	// The URL hotspot indicator (src/Editor.cpp:4421-4425). A plain underline
	// normally, a filled box while the pointer is over it.
	Send(SCI_INDICSETSTYLE, INDIC_URL_HOTSPOT, INDIC_PLAIN);
	Send(SCI_INDICSETHOVERSTYLE, INDIC_URL_HOTSPOT, INDIC_FULLBOX);
	Send(SCI_INDICSETALPHA, INDIC_URL_HOTSPOT, 70);
	// SC_INDICFLAG_VALUEFORE makes the drawn colour the per-range VALUE
	// (Indicator.cxx:33), so the INDICSETFORE the original does two lines earlier
	// with BasicColors::orange never reaches the screen - URLs are drawn in the
	// default text colour. Transcribed anyway, unlike the six dead brace calls:
	// this one addresses the right indicator and would start mattering the day
	// the flag changed.
	Core::SColor url;
	if (theme.ResolveColor("orange", url))
	{
		Send(SCI_INDICSETFORE, INDIC_URL_HOTSPOT, ToScintillaColour(url));
	}
	Send(SCI_INDICSETFLAGS, INDIC_URL_HOTSPOT, SC_INDICFLAG_VALUEFORE);

	Core::SColor colour;
	if (theme.ResolveRole("editorCaretColor", colour))
	{
		Send(SCI_SETCARETFORE, ToScintillaColour(colour));
		Send(SCI_SETADDITIONALCARETFORE, ToScintillaColour(colour));
	}
	// The caret line uses the text colour at low alpha, which is what
	// src/Editor.cpp:405 does. The palette's "currentline" entry is a Monokai
	// colour the lexer tables reference; it is not the caret-line background, and
	// using it here would invent a difference from the Windows build.
	Send(SCI_SETCARETLINEBACK, ToScintillaColour(fore));
	Send(SCI_SETCARETLINEBACKALPHA, 100);
	Send(SCI_SETCARETWIDTH, 2);
	if (theme.ResolveRole("editorIndicatorColor", colour))
	{
		Send(SCI_INDICSETFORE, FIND_INDICATOR, ToScintillaColour(colour));
	}

	Core::SColor margin, lineNumber;
	if (theme.ResolveRole("editorMarginBarColor", margin)
		&& theme.ResolveRole("lineNumberColor", lineNumber))
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

	// _StyleTable, not _Id: xml is coloured from html's table, because that is
	// the one the shipping app walks for it (doc/PORTING.md 6e).
	// Guides that stop at a blank line for Python, both directions otherwise.
	Send(SCI_SETINDENTATIONGUIDES,
		m_pLanguage->_IndentGuides == "lookforward" ? SC_IV_LOOKFORWARD : SC_IV_LOOKBOTH);

	const std::vector<Core::SStyleMapping>* pStyles =
		theme.FindStyles(m_pLanguage->_StyleTable);
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

	// Weight and slant come from the language, not the theme: the MFC lexer
	// initialisers apply the same ones in both builds (doc/PORTING.md 6e). This
	// has to run after SCI_STYLECLEARALL, which resets them.
	for (const Core::SStyleAttribute& attribute : m_pLanguage->_StyleAttributes)
	{
		if (attribute._Bold)
		{
			Send(SCI_STYLESETBOLD, attribute._Value, 1);
		}
		if (attribute._Italic)
		{
			Send(SCI_STYLESETITALIC, attribute._Value, 1);
		}
		if (attribute._Underline)
		{
			Send(SCI_STYLESETUNDERLINE, attribute._Value, 1);
		}
	}
	Send(SCI_COLOURISE, 0, -1);
}

// The fold properties and marker shapes CEditorCtrl::LoadEditorSettings sets
// (src/Editor.cpp:177-191 and 302-325). The marker SHAPES are the STYLE_TREE_BOX
// branch, which is what AppSettings ships as the default
// (src/AppSettings.h:115); the per-marker RGB literals in that branch are not
// transcribed because the theme colours below overwrite every one of them two
// lines later in the original.
void CEditorWidget::ApplyFoldMargin(const Core::CEditorTheme& theme)
{
	static const char* const FOLD_PROPERTIES[][2] = {
		{ "fold", "1" },
		{ "fold.compact", "0" },
		{ "fold.html", "1" },
		{ "fold.html.preprocessor", "1" },
		{ "fold.comment", "1" },
		{ "fold.at.else", "1" },
		{ "fold.flags", "1" },
		{ "fold.preprocessor", "1" },
		{ "styling.within.preprocessor", "1" },
		{ "asp.default.language", "1" },
	};
	for (const char* const* property : FOLD_PROPERTIES)
	{
		Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>(property[0]),
			reinterpret_cast<sptr_t>(property[1]));
	}

	static const struct { int _Marker; int _Shape; } FOLD_MARKERS[] = {
		{ SC_MARKNUM_FOLDEROPEN,    SC_MARK_BOXMINUS },
		{ SC_MARKNUM_FOLDER,        SC_MARK_BOXPLUS },
		{ SC_MARKNUM_FOLDERSUB,     SC_MARK_VLINE },
		{ SC_MARKNUM_FOLDERTAIL,    SC_MARK_LCORNER },
		{ SC_MARKNUM_FOLDEREND,     SC_MARK_BOXPLUSCONNECTED },
		{ SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED },
		{ SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER },
	};
	Core::SColor fore, back, margin;
	const bool bHaveFore = theme.ResolveRole("editorFolderForeColor", fore);
	const bool bHaveBack = theme.ResolveRole("editorFolderBackColor", back);
	for (const auto& marker : FOLD_MARKERS)
	{
		Send(SCI_MARKERDEFINE, marker._Marker, marker._Shape);
		if (bHaveFore && bHaveBack)
		{
			Send(SCI_MARKERSETFORE, marker._Marker, ToScintillaColour(fore));
			Send(SCI_MARKERSETBACK, marker._Marker, ToScintillaColour(back));
		}
	}
	if (theme.ResolveRole("editorMarginBarColor", margin))
	{
		// The non-classic branch: AppSettings ships m_bUseFolderMarginClassic
		// FALSE (src/AppSettings.h:95), so the margin takes the theme colour
		// rather than the black/grey pair.
		Send(SCI_SETFOLDMARGINCOLOUR, 1, ToScintillaColour(margin));
		Send(SCI_SETFOLDMARGINHICOLOUR, 1, ToScintillaColour(margin));
	}

	// What a collapsed block shows. Comes from the language, because the chain
	// this replaces keyed on a name ui-qt/ does not have - see doc/PORTING.md 6f.
	const std::string strMarker = (m_pLanguage != nullptr) ? m_pLanguage->_FoldMarker : "";
	if (!strMarker.empty())
	{
		Send(SCI_SETDEFAULTFOLDDISPLAYTEXT, 0, reinterpret_cast<sptr_t>(strMarker.c_str()));
	}
	Send(SCI_FOLDDISPLAYTEXTSETSTYLE, SC_FOLDDISPLAYTEXT_BOXED);

	// No lexer means no fold levels, so the margin would only ever be blank.
	Send(SCI_SETMARGINWIDTHN, MARGIN_FOLDING,
		m_pLanguage != nullptr ? FOLD_MARGIN_WIDTH : 0);
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
// View toggles

void CEditorWidget::SetWordWrap(bool bEnable)
{
	Send(SCI_SETWRAPMODE, bEnable ? SC_WRAP_WORD : SC_WRAP_NONE);
}

bool CEditorWidget::IsWordWrap() const
{
	return Send(SCI_GETWRAPMODE) != SC_WRAP_NONE;
}

void CEditorWidget::SetLongLineMarker(bool bEnable)
{
	Send(SCI_SETEDGEMODE, bEnable ? EDGE_LINE : EDGE_NONE);
}

bool CEditorWidget::IsLongLineMarker() const
{
	return Send(SCI_GETEDGEMODE) != EDGE_NONE;
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
