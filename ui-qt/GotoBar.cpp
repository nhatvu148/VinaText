/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "GotoBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QToolButton>

namespace
{
	// ES_NUMBER, which is what all four of IDD_POS's edits carry: digits only.
	// Deliberately NOT a QIntValidator bounded by the document size. A bound
	// would stop the user typing 5000 in a 900-line file, and the lesson from
	// Preferences (doc/PORTING.md, and the review on PR #45) is that a widget's
	// range is a guide for new input, not something to enforce by rewriting what
	// was entered. Out of range is Scintilla's to clamp, exactly as on Windows.
	QRegularExpressionValidator* MakeDigitsValidator(QObject* pParent)
	{
		return new QRegularExpressionValidator(
			QRegularExpression(QStringLiteral("[0-9]*")), pParent);
	}
}

CGotoBar::CGotoBar(QWidget* pParent)
	: QWidget(pParent)
{
	QLabel* pTitle = new QLabel(tr("Go to"), this);

	// Line. The MFC writes the range into the label itself - "Goto Line (1 -
	// %d):" in CGotoDlg::InitGotoRangeByDocument - so it is a label here too,
	// rather than a placeholder that disappears the moment you type.
	m_pLineRange = new QLabel(this);
	m_pLine = new QLineEdit(this);
	m_pLine->setValidator(MakeDigitsValidator(m_pLine));
	m_pLine->setMaximumWidth(90);

	QToolButton* pLineGo = new QToolButton(this);
	pLineGo->setText(tr("Go"));
	pLineGo->setToolTip(tr("Go to that line"));

	// Offset. The MFC labels this one "Goto Position:" with no range at all; the
	// hint is a small deliberate addition, because the one thing a user cannot
	// guess about a byte offset is how large it is allowed to be. A placeholder
	// and not a label, so it costs nothing once they have typed.
	QLabel* pOffsetLabel = new QLabel(tr("Offset:"), this);
	m_pOffset = new QLineEdit(this);
	m_pOffset->setValidator(MakeDigitsValidator(m_pOffset));
	m_pOffset->setMaximumWidth(110);

	QToolButton* pOffsetGo = new QToolButton(this);
	pOffsetGo->setText(tr("Go"));
	pOffsetGo->setToolTip(tr("Go to that byte offset"));

	QToolButton* pClose = new QToolButton(this);
	pClose->setText(QStringLiteral("✕"));
	pClose->setToolTip(tr("Close (Esc)"));

	QHBoxLayout* pLayout = new QHBoxLayout(this);
	pLayout->setContentsMargins(6, 3, 6, 3);
	pLayout->setSpacing(4);
	pLayout->addWidget(pTitle);
	pLayout->addWidget(m_pLineRange);
	pLayout->addWidget(m_pLine);
	pLayout->addWidget(pLineGo);
	pLayout->addSpacing(12);
	pLayout->addWidget(pOffsetLabel);
	pLayout->addWidget(m_pOffset);
	pLayout->addWidget(pOffsetGo);
	pLayout->addStretch(1);
	pLayout->addWidget(pClose);

	// Return in a field runs THAT field's goto, which is exactly what
	// CGotoDlg::PreTranslateMessage does with VK_RETURN - it routes on
	// pMsg->hwnd, m_EditLine versus m_EditPosition. A shared Go button would
	// have to guess.
	connect(m_pLine, &QLineEdit::returnPressed, this, &CGotoBar::GotoLineRequested);
	connect(pLineGo, &QToolButton::clicked, this, &CGotoBar::GotoLineRequested);
	connect(m_pOffset, &QLineEdit::returnPressed, this, &CGotoBar::GotoOffsetRequested);
	connect(pOffsetGo, &QToolButton::clicked, this, &CGotoBar::GotoOffsetRequested);
	connect(pClose, &QToolButton::clicked, this, &CGotoBar::CloseRequested);

	// The find bar's reasoning applies unchanged: a QShortcut rather than a
	// keyPressEvent override, because the key lands in the line edit and whether
	// an unhandled Escape propagates up is a detail of QLineEdit that
	// Esc-to-close should not rest on. WidgetWithChildren scopes it here, so
	// Escape does nothing while the editor has focus.
	QShortcut* pEscape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
	pEscape->setContext(Qt::WidgetWithChildrenShortcut);
	connect(pEscape, &QShortcut::activated, this, &CGotoBar::CloseRequested);

	SetDocumentRange(0, 0);
}

void CGotoBar::SetDocumentRange(int nLineCount, int nLength)
{
	m_pLineRange->setText(tr("line (1 - %1):").arg(nLineCount));
	m_pOffset->setPlaceholderText(tr("0 - %1").arg(nLength));
}

void CGotoBar::Activate(int nLineCount, int nLength, int nCaretPosition)
{
	SetDocumentRange(nLineCount, nLength);
	// The offset box opens on where the caret already is, which is
	// CGotoDlg::InitGotoInfoFromEditor writing GetCurrentPosition() into
	// m_EditPosition. It makes the field a readout of the current position as
	// well as an input, which is most of what anyone opens it for.
	m_pOffset->setText(QString::number(nCaretPosition));
	show();
	// Focus on the LINE field, matching InitGotoInfoFromEditor's closing
	// GetDlgItem(IDC_LINE)->SetFocus(). Going to a line is the common case; the
	// offset is there for the rarer one.
	m_pLine->setFocus();
	m_pLine->selectAll();
}

int CGotoBar::GetLine() const
{
	return m_pLine->text().toInt();
}

int CGotoBar::GetOffset() const
{
	return m_pOffset->text().toInt();
}

QString CGotoBar::GetLineRangeText() const
{
	return m_pLineRange->text();
}
