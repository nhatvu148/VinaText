/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The message log pane - the first of Phase 5's nine dock panes.
//
// The Qt counterpart of CMessagePane (src/MessageWindow.cpp, 582 lines), whose
// whole public surface is two methods: AddLogMessage(text, colour) and
// ClearAll(). Everything else in that file is MFC scaffolding - a CDockablePane,
// a CDialogEx hosted inside it, a CRichEditCtrl subclass for the context menu and
// word wrap, and the DDX plumbing joining them. QDockWidget replaces the first
// two outright, and QPlainTextEdit the rest.
//
// Chosen as the first pane because it is the simplest one that is useful the
// moment it exists: ui-qt/ reports file errors in a QMessageBox and a status-bar
// message that disappears after five seconds, so there is currently nowhere for a
// user to look and see what happened.

#pragma once

#include <QColor>
#include <QDockWidget>

class QPlainTextEdit;

class CMessagePane final : public QDockWidget
{
	Q_OBJECT

public:
	explicit CMessagePane(QWidget* pParent = nullptr);

	// Appends one line in the given colour. A string that already ends in a
	// newline is not given another, which is what CMessagePaneDlg::AddLogMessage
	// does - it checks for '\n' anywhere in the string rather than at the end,
	// and that difference is preserved in the comment on the implementation.
	//
	// An empty string is ignored, as in the original.
	void AddLogMessage(const QString& strText, const QColor& colour);
	void ClearAll();

	// What the pane is showing. For the self-test, which cannot read a widget's
	// pixels but can check that a message arrived, in order, in the right colour.
	QString GetText() const;
	int GetLineCount() const;
	QColor GetLineColour(int nLine) const;

private:
	QPlainTextEdit*	m_pOutput = nullptr;
};
