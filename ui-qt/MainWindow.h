/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The alpha shell: tabs, open/save, lexer + both themes, find, status bar.
//
// That list is D9's fixed checklist and it is exhaustive - no dialogs, no docking,
// no settings UI. Scope beyond it waits for beta, deliberately, so that
// interleaving Phase 3 with Phase 2 cannot degenerate into everything-half-done.
//
// This is where MainFrm.cpp's 4,486 lines eventually land. Almost none of it is
// here yet, and that is the point.

#pragma once

#include "EditorData.h"
#include "EncodingDialog.h"
#include "WindowListDialog.h"

#include <QColor>
#include <QMainWindow>

class CEditorWidget;
class CMessagePane;
class CFindBar;
class CGotoBar;
class QLabel;
class QMenu;
class QTabWidget;

class CMainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	// Non-const because Preferences writes settings back through it. The
	// editor widgets still take it CONST - they read settings and never
	// change them, and only the window owns that.
	explicit CMainWindow(CEditorData& data, QWidget* pParent = nullptr);

	// Returns false and reports to the user if the file cannot be read.
	bool OpenFile(const QString& strPath);
	CEditorWidget* GetCurrentEditor() const;
	int GetTabCount() const;

	// Headless verification - see main.cpp. Runs the shell through the checklist
	// and returns non-zero on the first thing that does not hold.
	int RunSelfTest(const QStringList& files);

	// Renders the window to <dir>/vinatext-{light,dark}.png and returns non-zero
	// on failure. A GUI cannot be asserted on, and a port whose whole point is
	// that it now runs on three platforms should be able to show that without
	// asking a reviewer to build it. Works under QT_QPA_PLATFORM=offscreen.
	int RenderScreenshots(const QStringList& files, const QString& strDirectory);

	// Appends a line to the message pane. The Qt counterpart of the
	// LOG_OUTPUT_MESSAGE family, which routes into CMessagePane on Windows;
	// ui-qt/ had nowhere for these to go before the pane existed.
	void LogMessage(const QString& strText, const QColor& colour = QColor());
	CMessagePane* GetMessagePane() const { return m_pMessagePane; }

protected:
	void closeEvent(QCloseEvent* pEvent) override;

private:
	// The About box, which carries D3's LGPLv3 attribution.
	void OnAbout();
	void OnPreferences();

	// Encoding. Two operations, never one - see CEditorWidget's encoding
	// section and doc/PORTING.md 6m.
	void BuildEncodingMenu(QMenu* pMenu, CEncodingDialog::EMode mode);
	void OnChooseEncoding(CEncodingDialog::EMode mode);

	// The window manager - COpenTabWindows. A dialog, not a dock pane; see
	// ui-qt/WindowListDialog.h and doc/PORTING.md 6n.
	void OnWindowManager();
	QList<CWindowListDialog::SEntry> CollectWindowList() const;
	void ApplyEncoding(CEncodingDialog::EMode mode, const QString& strEncoding);
	// Re-applies the current settings to every open editor.
	void ReapplySettings();

	// Dock geometry, so the panes come back where they were left. QSettings and
	// not AppSettings: the MFC persists docking through CDockingManager into the
	// registry, which has no portable counterpart and is not a file this port
	// could read anyway. Deliberate divergence in mechanism, same behaviour.
	void SaveDockState();
	void RestoreDockState();

private:
	void BuildMenus();
	void BuildStatusBar();

	CEditorWidget* AddTab(CEditorWidget* pEditor);
	CEditorWidget* NewUntitled();
	void UpdateTabLabel(CEditorWidget* pEditor);
	void UpdateStatusBar();
	void UpdateWindowTitle();

	void OnNew();
	void OnOpen();
	bool OnSave();
	bool OnSaveAs();
	bool OnCloseTab(int nIndex);
	void OnSetTheme(EEditorTheme theme);
	void OnToggleWordWrap(bool bEnable);
	void OnToggleLongLineMarker(bool bEnable);
	void OnShowFind() { ShowFindBar(false); }
	void OnShowReplace() { ShowFindBar(true); }
	void ShowFindBar(bool bReplace);
	void OnReplace();
	void OnReplaceAll();
	void OnHideFind();
	void OnFind(bool bBackward);
	void OnPatternChanged();

	// Goto. src/GotoDlg.cpp is tab 2 of the same MFC control the find bar is
	// tabs 0 and 1 of, so it lands in the same place - see ui-qt/GotoBar.h.
	void OnShowGoto();
	void OnHideGoto();
	void OnGotoLine();
	void OnGotoOffset();

	// Save/Discard/Cancel for one modified document. Returns false only for
	// Cancel, i.e. "do not proceed with whatever asked".
	bool ConfirmClose(CEditorWidget* pEditor);

	bool SaveEditor(CEditorWidget* pEditor, const QString& strPath);

	CEditorData&		m_Data;
	CMessagePane*			m_pMessagePane = nullptr;
	QTabWidget*			m_pTabs = nullptr;
	CFindBar*			m_pFindBar = nullptr;
	CGotoBar*			m_pGotoBar = nullptr;
	QLabel*				m_pStatusPosition = nullptr;
	QLabel*				m_pStatusLanguage = nullptr;
	QLabel*				m_pStatusEncoding = nullptr;
	QLabel*				m_pStatusEol = nullptr;
	EEditorTheme		m_Theme = EEditorTheme::Dark;
	// View state is per window, not per document: a new tab inherits it, which is
	// what every editor with a wrap toggle does.
	bool				m_bWordWrap = false;
	bool				m_bLongLineMarker = false;
	QString				m_strLastDirectory;
};
