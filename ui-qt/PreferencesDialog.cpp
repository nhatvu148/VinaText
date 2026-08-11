/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "PreferencesDialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
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
	// 1..512 normally - a column of 0 puts the marker down the left edge and a
	// very large one puts it where nobody will see it. But the range is WIDENED
	// to admit whatever is already stored, because a widget range is a guide
	// for new input, not a licence to rewrite existing data.
	//
	// Without that, a shared settings file holding 2000 was silently truncated
	// to 512 by opening Preferences and pressing OK without touching anything.
	m_pLongLine->setRange(1, std::max(512, current.LongLineColumnLimit()));
	pEditorForm->addRow(m_pUrlHighlight);
	pEditorForm->addRow(m_pCaretLineFrame);
	pEditorForm->addRow(tr("Long line marker at column"), m_pLongLine);

	// Font. A real family list rather than free text - a typo'd family name
	// leaves Scintilla to pick something arbitrary with nothing on screen
	// saying why. Monospaced only, since this is a code editor.
	m_pFontName = new QComboBox(pEditor);
	for (const QString& strFamily : QFontDatabase::families())
	{
		if (QFontDatabase::isFixedPitch(strFamily))
		{
			m_pFontName->addItem(strFamily);
		}
	}
	const QString strCurrentFont =
		QString::fromStdString(current.EditorFontName());
	// The stored family may not exist on this machine - a settings file
	// written on Windows naming Courier New, opened on a Linux box without
	// it. It is ADDED rather than dropped, so opening Preferences does not
	// silently rewrite a font the user chose elsewhere. Same rule as the
	// margin style: a widget's range is not a licence to rewrite stored data.
	if (m_pFontName->findText(strCurrentFont) < 0)
	{
		m_pFontName->insertItem(0, strCurrentFont);
	}
	m_pFontName->setCurrentText(strCurrentFont);
	pEditorForm->addRow(tr("Editor font"), m_pFontName);

	m_pFontSize = new QSpinBox(pEditor);
	m_pFontSize->setRange(6, 72);
	m_pFontSize->setValue(current.EditorFontPointSize());
	pEditorForm->addRow(tr("Font size"), m_pFontSize);

	m_pZoom = new QSpinBox(pEditor);
	m_pZoom->setRange(-10, 20);
	m_pZoom->setValue(current.EditorZoomFactor());
	pEditorForm->addRow(tr("Zoom"), m_pZoom);

	m_pCustomTabs = new QCheckBox(tr("Use a custom tab width"), pEditor);
	m_pCustomTabs->setChecked(current.UseCustomTabSettings());
	pEditorForm->addRow(m_pCustomTabs);

	m_pTabWidth = new QSpinBox(pEditor);
	m_pTabWidth->setRange(1, 16);
	m_pTabWidth->setValue(current.EditorTabWidth());
	pEditorForm->addRow(tr("Tab width"), m_pTabWidth);

	m_pUseTabs = new QCheckBox(tr("Indent with tabs rather than spaces"), pEditor);
	m_pUseTabs->setChecked(current.ProcessIndentationTab());
	pEditorForm->addRow(m_pUseTabs);

	m_pCaretBlink = new QCheckBox(tr("Blink the caret"), pEditor);
	m_pCaretBlink->setChecked(current.EnableCaretBlink());
	pEditorForm->addRow(m_pCaretBlink);

	m_pMultiCursor = new QCheckBox(tr("Multiple cursors"), pEditor);
	m_pMultiCursor->setChecked(current.EnableMultipleCursor());
	pEditorForm->addRow(m_pMultiCursor);

	// Line endings for NEW documents. The order is Scintilla's SC_EOL_*, which
	// core/ stores as a raw number because it cannot include Scintilla.
	m_pDefaultEol = new QComboBox(pEditor);
	m_pDefaultEol->addItems({ tr("CRLF (Windows)"), tr("CR (classic Mac)"),
		tr("LF (Unix)") });
	// UNCONDITIONALLY, so an out-of-range value leaves the combo at -1 and the
	// preserve-rather-than-write guard in GetSettings can see it. Guarding this
	// call instead left the combo at index 0, and 0 is a perfectly valid index,
	// so a stored 9 was silently rewritten as CRLF - the #45 bug, reproduced in
	// the code written to avoid it, and caught by the check written for it.
	m_pDefaultEol->setCurrentIndex(current.DefaultFileEol());
	pEditorForm->addRow(tr("New files use"), m_pDefaultEol);

	m_pNewlineAtEof = new QCheckBox(tr("Add a newline at end of file on save"), pEditor);
	m_pNewlineAtEof->setChecked(current.AutoAddNewLineAtEof());
	pEditorForm->addRow(m_pNewlineAtEof);
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
	// An out-of-range stored value leaves the combo at -1, showing blank, and
	// GetSettings then preserves the original rather than writing -1 back.
	//
	// That -1 was the worse half of the same bug: the MFC's four-way if-chain
	// over FOLDER_MARGIN_STYPE matches nothing at -1, so it would define NO
	// fold markers - a Qt user pressing OK could break the fold margin in the
	// Windows build, through a file both share.
	const int nStoredStyle = current.FolderMarginStyle();
	m_pMarginStyle->setCurrentIndex(
		(nStoredStyle >= 0 && nStoredStyle < m_pMarginStyle->count()) ? nStoredStyle : -1);
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
	// Preserve rather than write -1 back. The user has not chosen a style, so
	// this dialog has nothing to say about it.
	const int nStyle = m_pMarginStyle->currentIndex();
	result.SetFolderMarginStyle(nStyle >= 0 ? nStyle : m_Original.FolderMarginStyle());
	result.SetEnableHighlightFolder(m_pHighlightFolder->isChecked());
	result.SetDrawFoldingLineUnderLineStyle(m_pFoldingUnderline->isChecked());
	result.SetUseFolderMarginClassic(m_pMarginClassic->isChecked());

	result.SetEditorFontName(m_pFontName->currentText().toStdString());
	result.SetEditorFontPointSize(m_pFontSize->value());
	result.SetEditorZoomFactor(m_pZoom->value());
	result.SetUseCustomTabSettings(m_pCustomTabs->isChecked());
	result.SetEditorTabWidth(m_pTabWidth->value());
	result.SetProcessIndentationTab(m_pUseTabs->isChecked());
	result.SetEnableCaretBlink(m_pCaretBlink->isChecked());
	result.SetEnableMultipleCursor(m_pMultiCursor->isChecked());
	// Preserve rather than write -1, as the margin style above does.
	const int nEol = m_pDefaultEol->currentIndex();
	result.SetDefaultFileEol(nEol >= 0 ? nEol : m_Original.DefaultFileEol());
	result.SetAutoAddNewLineAtEof(m_pNewlineAtEof->isChecked());
	return result;
}
