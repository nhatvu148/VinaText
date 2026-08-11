/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The input prompt for the seventeen text-transform commands.
//
// ONE DIALOG WHERE THE MFC HAS FIVE, because the MFC's five are already
// generic forms reused across the seventeen - CEditWithXDlg alone backs ten of
// them, its caption set by each caller. Their fields between them are: one text
// box, a second text box, and a checkbox. That is what this takes.
//
//   CEditWithXDlg              m_sXInput
//   CRemoveAfterBeforeWordDlg  m_strWord
//   CInsertAfterWordInLineDlg  m_strWord, m_strInsertWhat
//   CInsertFromPositionXDlg    m_strPositionX, m_strInsertWhat, m_bInsertFromLineEnd
//   CRemoveFromXToYDlg         m_strFromX, m_strToY, m_bRemoveFromEndLine
//
// See doc/PORTING.md 6p.

#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;

class CTransformDialog final : public QDialog
{
	Q_OBJECT

public:
	struct SSpec
	{
		QString _Title;
		QString _Label1;
		QString _Value1;
		QString _Label2;		// empty: no second field
		QString _Value2;
		QString _CheckLabel;	// empty: no checkbox
		bool _DigitsOnly = false;
	};

	explicit CTransformDialog(const SSpec& spec, QWidget* pParent = nullptr);

	QString Value1() const;
	QString Value2() const;
	bool IsChecked() const;

	// For the self-test, which cannot type.
	void SetValuesForTest(const QString& str1, const QString& str2, bool bChecked);

private:
	QLineEdit*	m_pField1 = nullptr;
	QLineEdit*	m_pField2 = nullptr;
	QCheckBox*	m_pCheck = nullptr;
};
