/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "MainWindow.h"

#include "EditorWidget.h"
#include "FindBar.h"
#include "AboutDialog.h"
#include "MessagePane.h"

#include <Scintilla.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QPair>
#include <QRegularExpression>
#include <QStringList>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QWidget>

CMainWindow::CMainWindow(const CEditorData& data, QWidget* pParent)
	: QMainWindow(pParent)
	, m_Data(data)
{
	m_pTabs = new QTabWidget(this);
	m_pTabs->setTabsClosable(true);
	m_pTabs->setMovable(true);
	m_pTabs->setDocumentMode(true);

	m_pFindBar = new CFindBar(this);
	m_pFindBar->hide();

	QWidget* pCentral = new QWidget(this);
	QVBoxLayout* pLayout = new QVBoxLayout(pCentral);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pTabs, 1);
	pLayout->addWidget(m_pFindBar);
	setCentralWidget(pCentral);

	// The first of Phase 5's nine dock panes. Created before BuildMenus so the
	// View menu can take its toggleViewAction, which Qt keeps in step with the
	// pane's visibility for free - a hand-rolled checkable action would need
	// synchronising on every close, float and restore.
	m_pMessagePane = new CMessagePane(this);
	addDockWidget(Qt::BottomDockWidgetArea, m_pMessagePane);
	// A starting height, in case nothing was stored. RestoreDockState overrides
	// it when there is a saved layout, which is the order the user expects.
	resizeDocks({ m_pMessagePane }, { 120 }, Qt::Vertical);

	BuildMenus();
	BuildStatusBar();

	connect(m_pTabs, &QTabWidget::tabCloseRequested, this, &CMainWindow::OnCloseTab);
	connect(m_pTabs, &QTabWidget::currentChanged, this, [this](int)
	{
		UpdateStatusBar();
		UpdateWindowTitle();
		if (!m_pFindBar->isHidden())
		{
			// The highlights belong to the document that was searched. Re-running
			// the pattern here is what stops the count in the bar from describing
			// a tab the user is no longer looking at.
			OnPatternChanged();
		}
	});
	connect(m_pFindBar, &CFindBar::FindRequested, this, &CMainWindow::OnFind);
	connect(m_pFindBar, &CFindBar::ReplaceRequested, this, &CMainWindow::OnReplace);
	connect(m_pFindBar, &CFindBar::ReplaceAllRequested, this, &CMainWindow::OnReplaceAll);
	connect(m_pFindBar, &CFindBar::PatternChanged, this, &CMainWindow::OnPatternChanged);
	connect(m_pFindBar, &CFindBar::CloseRequested, this, &CMainWindow::OnHideFind);

	// Say where the settings came from. On macOS and Linux the file usually does
	// not exist - it is written by the Windows build - so "using defaults" is
	// the common case and worth stating rather than leaving the user to infer
	// from behaviour.
	if (m_Data.GetSettings().WasLoaded())
	{
		LogMessage(tr("Settings: read %1").arg(m_Data.GetSettingsPath()));
	}
	else
	{
		LogMessage(tr("Settings: no file at %1 - using defaults")
			.arg(m_Data.GetSettingsPath()));
	}

	resize(1100, 750);
	// After resize(), so a stored geometry wins over the default rather than
	// being overwritten by it.
	RestoreDockState();
	NewUntitled();
	UpdateWindowTitle();
}

void CMainWindow::BuildMenus()
{
	QMenu* pFile = menuBar()->addMenu(tr("&File"));
	pFile->addAction(tr("&New"), QKeySequence::New, this, &CMainWindow::OnNew);
	pFile->addAction(tr("&Open..."), QKeySequence::Open, this, &CMainWindow::OnOpen);
	pFile->addSeparator();
	pFile->addAction(tr("&Save"), QKeySequence::Save, this, [this] { OnSave(); });
	pFile->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, [this] { OnSaveAs(); });
	pFile->addSeparator();
	pFile->addAction(tr("&Close Tab"), QKeySequence::Close, this, [this]
	{
		if (m_pTabs->count() > 0)
		{
			OnCloseTab(m_pTabs->currentIndex());
		}
	});
	pFile->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

	QMenu* pSearch = menuBar()->addMenu(tr("&Search"));
	pSearch->addAction(tr("&Find..."), QKeySequence::Find, this, &CMainWindow::OnShowFind);
	pSearch->addAction(tr("Find &Next"), QKeySequence::FindNext, this, [this] { OnFind(false); });
	pSearch->addAction(tr("Find &Previous"), QKeySequence::FindPrevious,
		this, [this] { OnFind(true); });
	pSearch->addSeparator();
	QAction* pReplace = pSearch->addAction(tr("&Replace..."), this,
		&CMainWindow::OnShowReplace);
#ifdef Q_OS_MACOS
	// NOT QKeySequence::Replace on macOS. Qt resolves it to Cmd+H, which the
	// system reserves for Hide Application - so the key never reaches the app
	// and the menu item is unreachable by keyboard. Reported from a real macOS
	// run: "I tried Ctrl+H and nothing happens".
	//
	// Cmd+Alt+F is what Xcode, VS Code, Sublime Text and TextEdit all use, so
	// it is the binding a macOS user will already have in their fingers.
	pReplace->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
#else
	// Ctrl+H everywhere else, which is the convention there and does not
	// collide.
	pReplace->setShortcut(QKeySequence::Replace);
#endif

	// A theme switch, not a settings UI: two radio items, no page, nothing stored.
	// Persisting the choice is AppSettings, the last file in the Phase 2 backlog
	// (783 call sites, PORTING.md 6c correction 5), and the alpha does not need it.
	QMenu* pView = menuBar()->addMenu(tr("&View"));
	QActionGroup* pThemeGroup = new QActionGroup(this);
	QAction* pLight = pView->addAction(tr("&Light Theme"), this,
		[this] { OnSetTheme(EEditorTheme::Light); });
	QAction* pDark = pView->addAction(tr("&Dark Theme"), this,
		[this] { OnSetTheme(EEditorTheme::Dark); });
	for (QAction* pAction : { pLight, pDark })
	{
		pAction->setCheckable(true);
		pThemeGroup->addAction(pAction);
	}
	pDark->setChecked(true);

	pView->addSeparator();
	// Qt supplies the show/hide action, already checkable and already bound to
	// the pane's visibility in both directions.
	pView->addAction(m_pMessagePane->toggleViewAction());

	pView->addSeparator();
	QAction* pWrap = pView->addAction(tr("&Word Wrap"), this, [this](bool bOn)
	{
		OnToggleWordWrap(bOn);
	});
	pWrap->setCheckable(true);
	pWrap->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Z));
	QAction* pEdge = pView->addAction(tr("&Long Line Marker"), this, [this](bool bOn)
	{
		OnToggleLongLineMarker(bOn);
	});
	pEdge->setCheckable(true);

	// Help. The About box is not decoration here - D3 requires the LGPLv3
	// attribution and the corresponding-source offer to be reachable from the
	// running application, so this menu is part of shipping, not of polish.
	QMenu* pHelp = menuBar()->addMenu(tr("&Help"));
	pHelp->addAction(tr("&About VinaText"), this, &CMainWindow::OnAbout);
}

void CMainWindow::OnAbout()
{
	CAboutDialog dialog(this);
	dialog.exec();
}

void CMainWindow::OnToggleWordWrap(bool bEnable)
{
	m_bWordWrap = bEnable;
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		if (CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i)))
		{
			pEditor->SetWordWrap(bEnable);
		}
	}
}

void CMainWindow::OnToggleLongLineMarker(bool bEnable)
{
	m_bLongLineMarker = bEnable;
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		if (CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i)))
		{
			pEditor->SetLongLineMarker(bEnable);
		}
	}
}

void CMainWindow::BuildStatusBar()
{
	m_pStatusPosition = new QLabel(this);
	m_pStatusLanguage = new QLabel(this);
	m_pStatusEncoding = new QLabel(this);
	m_pStatusEol = new QLabel(this);
	// Permanent widgets sit on the right and survive showMessage(), which is what
	// transient errors use - so an error never wipes the position readout.
	for (QLabel* pLabel : { m_pStatusPosition, m_pStatusLanguage, m_pStatusEncoding, m_pStatusEol })
	{
		pLabel->setContentsMargins(8, 0, 8, 0);
		statusBar()->addPermanentWidget(pLabel);
	}
}

//////////////////////////////////////////////////////////////////////////
// Dock panes

void CMainWindow::LogMessage(const QString& strText, const QColor& colour)
{
	if (m_pMessagePane == nullptr)
	{
		return;
	}
	// An invalid QColor means "whatever the palette says", which is what a plain
	// informational line wants - the theme's own text colour, not a literal.
	m_pMessagePane->AddLogMessage(strText,
		colour.isValid() ? colour : palette().color(QPalette::Text));
}

// QMainWindow::saveState covers which docks exist, where they are docked, their
// sizes and whether they are floating; saveGeometry covers the window itself.
// Both are opaque blobs keyed by objectName, which is why CMessagePane sets one.
//
// The version number is Qt's own compatibility guard: restoreState refuses a
// blob written with a different one, so bumping it is how a future layout change
// discards stale state instead of half-applying it.
namespace
{
	const int DOCK_STATE_VERSION = 1;
	const char* const DOCK_STATE_KEY = "MainWindow/dockState";
	const char* const GEOMETRY_KEY = "MainWindow/geometry";
}

void CMainWindow::SaveDockState()
{
	QSettings settings;
	settings.setValue(QLatin1String(GEOMETRY_KEY), saveGeometry());
	settings.setValue(QLatin1String(DOCK_STATE_KEY), saveState(DOCK_STATE_VERSION));
}

void CMainWindow::RestoreDockState()
{
	QSettings settings;
	// Both calls are no-ops on a missing or unreadable value, so a first run and
	// a corrupt settings file both land on the built-in layout rather than on
	// something half-restored.
	const QByteArray geometry = settings.value(QLatin1String(GEOMETRY_KEY)).toByteArray();
	if (!geometry.isEmpty())
	{
		restoreGeometry(geometry);
	}
	const QByteArray state = settings.value(QLatin1String(DOCK_STATE_KEY)).toByteArray();
	if (!state.isEmpty())
	{
		restoreState(state, DOCK_STATE_VERSION);
	}
}

//////////////////////////////////////////////////////////////////////////
// Tabs

CEditorWidget* CMainWindow::AddTab(CEditorWidget* pEditor)
{
	const int nIndex = m_pTabs->addTab(pEditor, pEditor->GetDisplayName());
	m_pTabs->setTabToolTip(nIndex, pEditor->GetFilePath());
	m_pTabs->setCurrentIndex(nIndex);

	connect(pEditor, &ScintillaEditBase::savePointChanged, this, [this, pEditor](bool)
	{
		UpdateTabLabel(pEditor);
		UpdateWindowTitle();
	});
	connect(pEditor, &ScintillaEditBase::updateUi, this, [this, pEditor](Scintilla::Update)
	{
		if (pEditor == GetCurrentEditor())
		{
			UpdateStatusBar();
		}
	});

	pEditor->ApplyTheme(m_Theme);
	pEditor->SetWordWrap(m_bWordWrap);
	pEditor->SetLongLineMarker(m_bLongLineMarker);
	pEditor->setFocus();
	UpdateStatusBar();
	UpdateWindowTitle();
	return pEditor;
}

