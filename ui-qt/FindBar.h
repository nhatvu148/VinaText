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
class QMenu;
class QLineEdit;
class QToolButton;
class QWidget;

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
	// bReplace shows the replace row. The bar is one widget in two modes rather
	// than two widgets, so the pattern, the options and the match count survive
	// switching between Find and Replace - which is what a user expects after
	// typing a pattern and then deciding to replace it.
	void Activate(const QString& strInitial, bool bReplace = false);
	QString GetReplacement() const;
	bool IsReplaceVisible() const;
	void SetReplaceVisible(bool bVisible);
	void ShowStatus(const QString& strText, bool bIsMiss);

	// Turns regex mode on, which is also what reveals the preset list. Not
	// test-only: the screenshot path uses it, because a button that never
	// appears in a picture has not been shown to anybody.
	void SetRegex(bool bOn);

	// For the self-test, which cannot open a menu or click an item.
	bool IsRegexHelpVisible() const;
	bool InsertPresetForTest(int nIndex);

	// The preset menu, for the screenshot - a popup cannot be opened by the
	// self-test any other way, and a list nobody can see in a picture is the
	// thing this feature is.
	QMenu* PresetMenu() const;
	void TypePatternForTest(const QString& strText);

signals:
	void FindRequested(bool bBackward);
	void ReplaceRequested();
	void ReplaceAllRequested();
	void PatternChanged();
	void CloseRequested();

private:
	QLineEdit*	m_pPattern = nullptr;
	QLineEdit*	m_pReplacement = nullptr;
	QWidget*	m_pReplaceRow = nullptr;
	QToolButton*	m_pToggleReplace = nullptr;
	QCheckBox*	m_pMatchCase = nullptr;
	QCheckBox*	m_pWholeWord = nullptr;
	QCheckBox*	m_pRegex = nullptr;
	QLabel*		m_pStatus = nullptr;
	// The regex helper list from src/ComboboxRegexHelper.cpp - the one part of
	// FindDlg that is not find-in-files. Only useful in regex mode, so it
	// appears with the checkbox rather than sitting there inert.
	QToolButton*	m_pRegexHelp = nullptr;
};
