/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "BookmarkPane.h"

#include <QHeaderView>
#include <QTreeWidget>

CBookmarkPane::CBookmarkPane(QWidget* pParent)
	: QDockWidget(tr("Bookmarks"), pParent)
{
	// LOAD-BEARING, not tidiness: QMainWindow::saveState keys docks by
	// objectName, so a pane without one is not restored. §6k learned this by
	// mutation - removing it fails both persistence checks.
	setObjectName(QStringLiteral("BookmarkPane"));

	m_pList = new QTreeWidget(this);
	m_pList->setColumnCount(3);
	// src/BookmarkWindow.cpp:435-437's columns, in its order.
	m_pList->setHeaderLabels({ tr("Line Number"), tr("File Path"), tr("Line Text") });
	m_pList->setRootIsDecorated(false);
	m_pList->setAllColumnsShowFocus(true);
	m_pList->setUniformRowHeights(true);
	m_pList->setColumnWidth(0, 90);
	m_pList->setColumnWidth(1, 320);
	setWidget(m_pList);

	// A single click jumps, which is what CBookmarkDlg::OnClickList does -
	// itemActivated would need a double click on most platforms and this is a
	// navigation list, not a file manager.
	connect(m_pList, &QTreeWidget::itemClicked, this,
		[this](QTreeWidgetItem* pItem, int)
	{
		if (pItem == nullptr)
		{
			return;
		}
		emit BookmarkActivated(pItem->data(0, Qt::UserRole).toString(),
			pItem->text(1), pItem->text(0).toInt());
	});
}

void CBookmarkPane::SetEntries(const QList<SEntry>& entries)
{
	m_pList->clear();
	for (const SEntry& entry : entries)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem(m_pList,
			{ QString::number(entry._Line), entry._File, entry._Text });
		// The path rides on the item rather than in a column: an untitled
		// document has none, and the File column shows the display name so the
		// row is still identifiable.
		pItem->setData(0, Qt::UserRole, entry._Path);
	}
	// The count in the title, so the pane says how many there are without
	// having to be read. CBookmarkWindow does not do this; MessagePane's
	// precedent and the window manager's both do.
	setWindowTitle(entries.isEmpty()
		? tr("Bookmarks") : tr("Bookmarks (%1)").arg(entries.size()));
}

int CBookmarkPane::RowCount() const
{
	return m_pList->topLevelItemCount();
}

QString CBookmarkPane::RowText(int nRow, int nColumn) const
{
	if (nRow < 0 || nRow >= m_pList->topLevelItemCount())
	{
		return QString();
	}
	return m_pList->topLevelItem(nRow)->text(nColumn);
}

bool CBookmarkPane::ActivateRowForTest(int nRow)
{
	if (nRow < 0 || nRow >= m_pList->topLevelItemCount())
	{
		return false;
	}
	// Through the same signal a click produces, so the check exercises the
	// shipped path rather than a re-implementation of it - the lesson from §6n.
	emit m_pList->itemClicked(m_pList->topLevelItem(nRow), 0);
	return true;
}