CEditorWidget* CMainWindow::NewUntitled()
{
	return AddTab(new CEditorWidget(m_Data, this));
}

CEditorWidget* CMainWindow::GetCurrentEditor() const
{
	return qobject_cast<CEditorWidget*>(m_pTabs->currentWidget());
}

int CMainWindow::GetTabCount() const
{
	return m_pTabs->count();
}

void CMainWindow::UpdateTabLabel(CEditorWidget* pEditor)
{
	const int nIndex = m_pTabs->indexOf(pEditor);
	if (nIndex < 0)
	{
		return;
	}
	const QString strName = pEditor->GetDisplayName();
	m_pTabs->setTabText(nIndex, pEditor->IsModified()
		? strName + QStringLiteral(" *") : strName);
	m_pTabs->setTabToolTip(nIndex, pEditor->GetFilePath());
}

bool CMainWindow::OpenFile(const QString& strPath)
{
	// Already open? Raise that tab rather than opening a second view of one file,
	// which would give the user two editors that silently overwrite each other.
	const QString strCanonical = QFileInfo(strPath).canonicalFilePath();
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		CEditorWidget* pOpen = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
		if (pOpen != nullptr && !pOpen->GetFilePath().isEmpty()
			&& QFileInfo(pOpen->GetFilePath()).canonicalFilePath() == strCanonical)
		{
			m_pTabs->setCurrentIndex(i);
			return true;
		}
	}

	CEditorWidget* pEditor = new CEditorWidget(m_Data, this);
	QString strError;
	if (!pEditor->LoadFile(strPath, strError))
	{
		delete pEditor;
		QMessageBox::warning(this, tr("VinaText"), strError);
		statusBar()->showMessage(strError, 5000);
		// The status message expires after five seconds and the box is gone as
		// soon as it is dismissed. The pane is where it stays.
		LogMessage(strError, QColor(Qt::red));
		return false;
	}

	// The empty untitled document the window starts with is scaffolding, not a
	// document the user made - replace it rather than leaving it behind.
	CEditorWidget* pCurrent = GetCurrentEditor();
	const bool bReplaceScratch = m_pTabs->count() == 1 && pCurrent != nullptr
		&& pCurrent->IsUntitled() && !pCurrent->IsModified()
		&& pCurrent->Send(SCI_GETLENGTH) == 0;

	AddTab(pEditor);
	if (bReplaceScratch)
	{
		m_pTabs->removeTab(m_pTabs->indexOf(pCurrent));
		pCurrent->deleteLater();
	}
	m_strLastDirectory = QFileInfo(strPath).absolutePath();
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Commands

void CMainWindow::OnNew()
{
	NewUntitled();
}

void CMainWindow::OnOpen()
{
	const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Open File"),
		m_strLastDirectory);
	for (const QString& strPath : paths)
	{
		OpenFile(strPath);
	}
}

bool CMainWindow::OnSave()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return false;
	}
	if (pEditor->IsUntitled())
	{
		return OnSaveAs();
	}
	return SaveEditor(pEditor, pEditor->GetFilePath());
}

bool CMainWindow::OnSaveAs()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return false;
	}
	const QString strSuggested = pEditor->IsUntitled()
		? m_strLastDirectory + QLatin1Char('/') + pEditor->GetDisplayName()
		: pEditor->GetFilePath();
	const QString strPath = QFileDialog::getSaveFileName(this, tr("Save File As"), strSuggested);
	if (strPath.isEmpty())
	{
		return false;
	}
	return SaveEditor(pEditor, strPath);
}

bool CMainWindow::SaveEditor(CEditorWidget* pEditor, const QString& strPath)
{
	QString strError;
	if (!pEditor->SaveFile(strPath, strError))
	{
		QMessageBox::warning(this, tr("VinaText"), strError);
		statusBar()->showMessage(strError, 5000);
		// The status message expires after five seconds and the box is gone as
		// soon as it is dismissed. The pane is where it stays.
		LogMessage(strError, QColor(Qt::red));
		return false;
	}
	m_strLastDirectory = QFileInfo(strPath).absolutePath();
	UpdateTabLabel(pEditor);
	UpdateStatusBar();
	UpdateWindowTitle();
	statusBar()->showMessage(tr("Saved %1").arg(strPath), 3000);
	LogMessage(tr("Saved %1").arg(strPath));
	return true;
}

bool CMainWindow::ConfirmClose(CEditorWidget* pEditor)
{
	if (!pEditor->IsModified())
	{
		return true;
	}
	m_pTabs->setCurrentWidget(pEditor);
	const QMessageBox::StandardButton answer = QMessageBox::question(this, tr("VinaText"),
		tr("%1 has unsaved changes.").arg(pEditor->GetDisplayName()),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	if (answer == QMessageBox::Cancel)
	{
		return false;
	}
	if (answer == QMessageBox::Save)
	{
		return OnSave();
	}
	return true;
}

bool CMainWindow::OnCloseTab(int nIndex)
{
	CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(nIndex));
	if (pEditor == nullptr || !ConfirmClose(pEditor))
	{
		return false;
	}
	m_pTabs->removeTab(nIndex);
	pEditor->deleteLater();
	if (m_pTabs->count() == 0)
	{
		NewUntitled();				// the window always holds at least one document
	}
	UpdateStatusBar();
	UpdateWindowTitle();
	return true;
}

void CMainWindow::closeEvent(QCloseEvent* pEvent)
{
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
		if (pEditor != nullptr && !ConfirmClose(pEditor))
		{
			pEvent->ignore();
			return;
		}
	}
	// After the confirmations, so a close the user cancels does not persist a
	// layout they were only passing through.
	SaveDockState();
	pEvent->accept();
}

void CMainWindow::OnSetTheme(EEditorTheme theme)
{
	m_Theme = theme;
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
		if (pEditor != nullptr)
		{
			pEditor->ApplyTheme(theme);
		}
	}
	statusBar()->showMessage(tr("%1 theme")
		.arg(QString::fromStdString(m_Data.GetTheme(theme).GetName())), 2000);
}

//////////////////////////////////////////////////////////////////////////
// Find

void CMainWindow::ShowFindBar(bool bReplace)
{
	CEditorWidget* pEditor = GetCurrentEditor();
	QString strSelected;
	if (pEditor != nullptr)
	{
		const sptr_t nStart = pEditor->Send(SCI_GETSELECTIONSTART);
		const sptr_t nEnd = pEditor->Send(SCI_GETSELECTIONEND);
		if (nEnd > nStart && nEnd - nStart < 200)
		{
			QByteArray buffer(static_cast<int>(nEnd - nStart) + 1, '\0');
			pEditor->Send(SCI_GETSELTEXT, 0, reinterpret_cast<sptr_t>(buffer.data()));
			strSelected = QString::fromUtf8(buffer.constData());
		}
	}
	m_pFindBar->Activate(strSelected, bReplace);
	OnPatternChanged();
}

void CMainWindow::OnReplace()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	CEditorWidget::SFindOptions options;
	options._MatchCase = m_pFindBar->IsMatchCase();
	options._WholeWord = m_pFindBar->IsWholeWord();
	options._Regex = m_pFindBar->IsRegex();

	const bool bReplaced = pEditor->ReplaceNext(m_pFindBar->GetPattern(),
		m_pFindBar->GetReplacement(), options);
	m_pFindBar->ShowStatus(bReplaced ? tr("replaced") : tr("no match"), !bReplaced);
	// The highlights describe the text as it was before the edit, so re-run
	// them - otherwise an indicator sits over text that no longer matches.
	OnPatternChanged();
}

void CMainWindow::OnReplaceAll()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	CEditorWidget::SFindOptions options;
	options._MatchCase = m_pFindBar->IsMatchCase();
	options._WholeWord = m_pFindBar->IsWholeWord();
	options._Regex = m_pFindBar->IsRegex();

	const int nCount = pEditor->ReplaceAll(m_pFindBar->GetPattern(),
		m_pFindBar->GetReplacement(), options);
	m_pFindBar->ShowStatus(nCount == 0 ? tr("no match")
		: tr("%n replaced", nullptr, nCount), nCount == 0);
	// Worth a line in the message pane: a replace-all is the one find operation
	// that changes the document wholesale, and the count is the only evidence
	// of what it did.
	if (nCount > 0)
	{
		LogMessage(tr("Replaced %n occurrence(s) of '%1'", nullptr, nCount)
			.arg(m_pFindBar->GetPattern()));
	}
	OnPatternChanged();
}

void CMainWindow::OnHideFind()
{
	m_pFindBar->hide();
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor != nullptr)
	{
		pEditor->ClearHighlight();
		pEditor->setFocus();
	}
}

void CMainWindow::OnPatternChanged()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	CEditorWidget::SFindOptions options;
	options._MatchCase = m_pFindBar->IsMatchCase();
	options._WholeWord = m_pFindBar->IsWholeWord();
	options._Regex = m_pFindBar->IsRegex();

	const QString strPattern = m_pFindBar->GetPattern();
	const int nCount = pEditor->HighlightMatches(strPattern, options);
	if (strPattern.isEmpty())
	{
		m_pFindBar->ShowStatus(QString(), false);
	}
	else
	{
		const QString strStatus = (nCount == 0) ? tr("no matches")
			: (nCount == 1) ? tr("1 match") : tr("%1 matches").arg(nCount);
		m_pFindBar->ShowStatus(strStatus, nCount == 0);
	}
}

void CMainWindow::OnFind(bool bBackward)
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	if (m_pFindBar->isHidden())
	{
		OnShowFind();
		return;
	}
	CEditorWidget::SFindOptions options;
	options._MatchCase = m_pFindBar->IsMatchCase();
	options._WholeWord = m_pFindBar->IsWholeWord();
	options._Regex = m_pFindBar->IsRegex();
	options._Backward = bBackward;

	if (!pEditor->FindNext(m_pFindBar->GetPattern(), options))
	{
		m_pFindBar->ShowStatus(tr("no matches"), true);
	}
}

//////////////////////////////////////////////////////////////////////////
// Status bar and title

void CMainWindow::UpdateStatusBar()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	const int nSelected = pEditor->GetSelectedCharacterCount();
	m_pStatusPosition->setText(nSelected > 0
		? tr("Ln %1, Col %2  (%3 selected)")
			.arg(pEditor->GetCaretLine()).arg(pEditor->GetCaretColumn()).arg(nSelected)
		: tr("Ln %1, Col %2").arg(pEditor->GetCaretLine()).arg(pEditor->GetCaretColumn()));
	m_pStatusLanguage->setText(pEditor->GetLanguageLabel());
	m_pStatusEncoding->setText(pEditor->GetEncodingLabel());
	m_pStatusEol->setText(pEditor->GetEolLabel());
}

