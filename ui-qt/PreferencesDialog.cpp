/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "PreferencesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

CPreferencesDialog::CPreferencesDialog(const Core::CAppSettings& current, QWidget* pParent)
	: QDialog(pParent)
	, m_Original(current)
{
	setWindowTitle(tr("Preferences"));
	setObjectName(QStringLiteral("PreferencesDialog"));
	setMinimumWidth(420);

	QVBoxLayout* pLayout = new QVBoxLayout(this);

	QGroupBox* pEditor = new QGroupBox(tr("Editor"), this);
	QFormLayout* pEditorForm = new QFormLayout(pEditor);
	m_pUrlHighlight = new QCheckBox(tr("Underline URLs"), pEditor);
	m_pCaretLineFrame = new QCheckBox(tr("Frame the caret line"), pEditor);
	m_pLongLine = new QSpinBox(pEditor);
	// 1..512 rather than unbounded: a column of 0 turns the marker into a line
	// down the left edge and a very large one puts it where nobody will see it,
	// and neither is a thing a user means to ask for.
	m_pLongLine->setRange(1, 512);
	pEditorForm->addRow(m_pUrlHighlight);
	pEditorForm->addRow(m_pCaretLineFrame);
	pEditorForm->addRow(tr("Long line marker at column"), m_pLongLine);
	pLayout->addWidget(pEditor);

	QGroupBox* pAuto = new QGroupBox(tr("Autocomplete"), this);
	QFormLayout* pAutoForm = new QFormLayout(pAuto);
	m_pAutoComplete = new QCheckBox(tr("Suggest as you type"), pAuto);
	m_pIgnoreCase = new QCheckBox(tr("Ignore case when matching"), pAuto);
	m_pIgnoreNumbers = new QCheckBox(tr("Ignore purely numeric words"), pAuto);
	pAutoForm->addRow(m_pAutoComplete);
	pAutoForm->addRow(m_pIgnoreCase);
	pAutoForm->addRow(m_pIgnoreNumbers);
	// The two matching options are meaningless with suggestions off, so they
	// follow it rather than sitting there enabled and inert.
	connect(m_pAutoComplete, &QCheckBox::toggled, m_pIgnoreCase, &QWidget::setEnabled);
	connect(m_pAutoComplete, &QCheckBox::toggled, m_pIgnoreNumbers, &QWidget::setEnabled);
	pLayout->addWidget(pAuto);

	QGroupBox* pFold = new QGroupBox(tr("Folding margin"), this);
	QFormLayout* pFoldForm = new QFormLayout(pFold);
	m_pMarginStyle = new QComboBox(pFold);
	// Order matters: the index IS the FOLDER_MARGIN_STYPE value
	// (src/EnumDef.h:233-239), so these must stay in enum order.
	m_pMarginStyle->addItem(tr("Arrows"));			// 0 STYLE_ARROW
	m_pMarginStyle->addItem(tr("Plus and minus"));	// 1 STYLE_PLUS_MINUS
	m_pMarginStyle->addItem(tr("Tree, circles"));	// 2 STYLE_TREE_CIRCLE
	m_pMarginStyle->addItem(tr("Tree, boxes"));		// 3 STYLE_TREE_BOX
	m_pHighlightFolder = new QCheckBox(tr("Highlight the enclosing fold"), pFold);
	m_pFoldingUnderline = new QCheckBox(tr("Line under a collapsed fold"), pFold);
	m_pMarginClassic = new QCheckBox(tr("Classic margin colours (ignore theme)"), pFold);
	pFoldForm->addRow(tr("Marker style"), m_pMarginStyle);
	pFoldForm->addRow(m_pHighlightFolder);
	pFoldForm->addRow(m_pFoldingUnderline);
	pFoldForm->addRow(m_pMarginClassic);
	pLayout->addWidget(pFold);

	m_pUrlHighlight->setChecked(current.EnableUrlHighlight());
	m_pCaretLineFrame->setChecked(current.DrawCaretLineFrame());
	m_pLongLine->setValue(current.LongLineColumnLimit());
	m_pAutoComplete->setChecked(current.EnableAutoComplete());
	m_pIgnoreCase->setChecked(current.AutoCompleteIgnoreCase());
	m_pIgnoreNumbers->setChecked(current.AutoCompleteIgnoreNumbers());
	m_pIgnoreCase->setEnabled(current.EnableAutoComplete());
	m_pIgnoreNumbers->setEnabled(current.EnableAutoComplete());
	m_pMarginStyle->setCurrentIndex(current.FolderMarginStyle());
	m_pHighlightFolder->setChecked(current.EnableHighlightFolder());
	m_pFoldingUnderline->setChecked(current.DrawFoldingLineUnderLineStyle());
	m_pMarginClassic->setChecked(current.UseFolderMarginClassic());

	QDialogButtonBox* pButtons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	pLayout->addWidget(pButtons);
}

Core::CAppSettings CPreferencesDialog::GetSettings() const
{
	// A copy of what came in, with only the ten this dialog owns overwritten -
	// so anything core/AppSettings gains later survives being edited here.
	//
	// UNPROVABLE TODAY, and worth saying: the dialog owns ALL ten settings
	// core/AppSettings has, so starting from a default-constructed copy would
	// produce the same result and no check can tell the difference. Verified by
	// mutation. It becomes load-bearing the moment an eleventh setting is added
	// that this dialog does not expose - which is exactly when nobody would
	// think to add it.
	Core::CAppSettings result = m_Original;
	result.SetEnableUrlHighlight(m_pUrlHighlight->isChecked());
	result.SetDrawCaretLineFrame(m_pCaretLineFrame->isChecked());
	result.SetLongLineColumnLimit(m_pLongLine->value());
	result.SetEnableAutoComplete(m_pAutoComplete->isChecked());
	result.SetAutoCompleteIgnoreCase(m_pIgnoreCase->isChecked());
	result.SetAutoCompleteIgnoreNumbers(m_pIgnoreNumbers->isChecked());
	result.SetFolderMarginStyle(m_pMarginStyle->currentIndex());
	result.SetEnableHighlightFolder(m_pHighlightFolder->isChecked());
	result.SetDrawFoldingLineUnderLineStyle(m_pFoldingUnderline->isChecked());
	result.SetUseFolderMarginClassic(m_pMarginClassic->isChecked());
	return result;
}
