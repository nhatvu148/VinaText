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
#include <QSet>

#include <optional>
#include <QTextCodec>
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

	// src/EditorCommonDef.h:30-31. These two are compile-time constants of the
	// wire format, not settings - they say how SCI_AUTOCSHOW's list string is
	// punctuated - so they stay here while the settings above them moved to
	// core/AppSettings.
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

	// Marker NUMBERS, matching src/EditorCommonDef.h's SC_MARKER_*. Only the
	// bookmark is used here - breakpoints and the instruction pointer belong to
	// the debugger, which D10 defers - but the numbers are kept because the two
	// frontends share a margin and must not disagree about which bit is which.
	const int MARKER_BOOKMARK = 3;
	// ...and the MASK for it, which is a different thing and the source of two
	// defects in src/Editor.cpp. See CEditorWidget's bookmark section.
	const int MARKER_BOOKMARK_MASK = 1 << MARKER_BOOKMARK;
	const int SYMBOL_MARGIN_WIDTH = 16;

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
	// The symbol margin, which carried width 0 and a "Phase 5" note until
	// bookmarks arrived. Its mask admits ONLY the bookmark: src/Editor.cpp
	// admits four markers because it also has breakpoints and an instruction
	// pointer, and D10 defers both, so accepting them here would reserve space
	// for markers nothing can set.
	Send(SCI_SETMARGINTYPEN, MARGIN_SYMBOLS, SC_MARGIN_SYMBOL);
	Send(SCI_SETMARGINMASKN, MARGIN_SYMBOLS, MARKER_BOOKMARK_MASK);
	Send(SCI_SETMARGINWIDTHN, MARGIN_SYMBOLS, SYMBOL_MARGIN_WIDTH);
	// Clicking the margin toggles a bookmark. CEditorCtrl makes this margin
	// sensitive too (src/Editor.cpp:258).
	Send(SCI_SETMARGINSENSITIVEN, MARGIN_SYMBOLS, 1);
	connect(this, &ScintillaEditBase::marginClicked, this,
		&CEditorWidget::OnMarginClicked);
	// SC_MARK_BOOKMARK is a built-in shape, so no RGBA image and no icon
	// resource is needed - which is why this was cheaper than PORTING.md 6j's
	// three remaining SCI_REGISTERRGBAIMAGE calls suggested.
	Send(SCI_MARKERDEFINE, MARKER_BOOKMARK, SC_MARK_BOOKMARK);
	// RGB(255,0,0) on RGB(255,255,255), as src/Editor.cpp:255-256 sets them.
	// Scintilla takes colours as BGR, so red is 0x0000FF.
	Send(SCI_MARKERSETFORE, MARKER_BOOKMARK, 0x0000FF);
	Send(SCI_MARKERSETBACK, MARKER_BOOKMARK, 0xFFFFFF);

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

	ApplySettings();

	// Autocomplete options (src/Editor.cpp:361-368). The list is built and shown
	// by OnCharAdded; these only describe how Scintilla should read and size it.
	Send(SCI_AUTOCSETSEPARATOR, static_cast<uptr_t>(AUTOCOMPLETE_WORD_SEPARATOR));
	Send(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(AUTOCOMPLETE_TYPE_SEPARATOR));
	Send(SCI_AUTOCSETMAXWIDTH, 100);

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
	const Qt::CaseSensitivity sensitivity = m_Data.GetSettings().AutoCompleteIgnoreCase()
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
	if (!(m_Data.GetSettings().AutoCompleteIgnoreNumbers() && IsAllDigits(strPrefix)))
	{
		const QByteArray pattern = ("\\<" + strPrefix
			+ QStringLiteral("[^ \\t\\n\\r.,;:\"(){}=<>'+!\\[\\]]+")).toUtf8();

		// The MFC drops SCFIND_MATCHCASE when ignore-case is on and keeps the
		// other three flags either way.
		int nFlags = SCFIND_WORDSTART | SCFIND_REGEXP | SCFIND_POSIX;
		if (!m_Data.GetSettings().AutoCompleteIgnoreCase())
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
	if (!m_Data.GetSettings().EnableAutoComplete())
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
	if (!m_Data.GetSettings().EnableUrlHighlight())
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
	// Detection only ever yields a QStringConverter encoding, so a freshly
	// loaded document is always on the builtin path. A codec is something the
	// user asks for afterwards, never something guessed.
	m_CodecName.clear();

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

	// ENCODE BEFORE OPENING. The open carries Truncate, so it empties the file
	// the instant it succeeds - and this used to run first, which meant any
	// failure between it and the write left the user with a ZERO-BYTE file
	// where their document had been. Nothing could fail there when it was
	// written; the encoder can now, and a save that cannot produce bytes must
	// not have destroyed the old ones getting there.
	const std::optional<QByteArray> encoded = EncodeForSave(QString::fromUtf8(utf8));
	if (!encoded.has_value())
	{
		strErrorOut = tr("Cannot write %1: the encoding '%2' is not available in this "
			"build, and saving in a different one would write bytes you did not ask "
			"for.").arg(strPath, GetEncodingName());
		return false;
	}

	QFile file(strPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		strErrorOut = tr("Cannot write %1: %2").arg(strPath, file.errorString());
		return false;
	}
	if (file.write(*encoded) != encoded->size() || !file.flush())
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

std::optional<QByteArray> CEditorWidget::EncodeForSave(const QString& strText) const
{
	if (!m_CodecName.isEmpty())
	{
		if (QTextCodec* pCodec = QTextCodec::codecForName(m_CodecName))
		{
			// IgnoreHeader, because the BOM is ours to decide and this path
			// never has one - see the invariant on m_CodecName. Without it
			// QTextCodec would write a BOM of its own for the Unicode codecs,
			// which is the sort of byte a round-trip check exists to catch.
			//
			// NO TEST COVERS THIS FLAG, and it cannot be made to: every codec
			// that would emit a mark is a Unicode one, and SetSaveEncoding
			// routes all of those to QStringConverter instead, so nothing that
			// reaches here has a header to ignore. Removing it changes no
			// result today. It is defensive against the routing changing, and
			// is recorded as uncovered rather than left looking checked.
			QTextCodec::ConverterState state(QTextCodec::IgnoreHeader);
			return pCodec->fromUnicode(strText.constData(), strText.size(), &state);
		}
		// A codec that existed when it was chosen and does not now. THE SAVE
		// FAILS rather than falling through to the builtin encoding.
		//
		// Falling through wrote different bytes under the name the user picked,
		// and the label went on reporting the codec - found in review. Clearing
		// the name so the label matched was the suggested fix, but that only
		// makes the mislabelling honest AFTER the fact: the file still contains
		// an encoding nobody chose. Refusing is the one outcome that cannot
		// lose information, and the user can pick another encoding.
		qWarning("save: codec '%s' is unavailable; refusing to write",
			m_CodecName.constData());
		return std::nullopt;
	}
	QStringEncoder encoder(m_Encoding, m_bHasBom
		? QStringConverter::Flag::WriteBom
		: QStringConverter::Flag::Default);
	return encoder.encode(strText);
}

QString CEditorWidget::DecodeBytes(const QByteArray& raw) const
{
	if (!m_CodecName.isEmpty())
	{
		if (QTextCodec* pCodec = QTextCodec::codecForName(m_CodecName))
		{
			QTextCodec::ConverterState state(QTextCodec::IgnoreHeader);
			return pCodec->toUnicode(raw.constData(), raw.size(), &state);
		}
	}
	return QStringDecoder(m_Encoding).decode(raw);
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

void CEditorWidget::ApplySettings()
{
	const Core::CAppSettings& settings = m_Data.GetSettings();

	// AppSettings ships m_bDrawCaretLineFrame TRUE (src/AppSettings.h:76).
	Send(SCI_SETCARETLINEFRAME, settings.DrawCaretLineFrame() ? 1 : 0);

	// Stored as "LongLineColumnLimitation", NOT "LongLineMaximum" - the key is
	// not the member name. See core/AppSettings.h.
	Send(SCI_SETEDGECOLUMN, static_cast<uptr_t>(settings.LongLineColumnLimit()));

	// Explicitly BOTH ways: setting it only when true means turning the setting
	// off leaves the previous value in place, which is the classic bug in
	// re-appliable configuration.
	Send(SCI_AUTOCSETIGNORECASE, settings.AutoCompleteIgnoreCase() ? 1 : 0);

	// The fold-marker highlight (src/Editor.cpp:341-348) and its sibling
	// SCI_SETFOLDFLAGS (:187-190), which the original calls only when
	// m_bDrawFoldingLineUnderLineStyle is set.
	Send(SCI_MARKERENABLEHIGHLIGHT, settings.EnableHighlightFolder() ? 1 : 0);
	Send(SCI_SETFOLDFLAGS, settings.DrawFoldingLineUnderLineStyle()
		? SC_FOLDFLAG_LINEAFTER_CONTRACTED : 0, 0);
}

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

	// All four branches of CEditorCtrl's marker-shape chain (src/Editor.cpp:
	// 272-325), selected by FolderMarginStyle. Only STYLE_TREE_BOX was ported
	// before, because it is what AppSettings ships - but the setting is now
	// read, so the other three have to exist or configuring them does nothing.
	//
	// The per-branch RGB literals in the original are still NOT transcribed:
	// it overwrites every one of them with the theme's folder colours two lines
	// later, so copying them would be faithful to the text and wrong about the
	// behaviour (doc/PORTING.md 6f).
	static const struct { int _Marker; int _Arrow; int _PlusMinus;
		int _TreeCircle; int _TreeBox; } FOLD_MARKERS[] = {
		{ SC_MARKNUM_FOLDEROPEN,    SC_MARK_ARROWDOWN, SC_MARK_MINUS,
		  SC_MARK_CIRCLEMINUS,          SC_MARK_BOXMINUS },
		{ SC_MARKNUM_FOLDER,        SC_MARK_ARROW,     SC_MARK_PLUS,
		  SC_MARK_CIRCLEPLUS,           SC_MARK_BOXPLUS },
		{ SC_MARKNUM_FOLDERSUB,     SC_MARK_EMPTY,     SC_MARK_EMPTY,
		  SC_MARK_VLINE,                SC_MARK_VLINE },
		{ SC_MARKNUM_FOLDERTAIL,    SC_MARK_EMPTY,     SC_MARK_EMPTY,
		  SC_MARK_LCORNERCURVE,         SC_MARK_LCORNER },
		{ SC_MARKNUM_FOLDEREND,     SC_MARK_EMPTY,     SC_MARK_EMPTY,
		  SC_MARK_CIRCLEPLUSCONNECTED,  SC_MARK_BOXPLUSCONNECTED },
		{ SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY,     SC_MARK_EMPTY,
		  SC_MARK_CIRCLEMINUSCONNECTED, SC_MARK_BOXMINUSCONNECTED },
		{ SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY,     SC_MARK_EMPTY,
		  SC_MARK_TCORNERCURVE,         SC_MARK_TCORNER },
	};
	const int nStyle = m_Data.GetSettings().FolderMarginStyle();
	Core::SColor fore, back, margin;
	const bool bHaveFore = theme.ResolveRole("editorFolderForeColor", fore);
	const bool bHaveBack = theme.ResolveRole("editorFolderBackColor", back);
	for (const auto& marker : FOLD_MARKERS)
	{
		// FOLDER_MARGIN_STYPE: 0 arrow, 1 plus/minus, 2 tree circle, 3 tree box
		// (src/EnumDef.h:233-239). An out-of-range value falls back to the
		// shipped tree-box rather than drawing nothing.
		int nShape = marker._TreeBox;
		if (nStyle == 0)      { nShape = marker._Arrow; }
		else if (nStyle == 1) { nShape = marker._PlusMinus; }
		else if (nStyle == 2) { nShape = marker._TreeCircle; }
		Send(SCI_MARKERDEFINE, marker._Marker, nShape);
		if (bHaveFore && bHaveBack)
		{
			Send(SCI_MARKERSETFORE, marker._Marker, ToScintillaColour(fore));
			Send(SCI_MARKERSETBACK, marker._Marker, ToScintillaColour(back));
		}
	}
	// Both branches of src/Editor.cpp:260-269 now, rather than only the shipped
	// one: classic is a fixed black/grey pair that ignores the theme entirely,
	// which is the point of it.
	if (m_Data.GetSettings().UseFolderMarginClassic())
	{
		Send(SCI_SETFOLDMARGINCOLOUR, 1, 0x000000);			// RGB(0,0,0)
		Send(SCI_SETFOLDMARGINHICOLOUR, 1, 0x606060);		// RGB(96,96,96)
	}
	else if (theme.ResolveRole("editorMarginBarColor", margin))
	{
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
	// A codec chosen by the user is shown under the name they picked. Only the
	// builtin path gets the tidied-up spellings below, because only it has a
	// fixed, known set to tidy.
	if (!m_CodecName.isEmpty())
	{
		return QString::fromLatin1(m_CodecName);
	}
	const char* szName = "UTF-8";
	switch (m_Encoding)
	{
	case QStringConverter::Utf8:		szName = "UTF-8"; break;
	case QStringConverter::Utf16LE:		szName = "UTF-16 LE"; break;
	case QStringConverter::Utf16BE:		szName = "UTF-16 BE"; break;
	case QStringConverter::Utf32LE:		szName = "UTF-32 LE"; break;
	case QStringConverter::Utf32BE:		szName = "UTF-32 BE"; break;
	case QStringConverter::Latin1:		szName = "Latin-1"; break;
	// The system codepage, which the MFC's menu calls ANSI. Missing from this
	// switch it fell into default and every such document read "UTF-8" in the
	// status bar - a label that names a different encoding from the one about
	// to be written. Found in review.
	case QStringConverter::System:		szName = "System"; break;
	// NOT a silent default. Anything reaching here is an encoding
	// QStringConverter grew that this switch has not been told about, and
	// guessing "UTF-8" is how the System case hid. nameForEncoding always has
	// an answer, so an unknown one is named rather than mislabelled.
	default:
		return QString::fromLatin1(QStringConverter::nameForEncoding(m_Encoding))
			+ (m_bHasBom ? QStringLiteral(" BOM") : QString());
	}
	const QString strName = QString::fromLatin1(szName);
	return m_bHasBom ? strName + QStringLiteral(" BOM") : strName;
}

//////////////////////////////////////////////////////////////////////////
// Encoding
//
// Two operations, kept apart. See the header, and doc/PORTING.md 6m.

QStringList CEditorWidget::AvailableEncodings()
{
	QStringList names;
	QSet<QString> seen;
	// QStringConverter's own set first, so the encodings with a proven
	// round-trip are the ones a user meets at the top of the list, and so a
	// name that BOTH libraries know resolves to the builtin path.
	for (int i = 0; i <= static_cast<int>(QStringConverter::LastEncoding); ++i)
	{
		const QString strName = QString::fromLatin1(QStringConverter::nameForEncoding(
			static_cast<QStringConverter::Encoding>(i)));
		if (!strName.isEmpty() && !seen.contains(strName))
		{
			seen.insert(strName);
			names.append(strName);
		}
	}
	for (const QByteArray& codec : QTextCodec::availableCodecs())
	{
		const QString strName = QString::fromLatin1(codec);
		if (!seen.contains(strName))
		{
			seen.insert(strName);
			names.append(strName);
		}
	}
	return names;
}

bool CEditorWidget::SetSaveEncoding(const QString& strCodecName)
{
	const QByteArray name = strCodecName.toLatin1();

	// A name QStringConverter knows goes to the BUILTIN path even though
	// QTextCodec would also accept it. That is deliberate: those are the
	// encodings whose byte-for-byte behaviour this port has tests for, and
	// routing them through the compatibility module instead would quietly
	// change which bytes a UTF-8 save produces.
	if (const std::optional<QStringConverter::Encoding> builtin =
			QStringConverter::encodingForName(name.constData()))
	{
		m_Encoding = *builtin;
		m_CodecName.clear();
		return true;
	}

	QTextCodec* pCodec = QTextCodec::codecForName(name);
	if (pCodec == nullptr)
	{
		return false;
	}
	m_CodecName = pCodec->name();
	// m_bHasBom is deliberately LEFT ALONE. Forcing it false here was the first
	// version, and mutation testing showed the line was both untested and
	// wrong: the no-BOM invariant is enforced in EncodeForSave, which passes
	// IgnoreHeader and never consults m_bHasBom on this path - so clearing it
	// bought nothing - while it PERMANENTLY destroyed the mark for a document
	// that went UTF-8 -> some codepage -> UTF-8, because the builtin branch
	// above has nothing to restore it from. The BOM belongs to the file as it
	// was read, so it survives a visit to a codepage.
	return true;
}

bool CEditorWidget::ReloadWithEncoding(const QString& strCodecName, QString& strErrorOut)
{
	// An untitled document has no bytes on disk, so there is nothing to
	// reinterpret. Refused rather than silently treated as an empty file,
	// which would throw the user's typing away.
	if (IsUntitled())
	{
		strErrorOut = tr("This document has never been saved, so there is nothing "
			"on disk to re-read.");
		return false;
	}

	const QString strPath = m_strFilePath;
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

	// Resolve the encoding BEFORE touching the document, so an unknown codec
	// leaves the open file exactly as it was.
	const QStringConverter::Encoding previousEncoding = m_Encoding;
	const QByteArray previousCodec = m_CodecName;
	const bool bPreviousBom = m_bHasBom;
	if (!SetSaveEncoding(strCodecName))
	{
		strErrorOut = tr("%1 is not an encoding this build knows.").arg(strCodecName);
		return false;
	}

	// A BOM belongs to the bytes, not to the choice: re-reading as UTF-16 a
	// file that has no BOM must not then WRITE one back. So the mark is
	// re-derived from what is actually there.
	//
	// AND IT HAS TO BELONG TO **THIS** ENCODING. has_value() alone was the
	// first version and it asked only "do these bytes start with some mark
	// anybody would recognise" - so reinterpreting a UTF-8-with-BOM file left
	// the document claiming one whatever it was now being read as. Two
	// consequences, both measured:
	//
	//   as Latin-1   the label read "Latin-1 BOM", which is meaningless - the
	//                EF BB BF are three ordinary characters now. Bytes were
	//                unharmed, because Latin-1 ignores WriteBom.
	//   as UTF-16LE  the save INJECTED an FF FE the file never had, because
	//                that encoder does honour it.
	//
	// Comparing against the resolved encoding asks the right question.
	m_bHasBom = m_CodecName.isEmpty()
		&& QStringConverter::encodingForData(raw) == m_Encoding;

	const QString strText = DecodeBytes(raw);
	if (strText.isNull())
	{
		m_Encoding = previousEncoding;
		m_CodecName = previousCodec;
		m_bHasBom = bPreviousBom;
		strErrorOut = tr("%1 could not decode %2.").arg(strCodecName, strPath);
		return false;
	}

	const QByteArray utf8 = strText.toUtf8();
	Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(utf8.constData()));
	Send(SCI_EMPTYUNDOBUFFER);
	// A reinterpretation is not an edit - the file on disk still matches what
	// is now on screen, so the document is CLEAN. Leaving it modified would
	// invite the user to "save" a reinterpretation they were only inspecting.
	//
	// Redundant with SCI_EMPTYUNDOBUFFER above, which also clears the modified
	// flag - so a mutation removing THIS line alone is not caught, and the
	// self-test check covers the property rather than the statement. Kept
	// because it states the intent, and because LoadFile does both too.
	Send(SCI_SETSAVEPOINT);
	Send(SCI_GOTOPOS, 0);
	DetectEol(utf8);
	return true;
}

QString CEditorWidget::GetEncodingName() const
{
	return m_CodecName.isEmpty()
		? QString::fromLatin1(QStringConverter::nameForEncoding(m_Encoding))
		: QString::fromLatin1(m_CodecName);
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

int CEditorWidget::GetLineCount() const
{
	return static_cast<int>(Send(SCI_GETLINECOUNT));
}

int CEditorWidget::GetCaretPosition() const
{
	return static_cast<int>(Send(SCI_GETCURRENTPOS));
}

//////////////////////////////////////////////////////////////////////////
// Navigation
//
// Where src/GotoDlg.cpp's GO buttons end up. The dialog itself does not survive
// the port - see doc/PORTING.md 6l - but these are its behaviour, transcribed.

void CEditorWidget::ExpandAllFoldings()
{
	Send(SCI_FOLDALL, SC_FOLDACTION_EXPAND);
}

void CEditorWidget::SetFirstVisibleLine(int nLine)
{
	if (nLine < 0)
	{
		return;
	}
	// Through the VISIBLE line space, not the document one. With folds collapsed
	// or lines wrapped the two differ, and SCI_SETFIRSTVISIBLELINE speaks the
	// visible space - handing it a document number would scroll to the wrong
	// place by however many lines are hidden above it.
	const sptr_t nDisplayed = Send(SCI_GETFIRSTVISIBLELINE);
	const sptr_t nDocLine = Send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(nDisplayed));
	if (nDocLine != nLine)
	{
		Send(SCI_SETFIRSTVISIBLELINE,
			static_cast<uptr_t>(Send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(nLine))));
	}
}