void CMainWindow::UpdateWindowTitle()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		setWindowTitle(QStringLiteral("VinaText"));
		return;
	}
	setWindowTitle(QStringLiteral("%1%2 — VinaText")
		.arg(pEditor->GetDisplayName(), pEditor->IsModified() ? QStringLiteral(" *") : QString()));
}

//////////////////////////////////////////////////////////////////////////
// Headless self-test
//
// "It compiled" is not evidence that an editor edits, and CI cannot click. This
// walks D9's checklist - tabs, open/save, lexer, both themes, find, status bar -
// against real files and asserts on what the widgets actually report.

namespace
{
	int g_nSelfTestFailures = 0;
	int g_nSelfTestChecks = 0;

	void Require(bool bCondition, const QString& strWhat)
	{
		++g_nSelfTestChecks;
		if (!bCondition)
		{
			++g_nSelfTestFailures;
			qWarning("selftest: FAIL %s", qPrintable(strWhat));
		}
	}

	// A word that is certainly in the document, for the find checks: the first run
	// of >= 4 ASCII letters. Derived from the file rather than hardcoded, so this
	// keeps working when the files CI passes in change.
	QString FirstWordOf(CEditorWidget* pEditor)
	{
		const sptr_t nLength = pEditor->Send(SCI_GETLENGTH);
		QByteArray text(static_cast<int>(nLength) + 1, '\0');
		pEditor->Send(SCI_GETTEXT, static_cast<uptr_t>(nLength) + 1,
			reinterpret_cast<sptr_t>(text.data()));
		text.truncate(static_cast<int>(nLength));

		int nRunStart = -1;
		for (int i = 0; i <= text.size(); ++i)
		{
			const bool bIsLetter = i < text.size()
				&& ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z'));
			if (bIsLetter && nRunStart < 0)
			{
				nRunStart = i;
			}
			else if (!bIsLetter && nRunStart >= 0)
			{
				if (i - nRunStart >= 4)
				{
					return QString::fromLatin1(text.mid(nRunStart, i - nRunStart));
				}
				nRunStart = -1;
			}
		}
		return QString();
	}

	// Scintilla does not send SCN_UPDATEUI from the message that moved the caret.
	// It records what changed and flushes the notification from Editor::Paint and
	// Editor::Idle (Editor.cxx:1893, :5296), so a headless test that only sends
	// SCI_GOTOPOS has not run the handler at all - and would pass or fail on
	// whatever the previous check happened to leave behind.
	//
	// grab() forces a synchronous paintEvent, which is the paint path, and works
	// under QT_QPA_PLATFORM=offscreen where nothing is ever shown on a screen.
	void FlushUpdateUi(CEditorWidget* pEditor)
	{
		pEditor->viewport()->grab();
	}

