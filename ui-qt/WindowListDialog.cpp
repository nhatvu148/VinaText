/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "WindowListDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

// std::sort and std::greater. NOT reachable transitively by contract - this is
// the first use of either under ui-qt/, it compiles today only because libc++
// and libstdc++ happen to pull them in through Qt headers, and MSVC's STL is
// markedly less forgiving. The Qt CI matrix is [ubuntu, macos] with no Windows
// job, so NO CI JOB WOULD CATCH THIS - it would surface the day ui-qt/ is first
// built with MSVC. Found in review.
#include <algorithm>
#include <functional>

namespace
{
	// COpenTabWindows::InitiateList, which writes "N/A" when PathFileExists
	// fails rather than when the document is untitled.
	const char* const NOT_ON_DISK = "N/A";
}

CWindowListDialog::CWindowListDialog(QWidget* pParent)
	: QDialog(pParent)
{
	resize(700, 420);

	m_pList = new QTreeWidget(this);
	m_pList->setColumnCount(2);
	m_pList->setHeaderLabels({ tr("File Name"), tr("Full Path") });
	m_pList->setRootIsDecorated(false);
	m_pList->setAllColumnsShowFocus(true);
	m_pList->setUniformRowHeights(true);
	// Multiple, because Close Tabs works on a selection and Ctrl+A selects all
	// - both are COpenTabWindows behaviours.
	m_pList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pList->setColumnWidth(0, 200);

	m_pActivate = new QPushButton(tr("Activate"), this);
	m_pSave = new QPushButton(tr("Save"), this);
	m_pClose = new QPushButton(tr("Close Tab(s)"), this);
	m_pCopyPath = new QPushButton(tr("Copy Full Path"), this);

	QHBoxLayout* pButtons = new QHBoxLayout();
	pButtons->setContentsMargins(0, 0, 0, 0);
	pButtons->addWidget(m_pActivate);
	pButtons->addWidget(m_pSave);
	pButtons->addWidget(m_pClose);
	pButtons->addWidget(m_pCopyPath);
	pButtons->addStretch(1);

	QDialogButtonBox* pBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->addWidget(m_pList, 1);
	pLayout->addLayout(pButtons);
	pLayout->addWidget(pBox);

	auto CurrentRow = [this]
	{
		return m_pList->currentItem() == nullptr
			? -1 : m_pList->indexOfTopLevelItem(m_pList->currentItem());
	};

	connect(m_pActivate, &QPushButton::clicked, this, [this, CurrentRow]
	{
		if (CurrentRow() >= 0) { emit ActivateRequested(CurrentRow()); }
	});
	connect(m_pSave, &QPushButton::clicked, this, [this, CurrentRow]
	{
		if (CurrentRow() >= 0) { emit SaveRequested(CurrentRow()); }
	});
	connect(m_pClose, &QPushButton::clicked, this, [this]
	{
		const QList<int> rows = SelectedRows();
		if (!rows.isEmpty()) { emit CloseRequested(rows); }
	});
	connect(m_pCopyPath, &QPushButton::clicked, this, &CWindowListDialog::CopySelectedPath);
	// Double-click activates, as ON_NOTIFY(NM_DBLCLK) does.
	connect(m_pList, &QTreeWidget::itemDoubleClicked, this, [this, CurrentRow]
	{
		if (CurrentRow() >= 0) { emit ActivateRequested(CurrentRow()); }
	});
	connect(pBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	// Ctrl+Shift+C copies the path, which is COpenTabWindows'
	// PreTranslateMessage. Ctrl+A is left to QAbstractItemView, which already
	// implements select-all for an ExtendedSelection view - the MFC had to
	// hand-roll it because CListCtrl does not.
	QShortcut* pCopy = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
	pCopy->setContext(Qt::WidgetWithChildrenShortcut);
	connect(pCopy, &QShortcut::activated, this, &CWindowListDialog::CopySelectedPath);
}

void CWindowListDialog::SetEntries(const QList<SEntry>& entries)
{
	const int nPrevious = m_pList->currentItem() == nullptr
		? 0 : m_pList->indexOfTopLevelItem(m_pList->currentItem());

	m_pList->clear();
	for (const SEntry& entry : entries)
	{
		// A modified document is marked, which the MFC does not do - it offers
		// a Save button with no way of telling whether it would do anything.
		// One asterisk, the same mark the tab bar already uses.
		const QString strName = entry._Modified
			? entry._Name + QStringLiteral(" *") : entry._Name;
		new QTreeWidgetItem(m_pList, { strName,
			entry._Path.isEmpty() ? QLatin1String(NOT_ON_DISK) : entry._Path });
	}

	// The title carries the count, as COpenTabWindows::InitiateList sets it.
	setWindowTitle(tr("Current Windows (%1)").arg(entries.size()));

	// Keep the selection where it was if that row still exists, so closing one
	// of several tabs does not throw the user back to the top of the list.
	if (m_pList->topLevelItemCount() > 0)
	{
		const int nRow = qBound(0, nPrevious, m_pList->topLevelItemCount() - 1);
		m_pList->setCurrentItem(m_pList->topLevelItem(nRow));
	}
}

QList<int> CWindowListDialog::SelectedRows() const
{
	QList<int> rows;
	for (QTreeWidgetItem* pItem : m_pList->selectedItems())
	{
		rows.append(m_pList->indexOfTopLevelItem(pItem));
	}
	// Descending, so a caller closing tabs by index does not invalidate the
	// indices it has not used yet. The MFC walks the selection in order and
	// closes BY PATH, which sidesteps the problem; closing by row here does
	// not, so the order is load-bearing.
	std::sort(rows.begin(), rows.end(), std::greater<int>());
	return rows;
}

void CWindowListDialog::CopySelectedPath()
{
	QTreeWidgetItem* pItem = m_pList->currentItem();
	if (pItem == nullptr)
	{
		return;
	}
	const QString strPath = pItem->text(1);
	// Nothing to copy for a document that is not on disk. The MFC guards the
	// same way - PathFileExists before touching the clipboard - so an
	// untitled document silently copies nothing rather than the text "N/A".
	if (strPath == QLatin1String(NOT_ON_DISK))
	{
		return;
	}
	QApplication::clipboard()->setText(strPath);
	m_strLastCopied = strPath;
}

bool CWindowListDialog::SelectRowForTest(int nRow)
{
	if (nRow < 0 || nRow >= m_pList->topLevelItemCount())
	{
		return false;
	}
	m_pList->setCurrentItem(m_pList->topLevelItem(nRow));
	return true;
}

bool CWindowListDialog::SelectRowsForTest(const QList<int>& rows)
{
	m_pList->clearSelection();
	for (int nRow : rows)
	{
		if (nRow < 0 || nRow >= m_pList->topLevelItemCount())
		{
			return false;
		}
		m_pList->topLevelItem(nRow)->setSelected(true);
	}
	return m_pList->selectedItems().size() == rows.size();
}

int CWindowListDialog::RowCount() const
{
	return m_pList->topLevelItemCount();
}

QString CWindowListDialog::RowText(int nRow, int nColumn) const
{
	if (nRow < 0 || nRow >= m_pList->topLevelItemCount())
	{
		return QString();
	}
	return m_pList->topLevelItem(nRow)->text(nColumn);
}

void CWindowListDialog::TriggerForTest(const QString& strButton)
{
	if (strButton == QLatin1String("activate"))		{ m_pActivate->click(); }
	else if (strButton == QLatin1String("save"))	{ m_pSave->click(); }
	else if (strButton == QLatin1String("close"))	{ m_pClose->click(); }
	else if (strButton == QLatin1String("copy"))	{ m_pCopyPath->click(); }
}