void CEditorWidget::SetLineCenterDisplay(int nLine)
{
	if (nLine < 0)
	{
		return;
	}
	// The original's arithmetic, kept: two lines of margin, then half a screen
	// above the target, clamped at the top of the document.
	const int nLinesOnScreen = static_cast<int>(Send(SCI_LINESONSCREEN)) - 2;
	int nStart = nLine - (nLinesOnScreen / 2);
	if (nStart < 0)
	{
		nStart = 0;
	}
	SetFirstVisibleLine(nStart);
}

void CEditorWidget::GotoLine(int nLine)
{
	// The guard is < 0 and not < 1, which is the original's, and it is load
	// bearing rather than sloppy: line 0 is what an EMPTY field gives (the MFC
	// runs the text through _ttoi, which returns 0 for ""), so it reaches
	// SCI_GOTOLINE with -1 and Scintilla clamps that to the first line. Pressing
	// GO on an empty box goes to the top of the document, on both frontends.
	if (nLine < 0)
	{
		return;
	}
	ExpandAllFoldings();
	Send(SCI_GOTOLINE, static_cast<uptr_t>(nLine - 1));
	// GetCaretLine() is 1-based and SetLineCenterDisplay indexes document lines
	// from 0, so the view settles one line off centre. That is the original's
	// behaviour (src/Editor.cpp:2026 passes GetCurrentLine() straight in) and it
	// is reproduced rather than corrected, like the unsorted autocomplete list in
	// doc/PORTING.md 6j: a one-line difference in scroll position is not worth
	// the two frontends scrolling differently.
	SetLineCenterDisplay(GetCaretLine());
}

