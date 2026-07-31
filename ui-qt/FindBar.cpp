/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "FindBar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

CFindBar::CFindBar(QWidget* pParent)
	: QWidget(pParent)
{
	m_pPattern = new QLineEdit(this);
	m_pPattern->setPlaceholderText(tr("Find"));
	m_pPattern->setClearButtonEnabled(true);

	QToolButton* pPrevious = new QToolButton(this);
	pPrevious->setText(QStringLiteral("▲"));
	pPrevious->setToolTip(tr("Previous match (Shift+F3)"));
	QToolButton* pNext = new QToolButton(this);
	pNext->setText(QStringLiteral("▼"));
	pNext->setToolTip(tr("Next match (F3)"));

	m_pMatchCase = new QCheckBox(tr("Aa"), this);
	m_pMatchCase->setToolTip(tr("Match case"));
	m_pWholeWord = new QCheckBox(tr("W"), this);
	m_pWholeWord->setToolTip(tr("Whole word"));
	m_pRegex = new QCheckBox(tr(".*"), this);
	m_pRegex->setToolTip(tr("Regular expression"));

	m_pStatus = new QLabel(this);

	QToolButton* pClose = new QToolButton(this);
	pClose->setText(QStringLiteral("✕"));
	pClose->setToolTip(tr("Close (Esc)"));

	QHBoxLayout* pLayout = new QHBoxLayout(this);
	pLayout->setContentsMargins(6, 3, 6, 3);
	pLayout->setSpacing(4);
	pLayout->addWidget(m_pPattern, 1);
	pLayout->addWidget(pPrevious);
	pLayout->addWidget(pNext);
	pLayout->addWidget(m_pMatchCase);
	pLayout->addWidget(m_pWholeWord);
	pLayout->addWidget(m_pRegex);
	pLayout->addWidget(m_pStatus);
	pLayout->addStretch(0);
	pLayout->addWidget(pClose);

	connect(pNext, &QToolButton::clicked, this, [this] { emit FindRequested(false); });
	connect(pPrevious, &QToolButton::clicked, this, [this] { emit FindRequested(true); });
	connect(pClose, &QToolButton::clicked, this, &CFindBar::CloseRequested);
	connect(m_pPattern, &QLineEdit::returnPressed, this, [this] { emit FindRequested(false); });
	connect(m_pPattern, &QLineEdit::textChanged, this, &CFindBar::PatternChanged);
	for (QCheckBox* pBox : { m_pMatchCase, m_pWholeWord, m_pRegex })
	{
		connect(pBox, &QCheckBox::toggled, this, &CFindBar::PatternChanged);
	}
}

QString CFindBar::GetPattern() const
{
	return m_pPattern->text();
}

bool CFindBar::IsMatchCase() const
{
	return m_pMatchCase->isChecked();
}

bool CFindBar::IsWholeWord() const
{
	return m_pWholeWord->isChecked();
}

bool CFindBar::IsRegex() const
{
	return m_pRegex->isChecked();
}

void CFindBar::Activate(const QString& strInitial)
{
	if (!strInitial.isEmpty())
	{
		m_pPattern->setText(strInitial);
	}
	show();
	m_pPattern->setFocus();
	m_pPattern->selectAll();
}

void CFindBar::ShowStatus(const QString& strText, bool bIsMiss)
{
	m_pStatus->setText(strText);
	// Colour only the miss. Marking every state would make the bar shout at the
	// user while they are still typing the word they are looking for.
	m_pStatus->setStyleSheet(bIsMiss ? QStringLiteral("QLabel { color: #C0392B; }")
									 : QString());
}

void CFindBar::keyPressEvent(QKeyEvent* pEvent)
{
	if (pEvent->key() == Qt::Key_Escape)
	{
		emit CloseRequested();
		return;
	}
	QWidget::keyPressEvent(pEvent);
}
