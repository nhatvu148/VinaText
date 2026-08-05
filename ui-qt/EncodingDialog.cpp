/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "EncodingDialog.h"

#include "EditorWidget.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCodec>
#include <QTreeWidget>
#include <QVBoxLayout>

CEncodingDialog::CEncodingDialog(EMode mode, const QString& strCurrent, QWidget* pParent)
	: QDialog(pParent)
{
	setWindowTitle(tr("VinaText System CodePage"));		// the MFC's own caption
	resize(620, 460);

	// Says which of the two operations this is, in a sentence, because the
	// button caption alone has to be read to be understood and the cost of
	// misreading it is a file written in the wrong encoding.
	QLabel* pExplain = new QLabel(mode == EMode::Reinterpret
		? tr("Re-read the file from disk as the encoding you choose. "
			"Nothing is written, and unsaved changes are lost.")
		: tr("Write the file out in the encoding you choose. "
			"The text is unchanged; the bytes on disk change."), this);
	pExplain->setWordWrap(true);

	m_pFilter = new QLineEdit(this);
	// 805 codecs is unusable as a plain list, so the filter is not a nicety.
	// The MFC ships the same list with no filter at all.
	m_pFilter->setPlaceholderText(tr("Filter, e.g. utf, 1258, japanese"));
	m_pFilter->setClearButtonEnabled(true);

	m_pList = new QTreeWidget(this);
	m_pList->setColumnCount(2);
	m_pList->setHeaderLabels({ tr("Encoding"), tr("Also known as") });
	m_pList->setRootIsDecorated(false);
	m_pList->setAllColumnsShowFocus(true);
	m_pList->setUniformRowHeights(true);
	m_pList->setSortingEnabled(false);

	QTreeWidgetItem* pCurrent = nullptr;
	for (const QString& strName : CEditorWidget::AvailableEncodings())
	{
		QStringList aliases;
		if (QTextCodec* pCodec = QTextCodec::codecForName(strName.toLatin1()))
		{
			for (const QByteArray& alias : pCodec->aliases())
			{
				aliases.append(QString::fromLatin1(alias));
			}
		}
		QTreeWidgetItem* pItem = new QTreeWidgetItem(m_pList,
			{ strName, aliases.join(QStringLiteral(", ")) });
		if (strName.compare(strCurrent, Qt::CaseInsensitive) == 0)
		{
			pCurrent = pItem;
		}
	}
	m_pList->setColumnWidth(0, 200);

	m_pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	// The MFC's captions, kept verbatim. They are the only thing on screen that
	// distinguishes the two operations at the moment of committing to one.
	m_pButtons->button(QDialogButtonBox::Ok)->setText(
		mode == EMode::Reinterpret ? tr("Reopen File") : tr("Save File"));

	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->addWidget(pExplain);
	pLayout->addWidget(m_pFilter);
	pLayout->addWidget(m_pList, 1);
	pLayout->addWidget(m_pButtons);

	connect(m_pFilter, &QLineEdit::textChanged, this, &CEncodingDialog::ApplyFilter);
	connect(m_pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_pList, &QTreeWidget::itemDoubleClicked, this, &QDialog::accept);
	// Nothing selected means nothing to do, so OK stays disabled rather than
	// accepting and quietly doing nothing.
	connect(m_pList, &QTreeWidget::itemSelectionChanged, this, [this]
	{
		m_pButtons->button(QDialogButtonBox::Ok)->setEnabled(
			!m_pList->selectedItems().isEmpty());
	});

	if (pCurrent != nullptr)
	{
		m_pList->setCurrentItem(pCurrent);
		m_pList->scrollToItem(pCurrent, QAbstractItemView::PositionAtCenter);
	}
	m_pButtons->button(QDialogButtonBox::Ok)->setEnabled(
		!m_pList->selectedItems().isEmpty());
}

void CEncodingDialog::ApplyFilter(const QString& strText)
{
	const QString strNeedle = strText.trimmed();
	for (int i = 0; i < m_pList->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pList->topLevelItem(i);
		// Aliases are matched too, so "japanese" finds Shift_JIS and "1258"
		// finds the Vietnamese codepage under whatever Qt calls it.
		const bool bShow = strNeedle.isEmpty()
			|| pItem->text(0).contains(strNeedle, Qt::CaseInsensitive)
			|| pItem->text(1).contains(strNeedle, Qt::CaseInsensitive);
		pItem->setHidden(!bShow);
	}
}

QString CEncodingDialog::SelectedEncoding() const
{
	const QList<QTreeWidgetItem*> selected = m_pList->selectedItems();
	// No `|| selected.first()->isHidden()` here, though the first version had
	// one. QTreeWidget CLEARS the selection when the selected row is hidden, so
	// that clause could never be true - measured: selectedCount drops to 0 the
	// moment the filter hides the current row. It was dead code that looked
	// like a guard, and a mutation removing it was MISSED because there was
	// nothing there to remove.
	//
	// The behaviour is relied on rather than duplicated, and the self-test
	// asserts it: if Qt ever stops clearing the selection, the check fails and
	// the guard comes back.
	if (selected.isEmpty())
	{
		return QString();
	}
	return selected.first()->text(0);
}

void CEncodingDialog::SetFilterForTest(const QString& strText)
{
	m_pFilter->setText(strText);
}

int CEncodingDialog::VisibleRowCount() const
{
	int nVisible = 0;
	for (int i = 0; i < m_pList->topLevelItemCount(); ++i)
	{
		if (!m_pList->topLevelItem(i)->isHidden())
		{
			++nVisible;
		}
	}
	return nVisible;
}

bool CEncodingDialog::SelectForTest(const QString& strName)
{
	for (int i = 0; i < m_pList->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pList->topLevelItem(i);
		if (pItem->text(0) == strName)
		{
			m_pList->setCurrentItem(pItem);
			return true;
		}
	}
	return false;
}

QString CEncodingDialog::OkButtonText() const
{
	return m_pButtons->button(QDialogButtonBox::Ok)->text();
}