void CEditorWidget::GotoPosition(int nPosition)
{
	// NO guard and NO centring, unlike GotoLine. Both asymmetries are the
	// original's (src/Editor.cpp:1145): an out-of-range offset is Scintilla's to
	// clamp, and the view is left to SCI_GOTOPOS's own scrolling. Adding either
	// here would make the two frontends behave differently for the same input.
	ExpandAllFoldings();
	Send(SCI_GOTOPOS, static_cast<uptr_t>(nPosition));
}

void CEditorWidget::GotoPreviousParagraph()
{
	Send(SCI_PARAUP);
}

void CEditorWidget::GotoNextParagraph()
{
	Send(SCI_PARADOWN);
}

void CEditorWidget::ScrollToCaret()
{
	SetLineCenterDisplay(GetCaretLine());
}

void CEditorWidget::OnMarginClicked(Scintilla::Position position,
	Scintilla::KeyMod modifiers, int nMargin)
{
	Q_UNUSED(modifiers);
	if (nMargin != MARGIN_SYMBOLS)
	{
		return;
	}
	const int nLine = static_cast<int>(Send(SCI_LINEFROMPOSITION,
		static_cast<uptr_t>(position))) + 1;
	emit BookmarkToggleRequested(nLine);
}

//////////////////////////////////////////////////////////////////////////
// Bookmarks
//
// Every mask/number distinction here is one src/Editor.cpp gets wrong. See the
// header, and doc/PORTING.md 6o.

