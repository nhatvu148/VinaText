/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "MessagePane.h"

#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>

CMessagePane::CMessagePane(QWidget* pParent)
	: QDockWidget(tr("Message"), pParent)
{
	setObjectName(QStringLiteral("MessagePane"));	// QMainWindow::saveState needs it

	m_pOutput = new QPlainTextEdit(this);
	m_pOutput->setReadOnly(true);
	// CRichEditCtrlEX ships m_bEnableLineWrap FALSE (src/MessageWindow.h:26) and
	// offers wrapping through its own context menu; the menu is Phase 5 work of
	// its own, so this is the shipped default and nothing turns it on yet.
	m_pOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
	// A log of file paths and compiler output lines up only in a fixed font, and
	// this is the same font the editor asks for.
	m_pOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	// Undo history on a read-only log is memory spent on something unreachable.
	m_pOutput->setUndoRedoEnabled(false);

	// A log pane that opens taking a third of the window is a log pane the user
	// immediately drags smaller. QDockWidget sizes itself from its widget's
	// sizeHint, so the hint is what has to be modest - resizeDocks() from the
	// window would be overridden by a restored layout.
	m_pOutput->setMinimumHeight(40);
	m_pOutput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	setWidget(m_pOutput);
	setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
}

void CMessagePane::AddLogMessage(const QString& strText, const QColor& colour)
{
	if (strText.isEmpty())
	{
		return;
	}

	// A QTextCursor with a character format rather than appendHtml(): log lines
	// carry file paths and compiler output, which contain '<' and '&' as data.
	// Going through HTML would mean escaping them and would silently mangle any
	// line that was not escaped.
	QTextCursor cursor(m_pOutput->document());
	cursor.movePosition(QTextCursor::End);

	QTextCharFormat format;
	format.setForeground(colour);

	// The original tests `str.Find('\n') != -1` - a newline ANYWHERE in the
	// string, not at the end - and only then omits the one it appends
	// (src/MessageWindow.cpp). So a multi-line message ending without a newline
	// gets none, and the next message continues its last line. Preserved: the
	// callers pass single lines, and "fixing" it would change where the line
	// breaks fall for any caller that does not.
	cursor.insertText(strText.contains(QLatin1Char('\n'))
		? strText : strText + QLatin1Char('\n'), format);

	// Follow the tail unless the user is reading it. The original checks
	// GetFocus() for the same reason - scrolling away from someone who has
	// clicked into the log to copy a line is worse than not following.
	if (!m_pOutput->hasFocus())
	{
		m_pOutput->verticalScrollBar()->setValue(
			m_pOutput->verticalScrollBar()->maximum());
	}
}

void CMessagePane::ClearAll()
{
	m_pOutput->clear();
}

QString CMessagePane::GetText() const
{
	return m_pOutput->toPlainText();
}

int CMessagePane::GetLineCount() const
{
	// An empty document still has one (empty) block, and every AddLogMessage
	// leaves a trailing newline and so an empty final block. Reporting the number
	// of MESSAGES is what a caller means by "lines".
	const QString strText = m_pOutput->toPlainText();
	if (strText.isEmpty())
	{
		return 0;
	}
	return strText.count(QLatin1Char('\n'))
		+ (strText.endsWith(QLatin1Char('\n')) ? 0 : 1);
}

QColor CMessagePane::GetLineColour(int nLine) const
{
	const QTextBlock block = m_pOutput->document()->findBlockByNumber(nLine);
	if (!block.isValid())
	{
		return QColor();
	}
	// The colour lives on the text fragments, not the block, so an empty block
	// has none to report.
	for (QTextBlock::iterator it = block.begin(); it != block.end(); ++it)
	{
		if (it.fragment().isValid())
		{
			return it.fragment().charFormat().foreground().color();
		}
	}
	return QColor();
}