	// 0x00BBGGRR, as Scintilla returns colours - the inverse of EditorWidget's
	// ToScintillaColour, kept local to the test so the two do not share a bug.
	QString ColourToString(sptr_t nColour)
	{
		return QStringLiteral("#%1%2%3")
			.arg(nColour & 0xFF, 2, 16, QLatin1Char('0'))
			.arg((nColour >> 8) & 0xFF, 2, 16, QLatin1Char('0'))
			.arg((nColour >> 16) & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
	}

	// Every [start, end) run the tag-match indicator covers. Walks the document
	// with SCI_INDICATORVALUEAT / SCI_INDICATOREND rather than asking the matcher
	// what it painted, so the check sees what a user would see.
	QList<QPair<int, int>> MarkedRanges(CEditorWidget* pEditor, sptr_t nLength)
	{
		const int INDIC_TAGMATCH = 10;		// src/EditorCommonDef.h:48
		QList<QPair<int, int>> ranges;
		sptr_t at = 0;
		while (at < nLength)
		{
			if (pEditor->Send(SCI_INDICATORVALUEAT, INDIC_TAGMATCH, at) != 0)
			{
				const sptr_t nEnd = pEditor->Send(SCI_INDICATOREND, INDIC_TAGMATCH, at);
				// A zero-width run would spin here; INDICATOREND returning `at`
				// is the only way that happens and it means no run at all.
				if (nEnd <= at)
				{
					break;
				}
				ranges.append(qMakePair(static_cast<int>(at), static_cast<int>(nEnd)));
				at = nEnd;
			}
			else
			{
				++at;
			}
		}
		return ranges;
	}

	// src/EditorCommonDef.h:52, INDIC_CONTAINER + 6.
	const int URL_INDICATOR = 14;

	int DistinctStyleCount(CEditorWidget* pEditor)
	{
		pEditor->Send(SCI_COLOURISE, 0, -1);
		const sptr_t nLength = pEditor->Send(SCI_GETLENGTH);
		bool seen[256] = { false };
		int nDistinct = 0;
		for (sptr_t i = 0; i < nLength; ++i)
		{
			const int nStyle = static_cast<int>(pEditor->Send(SCI_GETSTYLEAT,
				static_cast<uptr_t>(i))) & 0xFF;
			if (!seen[nStyle])
			{
				seen[nStyle] = true;
				++nDistinct;
			}
		}
		return nDistinct;
	}
}

int CMainWindow::RunSelfTest(const QStringList& files)
{
	g_nSelfTestFailures = 0;
	g_nSelfTestChecks = 0;

	// The window has to be SHOWN, not merely constructed. A QDockWidget's
	// visibility is only real once its parent window is - setVisible(true) on a
	// child of a hidden window leaves isVisible() false and isHidden()
	// unchanged, so the dock show/hide checks below would assert against a state
	// no user could ever be in. Harmless under QT_QPA_PLATFORM=offscreen, which
	// is what CI runs, and it is what RenderScreenshots already does.
	show();

	if (files.isEmpty())
	{
		qWarning("selftest: no files given - nothing to check");
		return 1;
	}

	//----------------------------------------------------------------------
	// Tabs
	//----------------------------------------------------------------------
	for (const QString& strPath : files)
	{
		Require(OpenFile(strPath), QStringLiteral("open %1").arg(strPath));
	}
	Require(GetTabCount() == files.size(),
		QStringLiteral("one tab per file: %1 tabs for %2 files")
			.arg(GetTabCount()).arg(files.size()));
	// Opening the same path twice must raise the existing tab, not duplicate it.
	OpenFile(files.first());
	Require(GetTabCount() == files.size(), QStringLiteral("re-opening a file does not add a tab"));

	//----------------------------------------------------------------------
	// Lexer, both themes, find, status bar - per tab
	//----------------------------------------------------------------------
	int nFoldClicksChecked = 0;
	int nBraceMatchesChecked = 0;
	int nTagMatchFilesChecked = 0;
	int nUrlFilesChecked = 0;
	int nAutoCompleteChecked = 0;
	for (int i = 0; i < GetTabCount(); ++i)
	{
		m_pTabs->setCurrentIndex(i);
		CEditorWidget* pEditor = GetCurrentEditor();
		Require(pEditor != nullptr, QStringLiteral("tab %1 holds an editor").arg(i));
		if (pEditor == nullptr)
		{
			continue;
		}
		const QString strName = pEditor->GetDisplayName();

		// Lexer. Every file CI passes in has a known language; if that ever stops
		// being true this says so rather than quietly passing.
		const int nStyles = DistinctStyleCount(pEditor);
		Require(pEditor->GetLanguageLabel() != m_Data.GetPlainTextLabel(),
			QStringLiteral("%1: language recognised, got '%2'")
				.arg(strName, pEditor->GetLanguageLabel()));
		Require(nStyles >= 2,
			QStringLiteral("%1: lexer produced %2 distinct styles").arg(strName).arg(nStyles));

		// Both themes. The check is that the colours actually change - applying a
		// theme that resolves nothing would leave every style at its default and
		// look like success.
		OnSetTheme(EEditorTheme::Dark);
		const sptr_t nDarkBack = pEditor->Send(SCI_STYLEGETBACK, STYLE_DEFAULT);
		const sptr_t nDarkFore = pEditor->Send(SCI_STYLEGETFORE, STYLE_DEFAULT);
		OnSetTheme(EEditorTheme::Light);
		const sptr_t nLightBack = pEditor->Send(SCI_STYLEGETBACK, STYLE_DEFAULT);
		const sptr_t nLightFore = pEditor->Send(SCI_STYLEGETFORE, STYLE_DEFAULT);
		Require(nDarkBack != nLightBack && nDarkFore != nLightFore,
			QStringLiteral("%1: light and dark differ (back %2/%3, fore %4/%5)")
				.arg(strName).arg(nDarkBack).arg(nLightBack).arg(nDarkFore).arg(nLightFore));
		Require(DistinctStyleCount(pEditor) == nStyles,
			QStringLiteral("%1: the theme switch did not change what the lexer produced")
				.arg(strName));
		OnSetTheme(EEditorTheme::Dark);

		// Find.
		const QString strWord = FirstWordOf(pEditor);
		Require(!strWord.isEmpty(), QStringLiteral("%1: found a word to search for").arg(strName));
		CEditorWidget::SFindOptions options;
		Require(pEditor->HighlightMatches(strWord, options) >= 1,
			QStringLiteral("%1: '%2' matches at least once").arg(strName, strWord));
		pEditor->Send(SCI_GOTOPOS, 0);
		Require(pEditor->FindNext(strWord, options),
			QStringLiteral("%1: FindNext located '%2'").arg(strName, strWord));
		Require(pEditor->Send(SCI_GETSELECTIONEND) > pEditor->Send(SCI_GETSELECTIONSTART),
			QStringLiteral("%1: the match is selected").arg(strName));
		const QString strAbsent = QStringLiteral("zzq_not_in_this_file_zzq");
		Require(pEditor->HighlightMatches(strAbsent, options) == 0,
			QStringLiteral("%1: a pattern that is not there matches nothing").arg(strName));
		Require(!pEditor->FindNext(strAbsent, options),
			QStringLiteral("%1: FindNext reports a miss").arg(strName));

		// A regex whose every match is empty must terminate, and must find one
		// match per line. This is the guard in HighlightMatches, and it is the
		// difference between a count and a hang.
		CEditorWidget::SFindOptions regex;
		regex._Regex = true;
		const int nLineMatches = pEditor->HighlightMatches(QStringLiteral("^"), regex);
		const int nLines = static_cast<int>(pEditor->Send(SCI_GETLINECOUNT));
		Require(nLineMatches == nLines,
			QStringLiteral("%1: zero-length regex matched %2 times for %3 lines")
				.arg(strName).arg(nLineMatches).arg(nLines));
		pEditor->ClearHighlight();

		// Folding. Clicking the fold margin must actually fold, and this is the
		// only way to know: Scintilla handles that click internally under
		// SC_AUTOMATICFOLD_CLICK, so no signal fires and nothing in the widget's
		// own code runs. An A/B with this click is what proved a marginClicked
		// handler here would be dead code.
		if (pEditor->Send(SCI_GETMARGINWIDTHN, 2) > 0)
		{
			pEditor->Send(SCI_COLOURISE, 0, -1);
			sptr_t nHeaderLine = -1;
			const sptr_t nLines = pEditor->Send(SCI_GETLINECOUNT);
			for (sptr_t line = 0; line < nLines && nHeaderLine < 0; ++line)
			{
				if (pEditor->Send(SCI_GETFOLDLEVEL, static_cast<uptr_t>(line))
					& SC_FOLDLEVELHEADERFLAG)
				{
					nHeaderLine = line;
				}
			}
			// Not every file has a foldable block - a four-line markdown fixture
			// does not - so this is counted, not required per file, and the count
			// is asserted once at the end.
			if (nHeaderLine >= 0)
			{
				const int nX = static_cast<int>(pEditor->Send(SCI_GETMARGINWIDTHN, 0)) + 8;
				const int nY = static_cast<int>(pEditor->Send(SCI_POINTYFROMPOSITION, 0,
					pEditor->Send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(nHeaderLine)))) + 2;
				const QPointF at(nX, nY);
				QMouseEvent press(QEvent::MouseButtonPress, at, at,
					Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
				QMouseEvent release(QEvent::MouseButtonRelease, at, at,
					Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
				QApplication::sendEvent(pEditor->viewport(), &press);
				QApplication::sendEvent(pEditor->viewport(), &release);
				Require(pEditor->Send(SCI_GETALLLINESVISIBLE) == 0,
					QStringLiteral("%1: clicking the fold margin folded the block")
						.arg(strName));
				QApplication::sendEvent(pEditor->viewport(), &press);
				QApplication::sendEvent(pEditor->viewport(), &release);
				Require(pEditor->Send(SCI_GETALLLINESVISIBLE) == 1,
					QStringLiteral("%1: clicking it again unfolded").arg(strName));
				++nFoldClicksChecked;
			}
		}

		// View toggles. Both start off, as on Windows, and both must survive being
		// turned on and off again - a toggle that only works once is a real bug
		// and an easy one to ship.
		Require(!pEditor->IsWordWrap() && !pEditor->IsLongLineMarker(),
			QStringLiteral("%1: wrap and the long-line marker start off").arg(strName));
		OnToggleWordWrap(true);
		Require(pEditor->IsWordWrap(), QStringLiteral("%1: wrap turns on").arg(strName));
		OnToggleWordWrap(false);
		Require(!pEditor->IsWordWrap(), QStringLiteral("%1: wrap turns off").arg(strName));
		OnToggleLongLineMarker(true);
		Require(pEditor->IsLongLineMarker(),
			QStringLiteral("%1: the long-line marker turns on").arg(strName));
		OnToggleLongLineMarker(false);
		Require(!pEditor->IsLongLineMarker(),
			QStringLiteral("%1: the long-line marker turns off").arg(strName));

		// Indentation guides come from the language, and the one that differs is
		// python - so assert the mode matches what core/ says rather than that it
		// is merely non-zero.
		const Core::SLanguageInfo* pLang = m_Data.DetectLanguage(strName);
		if (pLang != nullptr)
		{
			const sptr_t nWanted = (pLang->_IndentGuides == "lookforward")
				? SC_IV_LOOKFORWARD : SC_IV_LOOKBOTH;
			Require(pEditor->Send(SCI_GETINDENTATIONGUIDES) == nWanted,
				QStringLiteral("%1: indentation guides are %2 as core/ specifies")
					.arg(strName, QString::fromStdString(pLang->_IndentGuides)));
		}

		// Brace matching. The highlight itself is paint-time state with no getter,
		// but the highlight GUIDE that DoBraceMatchHighlight sets alongside it does
		// have one - and it is set to the brace's own column on a match and to 0
		// on a miss, so a file with a brace at a non-zero column distinguishes the
		// two branches. Files with no such brace are counted, not required.
		{
			sptr_t nBrace = -1;
			const sptr_t nLength = pEditor->Send(SCI_GETLENGTH);
			for (sptr_t p = 0; p < nLength && nBrace < 0; ++p)
			{
				if (pEditor->Send(SCI_BRACEMATCH, static_cast<uptr_t>(p), 0) >= 0
					&& pEditor->Send(SCI_GETCOLUMN, static_cast<uptr_t>(p)) > 0)
				{
					nBrace = p;
				}
			}
			if (nBrace >= 0)
			{
				const sptr_t nColumn = pEditor->Send(SCI_GETCOLUMN, static_cast<uptr_t>(nBrace));
				// The caret goes AFTER the brace, which is where it lands when you
				// type one - CEditorCtrl matches on position - 1 for that reason.
				pEditor->Send(SCI_GOTOPOS, static_cast<uptr_t>(nBrace + 1));
				FlushUpdateUi(pEditor);
				Require(pEditor->Send(SCI_GETHIGHLIGHTGUIDE) == nColumn,
					QStringLiteral("%1: a matched brace highlights the guide at its column "
						"(%2, got %3)").arg(strName).arg(nColumn)
						.arg(pEditor->Send(SCI_GETHIGHLIGHTGUIDE)));

				// Position 0 asks Scintilla about position -1, which matches nothing,
				// so the highlight must be cleared rather than left behind.
				pEditor->Send(SCI_GOTOPOS, 0);
				FlushUpdateUi(pEditor);
				Require(pEditor->Send(SCI_GETHIGHLIGHTGUIDE) == 0,
					QStringLiteral("%1: no matching brace clears the guide").arg(strName));

				// The styling half. Without it the highlight falls back to
				// Scintilla's default and is nearly invisible - the logic can be
				// entirely correct and the feature still not visible on screen,
				// which is exactly what shipped before this check existed.
				Core::SColor light, bad;
				Require(m_Data.GetTheme(m_Theme).ResolveRole("braceLightColor", light)
					&& m_Data.GetTheme(m_Theme).ResolveRole("braceBadColor", bad),
					QStringLiteral("%1: core/ resolves both brace roles").arg(strName));
				const sptr_t nLight = (light._Blue << 16) | (light._Green << 8) | light._Red;
				const sptr_t nBad = (bad._Blue << 16) | (bad._Green << 8) | bad._Red;
				Require(pEditor->Send(SCI_STYLEGETFORE, STYLE_BRACELIGHT) == nLight,
					QStringLiteral("%1: the matched brace is styled %2, got %3")
						.arg(strName, ColourToString(nLight),
							ColourToString(pEditor->Send(SCI_STYLEGETFORE, STYLE_BRACELIGHT))));
				Require(pEditor->Send(SCI_STYLEGETFORE, STYLE_BRACEBAD) == nBad,
					QStringLiteral("%1: the unmatched brace is styled %2, got %3")
						.arg(strName, ColourToString(nBad),
							ColourToString(pEditor->Send(SCI_STYLEGETFORE, STYLE_BRACEBAD))));
				Require(pEditor->Send(SCI_STYLEGETBOLD, STYLE_BRACELIGHT) == 1
					&& pEditor->Send(SCI_STYLEGETBOLD, STYLE_BRACEBAD) == 1,
					QStringLiteral("%1: both brace styles are bold").arg(strName));
				// SCI_STYLECLEARALL resets these, so they have to be applied after
				// it. A check that the colour merely differs from black would pass
				// with the styling in the wrong place; this one would not.
				Require(nLight != pEditor->Send(SCI_STYLEGETFORE, STYLE_DEFAULT),
					QStringLiteral("%1: the brace colour survived STYLECLEARALL")
						.arg(strName));
				++nBraceMatchesChecked;
			}
		}

		// Selection painting. The caret-line band is drawn only when nothing is
		// selected; with a selection it would sit underneath and fight it.
		{
			pEditor->Send(SCI_GOTOPOS, 0);
			FlushUpdateUi(pEditor);
			Require(pEditor->Send(SCI_GETCARETLINEVISIBLE) == 1,
				QStringLiteral("%1: the caret line is drawn with no selection").arg(strName));

			pEditor->Send(SCI_SETSEL, 0, 4);
			FlushUpdateUi(pEditor);
			Require(pEditor->Send(SCI_GETCARETLINEVISIBLE) == 0,
				QStringLiteral("%1: the caret line is hidden while text is selected")
					.arg(strName));

			// Back to an empty selection: a toggle that only works one way is a real
			// bug and an easy one to ship.
			pEditor->Send(SCI_SETSEL, 4, 4);
			FlushUpdateUi(pEditor);
			Require(pEditor->Send(SCI_GETCARETLINEVISIBLE) == 1,
				QStringLiteral("%1: the caret line comes back when the selection is emptied")
					.arg(strName));
			Require(pEditor->Send(SCI_GETSELALPHA) == 60,
				QStringLiteral("%1: the selection is restored at alpha 60, got %2")
					.arg(strName).arg(pEditor->Send(SCI_GETSELALPHA)));
		}

		// The selection background comes from the selectionTextColor ROLE, which
		// takes the palette key "black" on light and "white" on dark - so it is
		// the one editor colour that cannot be resolved by a single palette key.
		//
		// KNOWN LIMIT, stated rather than papered over: this cannot distinguish
		// selectionTextColor from editorTextColor. Both roles resolve to #000000
		// on light and #FFFFFF on dark in the shipped themes, so a frontend that
		// read the wrong one would paint identical pixels and pass every check
		// below. Verified by mutation - swapping the role here leaves the selftest
		// green. What makes it right is the role name at the call site, which
		// core/tests/TestLanguageData.cpp pins to the palette keys the C++ names.
		//
		// The comparison against core/ is therefore the load-bearing half: it is a
		// tautology while the two agree and becomes a real check the day a palette
		// changes, which is exactly when a hard-coded literal would go stale.
		{
			for (int t = 0; t < 2; ++t)
			{
				const EEditorTheme which = (t == 0) ? EEditorTheme::Light : EEditorTheme::Dark;
				OnSetTheme(which);
				Core::SColor wanted;
				Require(m_Data.GetTheme(which).ResolveRole("selectionTextColor", wanted),
					QStringLiteral("%1: core/ resolves the selectionTextColor role").arg(strName));
				const sptr_t nGot = pEditor->Send(SCI_GETELEMENTCOLOUR,
					SC_ELEMENT_SELECTION_BACK) & 0xFFFFFF;
				const sptr_t nWanted = (wanted._Blue << 16) | (wanted._Green << 8) | wanted._Red;
				Require(nGot == nWanted,
					QStringLiteral("%1: %2 selection background matches core/ (%3, got %4)")
						.arg(strName, t == 0 ? QStringLiteral("light") : QStringLiteral("dark"),
							ColourToString(nWanted), ColourToString(nGot)));
			}

			// And the two literals the C++ names today, so a silent palette edit is
			// caught rather than merely tracked.
			OnSetTheme(EEditorTheme::Light);
			const sptr_t nLightSel = pEditor->Send(SCI_GETELEMENTCOLOUR,
				SC_ELEMENT_SELECTION_BACK) & 0xFFFFFF;
			OnSetTheme(EEditorTheme::Dark);
			const sptr_t nDarkSel = pEditor->Send(SCI_GETELEMENTCOLOUR,
				SC_ELEMENT_SELECTION_BACK) & 0xFFFFFF;
			Require(nLightSel == 0x000000,
				QStringLiteral("%1: light selection background is #000000, got %2")
					.arg(strName, ColourToString(nLightSel)));
			Require(nDarkSel == 0xFFFFFF,
				QStringLiteral("%1: dark selection background is #FFFFFF, got %2")
					.arg(strName, ColourToString(nDarkSel)));
			Require(nLightSel != nDarkSel,
				QStringLiteral("%1: the two themes select differently").arg(strName));
		}

		// XML/HTML tag matching. Checked by INVARIANT rather than against a table
		// of expected offsets: the matcher is a 250-line transcription, and a
		// table of numbers copied out of its own output would agree with it by
		// construction. Everything below is computed from the document text with
		// no reference to how the matcher works.
		{
			const Core::SLanguageInfo* pLang2 = m_Data.DetectLanguage(strName);
			const bool bShouldMatch = (pLang2 != nullptr) && pLang2->_TagMatch;

			const sptr_t nLength = pEditor->Send(SCI_GETLENGTH);
			QByteArray text(static_cast<int>(nLength) + 1, '\0');
			pEditor->Send(SCI_GETTEXT, static_cast<uptr_t>(nLength) + 1,
				reinterpret_cast<sptr_t>(text.data()));
			text.truncate(static_cast<int>(nLength));

			int nCaretsWithAMatch = 0;
			for (int nAngle = 0; nAngle < text.size(); ++nAngle)
			{
				if (text.at(nAngle) != '<')
				{
					continue;
				}
				// Two past the '<', so the caret is inside the name for both
				// "<name" and "</name".
				pEditor->Send(SCI_GOTOPOS, static_cast<uptr_t>(nAngle + 2));
				FlushUpdateUi(pEditor);

				const QList<QPair<int, int>> marked = MarkedRanges(pEditor, nLength);
				if (marked.isEmpty())
				{
					continue;
				}
				++nCaretsWithAMatch;

				// 1. Every marked range must start at a '<' or be a bare tail
				//    (">" or "/>"). Anything else means the highlight landed on
				//    text rather than on markup.
				// 2. The name in the open tag and the name in the close tag must
				//    be the same string.
				// 3. The pair must ENCLOSE the caret. This is the one that
				//    catches an off-by-one resolving to a neighbouring tag.
				QString strOpenName, strCloseName;
				int nFirstStart = -1, nLastEnd = -1;
				bool bWellFormed = true;
				for (const QPair<int, int>& range : marked)
				{
					const QByteArray piece = text.mid(range.first, range.second - range.first);
					if (nFirstStart < 0)
					{
						nFirstStart = range.first;
					}
					nLastEnd = range.second;
					if (piece == ">" || piece == "/>")
					{
						continue;			// the open tag's tail
					}
					if (!piece.startsWith('<'))
					{
						bWellFormed = false;
						continue;
					}
					const bool bClose = piece.startsWith("</");
					QString& strName2 = bClose ? strCloseName : strOpenName;
					strName2 = QString::fromUtf8(piece.mid(bClose ? 2 : 1))
						.remove(QLatin1Char('>'));
				}
				Require(bWellFormed,
					QStringLiteral("%1: every tag-match range begins at a '<' or is a tail")
						.arg(strName));
				Require(!strOpenName.isEmpty(),
					QStringLiteral("%1: the tag-match highlight names an open tag").arg(strName));
				if (!strCloseName.isEmpty())
				{
					Require(strOpenName == strCloseName,
						QStringLiteral("%1: open <%2> pairs with close </%3>")
							.arg(strName, strOpenName, strCloseName));
				}
				Require(nFirstStart <= nAngle && nLastEnd >= nAngle,
					QStringLiteral("%1: the pair at [%2..%3] encloses the caret's tag at %4")
						.arg(strName).arg(nFirstStart).arg(nLastEnd).arg(nAngle));

				// 4. At most three runs: the open tag's name, its tail, and the
				//    close tag - and fewer when two of them abut and Scintilla
				//    merges them. More than three means highlights from an
				//    earlier caret position were never cleared.
				Require(marked.size() <= 3,
					QStringLiteral("%1: %2 highlighted runs at caret %3, expected at most 3 "
						"- stale highlights are not being cleared")
						.arg(strName).arg(marked.size()).arg(nAngle));

				// 5. The open tag must END at a real close angle, with balanced
				//    quotes in between. This is what catches a search that walked
				//    into an attribute value: <item note="a>b"> would otherwise
				//    stop at the '>' inside the string, which still looks like a
				//    perfectly good tail to every check above.
				const int nOpenTagStart = nFirstStart;
				int nOpenTagEnd = -1;
				for (const QPair<int, int>& range : marked)
				{
					const QByteArray piece = text.mid(range.first, range.second - range.first);
					if (!piece.startsWith("</"))
					{
						nOpenTagEnd = range.second;
					}
				}
				if (nOpenTagEnd > nOpenTagStart)
				{
					const QByteArray tag = text.mid(nOpenTagStart, nOpenTagEnd - nOpenTagStart);
					Require(tag.endsWith('>'),
						QStringLiteral("%1: the open tag at %2 ends at a '>', got %3")
							.arg(strName).arg(nOpenTagStart).arg(QString::fromUtf8(tag)));
					Require(tag.count('"') % 2 == 0,
						QStringLiteral("%1: the open tag at %2 has balanced quotes, got %3 "
							"- the search stopped inside an attribute value")
							.arg(strName).arg(nOpenTagStart).arg(QString::fromUtf8(tag)));
				}
			}

			if (bShouldMatch)
			{
				Require(nCaretsWithAMatch > 0,
					QStringLiteral("%1: tag matching fired somewhere in the file").arg(strName));

				// The second gate: with a selection up, the highlight is not
				// RECOMPUTED. Note "not recomputed", not "cleared" - the MFC
				// gates the whole call (src/EditorView.cpp:6186-6189) and the
				// clearing lives inside it, so a highlight painted a moment ago
				// stays on screen while the user selects. This port does the
				// same, and an earlier version of this check asserted the
				// highlight vanished, which no build has ever done.
				//
				// So the check is that it FREEZES: park the caret in one tag,
				// select inside a different one, and the highlight must still
				// describe the first. Dropping the gate makes it follow the
				// second.
				const int nFirstAngle = text.indexOf('<');
				const int nSecondAngle = text.indexOf('<', nFirstAngle + 1);
				if (nFirstAngle >= 0 && nSecondAngle > nFirstAngle)
				{
					pEditor->Send(SCI_GOTOPOS, static_cast<uptr_t>(nFirstAngle + 2));
					FlushUpdateUi(pEditor);
					const QList<QPair<int, int>> before = MarkedRanges(pEditor, nLength);
					Require(!before.isEmpty(),
						QStringLiteral("%1: the first tag is highlighted to begin with")
							.arg(strName));

					pEditor->Send(SCI_SETSEL, static_cast<uptr_t>(nSecondAngle + 2),
						nSecondAngle + 4);
					FlushUpdateUi(pEditor);
					Require(MarkedRanges(pEditor, nLength) == before,
						QStringLiteral("%1: selecting inside another tag leaves the "
							"highlight where it was").arg(strName));

					// And it follows the caret again once nothing is selected - a
					// gate that latches off is as wrong as one that never fires.
					pEditor->Send(SCI_SETSEL, static_cast<uptr_t>(nSecondAngle + 2),
						nSecondAngle + 2);
					FlushUpdateUi(pEditor);
					const QList<QPair<int, int>> after = MarkedRanges(pEditor, nLength);
					Require(!after.isEmpty() && after != before,
						QStringLiteral("%1: emptying the selection moves the highlight to "
							"the caret's own tag").arg(strName));
				}
				++nTagMatchFilesChecked;
			}
			else
			{
				// A language that must NOT tag-match. Without this, marking every
				// language would pass every check above.
				Require(nCaretsWithAMatch == 0,
					QStringLiteral("%1: tag matching stays off for a non-tag language "
						"(fired %2 times)").arg(strName).arg(nCaretsWithAMatch));
			}
			pEditor->Send(SCI_GOTOPOS, 0);
			FlushUpdateUi(pEditor);
		}

		// URL hotspots. The check is the underlined TEXT, not offsets: what the
		// user sees is which characters are underlined, and comparing strings
		// says what went wrong when it breaks.
		{
			const sptr_t nLength = pEditor->Send(SCI_GETLENGTH);
			QByteArray text(static_cast<int>(nLength) + 1, '\0');
			pEditor->Send(SCI_GETTEXT, static_cast<uptr_t>(nLength) + 1,
				reinterpret_cast<sptr_t>(text.data()));
			text.truncate(static_cast<int>(nLength));

			QStringList underlined;
			sptr_t at = 0;
			while (at < nLength)
			{
				if (pEditor->Send(SCI_INDICATORVALUEAT, URL_INDICATOR, at) != 0)
				{
					const sptr_t nEnd = pEditor->Send(SCI_INDICATOREND, URL_INDICATOR, at);
					if (nEnd <= at)
					{
						break;
					}
					underlined.append(QString::fromUtf8(
						text.mid(static_cast<int>(at), static_cast<int>(nEnd - at))));
					at = nEnd;
				}
				else
				{
					++at;
				}
			}

			const bool bUrlsOn = m_Data.GetSettings().EnableUrlHighlight();
			if (!bUrlsOn)
			{
				// The setting is off, so NOTHING may be underlined - the
				// mirror image of the check below, and the one that proves
				// EnableUrlHighlight is actually consulted rather than assumed.
				Require(underlined.isEmpty(),
					QStringLiteral("%1: URL highlighting stays off when disabled, "
						"but underlined %2").arg(strName).arg(underlined.size()));
			}
			else if (strName == QStringLiteral("urls.md"))
			{
				// Exactly what should be underlined, in order. core/'s own
				// differential test covers the scanner; this covers the wiring -
				// that the right bytes reach Scintilla's indicator, including
				// past a multi-byte character where a character offset would be
				// four bytes short by the last line.
				const QStringList wanted = {
					QStringLiteral("https://example.com/docs"),
					QStringLiteral("mailto:someone@example.com"),
					QStringLiteral("ftp://files.example.com/pub/x.tar.gz"),
					QStringLiteral("file:///etc/hosts"),
					QStringLiteral("http://example.com/x"),
					QStringLiteral("http://example.com/a_(b)"),
					QString::fromUtf8("http://example.com/\xc3\xa1"),
				};
				Require(underlined == wanted,
					QStringLiteral("%1: underlined %2, wanted %3").arg(strName,
						underlined.join(QStringLiteral(" | ")),
						wanted.join(QStringLiteral(" | "))));

				// Re-styling must CLEAR a highlight over text that is not a URL.
				// A fresh document has nothing marked, so simply not clearing
				// would pass every check above - the stale mark has to be put
				// there deliberately. This is what the scanner reporting non-URL
				// segments is for.
				pEditor->Send(SCI_SETINDICATORCURRENT, URL_INDICATOR);
				pEditor->Send(SCI_SETINDICATORVALUE,
					pEditor->Send(SCI_STYLEGETFORE, STYLE_DEFAULT));
				pEditor->Send(SCI_INDICATORFILLRANGE, 0, 5);	// "Visit"
				Require(pEditor->Send(SCI_INDICATORVALUEAT, URL_INDICATOR, 0) != 0,
					QStringLiteral("%1: the stale mark was applied").arg(strName));
				pEditor->ApplyTheme(m_Theme);
				Require(pEditor->Send(SCI_INDICATORVALUEAT, URL_INDICATOR, 0) == 0,
					QStringLiteral("%1: re-styling clears a highlight over non-URL text")
						.arg(strName));
				++nUrlFilesChecked;
			}
			else
			{
				// Everywhere else the invariant is that nothing was underlined
				// that does not start with a supported scheme - source files are
				// full of "//" and "http" inside strings and comments.
				for (const QString& strUrl : underlined)
				{
					const QString strLower = strUrl.toLower();
					Require(strLower.startsWith(QStringLiteral("http:"))
						|| strLower.startsWith(QStringLiteral("https:"))
						|| strLower.startsWith(QStringLiteral("ftp:"))
						|| strLower.startsWith(QStringLiteral("file:"))
						|| strLower.startsWith(QStringLiteral("mailto:")),
						QStringLiteral("%1: underlined '%2' starts with a supported scheme")
							.arg(strName, strUrl));
				}
			}
		}

		// Autocomplete. The setup values first - these are what Scintilla needs in
		// order to read the list string the widget builds, so a mismatch between
		// the two would show a single entry containing every separator.
		Require(pEditor->Send(SCI_AUTOCGETSEPARATOR) == '$',
			QStringLiteral("%1: autocomplete word separator is '$'").arg(strName));
		Require(pEditor->Send(SCI_AUTOCGETTYPESEPARATOR) == '?',
			QStringLiteral("%1: autocomplete type separator is '?'").arg(strName));
		Require(pEditor->Send(SCI_AUTOCGETMAXWIDTH) == 100,
			QStringLiteral("%1: autocomplete max width is 100").arg(strName));
		// Follows the SETTING, not the shipped default it was originally
		// written against - which broke the moment the self-test started being
		// run against a settings file that turns it off.
		Require((pEditor->Send(SCI_AUTOCGETIGNORECASE) != 0)
				== m_Data.GetSettings().AutoCompleteIgnoreCase(),
			QStringLiteral("%1: autocomplete case-folding follows the setting (%2)")
				.arg(strName).arg(m_Data.GetSettings().AutoCompleteIgnoreCase()));

		// The list itself. Derived from core/'s own keyword blob rather than a
		// hard-coded word, so this works on every language in the corpus: take a
		// real keyword, ask for its own prefix, and it must be offered.
		{
			const Core::SLanguageInfo* pLang3 = m_Data.DetectLanguage(strName);
			if (pLang3 != nullptr && !pLang3->_Keywords.empty())
			{
				const QString strKeywords = QString::fromStdString(pLang3->_Keywords);
				const QStringList keywords = strKeywords.split(QLatin1Char(' '),
					Qt::SkipEmptyParts);
				// A keyword of at least three characters, so the prefix is not so
				// short that it proves nothing.
				QString strKeyword;
				for (const QString& candidate : keywords)
				{
					if (candidate.size() >= 3)
					{
						strKeyword = candidate;
						break;
					}
				}
				if (!strKeyword.isEmpty())
				{
					const QStringList offered =
						pEditor->GetAutoCompleteList(strKeyword.left(2));
					Require(offered.contains(strKeyword),
						QStringLiteral("%1: '%2' is offered for prefix '%3'")
							.arg(strName, strKeyword, strKeyword.left(2)));
					++nAutoCompleteChecked;
				}
			}

			// A prefix nothing starts with offers nothing. Without this, a list
			// builder that ignored the prefix entirely would pass the check above.
			Require(pEditor->GetAutoCompleteList(
					QStringLiteral("zzqzzq_not_a_prefix")).isEmpty(),
				QStringLiteral("%1: an unknown prefix offers nothing").arg(strName));
			// And an empty prefix, which would otherwise offer the whole document.
			Require(pEditor->GetAutoCompleteList(QString()).isEmpty(),
				QStringLiteral("%1: an empty prefix offers nothing").arg(strName));
			// Follows the SETTING. This asserted the shipped default and was the
			// last of three such checks - the other two broke as soon as a run
			// configured otherwise; this one survived only because the fixture
			// did not flip the key, which it now does.
			const bool bIgnoreNumbers = m_Data.GetSettings().AutoCompleteIgnoreNumbers();
			const QStringList numeric = pEditor->GetAutoCompleteList(QStringLiteral("1"));
			if (bIgnoreNumbers)
			{
				Require(numeric.isEmpty(),
					QStringLiteral("%1: a numeric prefix offers nothing when "
						"AutoCompleteIgnoreNumbers is set").arg(strName));
			}
			else
			{
				for (const QString& strWord : numeric)
				{
					Require(strWord.startsWith(QLatin1Char('1')),
						QStringLiteral("%1: with the setting off, numeric matches are "
							"offered and start with the prefix, got '%2'")
							.arg(strName, strWord));
				}
			}
		}

		// Status bar.
		pEditor->Send(SCI_GOTOPOS, 0);
		UpdateStatusBar();
		const QString strAtStart = m_pStatusPosition->text();
		pEditor->Send(SCI_GOTOLINE, 2);
		UpdateStatusBar();
		Require(m_pStatusPosition->text() != strAtStart,
			QStringLiteral("%1: the position readout follows the caret").arg(strName));
		Require(!m_pStatusEncoding->text().isEmpty() && !m_pStatusEol->text().isEmpty()
			&& !m_pStatusLanguage->text().isEmpty(),
			QStringLiteral("%1: status bar is populated").arg(strName));
	}

	//----------------------------------------------------------------------
	// The message pane - Phase 5's first dock pane. Three things have to hold
	// for the pane to be worth having: it shows what it is told, it can be
	// hidden and brought back, and its layout survives a restart.
	//----------------------------------------------------------------------
	{
		CMessagePane* pPane = GetMessagePane();
		Require(pPane != nullptr, QStringLiteral("the message pane exists"));
		if (pPane != nullptr)
		{
			pPane->ClearAll();
			Require(pPane->GetLineCount() == 0, QStringLiteral("pane: starts empty"));

			// Content, in order, in the colour asked for.
			LogMessage(QStringLiteral("first"), QColor(Qt::red));
			LogMessage(QStringLiteral("second"), QColor(Qt::green));
			Require(pPane->GetLineCount() == 2, QStringLiteral("pane: two messages, got %1")
				.arg(pPane->GetLineCount()));
			Require(pPane->GetText() == QStringLiteral("first\nsecond\n"),
				QStringLiteral("pane: text is both lines in order, got '%1'")
					.arg(pPane->GetText()));
			Require(pPane->GetLineColour(0) == QColor(Qt::red)
				&& pPane->GetLineColour(1) == QColor(Qt::green),
				QStringLiteral("pane: each line keeps its own colour"));

			// An empty message is ignored, and a message that already ends in a
			// newline does not get a second one - both as CMessagePaneDlg does.
			pPane->AddLogMessage(QString(), QColor(Qt::red));
			Require(pPane->GetLineCount() == 2,
				QStringLiteral("pane: an empty message is ignored"));
			pPane->AddLogMessage(QStringLiteral("third\n"), QColor(Qt::blue));
			Require(pPane->GetText() == QStringLiteral("first\nsecond\nthird\n"),
				QStringLiteral("pane: a message ending in a newline gets no second one"));

			pPane->ClearAll();
			Require(pPane->GetLineCount() == 0 && pPane->GetText().isEmpty(),
				QStringLiteral("pane: ClearAll empties it"));

			// Show and hide, through the same action the View menu uses. A pane
			// that cannot be brought back is a pane the user loses.
			QAction* pToggle = pPane->toggleViewAction();
			Require(pToggle != nullptr, QStringLiteral("pane: has a toggle action"));
			const bool bWasVisible = !pPane->isHidden();
			Require(bWasVisible, QStringLiteral("pane: visible to begin with"));
			pToggle->trigger();
			Require(pPane->isHidden(), QStringLiteral("pane: the toggle hides it"));
			Require(!pToggle->isChecked(),
				QStringLiteral("pane: the menu item unchecks with it"));
			pToggle->trigger();
			Require(!pPane->isHidden(), QStringLiteral("pane: the toggle brings it back"));
			Require(pToggle->isChecked(), QStringLiteral("pane: and rechecks"));

			// Layout persistence, through the real Save/RestoreDockState.
			// QSettings is already pointed at a scratch directory for the whole
			// process - main.cpp does it before the window is constructed,
			// because the constructor restores from QSettings and isolating it
			// here would be too late. That ordering is the whole reason this
			// block can assert the pane starts visible at all.
			{
				pPane->hide();
				SaveDockState();
				pPane->show();
				Require(!pPane->isHidden(), QStringLiteral("pane: shown again before restore"));
				RestoreDockState();
				Require(pPane->isHidden(),
					QStringLiteral("pane: a saved layout restores the pane's visibility"));

				// And the other way, so the check cannot pass on a restore that
				// simply hides everything.
				pPane->show();
				SaveDockState();
				pPane->hide();
				RestoreDockState();
				Require(!pPane->isHidden(),
					QStringLiteral("pane: a layout saved while visible restores visible"));
			}
		}
	}

	//----------------------------------------------------------------------
	// The LGPLv3 attribution. This is a COMPLIANCE check, not a feature check:
	// D3 requires the running application to state the exact Qt version it uses
	// and to offer that version's corresponding source, and under D10 this port
	// ships binaries to macOS and Linux. Asserted on the strings rather than the
	// dialog, because a modal box in a headless run has nobody to dismiss it.
	//----------------------------------------------------------------------
	{
		const QString strAttribution = About::AttributionText();

		Require(strAttribution.contains(QStringLiteral("LGPL v3")),
			QStringLiteral("attribution: names the LGPL v3"));

		// The exact version, and it must be the version actually LINKED - the
		// point of dynamic linking is that the user may swap it.
		const QString strQt = About::RuntimeQtVersion();
		Require(!strQt.isEmpty() && strQt.count(QLatin1Char('.')) >= 2,
			QStringLiteral("attribution: a full Qt version, got '%1'").arg(strQt));
		// In its STATEMENT, not merely somewhere in the blob. Checking
		// contains(strQt) alone passes on a wrong version, because the
		// corresponding-source URL further down carries the right one - the
		// assertion would be satisfied by a line other than the one it is about.
		// Found by mutation: hard-coding "Uses Qt 6.0.0" left it green.
		Require(strAttribution.contains(
				QStringLiteral("Uses Qt %1 under").arg(strQt)),
			QStringLiteral("attribution: the LGPL statement names the Qt actually "
				"linked (%1)").arg(strQt));

		// The corresponding-source offer, and it must point at THIS version.
		// A bare archive root would satisfy "contains a URL" and discharge
		// nothing.
		const QString strUrl = About::QtSourceUrl();
		Require(strUrl.contains(strQt),
			QStringLiteral("attribution: the source URL names this Qt version, got '%1'")
				.arg(strUrl));
		Require(strAttribution.contains(strUrl),
			QStringLiteral("attribution: the source URL is actually offered"));

		// And offered as a LINK, which is D3's actual wording. The dialog shows
		// the HTML form; without this check a future edit could quietly drop the
		// anchor and leave only selectable text, which is the weaker reading.
		const QString strHtml = About::AttributionHtml();
		Require(strHtml.contains(QStringLiteral("<a href=\"%1\">").arg(strUrl)),
			QStringLiteral("attribution: the source URL is an anchor, not just text"));
		// The two forms carry the same content - the HTML is derived from the
		// text, and this is what stops them drifting if that ever stops being so.
		QString strStripped = strHtml;
		strStripped.replace(QStringLiteral("<br>"), QStringLiteral("\n"));
		strStripped.remove(QRegularExpression(QStringLiteral("</?a[^>]*>")));
		Require(strStripped == strAttribution.toHtmlEscaped(),
			QStringLiteral("attribution: the HTML and plain forms say the same thing"));

		// The vendored components. These shipped as "()" for one build because
		// SCINTILLA_VERSION was set in a sibling CMake directory and arrived
		// empty - a licence attribution naming no version, which the compiler
		// was entirely happy with.
		Require(!strAttribution.contains(QStringLiteral("unknown"))
			&& !strAttribution.contains(QStringLiteral("Scintilla  ")),
			QStringLiteral("attribution: no placeholder or empty versions"));
		Require(strAttribution.contains(QStringLiteral("Scintilla 5."))
			&& strAttribution.contains(QStringLiteral("Lexilla 5.")),
			QStringLiteral("attribution: states the Scintilla and Lexilla versions"));

		// And the licence files it points at have to exist, or the offer is
		// a dangling reference.
		for (const QString& strFile : { QStringLiteral("License-Qt.txt"),
				QStringLiteral("License-Scintilla.txt") })
		{
			Require(strAttribution.contains(strFile),
				QStringLiteral("attribution: references %1").arg(strFile));
			// From the build, not the working directory. A relative path made
			// --selftest fail with "license file missing" for anyone running
			// the binary from qtbuild/ui-qt/ - a confusing way to report that
			// you are standing in the wrong place. Same convention as
			// VINATEXT_DATA_DIR.
			const QString strPath = QStringLiteral(VINATEXT_LICENSE_DIR)
				+ QLatin1Char('/') + strFile;
			Require(QFile::exists(strPath),
				QStringLiteral("attribution: %1 exists").arg(strPath));
		}
	}

	//----------------------------------------------------------------------
	// Replace. Run on a scratch document rather than the corpus, because these
	// checks MODIFY the text and the round-trip check below needs the files
	// unchanged. The last tab is untitled and empty - see NewUntitled in the
	// constructor - so nothing a user opened is touched.
	//----------------------------------------------------------------------
	{
		CEditorWidget* pScratch = NewUntitled();
		Require(pScratch != nullptr, QStringLiteral("replace: got a scratch document"));
		if (pScratch != nullptr)
		{
			auto SetText = [pScratch](const char* szText)
			{
				pScratch->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(szText));
				pScratch->Send(SCI_EMPTYUNDOBUFFER);
				pScratch->Send(SCI_GOTOPOS, 0);
			};
			auto GetText = [pScratch]() -> QString
			{
				const sptr_t n = pScratch->Send(SCI_GETLENGTH);
				QByteArray b(static_cast<int>(n) + 1, '\0');
				pScratch->Send(SCI_GETTEXT, static_cast<uptr_t>(n) + 1,
					reinterpret_cast<sptr_t>(b.data()));
				b.truncate(static_cast<int>(n));
				return QString::fromUtf8(b);
			};
			CEditorWidget::SFindOptions plain;
			CEditorWidget::SFindOptions regex;
			regex._Regex = true;

			// One at a time, leaving the rest alone.
			SetText("aa bb aa bb aa");
			Require(pScratch->ReplaceNext(QStringLiteral("aa"), QStringLiteral("XX"), plain),
				QStringLiteral("replace: ReplaceNext reports a hit"));
			Require(GetText() == QStringLiteral("XX bb aa bb aa"),
				QStringLiteral("replace: only the first match changed, got '%1'")
					.arg(GetText()));

			// And then the rest.
			Require(pScratch->ReplaceAll(QStringLiteral("aa"), QStringLiteral("YY"), plain) == 2,
				QStringLiteral("replace: ReplaceAll reports 2"));
			Require(GetText() == QStringLiteral("XX bb YY bb YY"),
				QStringLiteral("replace: every remaining match changed, got '%1'")
					.arg(GetText()));

			// A pattern that is not there changes nothing and says so.
			SetText("nothing to see");
			Require(!pScratch->ReplaceNext(QStringLiteral("zzq"), QStringLiteral("x"), plain),
				QStringLiteral("replace: ReplaceNext reports a miss"));
			Require(pScratch->ReplaceAll(QStringLiteral("zzq"), QStringLiteral("x"), plain) == 0,
				QStringLiteral("replace: ReplaceAll reports 0"));
			Require(GetText() == QStringLiteral("nothing to see"),
				QStringLiteral("replace: a miss leaves the document alone"));

			// Back-references, which need SCI_REPLACETARGETRE rather than
			// SCI_REPLACETARGET. Using the wrong one inserts a literal "\1".
			SetText("cat hat");
			Require(pScratch->ReplaceAll(QStringLiteral("\\([ch]\\)at"),
					QStringLiteral("\\1og"), regex) == 2,
				QStringLiteral("replace: regex ReplaceAll reports 2"));
			Require(GetText() == QStringLiteral("cog hog"),
				QStringLiteral("replace: back-references expand, got '%1'").arg(GetText()));

			// A literal backslash must NOT be treated as an escape when the
			// regex box is off - the other half of choosing between the two
			// messages.
			SetText("a b");
			pScratch->ReplaceAll(QStringLiteral("a"), QStringLiteral("\\1"), plain);
			Require(GetText() == QStringLiteral("\\1 b"),
				QStringLiteral("replace: a plain replacement is literal, got '%1'")
					.arg(GetText()));

			// THE HANG. A zero-width match replaced by nothing does not advance
			// the target, so the original's loop would never end. If this check
			// ever regresses the self-test does not fail, it stops - which is
			// why CI runs it under an alarm.
			SetText("l1\nl2\nl3");
			Require(pScratch->ReplaceAll(QStringLiteral("^"), QString(), regex) == 3,
				QStringLiteral("replace: a zero-width match terminates, one per line"));
			Require(GetText() == QStringLiteral("l1\nl2\nl3"),
				QStringLiteral("replace: replacing nothing with nothing changes nothing"));

			// One undo for the whole run. Without SCI_BEGINUNDOACTION the user
			// presses Ctrl+Z once per replacement.
			SetText("z z z z");
			Require(pScratch->ReplaceAll(QStringLiteral("z"), QStringLiteral("Q"), plain) == 4,
				QStringLiteral("replace: four replacements"));
			pScratch->Send(SCI_UNDO);
			Require(GetText() == QStringLiteral("z z z z"),
				QStringLiteral("replace: ONE undo takes back the whole replace-all, got '%1'")
					.arg(GetText()));

			// The view comes back where it was, rather than at the last match.
			SetText("x\nx\nx\nx\nx\nx\nx\nx\nx\nx\nx\nx");
			pScratch->Send(SCI_GOTOLINE, 2);
			const sptr_t nLineBefore = pScratch->Send(SCI_LINEFROMPOSITION,
				static_cast<uptr_t>(pScratch->Send(SCI_GETCURRENTPOS)));
			pScratch->ReplaceAll(QStringLiteral("x"), QStringLiteral("y"), plain);
			Require(pScratch->Send(SCI_LINEFROMPOSITION,
					static_cast<uptr_t>(pScratch->Send(SCI_GETCURRENTPOS))) == nLineBefore,
				QStringLiteral("replace: the caret line survives a replace-all"));

			// SETSAVEPOINT before closing, or ConfirmClose sees a modified
			// document and raises the unsaved-changes box - which in a headless
			// run has nobody to dismiss it and takes the whole self-test down
			// with it. Exactly the hazard main.cpp's bHeadless comment names;
			// this block earned it by hitting it.
			pScratch->Send(SCI_SETSAVEPOINT);
			OnCloseTab(m_pTabs->indexOf(pScratch));
		}
	}

	//----------------------------------------------------------------------
	// Menu shortcuts. Added because Replace shipped bound to a key the
	// operating system eats: QKeySequence::Replace resolves to Cmd+H on macOS,
	// which is Hide Application, so the menu item was unreachable by keyboard
	// on the platform D10 ships to. Nothing failed - the action existed, the
	// menu showed it, and the key simply never arrived.
	//----------------------------------------------------------------------
	{
		// Sequences the platform claims before an application sees them. Only
		// macOS is listed because only macOS reserves single-modifier
		// combinations this way; add to this list, do not replace it.
		const QList<QKeySequence> reserved = {
#ifdef Q_OS_MACOS
			QKeySequence(Qt::CTRL | Qt::Key_H),		// Hide Application
			QKeySequence(Qt::CTRL | Qt::Key_M),		// Minimise
			QKeySequence(Qt::CTRL | Qt::Key_Q),		// Quit - ours by convention
#endif
		};

		QList<QKeySequence> seen;
		int nWithShortcut = 0;
		for (QAction* pAction : menuBar()->findChildren<QAction*>())
		{
			const QKeySequence key = pAction->shortcut();
			if (key.isEmpty())
			{
				continue;
			}
			++nWithShortcut;

			// Quit is legitimately Cmd+Q, so it is exempt from its own entry.
			//
			// remove('&') first: the text is "E&xit", so a plain
			// contains("Exit") is false and this exemption could never fire.
			// It did not matter yet - Exit's shortcut is Qt::Key_Exit on macOS,
			// not Cmd+Q, so the reserved comparison never reached it - but a
			// safety valve that cannot open is worse than none, because the
			// first person to bind Exit to Cmd+Q gets a confusing failure.
			QString strPlain = pAction->text();
			strPlain.remove(QLatin1Char('&'));
			const bool bIsQuit = strPlain.contains(QStringLiteral("Exit"))
				|| strPlain.contains(QStringLiteral("Quit"))
				|| pAction->menuRole() == QAction::QuitRole;
			for (const QKeySequence& taken : reserved)
			{
				if (key == taken && !bIsQuit)
				{
					Require(false, QStringLiteral("shortcut: '%1' is bound to %2, which "
						"the platform reserves - the key never reaches the app")
						.arg(pAction->text(), key.toString(QKeySequence::NativeText)));
				}
			}

			// And no two menu items may share one sequence, which silently makes
			// one of them dead.
			Require(!seen.contains(key),
				QStringLiteral("shortcut: %1 is bound twice, most recently by '%2'")
					.arg(key.toString(QKeySequence::NativeText), pAction->text()));
			seen.append(key);
		}
		Require(nWithShortcut >= 8,
			QStringLiteral("shortcut: found %1 bound actions to check").arg(nWithShortcut));

		// And Replace specifically has one, since that is the regression.
		QAction* pReplaceAction = nullptr;
		for (QAction* pAction : menuBar()->findChildren<QAction*>())
		{
			if (pAction->text().contains(QStringLiteral("Replace")))
			{
				pReplaceAction = pAction;
			}
		}
		Require(pReplaceAction != nullptr && !pReplaceAction->shortcut().isEmpty(),
			QStringLiteral("shortcut: Replace has one"));

		// End to end: triggering the action must actually open the bar in
		// replace mode. The shortcut being right is only half of it - this is
		// the half that says the action does something.
		if (pReplaceAction != nullptr)
		{
			m_pFindBar->hide();
			pReplaceAction->trigger();
			Require(!m_pFindBar->isHidden() && m_pFindBar->IsReplaceVisible(),
				QStringLiteral("shortcut: Replace opens the bar with the replace row"));

			// And the bar's own toggle does it too, so the feature does not
			// depend on a single key working on a single platform.
			m_pFindBar->SetReplaceVisible(false);
			Require(!m_pFindBar->IsReplaceVisible(),
				QStringLiteral("shortcut: the replace row can be hidden"));
			m_pFindBar->SetReplaceVisible(true);
			Require(m_pFindBar->IsReplaceVisible(),
				QStringLiteral("shortcut: the bar's own toggle shows the replace row"));
			OnHideFind();
		}
	}

	//----------------------------------------------------------------------
	// Settings are actually WIRED, not merely loaded. Asserted against the
	// settings object rather than against literals, so this holds whether the
	// run has a settings file or the shipped defaults - and fails if any of
	// these stops being read.
	//----------------------------------------------------------------------
	{
		const Core::CAppSettings& settings = m_Data.GetSettings();
		CEditorWidget* pEditor = GetCurrentEditor();
		Require(pEditor != nullptr, QStringLiteral("settings: an editor to check"));
		if (pEditor != nullptr)
		{
			Require(pEditor->Send(SCI_GETEDGECOLUMN) == settings.LongLineColumnLimit(),
				QStringLiteral("settings: the long-line column is the configured %1, got %2")
					.arg(settings.LongLineColumnLimit())
					.arg(pEditor->Send(SCI_GETEDGECOLUMN)));
			Require((pEditor->Send(SCI_GETCARETLINEFRAME) != 0)
					== settings.DrawCaretLineFrame(),
				QStringLiteral("settings: the caret-line frame follows DrawCaretLineFrame"));
			Require((pEditor->Send(SCI_AUTOCGETIGNORECASE) != 0)
					== settings.AutoCompleteIgnoreCase(),
				QStringLiteral("settings: autocomplete case-folding follows the setting"));

			// The two the review found parsed, tested and never consumed. The
			// marker shape is the visible half of FolderMarginStyle: style 1 is
			// plus/minus, style 3 the shipped tree-box, and they differ in the
			// FOLDER marker - so reading that back says which branch ran.
			const sptr_t nFolderShape = pEditor->Send(SCI_MARKERSYMBOLDEFINED,
				SC_MARKNUM_FOLDER);
			const int aExpected[] = { SC_MARK_ARROW, SC_MARK_PLUS,
				SC_MARK_CIRCLEPLUS, SC_MARK_BOXPLUS };
			const int nStyle = settings.FolderMarginStyle();
			const int nWanted = (nStyle >= 0 && nStyle <= 3)
				? aExpected[nStyle] : SC_MARK_BOXPLUS;
			Require(nFolderShape == nWanted,
				QStringLiteral("settings: the fold marker follows FolderMarginStyle "
					"%1 (wanted %2, got %3)").arg(nStyle).arg(nWanted).arg(nFolderShape));

			// UseFolderMarginClassic is NOT verified here, and saying so is the
			// point of this comment. SCI_SETFOLDMARGINCOLOUR has no getter and
			// there is no SC_ELEMENT_FOLD_MARGIN, so the colour cannot be read
			// back through the public API - the only check available would be
			// sampling pixels from the margin, which is more fragile than the
			// thing it tests.
			//
			// A first attempt asserted that the THEME's margin colour is not
			// black, which is true whether or not the setting is consulted:
			// an assertion that cannot fail. Verified by mutation - disabling
			// the classic branch left it green - and removed rather than left
			// there looking like coverage.
			//
			// What IS checked is that the fixture exercises the branch at all,
			// so the code path runs even though its output is unreadable.
			if (m_Data.GetSettings().WasLoaded())
			{
				Require(settings.UseFolderMarginClassic(),
					QStringLiteral("settings: the flipped fixture exercises the "
						"classic fold margin"));
			}

			// Autocomplete gates behaviour rather than a Scintilla flag, so it
			// has to be provoked - on a SCRATCH document. Typing into pEditor
			// would rewrite a corpus file, and the byte-identical round-trip
			// check further down would then compare a 5-byte document against
			// an 86-byte original. That is exactly what happened on the first
			// run of this block, and only because the run had a settings file:
			// with the shipped defaults EnableAutoComplete is true, the branch
			// never executes, and the damage would have shipped unseen.
			CEditorWidget* pScratch = NewUntitled();
			if (pScratch != nullptr)
			{
				// "while" so the document itself supplies a completion: an
				// untitled scratch has no language, so keywords are empty and a
				// prefix with nothing longer after it would offer nothing
				// whatever the setting says.
				pScratch->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("while\nwh"));
				pScratch->Send(SCI_GOTOPOS, pScratch->Send(SCI_GETLENGTH));
				pScratch->OnCharAddedForTest('h');
				Require((pScratch->Send(SCI_AUTOCACTIVE) != 0)
						== settings.EnableAutoComplete(),
					QStringLiteral("settings: the autocomplete list follows "
						"EnableAutoComplete (%1)").arg(settings.EnableAutoComplete()));
				pScratch->Send(SCI_AUTOCCANCEL);
				pScratch->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pScratch));
			}
		}
	}

	Require(nFoldClicksChecked > 0,
		QStringLiteral("the fold-margin click was exercised on at least one file"));
	Require(nBraceMatchesChecked > 0,
		QStringLiteral("brace matching was exercised on at least one file"));
	Require(nTagMatchFilesChecked > 0,
		QStringLiteral("tag matching was exercised on at least one file"));
	Require(nUrlFilesChecked > 0 || !m_Data.GetSettings().EnableUrlHighlight(),
		QStringLiteral("URL hotspots were exercised on the urls.md fixture"));
	Require(nAutoCompleteChecked > 0,
		QStringLiteral("autocomplete was exercised on at least one file"));

	//----------------------------------------------------------------------
	// Save. The check is byte equality against the file that was opened: an
	// editor that cannot round-trip a file it did not modify is not an editor,
	// and encoding or line-ending damage is exactly the failure a user notices
	// only after their file is already overwritten.
	//----------------------------------------------------------------------
	QTemporaryDir temporary;
	Require(temporary.isValid(), QStringLiteral("created a temporary directory"));
	if (temporary.isValid())
	{
		for (int i = 0; i < GetTabCount(); ++i)
		{
			m_pTabs->setCurrentIndex(i);
			CEditorWidget* pEditor = GetCurrentEditor();
			if (pEditor == nullptr || pEditor->IsUntitled())
			{
				continue;
			}
			const QString strOriginal = pEditor->GetFilePath();
			const QString strCopy = temporary.filePath(QStringLiteral("roundtrip-%1-%2")
				.arg(i).arg(QFileInfo(strOriginal).fileName()));

			QString strError;
			Require(pEditor->SaveFile(strCopy, strError),
				QStringLiteral("saved a copy of %1: %2").arg(strOriginal, strError));

			QFile before(strOriginal);
			QFile after(strCopy);
			Require(before.open(QIODevice::ReadOnly) && after.open(QIODevice::ReadOnly),
				QStringLiteral("re-read both copies of %1").arg(strOriginal));
			const QByteArray bytesBefore = before.readAll();
			const QByteArray bytesAfter = after.readAll();
			Require(bytesBefore == bytesAfter,
				QStringLiteral("%1 round-trips byte for byte (%2 bytes in, %3 out)")
					.arg(QFileInfo(strOriginal).fileName())
					.arg(bytesBefore.size()).arg(bytesAfter.size()));

			// Saving must clear the modified flag, or the user is told their saved
			// file is still dirty and asked about it again on close.
			Require(!pEditor->IsModified(),
				QStringLiteral("%1 is not modified after saving").arg(strOriginal));
		}
	}

	qInfo("selftest: %d check(s) over %d tab(s) from %lld file(s), %d failure(s)",
		g_nSelfTestChecks, GetTabCount(), static_cast<long long>(files.size()),
		g_nSelfTestFailures);
	if (g_nSelfTestFailures == 0)
	{
		qInfo("selftest: PASSED");
	}
	return g_nSelfTestFailures == 0 ? 0 : 1;
}