bool CEditorWidget::IsLineBookmarked(int nLine) const
{
	if (nLine < 1)
	{
		return false;
	}
	// A BIT TEST, not an equality. SCI_MARKERGET returns every marker on the
	// line as one mask, so CEditorCtrl::IsLineHasBookMark's `== 8 || == 9`
	// answers correctly only when the bookmark is alone or sits with marker 0.
	// A bookmark alongside a DISABLED breakpoint is mask 10 and reports as no
	// bookmark. Its own comment says "check mask for markerbit 0"; the bookmark
	// is bit 3.
	const sptr_t nMask = Send(SCI_MARKERGET, static_cast<uptr_t>(nLine - 1));
	return (nMask & MARKER_BOOKMARK_MASK) != 0;
}

void CEditorWidget::ToggleBookmark(int nLine)
{
	if (nLine < 1)
	{
		return;
	}
	if (IsLineBookmarked(nLine))
	{
		Send(SCI_MARKERDELETE, static_cast<uptr_t>(nLine - 1), MARKER_BOOKMARK);
	}
	else
	{
		Send(SCI_MARKERADD, static_cast<uptr_t>(nLine - 1), MARKER_BOOKMARK);
	}
}

void CEditorWidget::ClearBookmarks()
{
	// A marker NUMBER here, which is what this one wants and what the MFC
	// correctly passes.
	Send(SCI_MARKERDELETEALL, MARKER_BOOKMARK);
}

