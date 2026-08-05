/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The code page table: every encoding this build of Qt can do.
//
// A dialog, unlike Goto - and for the opposite reason. src/CodePageMFCDlg.cpp
// IS a dialog (CDlgBase, DoModal, a real OK and Cancel), it is a list of 800-odd
// rows rather than one field, and the choice is deliberate and infrequent. None
// of the find-bar reasoning applies.
//
// ONE CLASS, TWO MODES, exactly as the MFC has it. CCodePageMFCDlg carries an
// m_bReopen flag that its two callers set, and it relabels the OK button
// "Reopen File" or "Save File" accordingly. That relabelling is the only thing
// telling the user which of two very different operations they are about to
// perform, so it is reproduced rather than tidied into one generic "OK".
//
// See doc/PORTING.md 6m.

#pragma once

#include <QDialog>
#include <QString>

class QDialogButtonBox;
class QLineEdit;
class QTreeWidget;

class CEncodingDialog final : public QDialog
{
	Q_OBJECT

public:
	enum class EMode
	{
		Reinterpret,		// re-read the bytes; nothing is written
		Convert,			// re-write the text; the bytes change
	};

	// strCurrent is preselected, so the dialog opens on what the document
	// already is rather than at the top of an alphabetical list of 800.
	CEncodingDialog(EMode mode, const QString& strCurrent, QWidget* pParent = nullptr);

	// Empty if nothing is selected.
	QString SelectedEncoding() const;

	// For the self-test, which cannot type into a QLineEdit or click a row.
	void SetFilterForTest(const QString& strText);
	int VisibleRowCount() const;
	bool SelectForTest(const QString& strName);
	QString OkButtonText() const;

private:
	void ApplyFilter(const QString& strText);

	QLineEdit*			m_pFilter = nullptr;
	QTreeWidget*		m_pList = nullptr;
	QDialogButtonBox*	m_pButtons = nullptr;
};