int CMainWindow::RenderScreenshots(const QStringList& files, const QString& strDirectory)
{
	for (const QString& strPath : files)
	{
		if (!OpenFile(strPath))
		{
			return 1;
		}
	}
	// The find bar is part of the checklist, so it belongs in the picture. Search
	// for something the file certainly contains rather than a guess that would
	// quietly render an empty bar.
	if (CEditorWidget* pEditor = GetCurrentEditor())
	{
		OnShowFind();
		m_pFindBar->Activate(FirstWordOf(pEditor));
		OnPatternChanged();
	}

	show();
	QApplication::processEvents();

	int nFailures = 0;
	const struct { EEditorTheme _Theme; const char* _Name; } shots[] = {
		{ EEditorTheme::Light, "light" },
		{ EEditorTheme::Dark, "dark" },
	};
	for (const auto& shot : shots)
	{
		OnSetTheme(shot._Theme);
		QApplication::processEvents();
		const QString strPath = QStringLiteral("%1/vinatext-%2.png")
			.arg(strDirectory, QLatin1String(shot._Name));
		if (!grab().save(strPath))
		{
			qWarning("could not write %s", qPrintable(strPath));
			++nFailures;
			continue;
		}
		qInfo("wrote %s", qPrintable(strPath));
	}
	return nFailures == 0 ? 0 : 1;
}