bool CEditorWidget::HasBookmarks() const
{
	// A MASK. CEditorCtrl::HasBookmarks passes SC_SETMARGINTYPE_MAKER, which is
	// 1 - the margin's number, used where a marker mask belongs. Mask 1 is
	// marker 0, the enabled breakpoint, so on Windows this function answers a
	// question about breakpoints and is named for bookmarks.
	return Send(SCI_MARKERNEXT, 0, MARKER_BOOKMARK_MASK) >= 0;
}

QList<int> CEditorWidget::BookmarkedLines() const
{
	QList<int> lines;
	sptr_t nLine = Send(SCI_MARKERNEXT, 0, MARKER_BOOKMARK_MASK);
	while (nLine >= 0)
	{
		lines.append(static_cast<int>(nLine) + 1);		// 1-based, as shown
		nLine = Send(SCI_MARKERNEXT, static_cast<uptr_t>(nLine + 1),
			MARKER_BOOKMARK_MASK);
	}
	return lines;
}

int CEditorWidget::NextBookmark()
{
	const int nFrom = GetCaretLine();		// 1-based
	sptr_t nLine = Send(SCI_MARKERNEXT, static_cast<uptr_t>(nFrom), MARKER_BOOKMARK_MASK);
	if (nLine < 0)
	{
		// Wrap. The MFC does not - its FindNextBreakPoint simply stops at the
		// last one - but a bookmark ring that dead-ends is a worse answer than
		// one that comes round, and every editor with this feature wraps.
		nLine = Send(SCI_MARKERNEXT, 0, MARKER_BOOKMARK_MASK);
	}
	if (nLine < 0)
	{
		return 0;
	}
	GotoLine(static_cast<int>(nLine) + 1);
	return static_cast<int>(nLine) + 1;
}

