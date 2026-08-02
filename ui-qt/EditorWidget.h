/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// One open document: a ScintillaEditBase plus what the shell needs to know about
// the file behind it.
//
// This is the Qt counterpart of CEditorCtrl (src/Editor.cpp, 5,221 lines) - but
// only of the part Phase 4 has reached. Folding, brace and tag matching, URL
// hotspots and autocomplete are here; bookmarks, breakpoints, spell check and
// print are not. See doc/QT-PORT-BRIEF.md D9 and doc/PORTING.md 6d-6j.

#pragma once

#include "EditorData.h"

#include <ScintillaEditBase.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QStringConverter>

class CEditorWidget final : public ScintillaEditBase
{
	Q_OBJECT

public:
	explicit CEditorWidget(const CEditorData& data, QWidget* pParent = nullptr);

	// File name and path. An unsaved document has an empty path and a generated
	// display name ("Untitled 1").
	const QString& GetFilePath() const { return m_strFilePath; }
	QString GetDisplayName() const;
	bool IsUntitled() const { return m_strFilePath.isEmpty(); }
	bool IsModified() const;

	bool LoadFile(const QString& strPath, QString& strErrorOut);
	bool SaveFile(const QString& strPath, QString& strErrorOut);

	// Re-styles for the current language. Called on open, and again on every
	// theme switch.
	void ApplyTheme(EEditorTheme theme);

	// View toggles, mirroring CEditorCtrl::EnableTextWrappingMode and
	// EnableLongLineChecker. Both start off, as they do on Windows.
	void SetWordWrap(bool bEnable);
	bool IsWordWrap() const;
	void SetLongLineMarker(bool bEnable);
	bool IsLongLineMarker() const;

	// Status-bar material.
	QString GetLanguageLabel() const;
	QString GetEncodingLabel() const;
	QString GetEolLabel() const;
	int GetCaretLine() const;			// 1-based, as shown to the user
	int GetCaretColumn() const;			// 1-based
	int GetSelectedCharacterCount() const;

	// Find. Searches from the caret, wrapping once; leaves the match selected and
	// visible. Returns false when the pattern is not in the document at all.
	struct SFindOptions
	{
		bool _MatchCase = false;
		bool _WholeWord = false;
		bool _Regex = false;
		bool _Backward = false;
	};
	bool FindNext(const QString& strPattern, const SFindOptions& options);

	// Replaces the next match at or after the caret and selects the one after
	// it, wrapping once - the shape of CEditorCtrl::ReplaceNext. Returns false
	// when the pattern is not in the document.
	bool ReplaceNext(const QString& strPattern, const QString& strReplacement,
		const SFindOptions& options);

	// Replaces every match and returns how many. Transcribes
	// CEditorCtrl::ReplaceAll, including restoring the caret line and the first
	// visible line afterwards, so the view does not jump.
	int ReplaceAll(const QString& strPattern, const QString& strReplacement,
		const SFindOptions& options);

	// Marks every match with an indicator and returns how many there are - what
	// the find bar counts. Leaves the caret and the selection alone.
	int HighlightMatches(const QString& strPattern, const SFindOptions& options);
	void ClearHighlight();

	// The autocomplete list for the word being typed: the language's keywords
	// plus the words already in the document, both filtered by prefix. Public so
	// the self-test can check the list without synthesising a key press.
	QStringList GetAutoCompleteList(const QString& strPrefix) const;
	// For the self-test: drives OnCharAdded without synthesising a key event,
	// which offscreen cannot deliver to Scintilla reliably.
	void OnCharAddedForTest(int nChar) { OnCharAdded(nChar); }

	sptr_t Send(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0) const
	{
		return send(iMessage, wParam, lParam);
	}

private slots:
	// CEditorView's SCN_CHARADDED case (src/EditorView.cpp:6157-6172), reduced to
	// the autocomplete half.
	void OnCharAdded(int nChar);
	// Everything CEditorView's SCN_UPDATEUI case drives (src/EditorView.cpp:6179).
	// Scintilla sends this after any change to the text, the styling, the caret or
	// the selection, so it runs constantly - the MFC carries a comment saying so.
	void OnUpdateUi(Scintilla::Update updated);

private:
	// Highlights the brace under the caret and its partner, or clears both when
	// there is no match. Transcribes CEditorCtrl::DoBraceMatchHighlight.
	void UpdateBraceMatch();
	// The selection background and whether the caret line is drawn at all.
	// Transcribes CEditorCtrl::UpdateCaretLineVisible.
	void UpdateSelectionPainting();
	// Underlines every URL in the document. Transcribes
	// CEditorCtrl::RenderHotSpotForUrlLinks, and like the original it runs when
	// the editor is styled rather than on every keystroke.
	void RenderUrlHotspots();
	// Underlines the caret's enclosing XML/HTML tag pair. Only runs for the
	// languages core/ marks _TagMatch, and only with an empty selection - both
	// gates are CEditorView's (src/EditorView.cpp:6183-6190).
	// Transcribes CEditorCtrl::DoXMLHTMLTagsHightlight.
	void UpdateTagMatch();

	void ApplyEditorStyles(const Core::CEditorTheme& theme);
	void ApplyLanguageStyles(const Core::CEditorTheme& theme);
	void ApplyFoldMargin(const Core::CEditorTheme& theme);
	void UpdateLineNumberMargin();
	void DetectEol(const QByteArray& utf8);

	// The document's bytes, as they will be written back. Reading a UTF-16 file
	// and saving it as UTF-8 would be data loss the user never asked for, so the
	// encoding and the byte-order mark are properties of the document, not
	// assumptions.
	QByteArray EncodeForSave(const QString& strText) const;

	// The selection background, as the active theme resolves it. Cached because
	// UpdateSelectionPainting runs on every caret move and re-resolving a role
	// through two std::map lookups on each one would be work for nothing.
	Core::SColor				m_SelectionBack;
	bool						m_bHaveSelectionBack = false;

	const CEditorData&			m_Data;
	QString						m_strFilePath;
	const Core::SLanguageInfo*	m_pLanguage = nullptr;
	EEditorTheme				m_Theme = EEditorTheme::Dark;
	QStringConverter::Encoding	m_Encoding = QStringConverter::Utf8;
	bool						m_bHasBom = false;
	int							m_nUntitledNumber = 0;
};
