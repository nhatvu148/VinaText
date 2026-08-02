/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Go to line, and go to offset: a strip below the editor, next to the find bar.
//
// NOT a dialog, and that is not a preference - src/GotoDlg.cpp is not a dialog
// either. IDD_POS is declared WS_CHILD and created as
// m_GotoDlg.Create(IDD_POS, &m_CTabCtrl) (src/SearchAndReplaceDlg.cpp:341), tab
// 2 of CSearchAndReplaceWindowDlg's tab control, beside Find, Replace and
// Bracket Outline. Its OnOK and OnCancel are overridden EMPTY, so it has no
// accept and no cancel; Escape hands focus back to the editor rather than
// closing anything. There is nothing there to make modal.
//
// ui-qt/ already ships tabs 0 and 1 of that same control as CFindBar, so putting
// goto in the same place reproduces the MFC's own grouping. The alternative -
// building the SearchAndReplaceWindow dock to host it - would mean standing up
// one of the six panes D10 defers in order to land one of the dialogs D10 keeps.
//
// A separate widget from CFindBar rather than a third mode of it. CFindBar is
// one widget in two modes BECAUSE find and replace share the pattern, the
// options and the match count; goto shares none of them, so folding it in would
// buy nothing and put an unrelated field on the same row.
//
// See doc/PORTING.md 6l, including the two operations of the tab that are not
// ported here and where the other three went.

#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

class CGotoBar final : public QWidget
{
	Q_OBJECT

public:
	explicit CGotoBar(QWidget* pParent = nullptr);

	// Takes focus and selects the line field, so typing replaces what is there.
	// nLineCount and nLength drive the range readouts, which the MFC refreshes
	// per document in CGotoDlg::InitGotoRangeByDocument.
	void Activate(int nLineCount, int nLength, int nCaretPosition);
	void SetDocumentRange(int nLineCount, int nLength);

	// Empty gives 0, matching what the MFC's _ttoi does with an empty edit - and
	// CEditorWidget::GotoLine documents why that is not the same as doing nothing.
	int GetLine() const;
	int GetOffset() const;

	// For the self-test, which cannot read a QLineEdit it did not type into.
	QString GetLineRangeText() const;

signals:
	void GotoLineRequested();
	void GotoOffsetRequested();
	void CloseRequested();

private:
	QLabel*		m_pLineRange = nullptr;
	QLineEdit*	m_pLine = nullptr;
	QLineEdit*	m_pOffset = nullptr;
};