int CEditorWidget::PreviousBookmark()
{
	const int nFrom = GetCaretLine();		// 1-based
	sptr_t nLine = Send(SCI_MARKERPREVIOUS, static_cast<uptr_t>(nFrom - 2),
		MARKER_BOOKMARK_MASK);
	if (nLine < 0)
	{
		nLine = Send(SCI_MARKERPREVIOUS, static_cast<uptr_t>(GetLineCount()),
			MARKER_BOOKMARK_MASK);
	}
	if (nLine < 0)
	{
		return 0;
	}
	GotoLine(static_cast<int>(nLine) + 1);
	return static_cast<int>(nLine) + 1;
}

QString CEditorWidget::TextOfLine(int nLine) const
{
	if (nLine < 1 || nLine > GetLineCount())
	{
		return QString();
	}
	const sptr_t nLength = Send(SCI_LINELENGTH, static_cast<uptr_t>(nLine - 1));
	if (nLength <= 0)
	{
		return QString();
	}
	QByteArray buffer(static_cast<int>(nLength) + 1, '\0');
	Send(SCI_GETLINE, static_cast<uptr_t>(nLine - 1),
		reinterpret_cast<sptr_t>(buffer.data()));
	buffer.truncate(static_cast<int>(nLength));
	return QString::fromUtf8(buffer).trimmed();
}

//////////////////////////////////////////////////////////////////////////
// Line transforms
//
// See the header for the two defects in src/EditorView.cpp that this
// deliberately does not reproduce, and doc/PORTING.md 6p.

