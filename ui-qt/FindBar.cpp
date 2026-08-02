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
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
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

	QHBoxLayout* pLayout = new QHBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
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

	// The replace row. Hidden in Find mode, so the bar costs one line until the
	// user asks for two.
	m_pReplacement = new QLineEdit(this);
	m_pReplacement->setPlaceholderText(tr("Replace with"));
	m_pReplacement->setClearButtonEnabled(true);

	QToolButton* pReplace = new QToolButton(this);
	pReplace->setText(tr("Replace"));
	pReplace->setToolTip(tr("Replace this match and move to the next"));
	QToolButton* pReplaceAll = new QToolButton(this);
	pReplaceAll->setText(tr("All"));
	pReplaceAll->setToolTip(tr("Replace every match in this document"));

	QHBoxLayout* pReplaceLayout = new QHBoxLayout();
	pReplaceLayout->setContentsMargins(0, 0, 0, 0);
	pReplaceLayout->setSpacing(4);
	pReplaceLayout->addWidget(m_pReplacement, 1);
	pReplaceLayout->addWidget(pReplace);
	pReplaceLayout->addWidget(pReplaceAll);
	pReplaceLayout->addStretch(0);

	m_pReplaceRow = new QWidget(this);
	m_pReplaceRow->setLayout(pReplaceLayout);
	m_pReplaceRow->hide();

	QVBoxLayout* pOuter = new QVBoxLayout(this);
	pOuter->setContentsMargins(6, 3, 6, 3);
	pOuter->setSpacing(3);
	pOuter->addLayout(pLayout);
	pOuter->addWidget(m_pReplaceRow);

	connect(pReplace, &QToolButton::clicked, this, &CFindBar::ReplaceRequested);
	connect(pReplaceAll, &QToolButton::clicked, this, &CFindBar::ReplaceAllRequested);
	// Return in the replacement field replaces, matching Return in the pattern
	// field finding. Anything else would make the two fields behave differently
	// for the same key.
	connect(m_pReplacement, &QLineEdit::returnPressed, this, &CFindBar::ReplaceRequested);

	connect(pNext, &QToolButton::clicked, this, [this] { emit FindRequested(false); });
	connect(pPrevious, &QToolButton::clicked, this, [this] { emit FindRequested(true); });
	connect(pClose, &QToolButton::clicked, this, &CFindBar::CloseRequested);
	connect(m_pPattern, &QLineEdit::returnPressed, this, [this] { emit FindRequested(false); });
	connect(m_pPattern, &QLineEdit::textChanged, this, &CFindBar::PatternChanged);
	for (QCheckBox* pBox : { m_pMatchCase, m_pWholeWord, m_pRegex })
	{
		connect(pBox, &QCheckBox::toggled, this, &CFindBar::PatternChanged);
	}

	// A shortcut rather than a keyPressEvent override: the key lands in the line
	// edit, and whether an unhandled Escape propagates up to this widget is a
	// detail of QLineEdit that Esc-to-close should not be resting on.
	// WidgetWithChildren scopes it to this bar, so Escape does nothing while the
	// editor has focus.
	QShortcut* pEscape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
	pEscape->setContext(Qt::WidgetWithChildrenShortcut);
	connect(pEscape, &QShortcut::activated, this, &CFindBar::CloseRequested);
}

QString CFindBar::GetReplacement() const
{
	return m_pReplacement->text();
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

void CFindBar::Activate(const QString& strInitial, bool bReplace)
{
	if (!strInitial.isEmpty())
	{
		m_pPattern->setText(strInitial);
	}
	m_pReplaceRow->setVisible(bReplace);
	show();
	// Focus stays on the PATTERN even in replace mode: you cannot replace
	// anything until you have said what to replace, and arriving in the second
	// field would mean tabbing backwards to start.
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
