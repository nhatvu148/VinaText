/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Every bookmark in every open document, and a way back to each.
//
// The second dock pane, and - after §6n corrected the inventory - THE LAST ONE
// in D10's kept set. It follows §6k's framework exactly: a QDockWidget, an
// objectName that saveState keys on, a toggleViewAction for the View menu, and
// geometry through the existing SaveDockState. Nothing new was needed, which is
// what a reviewed pattern being reused is supposed to look like.
//
// THE MARKERS ARE THE TRUTH. src/BookmarkWindow.cpp keeps a parallel
// std::vector<BOOKMARK_LINE_DATA> and appends to it when a bookmark is added,
// removing when one is deleted - but Scintilla MOVES its markers as the
// document is edited, and nothing updates the stored line numbers. Insert a
// line above a bookmark on Windows and the pane still names the old number.
// This pane is rebuilt from SCI_MARKERNEXT instead, so it cannot drift.
//
// See doc/PORTING.md 6o.

#pragma once

#include <QDockWidget>
#include <QList>
#include <QString>

class QTreeWidget;

class CBookmarkPane final : public QDockWidget
{
	Q_OBJECT

public:
	struct SEntry
	{
		QString _File;			// display name of the document
		QString _Path;			// full path, empty for an untitled document
		int _Line = 0;			// 1-based
		QString _Text;
	};

	explicit CBookmarkPane(QWidget* pParent = nullptr);

	void SetEntries(const QList<SEntry>& entries);
	int RowCount() const;
	QString RowText(int nRow, int nColumn) const;
	// For the self-test, which cannot click a row.
	bool ActivateRowForTest(int nRow);

signals:
	// The pane asks; the window does. Same split as the window manager, and for
	// the same reason - it keeps the pane testable without a CMainWindow.
	void BookmarkActivated(const QString& strPath, const QString& strFile, int nLine);

private:
	QTreeWidget* m_pList = nullptr;
};