int CEditorWidget::ApplyLineTransform(const FLineTransform& fTransform)
{
	// Scope: the selection expanded to whole lines, or the whole document.
	// CEditorView branches on GetSelectedText().IsEmpty() for the same choice.
	const sptr_t nSelStart = Send(SCI_GETSELECTIONSTART);
	const sptr_t nSelEnd = Send(SCI_GETSELECTIONEND);
	const bool bHasSelection = nSelEnd > nSelStart;

	const int nLineCount = GetLineCount();
	int nFirstLine = 1;
	int nLastLine = nLineCount;
	if (bHasSelection)
	{
		nFirstLine = static_cast<int>(Send(SCI_LINEFROMPOSITION,
			static_cast<uptr_t>(nSelStart))) + 1;
		nLastLine = static_cast<int>(Send(SCI_LINEFROMPOSITION,
			static_cast<uptr_t>(nSelEnd))) + 1;
	}
	if (nFirstLine > nLastLine)
	{
		return 0;
	}

	// The byte range being replaced: from the start of the first line to the
	// END OF THE LAST LINE'S TEXT, not including its line ending.
	// SCI_GETLINEENDPOSITION excludes the EOL, which is what makes the trailing
	// newline survivable - the transform never owns the final terminator.
	const sptr_t nFrom = Send(SCI_POSITIONFROMLINE,
		static_cast<uptr_t>(nFirstLine - 1));
	const sptr_t nTo = Send(SCI_GETLINEENDPOSITION,
		static_cast<uptr_t>(nLastLine - 1));

	// The line ending to join with, as Scintilla reports it for this document.
	const char* szEol = "\n";
	switch (Send(SCI_GETEOLMODE))
	{
	case SC_EOL_CRLF:	szEol = "\r\n"; break;
	case SC_EOL_CR:		szEol = "\r"; break;
	default:			szEol = "\n"; break;
	}
	const QByteArray eol(szEol);

	QByteArray replacement;
	int nIndex = 0;
	int nEmitted = 0;
	for (int nLine = nFirstLine; nLine <= nLastLine; ++nLine)
	{
		// The line's own text, without its EOL - TextOfLine trims, which is
		// wrong here, so the bytes are taken directly.
		const sptr_t nLineFrom = Send(SCI_POSITIONFROMLINE,
			static_cast<uptr_t>(nLine - 1));
		const sptr_t nLineTo = Send(SCI_GETLINEENDPOSITION,
			static_cast<uptr_t>(nLine - 1));
		QByteArray raw(static_cast<int>(nLineTo - nLineFrom) + 1, '\0');
		Send(SCI_SETTARGETRANGE, static_cast<uptr_t>(nLineFrom), nLineTo);
		Send(SCI_GETTARGETTEXT, 0, reinterpret_cast<sptr_t>(raw.data()));
		raw.truncate(static_cast<int>(nLineTo - nLineFrom));

		SLineContext context;
		context._Index = nIndex;
		context._Line = nLine;
		context._Total = nLastLine - nFirstLine + 1;
		context._Eol = QString::fromLatin1(eol);
		const std::optional<QString> result = fTransform(QString::fromUtf8(raw), context);
		++nIndex;
		if (!result.has_value())
		{
			continue;			// the line is dropped, which "remove lines
								// containing X" needs
		}
		if (nEmitted > 0)
		{
			replacement += eol;
		}
		replacement += result->toUtf8();
		++nEmitted;
	}

	// ONE undo action, so a single Ctrl+Z reverts the whole transform rather
	// than unpicking it line by line.
	Send(SCI_BEGINUNDOACTION);
	Send(SCI_SETTARGETRANGE, static_cast<uptr_t>(nFrom), nTo);
	// SCI_REPLACETARGET takes a BYTE length, and this passes one - which is the
	// second defect the header names. The original converted to UTF-8 and then
	// handed SCI_ADDTEXT a wide-character count, truncating any non-ASCII line.
	Send(SCI_REPLACETARGET, static_cast<uptr_t>(replacement.size()),
		reinterpret_cast<sptr_t>(replacement.constData()));
	Send(SCI_ENDUNDOACTION);

	return nEmitted;
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

namespace
{
	// SCI_REPLACETARGETRE honours \1..\9 back-references and is what the MFC
	// uses whenever SCFIND_REGEXP is set (src/Editor.cpp:3378, :3524). With a
	// plain search the two differ: REPLACETARGETRE would treat a backslash in
	// the replacement as an escape, so a literal replacement must not use it.
	unsigned int ReplaceMessage(bool bRegex)
	{
		return bRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET;
	}
}

bool CEditorWidget::ReplaceNext(const QString& strPattern, const QString& strReplacement,
	const SFindOptions& options)
{
	if (strPattern.isEmpty())
	{
		return false;
	}
	const QByteArray pattern = strPattern.toUtf8();
	const QByteArray replacement = strReplacement.toUtf8();
	Send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(ToSearchFlags(options)));

	// From the selection's START, not its end: if the selection IS the match -
	// which it is after a Find - this replaces the thing the user is looking at
	// rather than the one after it. CEditorCtrl::ReplaceNext does the same.
	const sptr_t nFrom = Send(SCI_GETSELECTIONSTART);
	const sptr_t nDocEnd = Send(SCI_GETLENGTH);

	Send(SCI_SETTARGETSTART, static_cast<uptr_t>(nFrom), 0);
	Send(SCI_SETTARGETEND, static_cast<uptr_t>(nDocEnd), 0);
	sptr_t nFound = Send(SCI_SEARCHINTARGET, static_cast<uptr_t>(pattern.size()),
		reinterpret_cast<sptr_t>(pattern.constData()));

	if (nFound < 0)
	{
		// Wrap once, as FindNext does - otherwise Replace stops working as soon
		// as the caret is past the last match, with no indication why.
		Send(SCI_SETTARGETSTART, 0, 0);
		Send(SCI_SETTARGETEND, static_cast<uptr_t>(nFrom), 0);
		nFound = Send(SCI_SEARCHINTARGET, static_cast<uptr_t>(pattern.size()),
			reinterpret_cast<sptr_t>(pattern.constData()));
		if (nFound < 0)
		{
			return false;
		}
	}

	const sptr_t nReplaced = Send(ReplaceMessage(options._Regex),
		static_cast<uptr_t>(replacement.size()),
		reinterpret_cast<sptr_t>(replacement.constData()));

	// Leave the replacement selected. The user's next Replace then acts on the
	// following match, and the one just made is visible as the thing that
	// changed.
	Send(SCI_SETSEL, static_cast<uptr_t>(nFound), nFound + nReplaced);
	Send(SCI_SCROLLCARET);
	return true;
}

