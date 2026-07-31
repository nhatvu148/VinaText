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
// only of the alpha's slice of it. No folding, no autocomplete, no bookmarks, no
// spell check, no print. Those are Phase 4 and later; see doc/QT-PORT-BRIEF.md
// D9 for what the alpha is fixed to.

#pragma once

#include "EditorData.h"

#include <ScintillaEditBase.h>

#include <QByteArray>
#include <QString>
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

	// Marks every match with an indicator and returns how many there are - what
	// the find bar counts. Leaves the caret and the selection alone.
	int HighlightMatches(const QString& strPattern, const SFindOptions& options);
	void ClearHighlight();

	sptr_t Send(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0) const
	{
		return send(iMessage, wParam, lParam);
	}

private:
	void ApplyEditorStyles(const Core::CEditorTheme& theme);
	void ApplyLanguageStyles(const Core::CEditorTheme& theme);
	void ApplyFoldMargin(const Core::CEditorTheme& theme);
	void OnMarginClicked(Scintilla::Position position, Scintilla::KeyMod modifiers, int margin);
	void UpdateLineNumberMargin();
	void DetectEol(const QByteArray& utf8);

	// The document's bytes, as they will be written back. Reading a UTF-16 file
	// and saving it as UTF-8 would be data loss the user never asked for, so the
	// encoding and the byte-order mark are properties of the document, not
	// assumptions.
	QByteArray EncodeForSave(const QString& strText) const;

	const CEditorData&			m_Data;
	QString						m_strFilePath;
	const Core::SLanguageInfo*	m_pLanguage = nullptr;
	EEditorTheme				m_Theme = EEditorTheme::Dark;
	QStringConverter::Encoding	m_Encoding = QStringConverter::Utf8;
	bool						m_bHasBom = false;
	int							m_nUntitledNumber = 0;
};
