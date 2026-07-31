/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The alpha's find: a strip below the editor, not a dialog.
//
// D9 fixes the alpha at "no dialogs", and this is why that is not a compromise -
// src/FindDlg.cpp is 899 lines of fixed-pixel .rc layout whose Qt equivalent is
// Phase 5 work. A find bar is the smaller thing that makes the editor usable now,
// and it is what the modern editors this port is chasing all do.

#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;

class CFindBar final : public QWidget
{
	Q_OBJECT

public:
	explicit CFindBar(QWidget* pParent = nullptr);

	QString GetPattern() const;
	bool IsMatchCase() const;
	bool IsWholeWord() const;
	bool IsRegex() const;

	// Takes focus and selects whatever is in the box, so typing replaces it.
	void Activate(const QString& strInitial);
	void ShowStatus(const QString& strText, bool bIsMiss);

signals:
	void FindRequested(bool bBackward);
	void PatternChanged();
	void CloseRequested();

protected:
	void keyPressEvent(QKeyEvent* pEvent) override;

private:
	QLineEdit*	m_pPattern = nullptr;
	QCheckBox*	m_pMatchCase = nullptr;
	QCheckBox*	m_pWholeWord = nullptr;
	QCheckBox*	m_pRegex = nullptr;
	QLabel*		m_pStatus = nullptr;
};