int CEditorWidget::ReplaceAll(const QString& strPattern, const QString& strReplacement,
	const SFindOptions& options)
{
	if (strPattern.isEmpty())
	{
		return 0;
	}
	const QByteArray pattern = strPattern.toUtf8();
	const QByteArray replacement = strReplacement.toUtf8();

	// Where the user was, so they can be put back. CEditorCtrl::ReplaceAll
	// restores both the caret line and the first visible line; replacing 200
	// matches and landing at the bottom of the file is disorienting.
	const sptr_t nCaretLine = Send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(
		Send(SCI_GETCURRENTPOS)));
	const sptr_t nFirstVisible = Send(SCI_GETFIRSTVISIBLELINE);

	Send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(ToSearchFlags(options)));

	// Group the run so one Ctrl+Z takes back the whole replace-all. This is the
	// documented way to guarantee it - but be clear about what is actually
	// demonstrated: removing the pair changes nothing this self-test can see,
	// because Scintilla already coalesces the four adjacent same-length
	// replacements the test makes. Verified by A/B - one undo reverts fully
	// either way, and SCI_CANUNDO is 0 after, both with and without.
	//
	// Kept because the coalescing is Scintilla's business and not a contract,
	// and because explicit grouping is what the documentation asks for. But it
	// is unproven here, not proven.
	Send(SCI_BEGINUNDOACTION);

	int nCount = 0;
	sptr_t nFrom = 0;
	while (true)
	{
		// The document length is re-read every iteration: each replacement
		// changes it, and a stale end bound would either stop early or search
		// past the buffer.
		const sptr_t nDocEnd = Send(SCI_GETLENGTH);
		if (nFrom > nDocEnd)
		{
			break;
		}
		Send(SCI_SETTARGETSTART, static_cast<uptr_t>(nFrom), 0);
		Send(SCI_SETTARGETEND, static_cast<uptr_t>(nDocEnd), 0);
		const sptr_t nFound = Send(SCI_SEARCHINTARGET, static_cast<uptr_t>(pattern.size()),
			reinterpret_cast<sptr_t>(pattern.constData()));
		if (nFound < 0)
		{
			break;
		}
		const sptr_t nMatchLength = Send(SCI_GETTARGETEND) - Send(SCI_GETTARGETSTART);
		const sptr_t nReplaced = Send(ReplaceMessage(options._Regex),
			static_cast<uptr_t>(replacement.size()),
			reinterpret_cast<sptr_t>(replacement.constData()));
		++nCount;

		// THE GUARD THE ORIGINAL DOES NOT HAVE. CEditorCtrl::ReplaceAll advances
		// to nFound + nReplaced. A zero-width match - regex "^", "\b", "x*" -
		// replaced by the empty string gives nReplaced == 0, so the target start
		// does not move, the same match is found again, and the loop never ends.
		// Same shape as the CDiffEngine hang in doc/PORTING.md 6c correction 4:
		// it allocates nothing, so it pins a core rather than crashing.
		//
		// Stepping one past a zero-length replacement cannot change any output
		// that previously existed, because the states it excludes are exactly
		// the ones that did not terminate.
		nFrom = nFound + nReplaced;
		if (nMatchLength == 0 && nReplaced == 0)
		{
			++nFrom;
		}
	}

	Send(SCI_ENDUNDOACTION);

	// Back where they were. GOTOLINE first, then the scroll position, because
	// moving the caret scrolls.
	Send(SCI_GOTOLINE, static_cast<uptr_t>(nCaretLine));
	Send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(nFirstVisible));
	return nCount;
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
