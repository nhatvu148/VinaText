/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The settings a user can actually change.
//
// NOT a port of EditorSettingDlg (src/EditorSettingDlg.cpp, 1,002 lines). That
// dialog exposes dozens of settings, most of which drive features ui-qt/ does
// not have - a compiler, a debugger, a file explorer. Under D10 those are
// deferred, so a faithful port would be mostly controls that do nothing, which
// is worse than fewer controls that all do something.
//
// This exposes exactly the settings core/AppSettings reads, which is exactly
// the set that changes this editor's behaviour. It grows when they do.
//
// Prompted by a user asking "how to see the app settings?" - there was no way
// to. Reading settings without being able to edit them is half a feature, and
// on macOS and Linux the file is usually absent entirely, so every setting was
// whatever src/AppSettings.h shipped with no way to change it.

#pragma once

#include <AppSettings.h>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;

class CPreferencesDialog final : public QDialog
{
	Q_OBJECT

public:
	// Edits a COPY. The caller applies the result only when accept() returns,
	// so Cancel genuinely cancels rather than relying on every control being
	// put back.
	explicit CPreferencesDialog(const Core::CAppSettings& current, QWidget* pParent = nullptr);

	// The edited settings. Meaningful after exec() returns QDialog::Accepted.
	Core::CAppSettings GetSettings() const;

private:
	Core::CAppSettings	m_Original;

	QCheckBox*	m_pUrlHighlight = nullptr;
	QCheckBox*	m_pAutoComplete = nullptr;
	QCheckBox*	m_pIgnoreCase = nullptr;
	QCheckBox*	m_pIgnoreNumbers = nullptr;
	QCheckBox*	m_pCaretLineFrame = nullptr;
	QCheckBox*	m_pHighlightFolder = nullptr;
	QCheckBox*	m_pFoldingUnderline = nullptr;
	QCheckBox*	m_pMarginClassic = nullptr;
	QComboBox*	m_pMarginStyle = nullptr;
	QSpinBox*	m_pLongLine = nullptr;
};
