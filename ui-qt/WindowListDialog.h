/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The window manager: every open document, and what you can do to it.
//
// A DIALOG, NOT A DOCK PANE, and that corrects the plan rather than departing
// from it. The brief's §3d lists OpenTabWindows among "9 dock panes" and D10
// keeps it as one of three - but `COpenTabWindows : CDlgBase` is opened with
// `dlg.DoModal()` from CMainFrame::OnWindowManager, and it is NOT among the
// eight classes that derive from CDockPaneBase. Re-derived rather than
// inherited from the document; see doc/PORTING.md 6n.
//
// So there is no §6k dock work here at all. The pattern this follows is the
// dialog one - CAboutDialog, CPreferencesDialog, CEncodingDialog.

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QDialogButtonBox;
class QPushButton;
class QTreeWidget;

class CWindowListDialog final : public QDialog
{
	Q_OBJECT

public:
	// One row. The path is empty for a document that has never been saved,
	// which the MFC renders as "N/A" - it tests PathFileExists rather than
	// asking the document, so an open file deleted underneath it reads the
	// same way. Reproduced.
	struct SEntry
	{
		QString _Name;
		QString _Path;
		bool _Modified = false;
	};

	explicit CWindowListDialog(QWidget* pParent = nullptr);

	// Rebuilds the list and re-titles the window. Called on open and again
	// after anything that changes the set of documents, which is what
	// COpenTabWindows::InitiateList does.
	void SetEntries(const QList<SEntry>& entries);

	QList<int> SelectedRows() const;

	// For the self-test, which cannot click a row or a button.
	bool SelectRowForTest(int nRow);
	// Selects SEVERAL rows. Needed because the descending-order guarantee is
	// invisible with one row selected - a one-element list is sorted both
	// ways, and the mutation reversing the sort went uncaught until this
	// existed.
	bool SelectRowsForTest(const QList<int>& rows);
	int RowCount() const;
	QString RowText(int nRow, int nColumn) const;
	QString CopiedPathForTest() const { return m_strLastCopied; }
	void TriggerForTest(const QString& strButton);

signals:
	// The dialog asks; the window does. It cannot reach the tabs itself, and
	// keeping it that way is what lets the self-test drive it without a
	// CMainWindow at all.
	void ActivateRequested(int nRow);
	void SaveRequested(int nRow);
	void CloseRequested(const QList<int>& rows);

private:
	void CopySelectedPath();

	QTreeWidget*	m_pList = nullptr;
	QPushButton*	m_pActivate = nullptr;
	QPushButton*	m_pSave = nullptr;
	QPushButton*	m_pClose = nullptr;
	QPushButton*	m_pCopyPath = nullptr;
	QString			m_strLastCopied;
};
