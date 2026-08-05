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

#include <optional>

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

	// Re-reads every setting that is not a theme colour and applies it. Called
	// from the constructor and again whenever Preferences changes something.
	//
	// It exists because these calls WERE in the constructor, where they ran
	// once and could never be re-applied - a changed setting reached the file
	// and the running editor never heard about it. Found by a self-test check
	// asserting that an applied change reaches an open document, which failed.
	void ApplySettings();

	// View toggles, mirroring CEditorCtrl::EnableTextWrappingMode and
	// EnableLongLineChecker. Both start off, as they do on Windows.
	void SetWordWrap(bool bEnable);
	bool IsWordWrap() const;
	void SetLongLineMarker(bool bEnable);
	bool IsLongLineMarker() const;

	//////////////////////////////////////////////////////////////////////
	// Encoding
	//
	// THE TWO OPERATIONS ARE NOT THE SAME OPERATION, and the whole design
	// rests on keeping them apart. src/CodePageMFCDlg.cpp does too - one
	// dialog with an m_bReopen mode flag, two callers, and even a different
	// OK button caption ("Reopen File" versus "Save File"):
	//
	//   REINTERPRET  re-reads the bytes on disk as a different encoding.
	//                Nothing is written. The bytes are the truth and the
	//                text was wrong. Discards unsaved edits, so it asks.
	//   CONVERT      re-writes the text in a different encoding. The text
	//                is the truth and the bytes change under it.
	//
	// Getting these the wrong way round loses data silently, which is the
	// failure this port has been most careful about - hence the byte-identical
	// round-trip checks.

	// The encoding this document will be WRITTEN in, as a codec name.
	QString GetEncodingName() const;

	// Re-read the file as strCodecName. The caller must have dealt with any
	// unsaved changes first - this throws them away. Fails on an untitled
	// document, which has no bytes on disk to reinterpret.
	bool ReloadWithEncoding(const QString& strCodecName, QString& strErrorOut);

	// Change the encoding used by the next save. Does NOT write anything -
	// the caller saves, exactly as CEditorDoc::OnFileSaveAsEncoding does.
	// Returns false if the name is not a codec this build has.
	bool SetSaveEncoding(const QString& strCodecName);

	// Every codec the running Qt can do, for the picker. QStringConverter's
	// own set first, then QTextCodec's, deduplicated.
	static QStringList AvailableEncodings();

	// Status-bar material.
	QString GetLanguageLabel() const;
	QString GetEncodingLabel() const;
	QString GetEolLabel() const;
	int GetCaretLine() const;			// 1-based, as shown to the user
	int GetCaretColumn() const;			// 1-based
	int GetSelectedCharacterCount() const;
	int GetLineCount() const;
	int GetCaretPosition() const;		// byte offset, as SCI_GETCURRENTPOS gives it

	// Navigation - the four GO buttons of src/GotoDlg.cpp, minus the one that is
	// not ported. Transcribes CEditorCtrl::GotoLine (src/Editor.cpp:2020) and
	// GotoPosition (:1145), whose asymmetry is deliberate and preserved: see the
	// .cpp. GotoLine takes the 1-based number the user types.
	void GotoLine(int nLine);
	void GotoPosition(int nPosition);
	// SCI_PARAUP / SCI_PARADOWN, from the Goto tab's two paragraph buttons. Also
	// reachable from the MFC's Edit menu (src/EditorView.cpp:3185-3193), which is
	// the shape they are ported in here.
	void GotoPreviousParagraph();
	void GotoNextParagraph();
	// The tab's "Goto Caret >> |" button: scroll the view back to the caret,
	// without moving it. CEditorCtrl::SetLineCenterDisplay(GetCurrentLine()).
	void ScrollToCaret();

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
	// Forces the stored codec name, so the self-test can drive the
	// codec-went-away path. SetSaveEncoding refuses names that do not resolve,
	// so there is no other way to reach it - and without a seam it would be
	// one more branch documented as uncovered instead of checked.
	void SetCodecNameForTest(const QByteArray& name) { m_CodecName = name; }

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

	// Both goto paths expand every fold first: a line inside a collapsed fold
	// cannot hold a caret, so jumping to it without expanding lands elsewhere.
	// CEditorCtrl::ExpandAllFoldings (src/Editor.cpp:3820).
	void ExpandAllFoldings();
	// Puts nLine roughly in the middle of the viewport. Transcribes
	// CEditorCtrl::SetLineCenterDisplay (:2034) and SetFirstVisibleLine (:1077).
	void SetLineCenterDisplay(int nLine);
	void SetFirstVisibleLine(int nLine);

	void ApplyEditorStyles(const Core::CEditorTheme& theme);
	void ApplyLanguageStyles(const Core::CEditorTheme& theme);
	void ApplyFoldMargin(const Core::CEditorTheme& theme);
	void UpdateLineNumberMargin();
	void DetectEol(const QByteArray& utf8);

	// The document's bytes, as they will be written back. Reading a UTF-16 file
	// and saving it as UTF-8 would be data loss the user never asked for, so the
	// encoding and the byte-order mark are properties of the document, not
	// assumptions.
	// Empty when the document names a codec this build no longer has. The
	// caller must then REFUSE the save - see the .cpp.
	std::optional<QByteArray> EncodeForSave(const QString& strText) const;
	// The other direction, and the only place the two encoding paths are
	// chosen between. Both go through here so they cannot drift apart.
	QString DecodeBytes(const QByteArray& raw) const;

	// The selection background, as the active theme resolves it. Cached because
	// UpdateSelectionPainting runs on every caret move and re-resolving a role
	// through two std::map lookups on each one would be work for nothing.
	Core::SColor				m_SelectionBack;
	bool						m_bHaveSelectionBack = false;

	const CEditorData&			m_Data;
	QString						m_strFilePath;
	const Core::SLanguageInfo*	m_pLanguage = nullptr;
	EEditorTheme				m_Theme = EEditorTheme::Dark;
	// The encoding, in two halves, because Qt 6 splits the job.
	//
	// m_Encoding is used whenever m_CodecName is EMPTY, and covers everything
	// QStringConverter can express - the Unicode encodings and Latin-1, which
	// is exactly the set the byte-identical round-trip fixtures exercise.
	// m_CodecName names a QTextCodec for anything beyond that (windows-1258
	// and the other 800), a module already linked because Scintilla's own Qt
	// binding needs it.
	//
	// INVARIANT: nothing written on the codec path carries a byte-order mark.
	// Enforced in EncodeForSave, which passes QTextCodec::IgnoreHeader and does
	// not consult m_bHasBom at all - NOT by clearing m_bHasBom, which would
	// destroy the mark for good on a UTF-8 -> codepage -> UTF-8 round trip.
	// Only the Unicode encodings have marks and all of those are expressible by
	// QStringConverter, so the whole BOM story stays on the path with tests.
	QStringConverter::Encoding	m_Encoding = QStringConverter::Utf8;
	QByteArray					m_CodecName;
	bool						m_bHasBom = false;
	int							m_nUntitledNumber = 0;
};
