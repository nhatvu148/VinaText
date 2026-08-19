/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "MainWindow.h"

#include "MacAppearance.h"
#include "ResourcePaths.h"

#include "EditorWidget.h"
#include "FindBar.h"
#include "GotoBar.h"
#include "EncodingDialog.h"
#include "WindowListDialog.h"
#include "BookmarkPane.h"
#include "LineTransforms.h"
#include "RegexPresets.h"
#include "TransformDialog.h"
#include "SingleInstance.h"
#include "AboutDialog.h"
#include "PreferencesDialog.h"
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
#include <QDir>
#include <QIcon>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QThread>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QSet>
#include <QTextCodec>

#include <algorithm>
#include <functional>
#include <limits>
#include <QWidget>

CMainWindow::CMainWindow(CEditorData& data, QWidget* pParent)
	: QMainWindow(pParent)
	, m_Data(data)
{
	m_pTabs = new QTabWidget(this);
	m_pTabs->setTabsClosable(true);
	m_pTabs->setMovable(true);
	m_pTabs->setDocumentMode(true);

	m_pFindBar = new CFindBar(this);
	m_pFindBar->hide();
	m_pGotoBar = new CGotoBar(this);
	m_pGotoBar->hide();

	QWidget* pCentral = new QWidget(this);
	QVBoxLayout* pLayout = new QVBoxLayout(pCentral);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pTabs, 1);
	pLayout->addWidget(m_pFindBar);
	// Below the find bar, and INDEPENDENT of it. The MFC makes the two mutually
	// exclusive because they are pages of one tab control, not because anything
	// about them conflicts; here they are separate strips, and hiding a search
	// the user has set up because they also want to jump to a line would be
	// losing state for no reason.
	pLayout->addWidget(m_pGotoBar);
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

	// The second dock pane, and the last one D10 keeps. Created before
	// BuildMenus so the View menu can take its toggleViewAction, exactly as
	// CMessagePane is.
	m_pBookmarkPane = new CBookmarkPane(this);
	addDockWidget(Qt::BottomDockWidgetArea, m_pBookmarkPane);
	// Tabbed with the message pane rather than stacked: two bottom docks each
	// wanting height leaves the editor squeezed, and these are both
	// consult-occasionally panes.
	tabifyDockWidget(m_pMessagePane, m_pBookmarkPane);
	m_pBookmarkPane->hide();

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
		if (!m_pGotoBar->isHidden())
		{
			// Same reason: "line (1 - 240)" describing a file that is no longer
			// in front of the user is worse than no readout at all. The MFC
			// refreshes it on every tab change too - InitGotoRangeByDocument is
			// called from OnTcnSelchangeTab (src/SearchAndReplaceDlg.cpp:390).
			if (CEditorWidget* pEditor = GetCurrentEditor())
			{
				// SyncToDocument and not just the ranges. The offset box shows
				// the caret position, so leaving it holding the previous
				// document's number makes it a readout that lies - and a byte
				// offset is meaningless outside the document it was measured in.
				// Found in review; the MFC has the same staleness, but its field
				// is refreshed only when the tab is re-opened, so the divergence
				// is deliberate.
				m_pGotoBar->SyncToDocument(pEditor->GetLineCount(),
					static_cast<int>(pEditor->Send(SCI_GETLENGTH)),
					pEditor->GetCaretPosition());
			}
		}
	});
	connect(m_pFindBar, &CFindBar::FindRequested, this, &CMainWindow::OnFind);
	connect(m_pFindBar, &CFindBar::ReplaceRequested, this, &CMainWindow::OnReplace);
	connect(m_pFindBar, &CFindBar::ReplaceAllRequested, this, &CMainWindow::OnReplaceAll);
	connect(m_pFindBar, &CFindBar::PatternChanged, this, &CMainWindow::OnPatternChanged);
	connect(m_pFindBar, &CFindBar::CloseRequested, this, &CMainWindow::OnHideFind);
	connect(m_pGotoBar, &CGotoBar::GotoLineRequested, this, &CMainWindow::OnGotoLine);
	connect(m_pGotoBar, &CGotoBar::GotoOffsetRequested, this, &CMainWindow::OnGotoOffset);
	connect(m_pGotoBar, &CGotoBar::CloseRequested, this, &CMainWindow::OnHideGoto);
	connect(m_pBookmarkPane, &CBookmarkPane::BookmarkActivated,
		this, &CMainWindow::OnBookmarkActivated);

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

	// AND WHERE THE REST CAME FROM. These two are resolved at run time from
	// several candidates (ResourcePaths.h), and until they were printed there
	// was no way to tell from the running app WHICH one won - a packaged copy
	// silently falling back to a build tree looks exactly like a working one.
	// That is not hypothetical: testing the relocation by eye needed a fake
	// theme colour to tell the two apart, which is a bad way to find out.
	// Labelled, because the path alone makes the reader work out the only thing
	// they want to know: did this copy find its own files, or fall back to
	// somebody's source tree? "(build tree)" answers it at a glance, and "~"
	// keeps the line short enough to read.
	const QString strLicenceDir = ResourcePaths::LicenseDir();
	LogMessage(tr("Data: %1 (%2)")
		.arg(ResourcePaths::ForDisplay(m_Data.GetDataDir()),
			ResourcePaths::DescribeSource(m_Data.GetDataDir(), QStringLiteral("data"))));
	LogMessage(tr("Licences: %1 (%2)")
		.arg(ResourcePaths::ForDisplay(strLicenceDir),
			ResourcePaths::DescribeSource(strLicenceDir, QStringLiteral("license"))));

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

	// The two encoding submenus, in the two places src/VinaText.rc:306-331 puts
	// them: reopen right under Open, save-as under the save group. Same six
	// fixed encodings, same separator, same "Code Page Table..." at the foot.
	// The MFC's own File menu is the layout being reproduced, not a convention.
	//
	// They are SEPARATE MENUS and not one, because they are separate
	// operations - see CEditorWidget's encoding section.
	BuildEncodingMenu(pFile->addMenu(tr("&Reopen With Encoding")),
		CEncodingDialog::EMode::Reinterpret);

	pFile->addSeparator();
	pFile->addAction(tr("&Save"), QKeySequence::Save, this, [this] { OnSave(); });
	pFile->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, [this] { OnSaveAs(); });
	BuildEncodingMenu(pFile->addMenu(tr("Save As &Encoding")),
		CEncodingDialog::EMode::Convert);
	pFile->addSeparator();
	pFile->addAction(tr("&Close Tab"), QKeySequence::Close, this, [this]
	{
		if (m_pTabs->count() > 0)
		{
			OnCloseTab(m_pTabs->currentIndex());
		}
	});
	QAction* pExit = pFile->addAction(tr("E&xit"), QKeySequence::Quit, this,
		&QWidget::close);
	// EXPLICIT QuitRole, not Qt's text heuristic. On macOS Qt merges this item
	// into the application menu and supplies Cmd+Q itself - which is why
	// QKeySequence::Quit resolves to the useless Qt::Key_Exit here rather than
	// to a real sequence. But the default role is TextHeuristicRole, which
	// decides by matching the ENGLISH word "Exit": the moment tr() returns
	// "Thoát" the merge stops happening and Cmd+Q goes with it. Saying the role
	// outright costs one line and does not depend on the language.
	pExit->setMenuRole(QAction::QuitRole);

	QMenu* pSearch = menuBar()->addMenu(tr("&Search"));

	// The sequence Go to Line claims, decided HERE - before Find Next is bound -
	// because the two want the same key on some platforms and the resolution has
	// to be a decision rather than whichever happens to be assigned second.
	//
	// Ctrl+G is src/VinaText.rc's own accelerator for ID_OPTIONS_GOTOLINE, and
	// what Notepad++, Visual Studio, VS Code, Sublime and gedit all use. On macOS
	// Qt maps Qt::CTRL to Command and Cmd+G is firmly Find Next, so goto takes
	// Cmd+L there - Xcode's and TextMate's jump-to-line.
#ifdef Q_OS_MACOS
	const QKeySequence gotoKey(Qt::CTRL | Qt::Key_L);
#else
	const QKeySequence gotoKey(Qt::CTRL | Qt::Key_G);
#endif

	pSearch->addAction(tr("&Find..."), QKeySequence::Find, this, &CMainWindow::OnShowFind);
	// setShortcutS, plural, and that is a fix rather than a tidy-up. The
	// single-sequence overload takes only the FIRST of a standard key's
	// bindings, and on macOS QKeySequence::FindNext is [F3, Cmd+G] - so Find
	// Next shipped responding to F3 only, while Cmd+G, which is the macOS
	// convention and which Qt itself lists, did nothing. F3 on a Mac laptop
	// needs Fn held down, so the reachable binding was the awkward one and the
	// natural one was dead.
	//
	// Third instance of the same bug: Replace on Cmd+H (unreachable), Exit on
	// Qt::Key_Exit (a key no Mac has), and now this. The pattern is that a
	// shortcut which RESOLVES is not the same as a shortcut that ARRIVES.
	//
	// Minus whatever goto has claimed, which is Qt's list filtered rather than
	// an #ifdef: on Linux and Windows Qt lists Ctrl+G for Find Next as well, and
	// installing it there would take the key away from Go to Line. Filtering by
	// gotoKey means the two can never both be assigned it, on any platform,
	// including ones neither of us has thought about. On macOS gotoKey is Cmd+L,
	// so removeAll does nothing and Find Next keeps both of its bindings.
	QAction* pFindNext = pSearch->addAction(tr("Find &Next"), this, [this] { OnFind(false); });
	QList<QKeySequence> findNextKeys = QKeySequence::keyBindings(QKeySequence::FindNext);
	findNextKeys.removeAll(gotoKey);
	pFindNext->setShortcuts(findNextKeys);

	QAction* pFindPrevious = pSearch->addAction(tr("Find &Previous"),
		this, [this] { OnFind(true); });
	QList<QKeySequence> findPreviousKeys =
		QKeySequence::keyBindings(QKeySequence::FindPrevious);
	findPreviousKeys.removeAll(gotoKey);
	pFindPrevious->setShortcuts(findPreviousKeys);
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

	pSearch->addSeparator();
	QAction* pGoto = pSearch->addAction(tr("&Go to Line..."), this,
		&CMainWindow::OnShowGoto);
	pGoto->setShortcut(gotoKey);

	// The three commands the Goto tab carries that are not prompts. They are
	// buttons in the MFC because the tab was somewhere to put them, but nothing
	// about them needs a text field.
	//
	// This grouping is not invented: src/VinaText.rc:402-411 is a "Goto..."
	// popup menu holding exactly these, in exactly this order, with separators
	// in exactly these two places - Goto Line and Goto Position, then the two
	// paragraph commands, then Goto To Caret. The tab and the menu are two
	// front ends onto the same six handlers, and the menu is the one that maps
	// onto an editor. Note what that menu does NOT carry: Goto Point.
	pSearch->addSeparator();
	pSearch->addAction(tr("Go to &Next Paragraph"),
		QKeySequence(Qt::CTRL | Qt::Key_BracketRight), this, [this]
	{
		if (CEditorWidget* pEditor = GetCurrentEditor())
		{
			pEditor->GotoNextParagraph();
		}
	});
	pSearch->addAction(tr("Go to &Previous Paragraph"),
		QKeySequence(Qt::CTRL | Qt::Key_BracketLeft), this, [this]
	{
		if (CEditorWidget* pEditor = GetCurrentEditor())
		{
			pEditor->GotoPreviousParagraph();
		}
	});
	// Bookmarks. src/VinaText.rc has no accelerators for these at all, so the
	// bindings are this port's - and the first version got them wrong in the
	// way this port keeps getting them wrong.
	//
	// It bound Ctrl+F2 / F2 / Shift+F2, which is what Notepad++, Visual Studio
	// and Qt Creator use, with the note "F2 is free on macOS in a way
	// Cmd+something rarely is". That reasoned about COLLISIONS and ignored
	// REACHABILITY: on a Mac the F-keys are brightness and Mission Control by
	// default, so a bare F2 never arrives unless the user has changed a system
	// setting. Confirmed by a person pressing it. It is the same error as
	// binding Find Next to F3 alone, which PR #47 fixed - one PR earlier.
	//
	// So each command gets TWO bindings: a Cmd/Ctrl one that always arrives,
	// and the F-key the convention expects, which still works for anyone whose
	// F-keys are standard. setShortcuts, not setShortcut - taking only the
	// first is how Find Next lost Cmd+G.
	//
	// Cmd+Shift+[ and Cmd+Shift+] mirror the paragraph commands on Cmd+[ and
	// Cmd+], which is the nearest thing this menu has to a convention.
	pSearch->addSeparator();
	QAction* pToggleMark = pSearch->addAction(tr("Toggle &Bookmark"), this,
		&CMainWindow::OnToggleBookmark);
	pToggleMark->setShortcuts({ QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B),
		QKeySequence(Qt::CTRL | Qt::Key_F2) });
	QAction* pNextMark = pSearch->addAction(tr("Next Book&mark"), this,
		&CMainWindow::OnNextBookmark);
	pNextMark->setShortcuts({ QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight),
		QKeySequence(Qt::Key_F2) });
	QAction* pPrevMark = pSearch->addAction(tr("Previous Boo&kmark"), this,
		&CMainWindow::OnPreviousBookmark);
	pPrevMark->setShortcuts({ QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft),
		QKeySequence(Qt::SHIFT | Qt::Key_F2) });
	pSearch->addAction(tr("Clear All Bookmarks"), this, &CMainWindow::OnClearBookmarks);

	// No keyboard shortcut, because the MFC gives it none either - its binding
	// is the MIDDLE MOUSE BUTTON ("Goto To Caret\tMiddle Mouse"). A menu item is
	// the entry point that survives having no third button, and it is one more
	// feature that does not depend on a key arriving.
	pSearch->addSeparator();
	pSearch->addAction(tr("Scroll to &Caret"), this, [this]
	{
		if (CEditorWidget* pEditor = GetCurrentEditor())
		{
			pEditor->ScrollToCaret();
		}
	});

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
	pView->addAction(m_pBookmarkPane->toggleViewAction());

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
	pView->addSeparator();
	// QKeySequence::Preferences is Cmd+, on macOS and nothing on Windows or
	// Linux, where Ctrl+, is the de facto convention - so both are given rather
	// than trusting one to resolve everywhere. That lesson cost a PR.
	QAction* pPreferences = pView->addAction(tr("&Preferences..."), this,
		&CMainWindow::OnPreferences);
	pPreferences->setShortcuts({ QKeySequence(QKeySequence::Preferences),
		QKeySequence(Qt::CTRL | Qt::Key_Comma) });
	pPreferences->setMenuRole(QAction::PreferencesRole);

	// The window manager. The MFC gives it a toolbar button and Ctrl+Shift+W
	// and NO menu item at all (src/VinaText.rc:179, :241, :1374) - ui-qt/ has
	// no toolbar, so it needs one, and a feature reachable only by a shortcut
	// is one platform quirk away from not existing.
	pView->addSeparator();
	QAction* pWindows = pView->addAction(tr("&Current Windows..."), this,
		&CMainWindow::OnWindowManager);
	pWindows->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));

	// The text transforms get their own menu, as the MFC gives them their own
	// popup rather than scattering them through Edit.
	QMenu* pTransform = menuBar()->addMenu(tr("&Transform"));
	for (const LineTransforms::SCommand& command : LineTransforms::All())
	{
		pTransform->addAction(tr(command._MenuLabel), this, [this, &command]
		{
			OnLineTransform(command);
		});
	}

	QMenu* pHelp = menuBar()->addMenu(tr("&Help"));
	pHelp->addAction(tr("&About VinaText"), this, &CMainWindow::OnAbout);
}

void CMainWindow::BuildEncodingMenu(QMenu* pMenu, CEncodingDialog::EMode mode)
{
	// The MFC's six, with its own labels (src/VinaText.rc:308-313). "ANSI"
	// means the system codepage there; Qt spells that encoding "System", so
	// the label is the MFC's and the value is Qt's.
	//
	// THE ENCODING IS DERIVED FROM THE ENUM, NEVER SPELLED OUT. This table
	// first read { "ANSI", "System" } - the enumerator's own name - and
	// QStringConverter calls that encoding "Locale". encodingForName("System")
	// resolves to nothing, so the ANSI item failed outright, and the eighth
	// instance of "the name of a thing is not the name of the thing it uses"
	// went in the same way as the previous seven: the string looked right.
	// nameForEncoding() is the only thing that knows, so ask it.
	const struct { const char* _Label; QStringConverter::Encoding _Encoding; } fixed[] = {
		{ "ANSI",		QStringConverter::System	},
		{ "UTF-8",		QStringConverter::Utf8		},
		{ "UTF-16-LE",	QStringConverter::Utf16LE	},
		{ "UTF-16-BE",	QStringConverter::Utf16BE	},
		{ "UTF-32-LE",	QStringConverter::Utf32LE	},
		{ "UTF-32-BE",	QStringConverter::Utf32BE	},
	};
	for (const auto& entry : fixed)
	{
		const QString strEncoding = QString::fromLatin1(
			QStringConverter::nameForEncoding(entry._Encoding));
		QAction* pAction = pMenu->addAction(tr(entry._Label), this,
			[this, mode, strEncoding]
		{
			ApplyEncoding(mode, strEncoding);
		});
		// The encoding is on the action as well as in the lambda, so the
		// self-test can ask each menu item what it will actually apply. Without
		// it a check can only confirm the item exists, which is what let an
		// item naming a non-existent encoding ship.
		pAction->setData(strEncoding);
	}
	pMenu->addSeparator();
	pMenu->addAction(tr("Code Page Table..."), this, [this, mode]
	{
		OnChooseEncoding(mode);
	});
}

void CMainWindow::OnChooseEncoding(CEncodingDialog::EMode mode)
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	CEncodingDialog dialog(mode, pEditor->GetEncodingName(), this);
	if (dialog.exec() != QDialog::Accepted)
	{
		return;
	}
	const QString strChosen = dialog.SelectedEncoding();
	if (!strChosen.isEmpty())
	{
		ApplyEncoding(mode, strChosen);
	}
}

void CMainWindow::ApplyEncoding(CEncodingDialog::EMode mode, const QString& strEncoding)
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}

	if (mode == CEncodingDialog::EMode::Reinterpret)
	{
		// THE DESTRUCTIVE HALF. Re-reading from disk throws away whatever is
		// unsaved, so it asks first - and defaults to Cancel, because the
		// answer that loses work should never be the one a stray Return picks.
		if (pEditor->IsModified())
		{
			const QMessageBox::StandardButton answer = QMessageBox::warning(this,
				tr("Reopen with encoding"),
				tr("%1 has unsaved changes.\n\nRe-reading it from disk as %2 will "
					"discard them.").arg(pEditor->GetDisplayName(), strEncoding),
				QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
			if (answer != QMessageBox::Discard)
			{
				return;
			}
		}
		QString strError;
		if (!pEditor->ReloadWithEncoding(strEncoding, strError))
		{
			LogMessage(tr("Reopen failed - %1").arg(strError), QColor(Qt::red));
			QMessageBox::warning(this, tr("Reopen with encoding"), strError);
			return;
		}
		LogMessage(tr("Reopened %1 as %2").arg(pEditor->GetDisplayName(), strEncoding));
	}
	else
	{
		if (!pEditor->SetSaveEncoding(strEncoding))
		{
			const QString strError =
				tr("%1 is not an encoding this build knows.").arg(strEncoding);
			LogMessage(strError, QColor(Qt::red));
			QMessageBox::warning(this, tr("Save as encoding"), strError);
			return;
		}
		// Set, then save - the order CEditorDoc::OnFileSaveAsEncoding uses.
		// If the save is cancelled or fails the document keeps the new encoding,
		// which is also what the MFC does: the choice outlives one save.
		if (OnSave())
		{
			LogMessage(tr("Saved %1 as %2")
				.arg(pEditor->GetDisplayName(), pEditor->GetEncodingName()));
		}
	}
	UpdateStatusBar();
	UpdateTabLabel(pEditor);
}

QList<CWindowListDialog::SEntry> CMainWindow::CollectWindowList() const
{
	QList<CWindowListDialog::SEntry> entries;
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		if (CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i)))
		{
			CWindowListDialog::SEntry entry;
			entry._Name = pEditor->GetDisplayName();
			// Empty for an untitled document, and ALSO for one whose file has
			// gone from disk - the MFC asks PathFileExists rather than the
			// document, so a deleted file reads "N/A" there too.
			entry._Path = QFileInfo::exists(pEditor->GetFilePath())
				? pEditor->GetFilePath() : QString();
			entry._Modified = pEditor->IsModified();
			entries.append(entry);
		}
	}
	return entries;
}

void CMainWindow::ConnectWindowList(CWindowListDialog* pDialog)
{
	connect(pDialog, &CWindowListDialog::ActivateRequested, this, [this](int nRow)
	{
		if (nRow >= 0 && nRow < m_pTabs->count())
		{
			m_pTabs->setCurrentIndex(nRow);
		}
	});
	connect(pDialog, &CWindowListDialog::SaveRequested, this, [this, pDialog](int nRow)
	{
		if (nRow < 0 || nRow >= m_pTabs->count())
		{
			return;
		}
		CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(nRow));
		if (pEditor == nullptr)
		{
			return;
		}

		// SAVES THE ROW WITHOUT SWITCHING TO IT. The first version called
		// OnSave, which works on whatever tab is current, so it had to make the
		// row current first - and saving a background document then silently
		// moved the user somewhere else. COpenTabWindows saves the document it
		// looked up and never touches the active view; matching that is one
		// qobject_cast. Found in review.
		//
		// The exception is a document that has never been saved: it needs a
		// path, and Save As is a modal prompt ABOUT that document, so bringing
		// it to the front first is the honest thing rather than asking the user
		// to name a file they cannot see.
		if (pEditor->IsUntitled())
		{
			m_pTabs->setCurrentIndex(nRow);
			OnSaveAs();
		}
		else
		{
			SaveEditor(pEditor, pEditor->GetFilePath());
		}
		pDialog->SetEntries(CollectWindowList());
	});
	connect(pDialog, &CWindowListDialog::CloseRequested, this,
		[this, pDialog](const QList<int>& rows)
	{
		// SelectedRows() hands these back descending, so closing one does not
		// shift the index of another still to come.
		for (int nRow : rows)
		{
			if (nRow >= 0 && nRow < m_pTabs->count())
			{
				OnCloseTab(nRow);
			}
		}
		pDialog->SetEntries(CollectWindowList());
	});
}

void CMainWindow::OnWindowManager()
{
	CWindowListDialog dialog(this);
	dialog.SetEntries(CollectWindowList());
	ConnectWindowList(&dialog);
	dialog.exec();
}

void CMainWindow::OnLineTransform(const LineTransforms::SCommand& command)
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}

	CTransformDialog dialog(command._Prompt, this);
	if (dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	QString strError;
	const CEditorWidget::FLineTransform fTransform = command._Build(dialog, strError);
	if (!fTransform)
	{
		// The MFC shows "[Error] Inputs are empty!" in a message box and
		// returns. Same shape, with the specific reason rather than one
		// message for every kind of bad input.
		QMessageBox::warning(this, tr("Transform"), strError);
		return;
	}

	const int nLines = pEditor->ApplyLineTransform(fTransform);
	LogMessage(tr("%1 - %2 line(s)").arg(tr(command._MenuLabel)).arg(nLines));
	UpdateStatusBar();
	UpdateTabLabel(pEditor);
}

void CMainWindow::RefreshBookmarks()
{
	// Rebuilt from the markers every time, across every open document. That is
	// the divergence from src/BookmarkWindow.cpp, which maintains a parallel
	// list and updates it only on add and delete - so a line inserted above a
	// bookmark leaves the Windows pane naming a number the marker no longer
	// sits on. Scintilla moves markers; a derived list cannot drift.
	QList<CBookmarkPane::SEntry> entries;
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
		if (pEditor == nullptr)
		{
			continue;
		}
		for (int nLine : pEditor->BookmarkedLines())
		{
			CBookmarkPane::SEntry entry;
			entry._File = pEditor->GetDisplayName();
			entry._Path = pEditor->GetFilePath();
			entry._Line = nLine;
			entry._Text = pEditor->TextOfLine(nLine);
			entries.append(entry);
		}
	}
	m_pBookmarkPane->SetEntries(entries);
}

void CMainWindow::OnToggleBookmark()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	// NO PathFileExists GUARD. CEditorView::OnOptionsAddBookmark refuses to
	// bookmark a document that is not on disk, so an unsaved buffer cannot be
	// marked at all on Windows. Nothing about a marker needs a file, the pane
	// shows the display name for exactly this case, and refusing would be a
	// restriction with no reason behind it.
	const int nLine = pEditor->GetCaretLine();
	ToggleBookmarkAt(pEditor, nLine);
}

void CMainWindow::ToggleBookmarkAt(CEditorWidget* pEditor, int nLine)
{
	pEditor->ToggleBookmark(nLine);
	RefreshBookmarks();
	LogMessage(pEditor->IsLineBookmarked(nLine)
		? tr("Added a bookmark at %1, line %2").arg(pEditor->GetDisplayName()).arg(nLine)
		: tr("Removed the bookmark at %1, line %2")
			.arg(pEditor->GetDisplayName()).arg(nLine));
}

void CMainWindow::OnNextBookmark()
{
	if (CEditorWidget* pEditor = GetCurrentEditor())
	{
		if (pEditor->NextBookmark() == 0)
		{
			statusBar()->showMessage(tr("No bookmarks in this document"), 3000);
		}
		UpdateStatusBar();
	}
}

void CMainWindow::OnPreviousBookmark()
{
	if (CEditorWidget* pEditor = GetCurrentEditor())
	{
		if (pEditor->PreviousBookmark() == 0)
		{
			statusBar()->showMessage(tr("No bookmarks in this document"), 3000);
		}
		UpdateStatusBar();
	}
}

void CMainWindow::OnClearBookmarks()
{
	// This document's, not every document's. CEditorCtrl::DeleteAllBookMark is
	// also per-document; the pane spans all of them, so the menu item says
	// which it means by living next to the other per-document commands.
	if (CEditorWidget* pEditor = GetCurrentEditor())
	{
		pEditor->ClearBookmarks();
		RefreshBookmarks();
	}
}

void CMainWindow::OnBookmarkActivated(const QString& strPath, const QString& strFile,
	int nLine)
{
	// Find the tab by path, falling back to the display name for a document
	// that has never been saved and therefore has no path to match on.
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
		if (pEditor == nullptr)
		{
			continue;
		}
		const bool bMatch = strPath.isEmpty()
			? pEditor->GetDisplayName() == strFile
			: pEditor->GetFilePath() == strPath;
		if (bMatch)
		{
			m_pTabs->setCurrentIndex(i);
			pEditor->GotoLine(nLine);
			pEditor->setFocus();
			UpdateStatusBar();
			return;
		}
	}
}

void CMainWindow::OnPreferences()
{
	CPreferencesDialog dialog(m_Data.GetSettings(), this);
	if (dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	QString strError;
	const bool bSaved = m_Data.ApplySettings(dialog.GetSettings(), strError);

	// Apply to the running editors whether or not the save succeeded: the user
	// asked for these settings, and refusing to honour them because a file
	// could not be written would be a second failure on top of the first.
	ReapplySettings();

	if (!bSaved)
	{
		LogMessage(tr("Settings not saved - %1").arg(strError), QColor(Qt::red));
		statusBar()->showMessage(tr("Settings applied but not saved"), 5000);
	}
	else
	{
		LogMessage(tr("Settings saved to %1").arg(m_Data.GetSettingsPath()));
	}
}

void CMainWindow::ReapplySettings()
{
	// BOTH calls. ApplySettings covers what is not a theme colour - the edge
	// column, the caret-line frame, autocomplete case folding, the fold flags -
	// and ApplyTheme covers the rest, including the fold marker shapes, which
	// depend on FolderMarginStyle as well as on the palette.
	for (int i = 0; i < m_pTabs->count(); ++i)
	{
		if (CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i)))
		{
			pEditor->ApplySettings();
			pEditor->ApplyTheme(m_Theme);
		}
	}
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
	// Margin clicks toggle bookmarks. Wired here rather than at each creation
	// site so a tab made by any route gets it - and through ToggleBookmarkAt,
	// which the menu command also uses, so the two cannot drift.
	connect(pEditor, &CEditorWidget::BookmarkToggleRequested, this,
		[this, pEditor](int nLine) { ToggleBookmarkAt(pEditor, nLine); });
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

namespace
{
	QColor ToQColor(const Core::SColor& c)
	{
		return QColor(c._Red, c._Green, c._Blue);
	}

	// The chrome sits slightly off the editor's own background so the panes and
	// the tab bar read as separate surfaces rather than one flat field. Which
	// direction depends on the theme: lighten a dark ground, darken a light one.
	QColor Chrome(const QColor& editorBack)
	{
		return editorBack.lightness() < 128 ? editorBack.lighter(140)
											: editorBack.darker(108);
	}
}

void CMainWindow::ApplyWindowTheme(EEditorTheme theme)
{
	// THE THEME STOPPED AT THE EDITOR. Everything else - tab bar, dock panes,
	// menus, status bar - took Qt's default palette, which on macOS follows the
	// OS appearance. So "light theme" on a Mac in Dark Mode gave a white editor
	// inside dark chrome, which reads as broken rather than as a choice. The MFC
	// has no such split: 15 files under src/ theme themselves from
	// IS_LIGHT_THEME. Reported from the UI - see doc/PORTING.md 6v.
	const Core::CEditorTheme& data = m_Data.GetTheme(theme);
	Core::SColor back;
	Core::SColor fore;
	// editorBackground is a palette key and editorTextColor is a role, exactly
	// as CEditorWidget reads them - the same two calls, so the chrome cannot
	// drift from the editor it surrounds.
	if (!data.ResolveColor("editorBackground", back)
		|| !data.ResolveRole("editorTextColor", fore))
	{
		// LEAVE THE SYSTEM PALETTE ALONE. A half-built palette from a theme file
		// missing a key would be worse than the platform default: unreadable
		// rather than merely inconsistent.
		qWarning("theme: no window palette - editorBackground or editorTextColor missing");
		return;
	}

	const QColor editorBack = ToQColor(back);
	const QColor text = ToQColor(fore);
	const QColor chrome = Chrome(editorBack);

	QPalette palette;
	palette.setColor(QPalette::Window, chrome);
	palette.setColor(QPalette::WindowText, text);
	palette.setColor(QPalette::Base, editorBack);
	palette.setColor(QPalette::AlternateBase, chrome);
	palette.setColor(QPalette::Text, text);
	palette.setColor(QPalette::Button, chrome);
	palette.setColor(QPalette::ButtonText, text);
	palette.setColor(QPalette::ToolTipBase, chrome);
	palette.setColor(QPalette::ToolTipText, text);
	// The selection colour the editor already uses - BLENDED, not raw. Scintilla
	// paints it with SCI_SETSELALPHA 60, so in the editor "black" is a pale grey
	// tint over white. A QPalette has no alpha, so handing it the raw value
	// painted a SOLID BLACK band behind selected text in the message pane, with
	// white text on it. Reported from the UI, and it was mine.
	//
	// Blending at the same weight gives the widgets the tint the editor shows,
	// and the ordinary text colour then stays readable on top of it - which the
	// raw version could not, since it had to invert the text to compensate.
	Core::SColor selection;
	if (data.ResolveRole("selectionTextColor", selection))
	{
		const int nSelAlpha = 60;			// the value passed to SCI_SETSELALPHA
		const QColor raw = ToQColor(selection);
		const auto Blend = [&](int nFore, int nBack)
		{
			return (nFore * nSelAlpha + nBack * (255 - nSelAlpha)) / 255;
		};
		palette.setColor(QPalette::Highlight, QColor(
			Blend(raw.red(), editorBack.red()),
			Blend(raw.green(), editorBack.green()),
			Blend(raw.blue(), editorBack.blue())));
		palette.setColor(QPalette::HighlightedText, text);
	}
	// Disabled text has to be derived - no theme key describes it - and a flat
	// grey would vanish on one theme or the other. Halfway to the ground it sits
	// on keeps it legible on both.
	const QColor dim = QColor::fromRgb(
		(text.red() + chrome.red()) / 2,
		(text.green() + chrome.green()) / 2,
		(text.blue() + chrome.blue()) / 2);
	palette.setColor(QPalette::Disabled, QPalette::WindowText, dim);
	palette.setColor(QPalette::Disabled, QPalette::Text, dim);
	palette.setColor(QPalette::Disabled, QPalette::ButtonText, dim);

	// On the APPLICATION, not this window: the dialogs are top-level windows of
	// their own, and a palette set here would leave Preferences and About still
	// wearing the system appearance.
	qApp->setPalette(palette);

	// AND THE TITLE BAR, which the palette cannot touch - macOS draws it and it
	// follows the OS appearance, so a light theme under Dark Mode kept a dark
	// bar on top of an otherwise light window. Reported from the UI after the
	// palette fix had already landed. Decided from the ground the theme gives
	// us rather than from which enum was passed, so a theme file whose "light"
	// is dark still gets a matching frame.
	MacAppearance::Apply(editorBack.lightness() < 128);
}

void CMainWindow::OnSetTheme(EEditorTheme theme)
{
	m_Theme = theme;
	ApplyWindowTheme(theme);
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

//////////////////////////////////////////////////////////////////////////
// Goto

void CMainWindow::OnShowGoto()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	m_pGotoBar->Activate(pEditor->GetLineCount(),
		static_cast<int>(pEditor->Send(SCI_GETLENGTH)),
		pEditor->GetCaretPosition());
}

void CMainWindow::OnHideGoto()
{
	m_pGotoBar->hide();
	if (CEditorWidget* pEditor = GetCurrentEditor())
	{
		// Focus back to the editor, which is what CGotoDlg::PreTranslateMessage
		// does with VK_ESCAPE - the one thing its Escape handler exists to do.
		pEditor->setFocus();
	}
}

void CMainWindow::OnGotoLine()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	pEditor->GotoLine(m_pGotoBar->GetLine());
	// The MFC calls SetFocus() on the editor after every GO, so the bar is not
	// somewhere you get stuck. The bar stays OPEN, though, which it also does on
	// Windows - a tab page does not close itself - and it means a second jump
	// costs one click rather than one shortcut plus one click.
	pEditor->setFocus();
	UpdateStatusBar();
}

void CMainWindow::OnGotoOffset()
{
	CEditorWidget* pEditor = GetCurrentEditor();
	if (pEditor == nullptr)
	{
		return;
	}
	pEditor->GotoPosition(m_pGotoBar->GetOffset());
	pEditor->setFocus();
	UpdateStatusBar();
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
		return;
	}

	// "2 of 7", not "7 matches". With every match highlighted, the count alone
	// does not change as you walk them, so the bar looked identical on every
	// press - which is exactly how this bug was reported.
	const CEditorWidget::SMatchPosition position =
		pEditor->LocateMatch(m_pFindBar->GetPattern(), options);
	m_pFindBar->ShowStatus(position._Ordinal > 0
		? tr("%1 of %2").arg(position._Ordinal).arg(position._Total)
		: tr("%1 matches").arg(position._Total), false);
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

	// THE STARTUP LOG, CAPTURED BEFORE ANY CHECK CAN CLEAR IT. The message-pane
	// checks below call ClearAll(), so reading the pane at the point of use
	// would find an empty one and report a missing line that was printed
	// correctly - a test failing on correct code.
	const QString strStartupLog = m_pMessagePane->GetText();

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
	int nWalkChecked = 0;
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
		// WHICH match you are on, and that it LOOKS different from the rest.
		// Positions were always right; what was wrong was that every match was
		// drawn alike, so pressing Next appeared to do nothing. Counted, and the
		// count asserted at the end, because a file with one match cannot
		// exercise any of this and would pass by not running.
		const int nMatches = pEditor->HighlightMatches(strWord, options);
		if (nMatches >= 2)
		{
			++nWalkChecked;
			pEditor->Send(SCI_GOTOPOS, 0);

			pEditor->FindNext(strWord, options);
			const sptr_t nFirst = pEditor->Send(SCI_GETSELECTIONSTART);
			CEditorWidget::SMatchPosition first = pEditor->LocateMatch(strWord, options);
			Require(first._Ordinal == 1 && first._Total == nMatches,
				QStringLiteral("%1: the first match is 1 of %2, got %3 of %4")
					.arg(strName).arg(nMatches).arg(first._Ordinal).arg(first._Total));

			// The current indicator is ON it - and that is the whole fix, so it
			// is asserted rather than assumed from the call having been made.
			Require((pEditor->Send(SCI_INDICATORALLONFOR, static_cast<uptr_t>(nFirst))
					& (1 << 16)) != 0,
				QStringLiteral("%1: the current-match indicator marks the match")
					.arg(strName));

			pEditor->FindNext(strWord, options);
			const sptr_t nSecond = pEditor->Send(SCI_GETSELECTIONSTART);
			Require(nSecond != nFirst,
				QStringLiteral("%1: Next MOVED, %2 -> %3").arg(strName)
					.arg(static_cast<qlonglong>(nFirst)).arg(static_cast<qlonglong>(nSecond)));
			CEditorWidget::SMatchPosition second = pEditor->LocateMatch(strWord, options);
			Require(second._Ordinal == 2,
				QStringLiteral("%1: and says 2 of %2, got %3")
					.arg(strName).arg(nMatches).arg(second._Ordinal));

			// AND THE OLD ONE IS RELEASED. Filling without clearing would leave
			// every visited match looking current, which is the same defect
			// wearing a different hat.
			Require((pEditor->Send(SCI_INDICATORALLONFOR, static_cast<uptr_t>(nFirst))
					& (1 << 16)) == 0,
				QStringLiteral("%1: the previous match is no longer marked current")
					.arg(strName));
			pEditor->ClearHighlight();
			Require((pEditor->Send(SCI_INDICATORALLONFOR, static_cast<uptr_t>(nSecond))
					& (1 << 16)) == 0,
				QStringLiteral("%1: ClearHighlight clears the current marker too")
					.arg(strName));

			// A REPLACE LEAVES NO CURRENT MATCH, in the one state where it
			// otherwise would: the caret moved after the Find, so the replace
			// lands somewhere else and the mark is not deleted along with the
			// text it was on. Found in review; measured at marker 85 /
			// selection 158 before the fix.
			pEditor->Send(SCI_GOTOPOS, 0);
			pEditor->HighlightMatches(strWord, options);
			pEditor->FindNext(strWord, options);
			const sptr_t nMarked = pEditor->Send(SCI_GETSELECTIONSTART);
			pEditor->Send(SCI_GOTOPOS, nMarked + strWord.size());
			Require(pEditor->ReplaceNext(strWord, strWord + QStringLiteral("_x"), options),
				QStringLiteral("%1: the replace ran").arg(strName));
			Require((pEditor->Send(SCI_INDICATORALLONFOR, static_cast<uptr_t>(nMarked))
					& (1 << 16)) == 0,
				QStringLiteral("%1: a replace elsewhere drops the old current mark")
					.arg(strName));
			// The document is shared with every later check on this tab, so put
			// it back rather than leaving a "_x" behind.
			pEditor->Send(SCI_UNDO);
			pEditor->Send(SCI_SETSAVEPOINT);
			pEditor->ClearHighlight();
		}

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
				// x is computed from the ACTUAL margin widths, not from
				// "margin 0 plus a bit". The original assumed margin 1 had
				// width 0 and so landed in the fold margin by luck; the moment
				// bookmarks gave margin 1 a width, the click started landing in
				// the SYMBOL margin and this check failed - correctly. It was
				// the only thing that noticed the layout had moved.
				const int nMargin0 = static_cast<int>(pEditor->Send(SCI_GETMARGINWIDTHN, 0));
				const int nMargin1 = static_cast<int>(pEditor->Send(SCI_GETMARGINWIDTHN, 1));
				const int nMargin2 = static_cast<int>(pEditor->Send(SCI_GETMARGINWIDTHN, 2));
				Require(nMargin2 > 0,
					QStringLiteral("%1: the fold margin has width, so a click can reach it")
						.arg(strName));
				const int nX = nMargin0 + nMargin1 + (nMargin2 / 2);
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
			const QString strPath = ResourcePaths::LicenseDir()
				+ QLatin1Char('/') + strFile;
			Require(QFile::exists(strPath),
				QStringLiteral("attribution: %1 exists").arg(strPath));
		}
	}

	//----------------------------------------------------------------------
	// The application icon. A .qrc path that does not resolve gives a null QIcon
	// SILENTLY - QIcon has no way to complain - so the app would simply wear the
	// generic binary icon again and nothing would say why. See doc/PORTING.md 6w.
	//----------------------------------------------------------------------
	{
		const QIcon icon = QApplication::windowIcon();
		Require(!icon.isNull(), QStringLiteral("icon: the window icon is set"));
		// AND HAS PIXELS. A QIcon built from a path Qt cannot decode is non-null
		// but empty, which is the failure this is actually guarding against -
		// the .ico plugin missing from a deployed build looks exactly like that.
		Require(!icon.availableSizes().isEmpty(),
			QStringLiteral("icon: and it decoded to at least one size"));
		const QPixmap pixmap = icon.pixmap(64, 64);
		Require(!pixmap.isNull() && pixmap.width() > 0,
			QStringLiteral("icon: it renders at 64px, got %1x%2")
				.arg(pixmap.width()).arg(pixmap.height()));

		// A LARGE SIZE, because the Dock and the app switcher ask for one and a
		// 32px source scaled up to 128 looks worse than no icon at all. The .ico
		// carries 16 through 256; this fails if somebody ships a trimmed one.
		int nLargest = 0;
		for (const QSize& size : icon.availableSizes())
		{
			nLargest = qMax(nLargest, size.width());
		}
		Require(nLargest >= 128,
			QStringLiteral("icon: it carries a large size for the Dock, largest is %1")
				.arg(nLargest));
	}

	//----------------------------------------------------------------------
	// The theme reaches the WINDOW, not just the editor. Reported from the UI:
	// light theme on a Mac in Dark Mode gave a white editor inside dark chrome.
	// See doc/PORTING.md 6v.
	//----------------------------------------------------------------------
	{
		auto WindowOf = [this](EEditorTheme theme)
		{
			OnSetTheme(theme);
			return qApp->palette();
		};
		const QPalette dark = WindowOf(EEditorTheme::Dark);
		const QColor darkWindow = dark.color(QPalette::Window);
		const QColor darkText = dark.color(QPalette::WindowText);
		const QPalette light = WindowOf(EEditorTheme::Light);
		const QColor lightWindow = light.color(QPalette::Window);

		Require(darkWindow != lightWindow,
			QStringLiteral("theme: the WINDOW palette changes with the theme (%1 vs %2)")
				.arg(darkWindow.name(), lightWindow.name()));
		Require(lightWindow.lightness() > darkWindow.lightness(),
			QStringLiteral("theme: and the light one is the lighter (%1 vs %2)")
				.arg(lightWindow.lightness()).arg(darkWindow.lightness()));

		// LEGIBILITY, not merely difference. A palette that applied cleanly and
		// painted text the colour of its own background would pass every check
		// above while being unusable - which is the failure this whole change
		// exists to fix, in a new disguise.
		for (const auto& entry : { std::make_pair(dark, "dark"), std::make_pair(light, "light") })
		{
			const int nGap = qAbs(entry.first.color(QPalette::WindowText).lightness()
				- entry.first.color(QPalette::Window).lightness());
			Require(nGap > 80,
				QStringLiteral("theme: %1 text stands off its background by %2")
					.arg(QLatin1String(entry.second)).arg(nGap));
		}

		// THE TITLE BAR'S INPUT. MacAppearance::Apply is a native call with no
		// Qt-visible effect, so the self-test cannot see the bar it paints -
		// only a human on a Mac can. What IS checkable is the decision it is
		// given: the dark theme's ground must be dark and the light theme's
		// light. A theme file edited the other way would hand the frame the
		// wrong answer, and this is the check that would say so.
		Require(dark.color(QPalette::Base).lightness() < 128,
			QStringLiteral("theme: the dark ground IS dark (%1), so the title bar follows")
				.arg(dark.color(QPalette::Base).lightness()));
		Require(light.color(QPalette::Base).lightness() >= 128,
			QStringLiteral("theme: and the light ground is light (%1)")
				.arg(light.color(QPalette::Base).lightness()));

		// THE SELECTION BAND IS A TINT, not the raw colour. The light theme's
		// selectionTextColor is literally "black" and Scintilla paints it at
		// alpha 60, so handing the raw value to a QPalette - which has no alpha -
		// put a solid black band behind selected text in the message pane.
		// Reported from the UI. The band must therefore stay on its own side of
		// the midpoint: light on a light theme, dark on a dark one.
		Require(light.color(QPalette::Highlight).lightness() > 128,
			QStringLiteral("theme: the light selection band is a light tint (%1)")
				.arg(light.color(QPalette::Highlight).lightness()));
		Require(dark.color(QPalette::Highlight).lightness() < 128,
			QStringLiteral("theme: the dark selection band is a dark tint (%1)")
				.arg(dark.color(QPalette::Highlight).lightness()));
		for (const auto& entry : { std::make_pair(dark, "dark"), std::make_pair(light, "light") })
		{
			const int nGap = qAbs(entry.first.color(QPalette::HighlightedText).lightness()
				- entry.first.color(QPalette::Highlight).lightness());
			Require(nGap > 60,
				QStringLiteral("theme: %1 selected text stands off its band by %2")
					.arg(QLatin1String(entry.second)).arg(nGap));
		}

		// The chrome is NOT the editor's own ground, so the panes and the tab bar
		// read as separate surfaces rather than one flat field.
		Require(dark.color(QPalette::Window) != dark.color(QPalette::Base),
			QStringLiteral("theme: the chrome sits off the editor background"));
		OnSetTheme(EEditorTheme::Dark);
		(void)darkText;
	}

	//----------------------------------------------------------------------
	// Where the app finds its files. This is what makes a copied build work,
	// so it is checked rather than assumed - see doc/PORTING.md 6u.
	//----------------------------------------------------------------------
	{
		const QStringList candidates = ResourcePaths::Candidates(QStringLiteral("data"));
		Require(candidates.size() >= 4,
			QStringLiteral("paths: %1 candidates for the data directory")
				.arg(candidates.size()));
		// THE ORDER IS THE CONTRACT. A bundle's own Resources must beat
		// everything, and the builder's source tree must lose to everything -
		// otherwise a packaged copy on a machine that happens to have the source
		// tree reads the wrong one, and works for exactly the wrong reason.
		Require(candidates.first().contains(QStringLiteral("/../Resources/")),
			QStringLiteral("paths: the bundle's Resources is tried FIRST, got '%1'")
				.arg(candidates.first()));
		Require(candidates.last() == QStringLiteral(VINATEXT_DATA_DIR),
			QStringLiteral("paths: the compiled-in build path is tried LAST, got '%1'")
				.arg(candidates.last()));

		Require(QFile::exists(ResourcePaths::DataDir() + QStringLiteral("/languages.json")),
			QStringLiteral("paths: the resolved data dir holds languages.json (%1)")
				.arg(ResourcePaths::DataDir()));

		// AND THE APP SAYS WHERE IT LOADED FROM. Without this the only way to
		// tell a packaged copy reading its own data from one silently falling
		// back to a build tree was to plant a fake theme colour and look - which
		// is how the first manual test of this actually went. The pane reports
		// what Load USED, not what the resolver would answer now, because --data
		// overrides the search.
		// NOT EMPTY FIRST. Without this the check passes on an empty string -
		// "Data: " + "" is a prefix of the line whatever the line says - and it
		// did: the accessor was added but never assigned, so the pane printed a
		// bare "Data: " and this check could not fail. Found by mutating the
		// value away and watching nothing happen.
		Require(!m_Data.GetDataDir().isEmpty(),
			QStringLiteral("paths: the loaded data dir is recorded, not empty"));
		Require(strStartupLog.contains(QStringLiteral("Data: ")
				+ ResourcePaths::ForDisplay(m_Data.GetDataDir())),
			QStringLiteral("paths: the message pane names the data dir it loaded (%1)")
				.arg(m_Data.GetDataDir()));
		Require(strStartupLog.contains(QStringLiteral("Licences: ")
				+ ResourcePaths::ForDisplay(ResourcePaths::LicenseDir())),
			QStringLiteral("paths: and the licence dir"));

		// AND WHICH CANDIDATE WON, which is the question the path alone makes
		// you answer yourself. A build-tree run says "build tree"; a bundle says
		// "bundle". Getting this label wrong would be worse than omitting it -
		// it would state the opposite of the truth - so it is checked against
		// the resolution rather than assumed from it.
		Require(strStartupLog.contains(QStringLiteral("(build tree)")),
			QStringLiteral("paths: a build-tree run says so, log was '%1'")
				.arg(strStartupLog.simplified().left(200)));
		Require(ResourcePaths::DescribeSource(
				ResourcePaths::Candidates(QStringLiteral("data")).first(),
				QStringLiteral("data")) == QStringLiteral("bundle"),
			QStringLiteral("paths: the first candidate is labelled 'bundle'"));
		Require(ResourcePaths::DescribeSource(QStringLiteral("/somewhere/else"),
				QStringLiteral("data")) == QStringLiteral("--data"),
			QStringLiteral("paths: anything off the list is labelled '--data'"));

		// ~ is display only. A tilde handed to QFile opens nothing, so the two
		// forms must not be confused - the log shows one and the resolver
		// returns the other.
		Require(!ResourcePaths::ForDisplay(m_Data.GetDataDir()).startsWith(QLatin1Char('/'))
				|| !m_Data.GetDataDir().startsWith(QDir::homePath()),
			QStringLiteral("paths: a path under HOME is displayed with ~"));
		Require(QFile::exists(ResourcePaths::LicenseDir()
				+ QStringLiteral("/License-VinaText.txt")),
			QStringLiteral("paths: the resolved licence dir holds the licences (%1)")
				.arg(ResourcePaths::LicenseDir()));

		// EXISTING IS NOT THE SAME AS USABLE, tested on a scratch directory
		// rather than next to the binary. The first version of this check wrote
		// a decoy into the executable's own directory, and review was right that
		// an install nobody can write to would fail it. It was worse than that:
		// in the packaged layout this PR is building towards, <exe>/data IS the
		// data directory, so mkpath succeeded trivially, the check asserted
		// nothing, and the rmdir afterwards was aimed at the app's own data.
		// Measured - the staged copy passed this check while testing none of it.
		QTemporaryDir scratch;
		Require(scratch.isValid(), QStringLiteral("paths: a scratch directory"));
		Require(!ResourcePaths::HoldsResources(scratch.path(), QStringLiteral("data")),
			QStringLiteral("paths: an EMPTY directory does not count as the data dir"));
		// Pinned as a contract, not as a separate mechanism: it holds because
		// the witness cannot exist inside a directory that does not, which is
		// why the resolver has no exists() guard of its own.
		Require(!ResourcePaths::HoldsResources(
				QStringLiteral("/no/such/directory/anywhere"), QStringLiteral("data")),
			QStringLiteral("paths: a directory that does not exist does not count"));
		{
			QFile witness(scratch.path() + QStringLiteral("/languages.json"));
			Require(witness.open(QIODevice::WriteOnly), QStringLiteral("paths: wrote a witness"));
			witness.close();
		}
		Require(ResourcePaths::HoldsResources(scratch.path(), QStringLiteral("data")),
			QStringLiteral("paths: the SAME directory counts once the witness is in it"));
		// And the rule is per-leaf: the data witness must not satisfy licences.
		Require(!ResourcePaths::HoldsResources(scratch.path(), QStringLiteral("license")),
			QStringLiteral("paths: languages.json does not make it a licence directory"));
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
	// Goto - src/GotoDlg.cpp, which is a tab page and not a dialog. See
	// doc/PORTING.md 6l and ui-qt/GotoBar.h.
	//----------------------------------------------------------------------
	{
		CEditorWidget* pScratch = NewUntitled();
		Require(pScratch != nullptr, QStringLiteral("goto: got a scratch document"));
		if (pScratch != nullptr)
		{
			// 400 numbered lines. The length is the point: every centring check
			// below is vacuous on a document that fits on screen, because then
			// nothing can scroll and every first-visible-line reads 0.
			QByteArray text;
			for (int i = 1; i <= 400; ++i)
			{
				text += QByteArray("line ") + QByteArray::number(i) + "\n";
			}
			pScratch->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.constData()));
			pScratch->Send(SCI_EMPTYUNDOBUFFER);

			auto FirstVisibleOf = [](CEditorWidget* pEditor)
			{
				return static_cast<int>(pEditor->Send(SCI_GETFIRSTVISIBLELINE));
			};
			auto FirstVisible = [pScratch, &FirstVisibleOf]
			{
				return FirstVisibleOf(pScratch);
			};

			const int nOnScreen = static_cast<int>(pScratch->Send(SCI_LINESONSCREEN));
			Require(nOnScreen > 4 && nOnScreen < 400,
				QStringLiteral("goto: %1 of 400 lines fit on screen, so scrolling is "
					"observable").arg(nOnScreen));
			Require(pScratch->GetLineCount() == 401,
				QStringLiteral("goto: 400 lines plus the empty one after the last newline, "
					"got %1").arg(pScratch->GetLineCount()));

			// 1-based, and exact.
			pScratch->GotoLine(200);
			Require(pScratch->GetCaretLine() == 200,
				QStringLiteral("goto: GotoLine(200) puts the caret on line 200, got %1")
					.arg(pScratch->GetCaretLine()));

			// And it centres, by the ORIGINAL's arithmetic - which is one line
			// out, because CEditorCtrl::GotoLine passes the 1-based
			// GetCurrentLine() to SetLineCenterDisplay, which indexes document
			// lines from 0. Exact centring of line 200 would be 199 - (n-2)/2.
			// This check encodes the off-by-one ON PURPOSE, so that "fixing" it
			// fails here and the person doing it reads 6l before deciding the two
			// frontends should scroll differently.
			const int nCentred200 = 200 - ((nOnScreen - 2) / 2);
			Require(FirstVisible() == nCentred200,
				QStringLiteral("goto: GotoLine centres the target line (first visible %1, "
					"expected %2)").arg(FirstVisible()).arg(nCentred200));

			// THE ASYMMETRY. GotoPosition does not centre and GotoLine does, and
			// asserting it relationally rather than against a literal keeps it
			// true whatever the offscreen viewport turns out to be.
			const int nLine235 = static_cast<int>(pScratch->Send(SCI_POSITIONFROMLINE, 234));
			pScratch->Send(SCI_SETFIRSTVISIBLELINE, 0);
			pScratch->GotoPosition(nLine235);
			Require(pScratch->GetCaretPosition() == nLine235,
				QStringLiteral("goto: GotoPosition lands on the offset asked for"));
			const int nAfterOffset = FirstVisible();
			pScratch->Send(SCI_SETFIRSTVISIBLELINE, 0);
			pScratch->GotoLine(235);
			Require(FirstVisible() > nAfterOffset,
				QStringLiteral("goto: GotoLine centres where GotoPosition only scrolls into "
					"view (first visible %1 vs %2)").arg(FirstVisible()).arg(nAfterOffset));

			// An EMPTY line box goes to the top of the document rather than doing
			// nothing, because "" is 0 and the guard is < 0. This is the check
			// that fails if the guard is tightened to < 1 to look tidier.
			Require(pScratch->GetCaretLine() == 235,
				QStringLiteral("goto: the caret is away from the top before the empty-box "
					"check, so that check can fail"));
			pScratch->GotoLine(m_pGotoBar->GetLine());		// the bar's box is empty
			Require(pScratch->GetCaretLine() == 1,
				QStringLiteral("goto: an empty line box goes to the top, got line %1")
					.arg(pScratch->GetCaretLine()));

			// A negative is refused. The digits-only validator means the BAR
			// cannot produce one, so this covers the editor API rather than the
			// widget - but it is still mutation-sensitive: without the guard,
			// SCI_GOTOLINE(-6) clamps to the first line and the caret moves.
			pScratch->GotoLine(100);
			pScratch->GotoLine(-5);
			Require(pScratch->GetCaretLine() == 100,
				QStringLiteral("goto: a negative line number is refused, got line %1")
					.arg(pScratch->GetCaretLine()));

			// Scroll to Caret moves the VIEW and not the caret. Both halves
			// matter: a no-op would pass the first on its own.
			pScratch->GotoLine(300);
			const int nCaretBefore = pScratch->GetCaretPosition();
			pScratch->Send(SCI_SETFIRSTVISIBLELINE, 0);
			pScratch->ScrollToCaret();
			Require(pScratch->GetCaretPosition() == nCaretBefore,
				QStringLiteral("goto: Scroll to Caret leaves the caret alone"));
			Require(FirstVisible() == 300 - ((nOnScreen - 2) / 2),
				QStringLiteral("goto: Scroll to Caret brings the caret back into view "
					"(first visible %1)").arg(FirstVisible()));

			// WITH LINES HIDDEN, which is the only state where
			// SetFirstVisibleLine's document-to-visible mapping is not the
			// identity function. Everything above runs on a fully expanded
			// document, where visible line n IS document line n - so deleting
			// SCI_VISIBLEFROMDOCLINE changed no result and the mutation went
			// UNCAUGHT. The checks were measuring an identity and could not have
			// said otherwise.
			//
			// Folding, not wrapping. Word wrap was the first attempt and it is
			// the wrong tool: SCI_VISIBLEFROMDOCLINE counts lines HIDDEN BY
			// FOLDS, and wrap rows are display rows it does not touch, so the
			// wrapped version asserted 182 != 182 and failed its own guard.
			//
			// Scroll to Caret is the one public path that can meet hidden lines
			// at all, because both goto paths expand folds before they scroll.
			// A user who has folded a file, scrolled away, and pressed Scroll to
			// Caret is doing nothing unusual.
			{
				CEditorWidget* pFolded = qobject_cast<CEditorWidget*>(m_pTabs->widget(0));
				if (pFolded != nullptr && pFolded != pScratch)
				{
					pFolded->Send(SCI_COLOURISE, 0, -1);

					// ONE fold, not SCI_FOLDALL. Contracting everything collapses
					// this 521-line file to 12 visible lines - fewer than fit on
					// screen - so nothing can scroll, first-visible is pinned at 0
					// and the mapping is unobservable. Measured, after the
					// all-folds version failed its own guard.
					const int nHalf = (nOnScreen - 2) / 2;
					const int nDocLines = pFolded->GetLineCount();
					for (int line = 0; line < nDocLines; ++line)
					{
						if ((pFolded->Send(SCI_GETFOLDLEVEL, static_cast<uptr_t>(line))
							& SC_FOLDLEVELHEADERFLAG) == 0)
						{
							continue;
						}
						// Big enough that the two line spaces diverge by a useful
						// margin; small enough that most of the file stays
						// scrollable.
						if (pFolded->Send(SCI_GETLASTCHILD, static_cast<uptr_t>(line), -1)
							- line < 5)
						{
							continue;
						}
						pFolded->Send(SCI_TOGGLEFOLD, static_cast<uptr_t>(line));
						break;
					}
					Require(pFolded->Send(SCI_GETALLLINESVISIBLE) == 0,
						QStringLiteral("goto: one block is folded, so lines are hidden"));

					const int nVisibleTotal = static_cast<int>(pFolded->Send(
						SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(nDocLines)));

					// A caret line deep enough to scroll, visible, whose centred
					// start sits behind hidden lines, and near enough the top of
					// the folded document that Scintilla will not clamp the
					// scroll - a clamped result would compare equal for the wrong
					// reason.
					int nCaretLine = -1;
					int nMapped = -1;
					int nRaw = -1;
					for (int line = nDocLines - 1; line >= 1 && nCaretLine < 0; --line)
					{
						if (pFolded->Send(SCI_GETLINEVISIBLE, static_cast<uptr_t>(line)) == 0)
						{
							continue;
						}
						const int nStart = line - nHalf;
						if (nStart < 1)
						{
							continue;
						}
						const int nAt = static_cast<int>(pFolded->Send(SCI_VISIBLEFROMDOCLINE,
							static_cast<uptr_t>(nStart)));
						if (nAt != nStart && nAt + nOnScreen <= nVisibleTotal)
						{
							nCaretLine = line;
							nMapped = nAt;
							nRaw = nStart;
						}
					}
					Require(nCaretLine > 0,
						QStringLiteral("goto: found a folded line where the visible and "
							"document line spaces differ, so the mapping check can fail"));
					if (nCaretLine > 0)
					{
						pFolded->Send(SCI_GOTOLINE, static_cast<uptr_t>(nCaretLine - 1));
						pFolded->Send(SCI_SETFIRSTVISIBLELINE, 0);
						pFolded->ScrollToCaret();
						Require(FirstVisibleOf(pFolded) == nMapped,
							QStringLiteral("goto: scrolling counts VISIBLE lines, not document "
								"lines (first visible %1, expected %2, unmapped would be %3)")
								.arg(FirstVisibleOf(pFolded)).arg(nMapped).arg(nRaw));
					}
					pFolded->Send(SCI_FOLDALL, SC_FOLDACTION_EXPAND);
					pFolded->Send(SCI_GOTOPOS, 0);
					m_pTabs->setCurrentIndex(m_pTabs->indexOf(pScratch));
				}
			}

			// Folds. Both goto paths expand first, and this is the check that
			// says so: collapse a document, confirm something is genuinely
			// hidden, then jump into it.
			//
			// On a REAL tab, not the scratch one. An untitled document has no
			// language and so no lexer, and without a lexer there are no fold
			// levels to contract - the first version of this check ran on the
			// scratch document, folded nothing, and was caught by its own guard
			// rather than by anything downstream. That guard is why it is
			// written as an else-Require and not an if.
			int nGotoFoldChecked = 0;
			for (int i = 0; i < GetTabCount() && nGotoFoldChecked == 0; ++i)
			{
				CEditorWidget* pFoldable = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
				if (pFoldable == nullptr || pFoldable == pScratch)
				{
					continue;
				}
				pFoldable->Send(SCI_COLOURISE, 0, -1);
				pFoldable->Send(SCI_FOLDALL, SC_FOLDACTION_CONTRACT);
				if (pFoldable->Send(SCI_GETALLLINESVISIBLE) != 0)
				{
					continue;			// nothing in this file folds
				}
				// The first line the fold actually hid.
				sptr_t nHidden = -1;
				const sptr_t nLines = pFoldable->Send(SCI_GETLINECOUNT);
				for (sptr_t line = 0; line < nLines && nHidden < 0; ++line)
				{
					if (pFoldable->Send(SCI_GETLINEVISIBLE, static_cast<uptr_t>(line)) == 0)
					{
						nHidden = line;
					}
				}
				Require(nHidden >= 0, QStringLiteral("goto: found a hidden line to jump to"));
				if (nHidden >= 0)
				{
					pFoldable->GotoLine(static_cast<int>(nHidden) + 1);		// 1-based
					Require(pFoldable->Send(SCI_GETLINEVISIBLE,
							static_cast<uptr_t>(nHidden)) != 0,
						QStringLiteral("goto: jumping into a collapsed fold expands it"));
					Require(pFoldable->GetCaretLine() == static_cast<int>(nHidden) + 1,
						QStringLiteral("goto: and the caret arrives on the line asked for, "
							"got %1").arg(pFoldable->GetCaretLine()));
					++nGotoFoldChecked;
				}
				pFoldable->Send(SCI_FOLDALL, SC_FOLDACTION_EXPAND);
				pFoldable->Send(SCI_GOTOPOS, 0);
			}
			Require(nGotoFoldChecked == 1,
				QStringLiteral("goto: the expand-on-jump check ran on a document that "
					"actually folds"));
			m_pTabs->setCurrentIndex(m_pTabs->indexOf(pScratch));

			// Paragraphs, in opposite directions - which is what fails if the two
			// Scintilla messages are swapped.
			pScratch->Send(SCI_SETTEXT, 0,
				reinterpret_cast<sptr_t>("alpha\n\nbeta\n\ngamma\n"));
			pScratch->Send(SCI_GOTOPOS, 0);
			pScratch->GotoNextParagraph();
			const int nAfterDown = pScratch->GetCaretPosition();
			Require(nAfterDown > 0,
				QStringLiteral("goto: next paragraph moves forward, to %1").arg(nAfterDown));
			pScratch->GotoPreviousParagraph();
			Require(pScratch->GetCaretPosition() < nAfterDown,
				QStringLiteral("goto: previous paragraph moves back, to %1")
					.arg(pScratch->GetCaretPosition()));

			//--------------------------------------------------------------
			// The bar
			//--------------------------------------------------------------
			pScratch->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.constData()));
			pScratch->Send(SCI_GOTOPOS, 77);
			OnShowGoto();
			Require(!m_pGotoBar->isHidden(), QStringLiteral("goto: the bar opens"));
			Require(m_pGotoBar->GetLineRangeText().contains(QStringLiteral("401")),
				QStringLiteral("goto: the readout carries this document's line count, got '%1'")
					.arg(m_pGotoBar->GetLineRangeText()));
			// The offset box opens on where the caret already is, which is what
			// makes it a readout as well as an input.
			Require(m_pGotoBar->GetOffset() == 77,
				QStringLiteral("goto: the offset box opens on the caret position, got %1")
					.arg(m_pGotoBar->GetOffset()));

			// Everything document-derived follows the TAB, not just the first
			// document the bar saw.
			const int nScratchTab = m_pTabs->indexOf(pScratch);
			CEditorWidget* pOther = qobject_cast<CEditorWidget*>(m_pTabs->widget(0));
			if (pOther != nullptr && nScratchTab != 0)
			{
				// Both asserted, because two documents with the same length - or
				// the same caret position - would let a bar that never
				// recalculates pass the checks below.
				pOther->Send(SCI_GOTOPOS, 13);
				Require(pOther->GetLineCount() != pScratch->GetLineCount(),
					QStringLiteral("goto: the two tabs differ in length (%1 vs %2), so the "
						"readout check can fail").arg(pOther->GetLineCount())
						.arg(pScratch->GetLineCount()));
				Require(pOther->GetCaretPosition() != pScratch->GetCaretPosition(),
					QStringLiteral("goto: the two tabs differ in caret position (%1 vs %2), "
						"so the offset check can fail").arg(pOther->GetCaretPosition())
						.arg(pScratch->GetCaretPosition()));

				m_pTabs->setCurrentIndex(0);
				Require(m_pGotoBar->GetLineRangeText().contains(
						QString::number(pOther->GetLineCount())),
					QStringLiteral("goto: switching tabs updates the readout to %1, got '%2'")
						.arg(pOther->GetLineCount()).arg(m_pGotoBar->GetLineRangeText()));
				// The offset field too, not only the labels. It shipped showing
				// the PREVIOUS document's caret offset - a number with no
				// meaning in the document now in front of the user. Found in
				// review.
				Require(m_pGotoBar->GetOffset() == pOther->GetCaretPosition(),
					QStringLiteral("goto: switching tabs refreshes the offset box to %1, "
						"got %2").arg(pOther->GetCaretPosition())
						.arg(m_pGotoBar->GetOffset()));
				m_pTabs->setCurrentIndex(nScratchTab);
			}

			//--------------------------------------------------------------
			// What a box's text means, including the numbers int cannot hold
			//--------------------------------------------------------------
			Require(CGotoBar::ParseTarget(QString()) == 0,
				QStringLiteral("goto: an empty box is 0, which is the top"));
			Require(CGotoBar::ParseTarget(QStringLiteral("42")) == 42,
				QStringLiteral("goto: a number is itself"));
			// QString::toInt OVERFLOWS TO ZERO, so without ParseTarget these two
			// would be indistinguishable from an empty box and would jump to the
			// TOP - the opposite end from the one asked for. Found in review.
			const int nMax = std::numeric_limits<int>::max();
			Require(CGotoBar::ParseTarget(QStringLiteral("99999999999")) == nMax,
				QStringLiteral("goto: a number too big for an int means the end, not 0"));
			Require(CGotoBar::ParseTarget(QStringLiteral("2147483648")) == nMax,
				QStringLiteral("goto: and that starts exactly one past INT_MAX"));

			// End to end, which is the half that matters: an overflowing line
			// number must land where a merely-large one lands.
			pScratch->GotoLine(CGotoBar::ParseTarget(QStringLiteral("999999")));
			const int nLargeLine = pScratch->GetCaretLine();
			pScratch->Send(SCI_GOTOPOS, 0);
			pScratch->GotoLine(CGotoBar::ParseTarget(QStringLiteral("99999999999")));
			Require(pScratch->GetCaretLine() == nLargeLine,
				QStringLiteral("goto: an overflowing line number lands where a large one "
					"does (line %1, expected %2)")
					.arg(pScratch->GetCaretLine()).arg(nLargeLine));
			Require(nLargeLine > 1,
				QStringLiteral("goto: and that is not line 1, so the check can fail"));

			// End to end through the menu action, as the Replace regression
			// taught: the shortcut being right is only half of it.
			QAction* pGotoAction = nullptr;
			for (QAction* pAction : menuBar()->findChildren<QAction*>())
			{
				QString strPlain = pAction->text();
				strPlain.remove(QLatin1Char('&'));
				if (strPlain.startsWith(QStringLiteral("Go to Line")))
				{
					pGotoAction = pAction;
				}
			}
			Require(pGotoAction != nullptr,
				QStringLiteral("goto: the Search menu carries a Go to Line action"));
			if (pGotoAction != nullptr)
			{
				Require(!pGotoAction->shortcut().isEmpty(),
					QStringLiteral("goto: Go to Line has a shortcut"));
				m_pGotoBar->hide();
				pGotoAction->trigger();
				Require(!m_pGotoBar->isHidden(),
					QStringLiteral("goto: the menu action opens the bar"));
			}

			// And the close path hides it, which is the bar's only way out
			// besides Escape - and the one that needs no keyboard.
			OnHideGoto();
			Require(m_pGotoBar->isHidden(), QStringLiteral("goto: the bar closes"));

			pScratch->Send(SCI_SETSAVEPOINT);
			OnCloseTab(m_pTabs->indexOf(pScratch));
		}
	}

	//----------------------------------------------------------------------
	// Encoding - src/CodePageMFCDlg.cpp. See doc/PORTING.md 6m.
	//
	// The point of every check here is that REINTERPRET and CONVERT are
	// different operations. Confusing them writes a file in an encoding the
	// user did not ask for, silently, which is the failure mode this port
	// spends its round-trip checks on.
	//----------------------------------------------------------------------
	{
		const QStringList encodings = CEditorWidget::AvailableEncodings();
		Require(encodings.size() > 100,
			QStringLiteral("encoding: the picker has %1 encodings to offer")
				.arg(encodings.size()));
		Require(encodings.contains(QStringLiteral("UTF-8")),
			QStringLiteral("encoding: UTF-8 is offered"));
		// The reason the compatibility module is used at all: QStringConverter
		// has no Vietnamese codepage, and this is a Vietnamese editor.
		Require(encodings.contains(QStringLiteral("windows-1258")),
			QStringLiteral("encoding: windows-1258 is offered"));
		Require(encodings.size() == QSet<QString>(encodings.begin(), encodings.end()).size(),
			QStringLiteral("encoding: the list has no duplicates"));

		// QFile::open is [[nodiscard]], and a read that silently failed would
		// make every byte comparison below compare two empty arrays and pass.
		auto ReadAll = [](const QString& strFile)
		{
			QFile f(strFile);
			if (!f.open(QIODevice::ReadOnly))
			{
				return QByteArray();
			}
			return f.readAll();
		};

		//--------------------------------------------------------------
		// EVERY encoding offered must actually work, and must not lie
		// about its own name.
		//
		// The checks below this used to exercise three encodings by hand -
		// UTF-8, UTF-16LE, windows-1258 - out of the 805 the picker offers and
		// the six the menus offer. So a name that resolved to nothing, and a
		// label that reported the wrong encoding, both shipped. Offering an
		// encoding in a list is a promise that choosing it works; this is that
		// promise, checked.
		//--------------------------------------------------------------
		{
			CEditorWidget* pProbe = NewUntitled();
			Require(pProbe != nullptr, QStringLiteral("encoding: got a probe document"));
			if (pProbe != nullptr)
			{
				int nRejected = 0;
				int nMislabelled = 0;
				QString strFirstRejected;
				QString strFirstMislabelled;
				for (const QString& strName : encodings)
				{
					if (!pProbe->SetSaveEncoding(strName))
					{
						++nRejected;
						if (strFirstRejected.isEmpty()) { strFirstRejected = strName; }
						continue;
					}
					// A label reading "UTF-8" for something that is not UTF-8
					// names a different encoding from the one about to be
					// written, which is the whole failure mode this feature
					// has to avoid.
					//
					// Compared against the RESOLVED encoding, not against the
					// name asked for. The first version compared the requested
					// name and flagged 13 encodings that are simply ALIASES of
					// UTF-8 - ibm-1208, utf8, UTF8 and friends - which are
					// labelled "UTF-8" entirely correctly. A check that cries
					// wolf on correct behaviour gets deleted, not obeyed.
					const QString strLabel = pProbe->GetEncodingLabel();
					if (strLabel == QStringLiteral("UTF-8")
						&& pProbe->GetEncodingName() != QStringLiteral("UTF-8"))
					{
						++nMislabelled;
						if (strFirstMislabelled.isEmpty()) { strFirstMislabelled = strName; }
					}
				}
				Require(nRejected == 0,
					QStringLiteral("encoding: every offered encoding is accepted; %1 were "
						"not, first '%2'").arg(nRejected).arg(strFirstRejected));
				Require(nMislabelled == 0,
					QStringLiteral("encoding: no encoding is labelled UTF-8 when it is not; "
						"%1 were, first '%2'").arg(nMislabelled).arg(strFirstMislabelled));

				pProbe->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pProbe));
			}
		}

		QTemporaryDir scratch;
		Require(scratch.isValid(), QStringLiteral("encoding: got a scratch directory"));
		if (scratch.isValid())
		{
			const QString strPath = scratch.filePath(QStringLiteral("enc.txt"));
			const QString strText = QStringLiteral("cafeé naïve\n");

			// A UTF-8 file to start from.
			{
				QFile seed(strPath);
				Require(seed.open(QIODevice::WriteOnly),
					QStringLiteral("encoding: wrote the seed file"));
				const QByteArray seedBytes = strText.toUtf8();
				Require(seed.write(seedBytes) == seedBytes.size(),
					QStringLiteral("encoding: the whole seed file was written"));
				seed.close();
			}
			Require(OpenFile(strPath), QStringLiteral("encoding: opened the seed file"));
			CEditorWidget* pEnc = GetCurrentEditor();
			Require(pEnc != nullptr, QStringLiteral("encoding: the seed file is current"));
			if (pEnc != nullptr)
			{
				const QByteArray utf8Bytes = ReadAll(strPath);
				Require(!utf8Bytes.isEmpty(),
					QStringLiteral("encoding: the seed file has bytes to compare against"));

				// A name BOTH libraries know must resolve to the BUILTIN path,
				// so the encodings with proven round-trips keep them.
				Require(pEnc->SetSaveEncoding(QStringLiteral("UTF-8")),
					QStringLiteral("encoding: UTF-8 is accepted"));
				Require(pEnc->GetEncodingName() == QStringLiteral("UTF-8"),
					QStringLiteral("encoding: and is reported as UTF-8, got '%1'")
						.arg(pEnc->GetEncodingName()));

				// An unknown name is refused and changes NOTHING.
				const QString strBefore = pEnc->GetEncodingName();
				Require(!pEnc->SetSaveEncoding(QStringLiteral("not-an-encoding")),
					QStringLiteral("encoding: an unknown name is refused"));
				Require(pEnc->GetEncodingName() == strBefore,
					QStringLiteral("encoding: and leaves the encoding alone, got '%1'")
						.arg(pEnc->GetEncodingName()));

				//------------------------------------------------------
				// CONVERT writes. The bytes change; the text does not.
				//------------------------------------------------------
				Require(pEnc->SetSaveEncoding(QStringLiteral("UTF-16LE")),
					QStringLiteral("encoding: UTF-16LE is accepted"));
				QString strSaveError;
				Require(pEnc->SaveFile(strPath, strSaveError),
					QStringLiteral("encoding: saved as UTF-16LE (%1)").arg(strSaveError));
				const QByteArray after = ReadAll(strPath);
				Require(after != utf8Bytes,
					QStringLiteral("encoding: converting CHANGED the bytes on disk"));
				Require(QStringDecoder(QStringConverter::Utf16LE).decode(after) == strText,
					QStringLiteral("encoding: and the text survived the conversion"));

				//------------------------------------------------------
				// REINTERPRET does not write. The text changes; the bytes
				// do not. This is the check that fails if the two
				// operations are ever wired to the same code path.
				//------------------------------------------------------
				const QByteArray beforeReload = after;
				QString strReloadError;
				Require(pEnc->ReloadWithEncoding(QStringLiteral("ISO-8859-1"), strReloadError),
					QStringLiteral("encoding: reinterpreted as Latin-1 (%1)")
						.arg(strReloadError));
				const QByteArray afterReload = ReadAll(strPath);
				Require(afterReload == beforeReload,
					QStringLiteral("encoding: reinterpreting wrote NOTHING to disk"));
				Require(!pEnc->IsModified(),
					QStringLiteral("encoding: and leaves the document clean, because the "
						"file still matches what is on screen"));

				// Reading UTF-16 bytes as Latin-1 must actually have changed
				// what is on screen - otherwise the check above is vacuous.
				const sptr_t nLen = pEnc->Send(SCI_GETLENGTH);
				QByteArray shown(static_cast<int>(nLen) + 1, '\0');
				pEnc->Send(SCI_GETTEXT, static_cast<uptr_t>(nLen) + 1,
					reinterpret_cast<sptr_t>(shown.data()));
				shown.truncate(static_cast<int>(nLen));
				Require(QString::fromUtf8(shown) != strText,
					QStringLiteral("encoding: reinterpreting DID change the text, so the "
						"no-write check above means something"));

				//------------------------------------------------------
				// WHICH PATH a name resolves to, tested through the BOM
				// rather than through the name.
				//
				// GetEncodingName() returns "UTF-8" whichever library handled
				// it, so a check on the name cannot tell the two apart - the
				// first version of this block asserted exactly that and a
				// mutation forcing every name onto the codec path went
				// UNCAUGHT. The observable difference is the byte-order mark:
				// the codec path forces m_bHasBom false, so a UTF-8 file with
				// a BOM would silently lose it on save.
				//------------------------------------------------------
				{
					const QString strBomPath = scratch.filePath(QStringLiteral("bom.txt"));
					QFile bom(strBomPath);
					Require(bom.open(QIODevice::WriteOnly),
						QStringLiteral("encoding: wrote a BOM'd seed file"));
					bom.write("\xEF\xBB\xBF");
					bom.write(strText.toUtf8());
					bom.close();

					Require(OpenFile(strBomPath),
						QStringLiteral("encoding: opened the BOM'd file"));
					CEditorWidget* pBom = GetCurrentEditor();
					Require(pBom != nullptr && pBom->GetEncodingLabel().contains(
							QStringLiteral("BOM")),
						QStringLiteral("encoding: it is recognised as carrying a BOM, got '%1'")
							.arg(pBom == nullptr ? QString() : pBom->GetEncodingLabel()));
					if (pBom != nullptr)
					{
						// UTF-8 must go to the BUILTIN path, which is the only
						// one that can write a BOM back.
						Require(pBom->SetSaveEncoding(QStringLiteral("UTF-8")),
							QStringLiteral("encoding: UTF-8 accepted on the BOM'd file"));
						QString strErr;
						Require(pBom->SaveFile(strBomPath, strErr),
							QStringLiteral("encoding: saved it (%1)").arg(strErr));
						Require(ReadAll(strBomPath).startsWith("\xEF\xBB\xBF"),
							QStringLiteral("encoding: THE ROUTING - choosing UTF-8 keeps the "
								"BOM, so the name went to QStringConverter and not to a codec"));

						// And switching to a legacy codepage drops it, which is
						// the invariant stated on m_CodecName.
						Require(pBom->SetSaveEncoding(QStringLiteral("windows-1258")),
							QStringLiteral("encoding: windows-1258 accepted on a BOM'd file"));
						Require(pBom->SaveFile(strBomPath, strErr),
							QStringLiteral("encoding: saved as windows-1258 (%1)").arg(strErr));
						Require(!ReadAll(strBomPath).startsWith("\xEF\xBB\xBF"),
							QStringLiteral("encoding: THE INVARIANT - a codepage save drops "
								"the byte-order mark rather than writing a UTF-8 one"));

						// AND COMING BACK RESTORES IT. The mark belongs to the
						// file as it was read, not to the last encoding chosen,
						// so a visit to a codepage must not destroy it for good.
						// The first version cleared m_bHasBom when a codec was
						// picked, which did exactly that - mutation testing
						// showed the line was untested AND wrong.
						Require(pBom->SetSaveEncoding(QStringLiteral("UTF-8")),
							QStringLiteral("encoding: back to UTF-8 from a codepage"));
						Require(pBom->SaveFile(strBomPath, strErr),
							QStringLiteral("encoding: saved again (%1)").arg(strErr));
						Require(ReadAll(strBomPath).startsWith("\xEF\xBB\xBF"),
							QStringLiteral("encoding: and the BOM came back, because a detour "
								"through a codepage must not destroy it permanently"));

						// A BOM BELONGS TO ONE ENCODING, not to "the bytes start
						// with something". Reinterpreting a UTF-8-with-BOM file
						// as Latin-1 must not leave the document claiming a
						// mark: Latin-1 has no such concept, and the EF BB BF
						// is now three ordinary characters of text.
						//
						// Found in review. The test is written before the fix,
						// so its failure is the evidence the defect was real.
						QString strLatinError;
						Require(pBom->ReloadWithEncoding(QStringLiteral("ISO-8859-1"),
								strLatinError),
							QStringLiteral("encoding: reinterpreted the BOM'd file as Latin-1 "
								"(%1)").arg(strLatinError));
						Require(!pBom->GetEncodingLabel().contains(QStringLiteral("BOM")),
							QStringLiteral("encoding: Latin-1 does not claim a byte-order "
								"mark, got '%1'").arg(pBom->GetEncodingLabel()));
						// NOT a byte check here, and the reason is worth stating:
						// reinterpreted as Latin-1 those EF BB BF bytes are
						// three ORDINARY CHARACTERS of text, and writing them
						// back is correct - measured, the file is 10 bytes in
						// and 10 bytes out, because Latin-1 ignores WriteBom.
						// A byte assertion here would fail on correct
						// behaviour. The label is the whole defect.
						//
						// The encoding where the flag really does inject bytes
						// is a Unicode one:
						QFile reseed(strBomPath);
						Require(reseed.open(QIODevice::WriteOnly),
							QStringLiteral("encoding: re-seeded the BOM'd file"));
						reseed.write("\xEF\xBB\xBF");
						reseed.write(strText.toUtf8());
						reseed.close();
						Require(pBom->ReloadWithEncoding(QStringLiteral("UTF-8"), strErr),
							QStringLiteral("encoding: back to UTF-8 (%1)").arg(strErr));
						Require(pBom->ReloadWithEncoding(QStringLiteral("UTF-16LE"), strErr),
							QStringLiteral("encoding: reinterpreted the UTF-8 BOM'd file as "
								"UTF-16LE (%1)").arg(strErr));
						Require(pBom->SaveFile(strBomPath, strErr),
							QStringLiteral("encoding: saved it as UTF-16LE (%1)").arg(strErr));
						Require(!ReadAll(strBomPath).startsWith("\xFF\xFE"),
							QStringLiteral("encoding: and did NOT inject a UTF-16 mark the "
								"file never had - the UTF-8 mark it did have belongs to a "
								"different encoding"));

						pBom->Send(SCI_SETSAVEPOINT);
						OnCloseTab(m_pTabs->indexOf(pBom));
					}
					m_pTabs->setCurrentIndex(m_pTabs->indexOf(pEnc));
				}

				//------------------------------------------------------
				// A legacy codec round-trips, and never claims a BOM.
				//------------------------------------------------------
				if (QTextCodec* pCodec = QTextCodec::codecForName("windows-1258"))
				{
					if (pCodec->canEncode(strText))
					{
						Require(pEnc->ReloadWithEncoding(QStringLiteral("UTF-16LE"),
								strReloadError),
							QStringLiteral("encoding: back to the UTF-16LE text"));
						Require(pEnc->SetSaveEncoding(QStringLiteral("windows-1258")),
							QStringLiteral("encoding: windows-1258 is accepted"));
						Require(pEnc->GetEncodingName() == QStringLiteral("windows-1258"),
							QStringLiteral("encoding: and is reported, got '%1'")
								.arg(pEnc->GetEncodingName()));
						Require(!pEnc->GetEncodingLabel().contains(QStringLiteral("BOM")),
							QStringLiteral("encoding: THE INVARIANT - a codec-path encoding "
								"never carries a BOM, got '%1'")
								.arg(pEnc->GetEncodingLabel()));
						Require(pEnc->SaveFile(strPath, strSaveError),
							QStringLiteral("encoding: saved as windows-1258 (%1)")
								.arg(strSaveError));
						const QByteArray legacy = ReadAll(strPath);
						Require(!legacy.startsWith("\xEF\xBB\xBF")
								&& !legacy.startsWith("\xFF\xFE"),
							QStringLiteral("encoding: and wrote no byte-order mark"));
						Require(pEnc->ReloadWithEncoding(QStringLiteral("windows-1258"),
								strReloadError),
							QStringLiteral("encoding: read it back as windows-1258"));
						QByteArray back(static_cast<int>(pEnc->Send(SCI_GETLENGTH)) + 1, '\0');
						pEnc->Send(SCI_GETTEXT,
							static_cast<uptr_t>(pEnc->Send(SCI_GETLENGTH)) + 1,
							reinterpret_cast<sptr_t>(back.data()));
						back.truncate(static_cast<int>(pEnc->Send(SCI_GETLENGTH)));
						Require(QString::fromUtf8(back) == strText,
							QStringLiteral("encoding: and the text round-tripped through a "
								"legacy codepage"));
					}
				}

				//------------------------------------------------------
				// THROUGH ApplyEncoding, which is what the menus call.
				//
				// Everything above drives the editor directly, so swapping the
				// two submenus - the single worst mistake available here -
				// would not have failed one of them. This is the check that
				// covers the wiring rather than the mechanism.
				//------------------------------------------------------
				const QByteArray beforeMenu = ReadAll(strPath);
				ApplyEncoding(CEncodingDialog::EMode::Reinterpret,
					QStringLiteral("ISO-8859-1"));
				Require(ReadAll(strPath) == beforeMenu,
					QStringLiteral("encoding: the REINTERPRET menu path writes nothing"));

				ApplyEncoding(CEncodingDialog::EMode::Convert, QStringLiteral("UTF-8"));
				const QByteArray afterMenu = ReadAll(strPath);
				Require(afterMenu != beforeMenu,
					QStringLiteral("encoding: the CONVERT menu path does write"));
				Require(pEnc->GetEncodingName() == QStringLiteral("UTF-8"),
					QStringLiteral("encoding: and left the document on UTF-8, got '%1'")
						.arg(pEnc->GetEncodingName()));

				//------------------------------------------------------
				// A CODEC THAT WENT AWAY between choosing it and saving.
				//
				// Reachable only through a test seam, because SetSaveEncoding
				// refuses names that do not resolve - so without one this would
				// be a third branch documented as uncovered. Found in review:
				// it used to fall through and write the BUILTIN encoding while
				// the label went on naming the codec.
				//------------------------------------------------------
				{
					const QByteArray intact = ReadAll(strPath);
					Require(!intact.isEmpty(),
						QStringLiteral("encoding: the file has content before the "
							"gone-codec check, so truncation would be visible"));

					pEnc->SetCodecNameForTest(QByteArray("no-such-codec-exists"));
					QString strGoneError;
					Require(!pEnc->SaveFile(strPath, strGoneError),
						QStringLiteral("encoding: a save REFUSES when the chosen codec is "
							"gone, rather than writing some other encoding"));
					Require(strGoneError.contains(QStringLiteral("not available")),
						QStringLiteral("encoding: and says why, got '%1'").arg(strGoneError));

					// THE FILE MUST STILL BE THERE. SaveFile opened with
					// Truncate BEFORE encoding, so a refused save emptied the
					// document it was supposed to write - a zero-byte file
					// where the user's text had been. The encode now happens
					// first, and this is the check that says so.
					Require(ReadAll(strPath) == intact,
						QStringLiteral("encoding: and the file on disk is UNTOUCHED - not "
							"truncated to nothing on the way to failing"));

					// Back to something real, so the tab can be closed cleanly.
					Require(pEnc->SetSaveEncoding(QStringLiteral("UTF-8")),
						QStringLiteral("encoding: recovered to UTF-8 after the gone codec"));
				}

				pEnc->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pEnc));
			}

			// An untitled document has nothing on disk to reinterpret, and
			// saying so beats silently loading an empty file over the user's
			// unsaved typing.
			CEditorWidget* pFresh = NewUntitled();
			QString strFreshError;
			Require(pFresh != nullptr && !pFresh->ReloadWithEncoding(
					QStringLiteral("UTF-8"), strFreshError),
				QStringLiteral("encoding: an untitled document refuses to be reinterpreted"));
			// The specific message, not merely "some error". Without the
			// IsUntitled guard the code reaches QFile("") which also fails, so
			// a non-empty error proves nothing about which check refused - the
			// first version asserted exactly that and the mutation was MISSED.
			Require(strFreshError.contains(QStringLiteral("never been saved")),
				QStringLiteral("encoding: and refuses for the RIGHT reason, got '%1'")
					.arg(strFreshError));
			if (pFresh != nullptr)
			{
				pFresh->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pFresh));
			}
		}

		//--------------------------------------------------------------
		// The dialog: one class, two modes, and the caption is the only
		// thing distinguishing them at the moment of committing.
		//--------------------------------------------------------------
		{
			CEncodingDialog reopen(CEncodingDialog::EMode::Reinterpret,
				QStringLiteral("UTF-8"), this);
			CEncodingDialog convert(CEncodingDialog::EMode::Convert,
				QStringLiteral("UTF-8"), this);
			Require(reopen.OkButtonText() != convert.OkButtonText(),
				QStringLiteral("encoding: the two modes label the OK button differently "
					"('%1' vs '%2')").arg(reopen.OkButtonText(), convert.OkButtonText()));
			Require(reopen.OkButtonText() == QStringLiteral("Reopen File"),
				QStringLiteral("encoding: reinterpret says 'Reopen File', got '%1'")
					.arg(reopen.OkButtonText()));
			Require(convert.OkButtonText() == QStringLiteral("Save File"),
				QStringLiteral("encoding: convert says 'Save File', got '%1'")
					.arg(convert.OkButtonText()));

			// It opens on what the document already is, not at the top of an
			// alphabetical list of 800.
			Require(reopen.SelectedEncoding() == QStringLiteral("UTF-8"),
				QStringLiteral("encoding: the dialog opens on the current encoding, got '%1'")
					.arg(reopen.SelectedEncoding()));

			const int nAll = reopen.VisibleRowCount();
			reopen.SetFilterForTest(QStringLiteral("1258"));
			const int nFiltered = reopen.VisibleRowCount();
			Require(nFiltered > 0 && nFiltered < nAll,
				QStringLiteral("encoding: the filter narrows %1 rows to %2")
					.arg(nAll).arg(nFiltered));
			// A row hidden by the filter must not still be returned, or accepting
			// would apply an encoding the user can no longer see. This holds
			// because QTreeWidget clears the selection when the row is hidden -
			// so this check is asserting QT'S behaviour, which the dialog relies
			// on instead of re-guarding. If Qt ever changes, this fails and the
			// explicit isHidden() guard goes back into SelectedEncoding().
			Require(reopen.SelectedEncoding().isEmpty(),
				QStringLiteral("encoding: a filtered-out selection returns nothing, got '%1'")
					.arg(reopen.SelectedEncoding()));
			Require(reopen.SelectForTest(QStringLiteral("windows-1258"))
					&& reopen.SelectedEncoding() == QStringLiteral("windows-1258"),
				QStringLiteral("encoding: selecting a visible row returns it"));
			reopen.SetFilterForTest(QString());
			Require(reopen.VisibleRowCount() == nAll,
				QStringLiteral("encoding: clearing the filter restores every row"));
		}

		//--------------------------------------------------------------
		// The six fixed MENU entries, through the menu.
		//
		// The list check above covers what the dialog offers; this covers what
		// the menus hard-code, which is a different set of strings and is where
		// "System" - a name no library resolves - shipped.
		//--------------------------------------------------------------
		{
			CEditorWidget* pMenuProbe = NewUntitled();
			Require(pMenuProbe != nullptr,
				QStringLiteral("encoding: got a document for the menu check"));
			QMenu* pSaveMenu = nullptr;
			for (QMenu* pMenu : menuBar()->findChildren<QMenu*>())
			{
				QString strTitle = pMenu->title();
				strTitle.remove(QLatin1Char('&'));
				if (strTitle == QStringLiteral("Save As Encoding"))
				{
					pSaveMenu = pMenu;
				}
			}
			Require(pSaveMenu != nullptr,
				QStringLiteral("encoding: found the Save As Encoding menu"));
			if (pSaveMenu != nullptr && pMenuProbe != nullptr)
			{
				int nFixed = 0;
				for (QAction* pAction : pSaveMenu->actions())
				{
					if (pAction->isSeparator()
						|| pAction->text().contains(QStringLiteral("Code Page")))
					{
						continue;
					}
					++nFixed;
					// Set it to something else first, so "it worked" cannot be
					// the previous iteration's leftover.
					Require(pMenuProbe->SetSaveEncoding(QStringLiteral("ISO-8859-1")),
						QStringLiteral("encoding: reset before '%1'").arg(pAction->text()));
					const QString strEncoding = pAction->data().toString();
					Require(!strEncoding.isEmpty(),
						QStringLiteral("encoding: menu item '%1' carries the encoding it "
							"applies").arg(pAction->text()));
					Require(pMenuProbe->SetSaveEncoding(strEncoding),
						QStringLiteral("encoding: menu item '%1' names an encoding that "
							"resolves, got '%2'").arg(pAction->text(), strEncoding));
				}
				Require(nFixed == 6,
					QStringLiteral("encoding: the menu offers the MFC's six fixed encodings, "
						"got %1").arg(nFixed));
				pMenuProbe->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pMenuProbe));
			}
		}
	}

	//----------------------------------------------------------------------
	// The window manager - src/OpenTabWindows.cpp. A dialog, not a dock pane:
	// see doc/PORTING.md 6n. LIGHT rigour per D10 - this is the shallow
	// visible tail, so a check and a screenshot, not a differential test.
	//----------------------------------------------------------------------
	{
		CWindowListDialog dialog(this);
		dialog.SetEntries(CollectWindowList());

		Require(dialog.RowCount() == GetTabCount(),
			QStringLiteral("windows: one row per open tab, %1 rows for %2 tabs")
				.arg(dialog.RowCount()).arg(GetTabCount()));
		Require(dialog.windowTitle().contains(QString::number(GetTabCount())),
			QStringLiteral("windows: the title carries the count, got '%1'")
				.arg(dialog.windowTitle()));

		// Every row names a real tab, in tab order - which is what makes an
		// index handed back by this dialog safe to use as a tab index.
		int nMismatched = 0;
		for (int i = 0; i < dialog.RowCount() && i < GetTabCount(); ++i)
		{
			CEditorWidget* pEditor = qobject_cast<CEditorWidget*>(m_pTabs->widget(i));
			if (pEditor != nullptr
				&& !dialog.RowText(i, 0).startsWith(pEditor->GetDisplayName()))
			{
				++nMismatched;
			}
		}
		Require(nMismatched == 0,
			QStringLiteral("windows: every row names its tab, in tab order; %1 did not")
				.arg(nMismatched));

		// A saved document shows its path; an untitled one shows N/A. Both
		// asserted, and the presence of both is asserted too - a run with only
		// one kind would let a hard-coded answer pass.
		CEditorWidget* pFresh = NewUntitled();
		dialog.SetEntries(CollectWindowList());
		const int nUntitledRow = m_pTabs->indexOf(pFresh);
		Require(dialog.RowText(nUntitledRow, 1) == QStringLiteral("N/A"),
			QStringLiteral("windows: an untitled document shows N/A, got '%1'")
				.arg(dialog.RowText(nUntitledRow, 1)));
		Require(dialog.RowText(0, 1) != QStringLiteral("N/A"),
			QStringLiteral("windows: a document on disk shows its path, got '%1'")
				.arg(dialog.RowText(0, 1)));

		// Copy Full Path takes the path, and refuses the one that is not a
		// path. The MFC guards with PathFileExists for the same reason.
		Require(dialog.SelectRowForTest(0), QStringLiteral("windows: selected a saved row"));
		dialog.TriggerForTest(QStringLiteral("copy"));
		Require(dialog.CopiedPathForTest() == dialog.RowText(0, 1),
			QStringLiteral("windows: Copy Full Path copied '%1'")
				.arg(dialog.CopiedPathForTest()));
		const QString strCopiedBefore = dialog.CopiedPathForTest();
		Require(dialog.SelectRowForTest(nUntitledRow),
			QStringLiteral("windows: selected the untitled row"));
		dialog.TriggerForTest(QStringLiteral("copy"));
		Require(dialog.CopiedPathForTest() == strCopiedBefore,
			QStringLiteral("windows: and copies NOTHING for a document with no path, got "
				"'%1'").arg(dialog.CopiedPathForTest()));

		// THROUGH THE SHIPPED HANDLERS. The first version connected a throwaway
		// lambda of its own and moved m_pTabs directly, so the handlers in
		// OnWindowManager were never covered - only a re-implementation of
		// them was. Found in review, and it is the same hole 6m found in the
		// encoding menus. ConnectWindowList is now the single wiring point that
		// OnWindowManager and this check share.
		ConnectWindowList(&dialog);

		// Activate switches tabs. Checked against a row that is NOT already
		// current, or it would pass without doing anything.
		const int nOther = (m_pTabs->currentIndex() == 0) ? 1 : 0;
		Require(m_pTabs->currentIndex() != nOther,
			QStringLiteral("windows: the activate target is not already current"));
		Require(dialog.SelectRowForTest(nOther), QStringLiteral("windows: selected it"));
		dialog.TriggerForTest(QStringLiteral("activate"));
		Require(m_pTabs->currentIndex() == nOther,
			QStringLiteral("windows: Activate switched to tab %1, current is %2")
				.arg(nOther).arg(m_pTabs->currentIndex()));

		// SAVE, which was not covered at all - Activate and Close were, and the
		// gap was raised in review. On a scratch file in a temporary directory,
		// never a fixture: a check that writes to the corpus it reads from has
		// bitten this port before.
		{
			QTemporaryDir saveDir;
			Require(saveDir.isValid(),
				QStringLiteral("windows: got a directory to save into"));
			if (saveDir.isValid())
			{
				const QString strPath = saveDir.filePath(QStringLiteral("bg.txt"));
				QFile seed(strPath);
				Require(seed.open(QIODevice::WriteOnly),
					QStringLiteral("windows: seeded a file to save"));
				seed.write("original\n");
				seed.close();

				Require(OpenFile(strPath), QStringLiteral("windows: opened it"));
				CEditorWidget* pBg = GetCurrentEditor();
				const int nBgRow = m_pTabs->indexOf(pBg);
				Require(pBg != nullptr, QStringLiteral("windows: it is current"));
				pBg->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("edited\n"));
				Require(pBg->IsModified(), QStringLiteral("windows: and modified"));

				// Make it a BACKGROUND row - the whole point of the check.
				m_pTabs->setCurrentIndex(0);
				const int nCurrentBefore = m_pTabs->currentIndex();
				Require(nCurrentBefore != nBgRow,
					QStringLiteral("windows: the row to save is not the current tab"));

				dialog.SetEntries(CollectWindowList());
				Require(dialog.SelectRowForTest(nBgRow),
					QStringLiteral("windows: selected the background row"));
				dialog.TriggerForTest(QStringLiteral("save"));

				Require(!pBg->IsModified(),
					QStringLiteral("windows: Save wrote the background document"));
				QFile written(strPath);
				QByteArray onDisk;
				if (written.open(QIODevice::ReadOnly)) { onDisk = written.readAll(); }
				Require(onDisk == QByteArray("edited\n"),
					QStringLiteral("windows: and the bytes reached the file, got '%1'")
						.arg(QString::fromUtf8(onDisk)));
				// AND IT DID NOT MOVE THE USER. Saving a background row used to
				// switch the active tab to it as a side effect, which
				// COpenTabWindows does not do - it saves the document it looked
				// up and never touches the active view.
				Require(m_pTabs->currentIndex() == nCurrentBefore,
					QStringLiteral("windows: and left the active tab alone (%1, was %2)")
						.arg(m_pTabs->currentIndex()).arg(nCurrentBefore));

				pBg->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pBg));
			}
		}

		// And Close, which was not covered at all. Closes the untitled scratch
		// row through the dialog and checks BOTH that the tab went and that the
		// list refreshed itself afterwards - the MFC calls InitiateList for
		// exactly that reason.
		const int nBeforeClose = GetTabCount();
		pFresh->Send(SCI_SETSAVEPOINT);		// or ConfirmClose blocks on a prompt
		Require(dialog.SelectRowsForTest({ m_pTabs->indexOf(pFresh) }),
			QStringLiteral("windows: selected the scratch row to close"));
		dialog.TriggerForTest(QStringLiteral("close"));
		Require(GetTabCount() == nBeforeClose - 1,
			QStringLiteral("windows: Close Tab(s) closed one tab, %1 -> %2")
				.arg(nBeforeClose).arg(GetTabCount()));
		Require(dialog.RowCount() == GetTabCount(),
			QStringLiteral("windows: and the list refreshed itself, %1 rows for %2 tabs")
				.arg(dialog.RowCount()).arg(GetTabCount()));

		// Selected rows come back DESCENDING, which is what stops a caller
		// closing by index from invalidating the indices it has not reached.
		if (dialog.RowCount() >= 3)
		{
			// THREE rows, not one. A single-row selection is sorted both ways,
			// so the first version of this check could not fail and the
			// mutation reversing the comparator went straight through it.
			Require(dialog.SelectRowsForTest({ 0, 2, 1 }),
				QStringLiteral("windows: selected three rows out of order"));
			const QList<int> rows = dialog.SelectedRows();
			Require(rows.size() == 3,
				QStringLiteral("windows: all three come back, got %1").arg(rows.size()));
			QList<int> sorted = rows;
			std::sort(sorted.begin(), sorted.end(), std::greater<int>());
			Require(rows == sorted && rows.first() > rows.last(),
				QStringLiteral("windows: selected rows come back highest-first (%1), so "
					"closing by index stays valid")
					.arg(QStringList{ QString::number(rows.at(0)),
						QString::number(rows.at(1)),
						QString::number(rows.at(2)) }.join(QLatin1Char(','))));
		}

	}

	//----------------------------------------------------------------------
	// Bookmarks - src/BookmarkWindow.cpp and CEditorCtrl's marker calls.
	// See doc/PORTING.md 6o.
	//----------------------------------------------------------------------
	{
		CEditorWidget* pMarks = GetCurrentEditor();
		Require(pMarks != nullptr, QStringLiteral("bookmarks: got a document"));
		if (pMarks != nullptr)
		{
			pMarks->ClearBookmarks();
			RefreshBookmarks();
			Require(!pMarks->HasBookmarks(),
				QStringLiteral("bookmarks: none to start with"));
			Require(m_pBookmarkPane->RowCount() == 0,
				QStringLiteral("bookmarks: and the pane is empty, got %1 rows")
					.arg(m_pBookmarkPane->RowCount()));

			// THE MARGIN HAS WIDTH. It was 0 until this change, so a marker set
			// on it would have been invisible - the check that would have
			// caught shipping the markers without the margin.
			Require(pMarks->Send(SCI_GETMARGINWIDTHN, 1) > 0,
				QStringLiteral("bookmarks: the symbol margin has width, so a marker on it "
					"can be seen"));
			Require((pMarks->Send(SCI_GETMARGINMASKN, 1) & (1 << 3)) != 0,
				QStringLiteral("bookmarks: and admits the bookmark marker"));

			// Toggle on, off, on.
			pMarks->ToggleBookmark(5);
			Require(pMarks->IsLineBookmarked(5),
				QStringLiteral("bookmarks: toggled one on at line 5"));
			Require(pMarks->HasBookmarks(),
				QStringLiteral("bookmarks: and the document reports having some"));
			pMarks->ToggleBookmark(5);
			Require(!pMarks->IsLineBookmarked(5),
				QStringLiteral("bookmarks: toggled it off again"));
			pMarks->ToggleBookmark(5);

			// A MASK, NOT AN EQUALITY. CEditorCtrl::IsLineHasBookMark tests
			// `== 8 || == 9`, so a bookmark sharing a line with marker 1 - mask
			// 10 - reports as absent. ui-qt has no breakpoints, so marker 1 is
			// set directly here to reach the state that exposes it.
			pMarks->Send(SCI_MARKERADD, 4, 1);		// marker 1 on line 5
			Require(pMarks->Send(SCI_MARKERGET, 4) == 10,
				QStringLiteral("bookmarks: line 5 now carries mask %1, which the MFC's "
					"equality test does not admit")
					.arg(static_cast<int>(pMarks->Send(SCI_MARKERGET, 4))));
			Require(pMarks->IsLineBookmarked(5),
				QStringLiteral("bookmarks: and it is STILL bookmarked, because the test "
					"masks rather than compares"));
			pMarks->Send(SCI_MARKERDELETE, 4, 1);

			// Enumeration, ascending, and the pane built from it.
			pMarks->ToggleBookmark(12);
			pMarks->ToggleBookmark(2);
			const QList<int> lines = pMarks->BookmarkedLines();
			Require(lines == QList<int>({ 2, 5, 12 }),
				QStringLiteral("bookmarks: enumerated ascending, got %1")
					.arg(QStringList{ QString::number(lines.value(0)),
						QString::number(lines.value(1)),
						QString::number(lines.value(2)) }.join(QLatin1Char(','))));
			RefreshBookmarks();
			Require(m_pBookmarkPane->RowCount() == 3,
				QStringLiteral("bookmarks: the pane shows three, got %1")
					.arg(m_pBookmarkPane->RowCount()));
			Require(m_pBookmarkPane->RowText(0, 0) == QStringLiteral("2"),
				QStringLiteral("bookmarks: first row is line 2, got '%1'")
					.arg(m_pBookmarkPane->RowText(0, 0)));
			Require(m_pBookmarkPane->RowText(0, 2) == pMarks->TextOfLine(2),
				QStringLiteral("bookmarks: and carries that line's text"));

			// THE MARKERS ARE THE TRUTH. Inserting a line above a bookmark
			// moves the marker, and a list DERIVED from the markers follows it.
			// src/BookmarkWindow.cpp stores line numbers and updates them only
			// on add and delete, so the Windows pane would still say 2 here.
			pMarks->Send(SCI_GOTOPOS, 0);
			pMarks->Send(SCI_ADDTEXT, 1, reinterpret_cast<sptr_t>("\n"));
			const QList<int> moved = pMarks->BookmarkedLines();
			Require(moved == QList<int>({ 3, 6, 13 }),
				QStringLiteral("bookmarks: an inserted line moved every marker down one, "
					"got %1").arg(QStringList{ QString::number(moved.value(0)),
						QString::number(moved.value(1)),
						QString::number(moved.value(2)) }.join(QLatin1Char(','))));
			RefreshBookmarks();
			Require(m_pBookmarkPane->RowText(0, 0) == QStringLiteral("3"),
				QStringLiteral("bookmarks: and the pane followed, got '%1'")
					.arg(m_pBookmarkPane->RowText(0, 0)));
			pMarks->Send(SCI_UNDO);
			pMarks->Send(SCI_SETSAVEPOINT);

			// Navigation, which on Windows calls the BREAKPOINT functions -
			// OnOptionsFindNextBookmark is FindNextBreakPoint - and would move
			// to breakpoints, or in ui-qt to nothing at all.
			pMarks->GotoLine(1);
			Require(pMarks->NextBookmark() == 2,
				QStringLiteral("bookmarks: next from line 1 is 2"));
			Require(pMarks->NextBookmark() == 5,
				QStringLiteral("bookmarks: then 5"));
			Require(pMarks->PreviousBookmark() == 2,
				QStringLiteral("bookmarks: and back to 2"));
			// BOTH directions wrap, which the MFC's do not. Checking only one
			// left the other's wrap untested - a mutation removing NextBookmark's
			// fallback went straight through, because every assertion up to here
			// used PreviousBookmark for the wrap case.
			Require(pMarks->PreviousBookmark() == 12,
				QStringLiteral("bookmarks: previous from the first wraps to the last"));
			Require(pMarks->NextBookmark() == 2,
				QStringLiteral("bookmarks: and next from the last wraps to the first"));

			// A REAL MARGIN CLICK, through the shipped signal and the shipped
			// toggle - not a direct call to ToggleBookmark.
			const int nBeforeClick = pMarks->BookmarkedLines().size();
			const int nMarginX = static_cast<int>(pMarks->Send(SCI_GETMARGINWIDTHN, 0)) + 4;
			const int nMarginY = static_cast<int>(pMarks->Send(SCI_POINTYFROMPOSITION, 0,
				pMarks->Send(SCI_POSITIONFROMLINE, 7))) + 2;
			const QPointF at(nMarginX, nMarginY);
			QMouseEvent press(QEvent::MouseButtonPress, at, at,
				Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
			QMouseEvent release(QEvent::MouseButtonRelease, at, at,
				Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
			QApplication::sendEvent(pMarks->viewport(), &press);
			QApplication::sendEvent(pMarks->viewport(), &release);
			Require(pMarks->BookmarkedLines().size() == nBeforeClick + 1,
				QStringLiteral("bookmarks: clicking the symbol margin added one, %1 -> %2")
					.arg(nBeforeClick).arg(pMarks->BookmarkedLines().size()));
			Require(pMarks->IsLineBookmarked(8),
				QStringLiteral("bookmarks: on the line clicked"));
			QApplication::sendEvent(pMarks->viewport(), &press);
			QApplication::sendEvent(pMarks->viewport(), &release);
			Require(!pMarks->IsLineBookmarked(8),
				QStringLiteral("bookmarks: and clicking again removed it"));

			// Clicking a pane row jumps to the bookmark, through the pane's own
			// signal.
			RefreshBookmarks();
			m_pTabs->setCurrentIndex(m_pTabs->count() - 1);
			Require(GetCurrentEditor() != pMarks,
				QStringLiteral("bookmarks: moved to a different tab first"));
			Require(m_pBookmarkPane->ActivateRowForTest(1),
				QStringLiteral("bookmarks: activated the second pane row"));
			Require(GetCurrentEditor() == pMarks,
				QStringLiteral("bookmarks: which switched back to the right document"));
			Require(pMarks->GetCaretLine() == 5,
				QStringLiteral("bookmarks: and put the caret on line 5, got %1")
					.arg(pMarks->GetCaretLine()));

			// Clear All empties both the document and the pane.
			pMarks->ClearBookmarks();
			RefreshBookmarks();
			Require(!pMarks->HasBookmarks(),
				QStringLiteral("bookmarks: Clear All removed them"));
			Require(m_pBookmarkPane->RowCount() == 0,
				QStringLiteral("bookmarks: and emptied the pane"));

			// The pane is a real dock: show and hide through the same action
			// the View menu uses, as §6k established for the message pane.
			// trigger(), NOT setChecked(). Qt connects toggleViewAction to the
			// dock through QAction::triggered, so setChecked moves the tick and
			// leaves the pane where it was - measured: checked went 0 -> 1 with
			// hidden staying 1. It fails loudly here; in product code it would
			// be a menu item whose tick disagreed with the screen. §6k's
			// message-pane check already used trigger(); this one did not, and
			// the check caught it.
			QAction* pToggle = m_pBookmarkPane->toggleViewAction();
			Require(m_pBookmarkPane->isHidden(),
				QStringLiteral("bookmarks: the pane starts hidden"));
			pToggle->trigger();
			Require(!m_pBookmarkPane->isHidden(),
				QStringLiteral("bookmarks: the View action shows the pane"));
			Require(pToggle->isChecked(),
				QStringLiteral("bookmarks: and the menu item ticks with it"));
			pToggle->trigger();
			Require(m_pBookmarkPane->isHidden(),
				QStringLiteral("bookmarks: and hides it again"));
			Require(!m_pBookmarkPane->objectName().isEmpty(),
				QStringLiteral("bookmarks: the pane has an objectName, which saveState "
					"keys the layout on"));
		}
	}

	//----------------------------------------------------------------------
	// Line transforms - the engine behind the seventeen text-transform
	// commands. See doc/PORTING.md 6p.
	//
	// THE IDENTITY CHECK IS THE POINT. These rewrite whole documents, so the
	// first thing asked of the primitive is that a transform which changes
	// nothing leaves the bytes untouched - across every fixture, which between
	// them cover CRLF+BOM, UTF-16, Latin-1 and a file with no trailing
	// newline. It catches EOL drift, trailing-newline drift and encoding
	// drift in one assertion, which is what protected the save path too.
	//----------------------------------------------------------------------
	{
		auto Identity = [](const QString& strLine, const CEditorWidget::SLineContext&)
		{
			return std::optional<QString>(strLine);
		};

		int nIdentityChecked = 0;
		for (int i = 0; i < GetTabCount(); ++i)
		{
			m_pTabs->setCurrentIndex(i);
			CEditorWidget* pEditor = GetCurrentEditor();
			if (pEditor == nullptr)
			{
				continue;
			}
			const QString strName = pEditor->GetDisplayName();

			auto WholeDocument = [pEditor]
			{
				const sptr_t n = pEditor->Send(SCI_GETLENGTH);
				QByteArray buffer(static_cast<int>(n) + 1, '\0');
				pEditor->Send(SCI_GETTEXT, static_cast<uptr_t>(n) + 1,
					reinterpret_cast<sptr_t>(buffer.data()));
				buffer.truncate(static_cast<int>(n));
				return buffer;
			};

			const QByteArray before = WholeDocument();
			pEditor->Send(SCI_SETSEL, 0, 0);			// no selection: whole document
			const int nLines = pEditor->ApplyLineTransform(Identity);
			Require(nLines == pEditor->GetLineCount(),
				QStringLiteral("transform: %1: identity visited every line, %2 of %3")
					.arg(strName).arg(nLines).arg(pEditor->GetLineCount()));
			Require(WholeDocument() == before,
				QStringLiteral("transform: %1: an identity transform left the document "
					"BYTE-IDENTICAL").arg(strName));
			++nIdentityChecked;

			// And it is undoable as ONE action, not one per line.
			pEditor->ApplyLineTransform([](const QString& strLine,
				const CEditorWidget::SLineContext&)
			{
				return std::optional<QString>(strLine + QStringLiteral("X"));
			});
			Require(WholeDocument() != before,
				QStringLiteral("transform: %1: a real transform did change it").arg(strName));
			pEditor->Send(SCI_UNDO);
			Require(WholeDocument() == before,
				QStringLiteral("transform: %1: and ONE undo put it back, so the whole "
					"transform is a single undo action").arg(strName));
			pEditor->Send(SCI_SETSAVEPOINT);
		}
		Require(nIdentityChecked >= 6,
			QStringLiteral("transform: identity ran on %1 documents, covering the encoding "
				"and line-ending fixtures").arg(nIdentityChecked));

		//--------------------------------------------------------------
		// All seventeen, table-driven.
		//
		// One row each, so coverage is by construction rather than by
		// seventeen hand-written blocks - and a command added to
		// LineTransforms::All() without a row here fails the count check at
		// the end rather than slipping in untested.
		//--------------------------------------------------------------
		struct SCase
		{
			const char* _Label;			// must match the command's menu label
			const char* _In1;
			const char* _In2;
			bool _Check;
			const char* _Input;			// lines separated by \n
			const char* _Expected;		// or nullptr when the input is refused
		};
		const SCase cases[] = {
			{ "Remove Lines Containing...",       "b",   "",    false, "aa\nbb\ncc",   "aa\ncc" },
			{ "Remove Lines Not Containing...",   "b",   "",    false, "aa\nbb\ncc",   "bb" },
			{ "Insert At Line Start...",          ">",   "",    false, "a\nb",         ">a\n>b" },
			{ "Insert At Line End...",            ";",   "",    false, "a\nb",         "a;\nb;" },
			{ "Insert Line Numbers (prefix)...",  "1",   "",    false, "a\nb",         "1 - a\n2 - b" },
			{ "Insert Line Numbers (suffix)...",  "1",   "",    false, "a\nb",         "a1\nb2" },
			{ "Insert Alphabet Index...",         "A",   "",    false, "a\nb",         "A - a\nB - b" },
			{ "Insert Roman Numerals...",         "4",   "",    false, "a\nb",         "IV - a\nV - b" },
			{ "Remove Before Word...",            "X",   "",    false, "abXcd\nno",    "Xcd\nno" },
			{ "Remove After Word...",             "X",   "",    false, "abXcd\nno",    "abX\nno" },
			{ "Insert Before Word...",            "X",   "[",   false, "abXcd",        "ab[Xcd" },
			{ "Insert After Word...",             "X",   "]",   false, "abXcd",        "abX]cd" },
			{ "Remove From Column X To Y...",     "1",   "3",   false, "abcde",        "ade" },
			{ "Remove From Column X To Y...",     "1",   "3",   true,  "abcde",        "abe" },
			{ "Insert At Column...",              "2",   "-",   false, "abcd",         "ab-cd" },
			{ "Insert At Column...",              "1",   "-",   true,  "abcd",         "abc-d" },
			{ "Remove Between Characters...",     "[",   "]",   false, "a[bcd]e\nno",  "a[]e\nno" },
			{ "Remove Between Characters...",     "[",   "]",   true,  "a[b]c[d]e",    "a[b]c[]e" },
			// ONE character present and the other absent. A line missing BOTH
			// is unchanged either way - QString::mid(-1) returns the whole
			// string - so it cannot tell the guard from its absence. This one
			// can: without the guard the prefix is duplicated onto the line,
			// which is what the MFC does.
			{ "Remove Between Characters...",     "[",   "]",   false, "a[bc",         "a[bc" },
			{ "Split Lines On Delimiter...",      ",",   "",    false, "a,b\nc",       "a\nb\nc" },
			{ "Join Lines With Delimiter...",     "+",   "",    false, "a\nb\n\nc",    "a+b+c" },
			// Refusals: the input never reaches the document.
			{ "Insert At Line Start...",          "",    "",    false, "a\nb",         nullptr },
			{ "Insert Roman Numerals...",         "4000","",    false, "a",            nullptr },
			{ "Insert Alphabet Index...",         "AB",  "",    false, "a",            nullptr },
		};

		CEditorWidget* pScratch = NewUntitled();
		Require(pScratch != nullptr, QStringLiteral("transform: got a scratch document"));
		if (pScratch != nullptr)
		{
			pScratch->Send(SCI_SETEOLMODE, SC_EOL_LF);
			QSet<QString> covered;
			for (const SCase& test : cases)
			{
				const LineTransforms::SCommand* pCommand = nullptr;
				for (const LineTransforms::SCommand& c : LineTransforms::All())
				{
					if (qstrcmp(c._MenuLabel, test._Label) == 0) { pCommand = &c; }
				}
				Require(pCommand != nullptr,
					QStringLiteral("transform: '%1' is a real command")
						.arg(QLatin1String(test._Label)));
				if (pCommand == nullptr) { continue; }
				covered.insert(QLatin1String(test._Label));

				CTransformDialog dialog(pCommand->_Prompt, this);
				dialog.SetValuesForTest(QLatin1String(test._In1),
					QLatin1String(test._In2), test._Check);
				QString strError;
				const CEditorWidget::FLineTransform fTransform =
					pCommand->_Build(dialog, strError);

				if (test._Expected == nullptr)
				{
					// A refusal must refuse, AND say why - "some error" is not
					// "the right error", which §6m learned the hard way.
					Require(!fTransform,
						QStringLiteral("transform: '%1' refuses '%2'")
							.arg(QLatin1String(test._Label), QLatin1String(test._In1)));
					Require(!strError.isEmpty(),
						QStringLiteral("transform: '%1' says why it refused")
							.arg(QLatin1String(test._Label)));
					continue;
				}

				Require(static_cast<bool>(fTransform),
					QStringLiteral("transform: '%1' accepts its input, said '%2'")
						.arg(QLatin1String(test._Label), strError));
				if (!fTransform) { continue; }

				const QByteArray input = QByteArray(test._Input);
				pScratch->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(input.constData()));
				pScratch->Send(SCI_EMPTYUNDOBUFFER);
				pScratch->Send(SCI_SETSEL, 0, 0);
				pScratch->ApplyLineTransform(fTransform);

				const sptr_t nLen = pScratch->Send(SCI_GETLENGTH);
				QByteArray got(static_cast<int>(nLen) + 1, '\0');
				pScratch->Send(SCI_GETTEXT, static_cast<uptr_t>(nLen) + 1,
					reinterpret_cast<sptr_t>(got.data()));
				got.truncate(static_cast<int>(nLen));
				Require(got == QByteArray(test._Expected),
					QStringLiteral("transform: '%1' on '%2' gives '%3', got '%4'")
						.arg(QLatin1String(test._Label),
							QString::fromUtf8(input).replace(QLatin1Char('\n'),
								QStringLiteral("\\n")),
							QString::fromUtf8(test._Expected).replace(QLatin1Char('\n'),
								QStringLiteral("\\n")),
							QString::fromUtf8(got).replace(QLatin1Char('\n'),
								QStringLiteral("\\n"))));
			}

			// EVERY command has a row. Without this a new transform could be
			// added to the table in LineTransforms.cpp and never be tested.
			Require(covered.size() == static_cast<int>(LineTransforms::All().size()),
				QStringLiteral("transform: all %1 commands are covered, %2 have rows")
					.arg(LineTransforms::All().size()).arg(covered.size()));
			Require(LineTransforms::All().size() == 17,
				QStringLiteral("transform: seventeen commands, got %1")
					.arg(LineTransforms::All().size()));

			// SPLIT ON A CRLF DOCUMENT. Every row above runs on an LF
			// document, where a mutation truncating the line ending to its
			// first character changes nothing - "\n".left(1) is "\n". Only a
			// CRLF document can tell the two apart, and split is the one
			// transform that writes line endings of its own.
			{
				pScratch->Send(SCI_SETEOLMODE, SC_EOL_CRLF);
				const QByteArray crlfIn("a,b\r\nc");
				pScratch->Send(SCI_SETTEXT, 0,
					reinterpret_cast<sptr_t>(crlfIn.constData()));
				pScratch->Send(SCI_SETSEL, 0, 0);
				const LineTransforms::SCommand* pSplit = nullptr;
				for (const LineTransforms::SCommand& c : LineTransforms::All())
				{
					if (qstrcmp(c._MenuLabel, "Split Lines On Delimiter...") == 0)
					{
						pSplit = &c;
					}
				}
				Require(pSplit != nullptr, QStringLiteral("transform: found split"));
				if (pSplit != nullptr)
				{
					CTransformDialog dlg(pSplit->_Prompt, this);
					dlg.SetValuesForTest(QStringLiteral(","), QString(), false);
					QString strErr;
					pScratch->ApplyLineTransform(pSplit->_Build(dlg, strErr));
					const sptr_t n = pScratch->Send(SCI_GETLENGTH);
					QByteArray out(static_cast<int>(n) + 1, '\0');
					pScratch->Send(SCI_GETTEXT, static_cast<uptr_t>(n) + 1,
						reinterpret_cast<sptr_t>(out.data()));
					out.truncate(static_cast<int>(n));
					Require(out == QByteArray("a\r\nb\r\nc"),
						QStringLiteral("transform: split writes the document's OWN line "
							"ending, got '%1'").arg(QString::fromUtf8(out.toHex(' '))));
				}
				pScratch->Send(SCI_SETEOLMODE, SC_EOL_LF);
			}

			// Roman numerals directly, since the table only reaches two values.
			Require(LineTransforms::ToRoman(1) == QStringLiteral("I")
					&& LineTransforms::ToRoman(4) == QStringLiteral("IV")
					&& LineTransforms::ToRoman(9) == QStringLiteral("IX")
					&& LineTransforms::ToRoman(14) == QStringLiteral("XIV")
					&& LineTransforms::ToRoman(40) == QStringLiteral("XL")
					&& LineTransforms::ToRoman(1987) == QStringLiteral("MCMLXXXVII")
					&& LineTransforms::ToRoman(3999) == QStringLiteral("MMMCMXCIX"),
				QStringLiteral("transform: Roman numerals, including the subtractive "
					"pairs and the 3999 ceiling"));

			// The SELECTION path: only the selected lines change, and they are
			// not duplicated - which is what four of the MFC's eight selection
			// sites get wrong.
			const QByteArray sel("a\nb\nc\nd");
			pScratch->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(sel.constData()));
			const sptr_t nL1 = pScratch->Send(SCI_POSITIONFROMLINE, 1);
			const sptr_t nL2 = pScratch->Send(SCI_GETLINEENDPOSITION, 2);
			pScratch->Send(SCI_SETSEL, static_cast<uptr_t>(nL1), nL2);
			pScratch->ApplyLineTransform([](const QString& strLine,
				const CEditorWidget::SLineContext&)
			{
				return std::optional<QString>(strLine.toUpper());
			});
			const sptr_t nLen2 = pScratch->Send(SCI_GETLENGTH);
			QByteArray got2(static_cast<int>(nLen2) + 1, '\0');
			pScratch->Send(SCI_GETTEXT, static_cast<uptr_t>(nLen2) + 1,
				reinterpret_cast<sptr_t>(got2.data()));
			got2.truncate(static_cast<int>(nLen2));
			Require(got2 == QByteArray("a\nB\nC\nd"),
				QStringLiteral("transform: a selection transforms ONLY its own lines and "
					"does not duplicate them, got '%1'")
					.arg(QString::fromUtf8(got2).replace(QLatin1Char('\n'),
						QStringLiteral("\\n"))));

			pScratch->Send(SCI_SETSAVEPOINT);
			OnCloseTab(m_pTabs->indexOf(pScratch));
		}
	}

	//----------------------------------------------------------------------
	// Single instance - CSingleInstanceApp, via QLocalServer. See
	// doc/PORTING.md 6q.
	//
	// ON A TEST-ONLY SOCKET NAME. Calling HandOff() under the real name would
	// connect to the user's ACTUAL running editor and open these files in it,
	// which is a self-test with side effects on the machine it runs on.
	//----------------------------------------------------------------------
	{
		const QString strTestName = CSingleInstance::SocketName()
			+ QStringLiteral("-selftest");
		Require(strTestName != CSingleInstance::SocketName(),
			QStringLiteral("instance: the test uses its own socket, not the real one"));
		Require(!CSingleInstance::SocketName().contains(QLatin1Char('/')),
			QStringLiteral("instance: the socket name is a name, not a path, got '%1'")
				.arg(CSingleInstance::SocketName()));

		// Nobody listening yet: a handoff must fail rather than hang, so a
		// first launch starts normally.
		QLocalServer::removeServer(strTestName);
		Require(!CSingleInstance::HandOff(QStringList(), strTestName),
			QStringLiteral("instance: with nobody listening, the handoff fails and this "
				"process becomes the instance"));

		CSingleInstance server(nullptr, strTestName);
		Require(server.Listen(),
			QStringLiteral("instance: claimed the socket"));

		QStringList received;
		bool bGotSignal = false;
		QObject::connect(&server, &CSingleInstance::FilesReceived, this,
			[&received, &bGotSignal](const QStringList& files)
		{
			received = files;
			bGotSignal = true;
		});

		// A real handoff, through the shipped code on both ends.
		QTemporaryDir handoffDir;
		Require(handoffDir.isValid(), QStringLiteral("instance: got a directory"));
		const QString strFile = handoffDir.filePath(QStringLiteral("handed.txt"));
		QFile seed(strFile);
		Require(seed.open(QIODevice::WriteOnly), QStringLiteral("instance: seeded a file"));
		seed.write("handed over\n");
		seed.close();

		Require(CSingleInstance::HandOff({ strFile }, strTestName),
			QStringLiteral("instance: the handoff was accepted"));
		// The signal arrives on the event loop, which a self-test does not run.
		for (int i = 0; i < 50 && !bGotSignal; ++i)
		{
			QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		Require(bGotSignal, QStringLiteral("instance: the listening side was told"));
		Require(received == QStringList{ strFile },
			QStringLiteral("instance: and got the file, expected '%1' got '%2'")
				.arg(strFile, received.join(QLatin1Char(','))));

		// RELATIVE PATHS ARE MADE ABSOLUTE. The running instance has its own
		// working directory, so a relative path would resolve against the
		// wrong one - and silently open a different file, or none.
		received.clear();
		bGotSignal = false;
		const QString strRelative = QDir::current().relativeFilePath(strFile);
		Require(strRelative != strFile,
			QStringLiteral("instance: the relative form differs, so this check can fail"));
		Require(CSingleInstance::HandOff({ strRelative }, strTestName),
			QStringLiteral("instance: handed over a relative path"));
		for (int i = 0; i < 50 && !bGotSignal; ++i)
		{
			QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		Require(received == QStringList{ strFile },
			QStringLiteral("instance: which arrived ABSOLUTE, got '%1'")
				.arg(received.join(QLatin1Char(','))));

		// A STALE SOCKET does not lock the editor out forever. On Unix the
		// file outlives a crashed process; without removeServer every launch
		// after a crash would open a new window instead of reusing one.
		// Simulated by leaving a FILE at the socket path, which is what a
		// killed process leaves behind. QLocalServer::close() would not do:
		// it removes the file, so closing a server cleanly proves nothing
		// about a crash - the first version of this check did exactly that
		// and the mutation removing removeServer() went uncaught.
		const QString strStale = strTestName + QStringLiteral("-stale");
		const QString strStalePath = QDir::tempPath() + QLatin1Char('/') + strStale;
		QFile debris(strStalePath);
		Require(debris.open(QIODevice::WriteOnly),
			QStringLiteral("instance: left debris at the socket path"));
		debris.close();
		Require(QFile::exists(strStalePath),
			QStringLiteral("instance: which is really there, so the check can fail"));
		{
			// Without removeServer this fails with AddressInUseError, and every
			// launch after a crash would open a new window instead of reusing
			// the running editor.
			QLocalServer blocked;
			Require(!blocked.listen(strStale),
				QStringLiteral("instance: and a plain listen() is genuinely blocked by it"));
		}
		CSingleInstance revived(nullptr, strStale);
		Require(revived.Listen(),
			QStringLiteral("instance: a later launch claims a socket left by a crash"));

		// THE DECISION IS SERIALISED. Connect-then-listen is only safe under a
		// lock: without it, several launches all fail HandOff (nobody is
		// listening YET) and then race into Listen(), where each one's
		// removeServer() unlinks the last winner's live socket. Measured
		// before the fix: SIX simultaneous launches left FOUR windows.
		{
			const QString strRace = strTestName + QStringLiteral("-race");
			QFile::remove(CSingleInstance::LockPath(strRace));
			QLocalServer::removeServer(strRace);

			CSingleInstance first(nullptr, strRace);
			bool bFirstIsServer = false;
			Require(!first.TakeOverOrHandOff(QStringList(), bFirstIsServer),
				QStringLiteral("instance: the first launch does not hand off"));
			Require(bFirstIsServer,
				QStringLiteral("instance: it becomes the server instead"));

			// A second launch must hand off, NOT become a second server - and
			// must not unlink the first one's socket on the way.
			CSingleInstance second(nullptr, strRace);
			bool bSecondIsServer = false;
			Require(second.TakeOverOrHandOff(QStringList(), bSecondIsServer),
				QStringLiteral("instance: the second launch hands off"));
			Require(!bSecondIsServer,
				QStringLiteral("instance: and does NOT become a second server"));

			// The first one's socket is still live afterwards, which is the
			// property removeServer destroyed when it ran unserialised.
			QLocalSocket probe;
			probe.connectToServer(strRace);
			Require(probe.waitForConnected(500),
				QStringLiteral("instance: and the first one's socket still answers"));
			probe.abort();

			// THE LOCK IS ACTUALLY CONSULTED. The two launches above run one
			// after the other, so they never contend and a mutation removing
			// the lock passes them both - measured. Holding the lock here
			// makes the next call wait for it, which nothing else would.
			//
			// The real race needs concurrent PROCESSES and is verified outside
			// this suite: six simultaneous launches left four windows before
			// the fix and one after, three runs running.
			{
				const QString strLocked = strTestName + QStringLiteral("-locked");
				QFile::remove(CSingleInstance::LockPath(strLocked));
				QLocalServer::removeServer(strLocked);
				QLockFile held(CSingleInstance::LockPath(strLocked));
				Require(held.tryLock(1000),
					QStringLiteral("instance: the test holds the lock"));

				CSingleInstance contender(nullptr, strLocked);
				bool bContenderIsServer = false;
				QElapsedTimer waited;
				waited.start();
				contender.TakeOverOrHandOff(QStringList(), bContenderIsServer);
				Require(waited.elapsed() >= 1500,
					QStringLiteral("instance: a launch WAITS for the lock before deciding "
						"(waited %1ms)").arg(waited.elapsed()));
				held.unlock();
			}
		}

		// A CONNECTION THAT SAYS NOTHING must not block the GUI thread. The
		// read is signal-driven with a deadline; a blocking waitForReadyRead
		// let any local process freeze the editor for a second by connecting
		// and staying silent. Found in review.
		{
			received.clear();
			bGotSignal = false;
			QLocalSocket silent;
			silent.connectToServer(strTestName);
			Require(silent.waitForConnected(500),
				QStringLiteral("instance: a silent client connects"));
			// Waited on ELAPSED TIME, not iterations. processEvents returns
			// immediately when the queue is empty, so a fixed loop count spins
			// through in far less than the one-second deadline and the check
			// fails for the wrong reason - which it did.
			QElapsedTimer elapsed;
			elapsed.start();
			while (!bGotSignal && elapsed.elapsed() < 4000)
			{
				QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
				QThread::msleep(10);
			}
			Require(bGotSignal,
				QStringLiteral("instance: silence is treated as 'come to the front'"));
			Require(received.isEmpty(),
				QStringLiteral("instance: with no files, got %1").arg(received.size()));
			silent.abort();
		}
	}

	//----------------------------------------------------------------------
	// The eight editor settings. Each is asserted as a BEHAVIOUR reaching
	// Scintilla, not as a value stored - a control whose setting nothing
	// reads is exactly what PR #45 refused to build. See doc/PORTING.md 6r.
	//----------------------------------------------------------------------
	{
		CEditorWidget* pCfg = GetCurrentEditor();
		Require(pCfg != nullptr, QStringLiteral("settings: got a document"));
		if (pCfg != nullptr)
		{
			const Core::CAppSettings& settings = m_Data.GetSettings();

			// Asserted against the SETTINGS OBJECT, not against literals, so
			// these hold whether the run has a settings file or the shipped
			// defaults - the pattern §6m established.
			const int nExpectedTab = settings.UseCustomTabSettings()
				? settings.EditorTabWidth() : 4;
			Require(pCfg->Send(SCI_GETTABWIDTH) == nExpectedTab,
				QStringLiteral("settings: tab width is %1, got %2")
					.arg(nExpectedTab).arg(pCfg->Send(SCI_GETTABWIDTH)));

			// THE GATE, driven both ways. At the defaults UseCustomTabSettings
			// is false and EditorTabWidth is 4, so the branch above computes 4
			// either way and cannot tell whether the gate is consulted - a
			// mutation ignoring it went straight through. These make the two
			// disagree.
			{
				QString strIgnored;
				Core::CAppSettings gated = settings;
				gated.SetEditorTabWidth(8);
				gated.SetUseCustomTabSettings(false);
				m_Data.ApplySettings(gated, strIgnored);
				pCfg->ApplySettings();
				Require(pCfg->Send(SCI_GETTABWIDTH) == 4,
					QStringLiteral("settings: with the gate OFF a stored 8 is ignored, "
						"got %1").arg(pCfg->Send(SCI_GETTABWIDTH)));
				gated.SetUseCustomTabSettings(true);
				m_Data.ApplySettings(gated, strIgnored);
				pCfg->ApplySettings();
				Require(pCfg->Send(SCI_GETTABWIDTH) == 8,
					QStringLiteral("settings: and with it ON the 8 is used, got %1")
						.arg(pCfg->Send(SCI_GETTABWIDTH)));
				m_Data.ApplySettings(settings, strIgnored);
				pCfg->ApplySettings();
			}
			Require((pCfg->Send(SCI_GETUSETABS) != 0) == settings.ProcessIndentationTab(),
				QStringLiteral("settings: tabs-versus-spaces follows the setting"));
			Require(pCfg->Send(SCI_GETZOOM) == settings.EditorZoomFactor(),
				QStringLiteral("settings: zoom is %1, got %2")
					.arg(settings.EditorZoomFactor()).arg(pCfg->Send(SCI_GETZOOM)));
			// Scintilla has no boolean for this: a period of 0 IS "no blink".
			Require((pCfg->Send(SCI_GETCARETPERIOD) != 0) == settings.EnableCaretBlink(),
				QStringLiteral("settings: caret blink maps to a period, got %1")
					.arg(pCfg->Send(SCI_GETCARETPERIOD)));
			Require((pCfg->Send(SCI_GETMULTIPLESELECTION) != 0)
					== settings.EnableMultipleCursor(),
				QStringLiteral("settings: multiple cursors follow the setting"));
			Require((pCfg->Send(SCI_GETADDITIONALSELECTIONTYPING) != 0)
					== settings.EnableMultipleCursor(),
				QStringLiteral("settings: and typing into them, which is what makes them "
					"useful rather than decorative"));

			// THE FONT SURVIVES A THEME SWITCH. This is the check that would
			// have caught the bug this section shipped with: ApplyEditorStyles
			// set STYLE_DEFAULT from QFontDatabase and then SCI_STYLECLEARALL,
			// and ReapplySettings runs ApplySettings() before ApplyTheme() -
			// so every theme change threw the user's font away.
			auto FontOf = [pCfg]
			{
				char szName[128] = { 0 };
				pCfg->Send(SCI_STYLEGETFONT, STYLE_DEFAULT,
					reinterpret_cast<sptr_t>(szName));
				return QString::fromUtf8(szName);
			};
			const QString strWanted =
				QString::fromStdString(settings.EditorFontName());
			Require(FontOf() == strWanted,
				QStringLiteral("settings: the editor font is '%1', got '%2'")
					.arg(strWanted, FontOf()));
			Require(pCfg->Send(SCI_STYLEGETSIZE, STYLE_DEFAULT)
					== settings.EditorFontPointSize(),
				QStringLiteral("settings: at %1 point, got %2")
					.arg(settings.EditorFontPointSize())
					.arg(pCfg->Send(SCI_STYLEGETSIZE, STYLE_DEFAULT)));

			OnSetTheme(EEditorTheme::Light);
			Require(FontOf() == strWanted,
				QStringLiteral("settings: AND IT SURVIVES A THEME SWITCH, got '%1'")
					.arg(FontOf()));
			OnSetTheme(EEditorTheme::Dark);
			Require(FontOf() == strWanted,
				QStringLiteral("settings: and switching back, got '%1'").arg(FontOf()));

			// ReapplySettings is the path Preferences uses, and it runs
			// ApplySettings then ApplyTheme - the exact order that lost the
			// font. Driven here rather than assumed.
			ReapplySettings();
			Require(FontOf() == strWanted,
				QStringLiteral("settings: and a full re-apply, which is what Preferences "
					"does, got '%1'").arg(FontOf()));

			// AUTO-NEWLINE-AT-EOF, on the SAVE PATH, so it gets the same care
			// as everything else that writes bytes. It ships OFF, which is why
			// the byte-identical fixtures above are untouched by it - and the
			// check drives BOTH states rather than trusting the default.
			{
				QTemporaryDir eofDir;
				Require(eofDir.isValid(), QStringLiteral("settings: got a directory"));
				const QString strPath = eofDir.filePath(QStringLiteral("eof.txt"));
				auto Bytes = [&strPath]
				{
					QFile f(strPath);
					return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
				};

				CEditorWidget* pEof = NewUntitled();
				Require(pEof != nullptr, QStringLiteral("settings: got a document"));
				pEof->Send(SCI_SETEOLMODE, SC_EOL_LF);
				pEof->Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("no newline"));

				Core::CAppSettings off = m_Data.GetSettings();
				off.SetAutoAddNewLineAtEof(false);
				QString strIgnored;
				m_Data.ApplySettings(off, strIgnored);
				QString strErr;
				Require(pEof->SaveFile(strPath, strErr),
					QStringLiteral("settings: saved with the setting off"));
				Require(Bytes() == QByteArray("no newline"),
					QStringLiteral("settings: which added NOTHING, got '%1'")
						.arg(QString::fromUtf8(Bytes())));

				Core::CAppSettings on = m_Data.GetSettings();
				on.SetAutoAddNewLineAtEof(true);
				m_Data.ApplySettings(on, strIgnored);
				Require(pEof->SaveFile(strPath, strErr),
					QStringLiteral("settings: saved with it on"));
				Require(Bytes() == QByteArray("no newline\n"),
					QStringLiteral("settings: which added exactly one, got '%1'")
						.arg(QString::fromUtf8(Bytes())));
				// AND NOT A SECOND ONE. Saving the same document twice does
				// NOT test this - SaveFile never changes the document, so the
				// second save sees the same newline-less text and appends once
				// again, giving the same bytes. The mutation removing the guard
				// went straight through. The text itself has to end in one.
				pEof->Send(SCI_SETTEXT, 0,
					reinterpret_cast<sptr_t>("already ends\n"));
				Require(pEof->SaveFile(strPath, strErr),
					QStringLiteral("settings: saved text that already ends in a newline"));
				Require(Bytes() == QByteArray("already ends\n"),
					QStringLiteral("settings: and did NOT add another, got '%1'")
						.arg(QString::fromUtf8(Bytes())));

				// A CR-ONLY DOCUMENT. Its lines end in a bare \r, which
				// endsWith('\n') never matches - so the guard above missed it
				// entirely and every save appended another terminator. Found
				// in review, and it is this PR's own "New files use: CR
				// (classic Mac)" option that makes the state reachable.
				pEof->Send(SCI_SETEOLMODE, SC_EOL_CR);
				pEof->Send(SCI_SETTEXT, 0,
					reinterpret_cast<sptr_t>("classic mac\r"));
				m_Data.ApplySettings(on, strIgnored);
				Require(pEof->SaveFile(strPath, strErr),
					QStringLiteral("settings: saved a CR-terminated document"));
				Require(Bytes() == QByteArray("classic mac\r"),
					QStringLiteral("settings: a bare CR counts as terminated, got '%1'")
						.arg(QString::fromUtf8(Bytes().toHex(' '))));
				pEof->Send(SCI_SETEOLMODE, SC_EOL_LF);

				m_Data.ApplySettings(off, strIgnored);
				pEof->Send(SCI_SETSAVEPOINT);
				OnCloseTab(m_pTabs->indexOf(pEof));
			}

			// THE DEFAULT EOL applies to a NEW document only. A loaded file's
			// ending comes from its bytes, and overriding that would silently
			// re-end every line the next time it was saved.
			{
				Core::CAppSettings eol = m_Data.GetSettings();
				eol.SetDefaultFileEol(1);			// SC_EOL_CR, which nothing else uses
				QString strIgnored;
				m_Data.ApplySettings(eol, strIgnored);

				CEditorWidget* pNew = NewUntitled();
				Require(pNew != nullptr && pNew->Send(SCI_GETEOLMODE) == SC_EOL_CR,
					QStringLiteral("settings: a new document takes the default EOL, got %1")
						.arg(pNew == nullptr ? -1 : (int)pNew->Send(SCI_GETEOLMODE)));

				// A LOADED file keeps its own, whatever the default says.
				CEditorWidget* pLoaded =
					qobject_cast<CEditorWidget*>(m_pTabs->widget(0));
				Require(pLoaded != nullptr, QStringLiteral("settings: got a loaded tab"));
				if (pLoaded != nullptr)
				{
					pLoaded->ApplySettings();
					Require(pLoaded->Send(SCI_GETEOLMODE) != SC_EOL_CR,
						QStringLiteral("settings: a LOADED document keeps the ending its "
							"bytes had, not the default"));
				}
				if (pNew != nullptr)
				{
					pNew->Send(SCI_SETSAVEPOINT);
					OnCloseTab(m_pTabs->indexOf(pNew));
				}
				m_Data.ApplySettings(settings, strIgnored);
			}
		}
	}

	//----------------------------------------------------------------------
	// Regex presets - the one part of FindDlg that is not find-in-files.
	// See doc/PORTING.md 6s.
	//
	// EVERY PRESET IS RUN THROUGH THE EDITOR'S OWN SEARCH. Offering a pattern
	// is a promise that it works here, and both frontends use plain
	// SCFIND_REGEXP - which has no lookahead, no named groups and no .NET
	// character-class subtraction, all of which the originals use.
	//----------------------------------------------------------------------
	{
		Require(!RegexPresets::All().empty(),
			QStringLiteral("regex: there are presets"));

		CEditorWidget* pRe = NewUntitled();
		Require(pRe != nullptr, QStringLiteral("regex: got a scratch document"));
		if (pRe != nullptr)
		{
			int nBroken = 0;
			QString strFirstBroken;
			for (const RegexPresets::SPreset& preset : RegexPresets::All())
			{
				Require(preset._Sample != nullptr && *preset._Sample != '\0',
					QStringLiteral("regex: '%1' carries a sample to match")
						.arg(QLatin1String(preset._Label)));
				const QByteArray sample(preset._Sample);
				pRe->Send(SCI_SETTEXT, 0,
					reinterpret_cast<sptr_t>(sample.constData()));
				CEditorWidget::SFindOptions options;
				options._Regex = true;
				if (pRe->HighlightMatches(QString::fromUtf8(preset._Pattern), options) < 1)
				{
					++nBroken;
					// ALL of them, not just the first - a list that names one
					// failure hides however many follow it, and the point of
					// this check is to find every preset the engine cannot run.
					if (!strFirstBroken.isEmpty()) { strFirstBroken += QStringLiteral("; "); }
					strFirstBroken += QStringLiteral("%1 (%2)")
						.arg(QLatin1String(preset._Label),
							QLatin1String(preset._Pattern));
				}
				pRe->ClearHighlight();
			}
			Require(nBroken == 0,
				QStringLiteral("regex: every offered preset matches its own sample in "
					"THIS editor's engine; %1 did not: %2")
					.arg(nBroken).arg(strFirstBroken));

			pRe->Send(SCI_SETSAVEPOINT);
			OnCloseTab(m_pTabs->indexOf(pRe));
		}

		// AND THE BAR OFFERS THEM, only in regex mode.
		m_pFindBar->Activate(QString());
		m_pFindBar->SetReplaceVisible(false);
		Require(!m_pFindBar->IsRegexHelpVisible(),
			QStringLiteral("regex: the helper is hidden while the search is plain text"));

		// The checkbox is what reveals it, driven through the same signal a
		// click produces.
		Require(m_pFindBar->IsRegex() == false,
			QStringLiteral("regex: the bar starts in plain-text mode"));
		m_pFindBar->SetRegex(true);
		Require(m_pFindBar->IsRegexHelpVisible(),
			QStringLiteral("regex: turning regex on reveals the helper"));

		// INSERTS AT THE CARET rather than replacing the box. The MFC's
		// SetSearchFields overwrites the whole field, discarding whatever was
		// being typed.
		m_pFindBar->Activate(QString());
		m_pFindBar->SetRegex(true);
		m_pFindBar->TypePatternForTest(QStringLiteral("abc"));
		Require(m_pFindBar->InsertPresetForTest(0),
			QStringLiteral("regex: inserted the first preset"));
		Require(m_pFindBar->GetPattern().startsWith(QStringLiteral("abc")),
			QStringLiteral("regex: and what was already typed SURVIVED, got '%1'")
				.arg(m_pFindBar->GetPattern()));
		Require(m_pFindBar->GetPattern() != QStringLiteral("abc"),
			QStringLiteral("regex: while the preset was actually added, got '%1'")
				.arg(m_pFindBar->GetPattern()));

		// THE OTHER HALF, and it is deliberate rather than an oversight:
		// Cmd+F prefills the box with the selection and SELECTS it, so that
		// typing replaces. A preset picked in that state replaces too - the
		// same rule QLineEdit applies to a paste. Pinned so nobody "fixes" the
		// insert into never replacing.
		m_pFindBar->Activate(QStringLiteral("abc"));
		m_pFindBar->SetRegex(true);
		Require(m_pFindBar->InsertPresetForTest(0),
			QStringLiteral("regex: inserted over the preselected pattern"));
		Require(!m_pFindBar->GetPattern().contains(QStringLiteral("abc")),
			QStringLiteral("regex: a PRESELECTED pattern is replaced, got '%1'")
				.arg(m_pFindBar->GetPattern()));

		m_pFindBar->SetRegex(false);
		OnHideFind();
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
		// EVERY sequence an action answers to, not just shortcut(), which
		// returns the primary one. An action can carry several - Find Next
		// carries two on macOS, Preferences carries two everywhere - and the
		// single-shortcut version of this loop was blind to all but the first.
		// It therefore could not see the very defect it exists to catch: a
		// SECONDARY binding landing on a reserved or already-taken key would
		// have passed silently.
		for (QAction* pAction : menuBar()->findChildren<QAction*>())
		{
			for (const QKeySequence& key : pAction->shortcuts())
			{
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
		}
		// EVERY ACTION MUST HAVE A BINDING THAT ACTUALLY ARRIVES - ON macOS.
		//
		// SCOPED TO macOS, because the premise is a macOS hardware behaviour
		// and not a universal one. Linux CI failed the first version on
		// 'Find Next', which legitimately holds F3 alone there: Go to Line owns
		// Ctrl+G on Linux (§6m), the filter removes it from Find Next, and a
		// bare F3 is perfectly reachable on a PC keyboard. The check was right
		// about the binding and wrong about the platform.
		//
		// On macOS the function keys are brightness, Mission Control and the
		// rest by default, so a bare F-key never reaches the application unless
		// the user has changed a system setting. An action whose ONLY binding
		// is a bare function key is therefore unreachable for most people - and
		// this has now happened twice: Find Next on F3 alone (fixed in #47) and
		// the bookmark commands on F2/Shift+F2 alone, which shipped in the
		// first version of this very PR and was caught by a person pressing it.
		//
		// The rule is general, so the check is too: at least one sequence per
		// action must carry a modifier that is not Shift. It would have caught
		// both instances, and it is the third time this port has learned that a
		// shortcut which RESOLVES is not a shortcut that ARRIVES.
#ifdef Q_OS_MACOS
		for (QAction* pAction : menuBar()->findChildren<QAction*>())
		{
			if (pAction->shortcuts().isEmpty())
			{
				continue;
			}
			// Items macOS merges into the application menu are exempt: the
			// platform supplies their shortcut, which is exactly why Qt
			// resolves QKeySequence::Quit to Qt::Key_Exit rather than to a real
			// sequence. Exempted on the ROLE, which is the actual mechanism,
			// rather than on the text - the duplicate-shortcut check below
			// already learned that matching "Exit" fails on "E&xit".
			const QAction::MenuRole role = pAction->menuRole();
			if (role == QAction::QuitRole || role == QAction::PreferencesRole
				|| role == QAction::AboutRole || role == QAction::AboutQtRole)
			{
				continue;
			}

			bool bReachable = false;
			for (const QKeySequence& key : pAction->shortcuts())
			{
				if (key.isEmpty())
				{
					continue;
				}
				const int nModifiers = key[0].keyboardModifiers();
				// Shift alone does not rescue a function key: Shift+F3 needs Fn
				// exactly as F3 does.
				if ((nModifiers & ~Qt::ShiftModifier) != 0)
				{
					bReachable = true;
				}
			}
			Require(bReachable,
				QStringLiteral("shortcut: '%1' has a binding that does not need a function "
					"key, so it can be reached on a Mac")
					.arg(pAction->text()));
		}
#endif

		Require(nWithShortcut >= 8,
			QStringLiteral("shortcut: found %1 bound actions to check").arg(nWithShortcut));

		// An action on a standard key must answer to EVERY binding Qt lists for
		// it, not just the first. This is what shipped wrong: Find Next was
		// given QKeySequence::FindNext through the single-sequence overload, so
		// on macOS it took F3 and left Cmd+G - which Qt lists, and which is the
		// platform convention - bound to nothing.
		//
		// Written against keyBindings() rather than against "F3 and Cmd+G", so
		// it states the rule on every platform instead of encoding one
		// platform's answer.
		const struct { const char* _Name; QKeySequence::StandardKey _Key; } standard[] = {
			{ "Find Next", QKeySequence::FindNext },
			{ "Find Previous", QKeySequence::FindPrevious },
		};
		for (const auto& entry : standard)
		{
			QAction* pFound = nullptr;
			for (QAction* pAction : menuBar()->findChildren<QAction*>())
			{
				QString strPlain = pAction->text();
				strPlain.remove(QLatin1Char('&'));
				if (strPlain == QLatin1String(entry._Name))
				{
					pFound = pAction;
				}
			}
			Require(pFound != nullptr,
				QStringLiteral("shortcut: found the '%1' action")
					.arg(QLatin1String(entry._Name)));
			if (pFound != nullptr)
			{
				// The rule is not "every binding is installed" - that was the
				// first version and Linux failed it, correctly. Qt lists Ctrl+G
				// for Find Next there, and Go to Line deliberately owns Ctrl+G
				// on every platform but macOS.
				//
				// The real invariant is that no binding Qt lists is left doing
				// NOTHING: each is either installed on this action or claimed by
				// another one. That is what the shipped defect violated - Cmd+G
				// was neither.
				for (const QKeySequence& key : QKeySequence::keyBindings(entry._Key))
				{
					if (pFound->shortcuts().contains(key))
					{
						continue;
					}
					QString strOwner;
					for (QAction* pOther : menuBar()->findChildren<QAction*>())
					{
						if (pOther != pFound && pOther->shortcuts().contains(key))
						{
							strOwner = pOther->text();
						}
					}
					Require(!strOwner.isEmpty(),
						QStringLiteral("shortcut: Qt lists %1 for '%2', which neither has it "
							"nor any other action - so the key does nothing")
							.arg(key.toString(QKeySequence::NativeText),
								QLatin1String(entry._Name)));
				}
			}
		}

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

	//----------------------------------------------------------------------
	// Preferences. The dialog must round-trip untouched settings unchanged -
	// otherwise opening it and pressing OK silently rewrites the file - and an
	// applied change must reach an open document.
	//----------------------------------------------------------------------
	{
		const Core::CAppSettings before = m_Data.GetSettings();
		{
			CPreferencesDialog dialog(before, this);
			const Core::CAppSettings after = dialog.GetSettings();
			Require(after.EnableUrlHighlight() == before.EnableUrlHighlight()
				&& after.EnableAutoComplete() == before.EnableAutoComplete()
				&& after.AutoCompleteIgnoreCase() == before.AutoCompleteIgnoreCase()
				&& after.AutoCompleteIgnoreNumbers() == before.AutoCompleteIgnoreNumbers()
				&& after.DrawCaretLineFrame() == before.DrawCaretLineFrame()
				&& after.DrawFoldingLineUnderLineStyle()
					== before.DrawFoldingLineUnderLineStyle()
				&& after.EnableHighlightFolder() == before.EnableHighlightFolder()
				&& after.UseFolderMarginClassic() == before.UseFolderMarginClassic()
				&& after.FolderMarginStyle() == before.FolderMarginStyle()
				&& after.LongLineColumnLimit() == before.LongLineColumnLimit(),
				QStringLiteral("preferences: opening and closing changes nothing"));
		}

		// Values OUTSIDE what the widgets can show must survive untouched. A
		// spin box range and a combo's item count are guides for new input, not
		// a licence to rewrite existing data - and the shared settings file may
		// hold anything the MFC put there.
		//
		// The combo case is the dangerous one: an out-of-range index leaves it
		// at -1, and writing -1 back would break the WINDOWS build, whose
		// four-way FOLDER_MARGIN_STYPE chain then matches nothing and defines
		// no fold markers at all.
		{
			Core::CAppSettings odd = before;
			odd.SetLongLineColumnLimit(2000);		// beyond the spin box maximum
			odd.SetFolderMarginStyle(7);			// beyond the combo's four items
			CPreferencesDialog dialog(odd, this);
			const Core::CAppSettings out = dialog.GetSettings();
			Require(out.LongLineColumnLimit() == 2000,
				QStringLiteral("preferences: an out-of-range column survives OK, got %1")
					.arg(out.LongLineColumnLimit()));
			Require(out.FolderMarginStyle() == 7,
				QStringLiteral("preferences: an unknown margin style survives OK, got %1")
					.arg(out.FolderMarginStyle()));
		}

		// THE SAME HAZARD, for the eight settings added alongside the editor
		// behaviours. The font is the one that bites: a settings file written
		// on Windows names a family this machine may not have, and a combo box
		// built from the local font list would quietly replace it - through the
		// file both frontends share, so the WINDOWS user's font would change
		// because someone opened Preferences on a Mac.
		{
			Core::CAppSettings odd = before;
			odd.SetEditorFontName("A Font This Machine Does Not Have");
			odd.SetEditorFontPointSize(9);
			odd.SetEditorTabWidth(3);
			odd.SetUseCustomTabSettings(true);
			odd.SetProcessIndentationTab(false);
			odd.SetEditorZoomFactor(2);
			odd.SetEnableCaretBlink(true);
			odd.SetEnableMultipleCursor(false);
			odd.SetDefaultFileEol(2);
			odd.SetAutoAddNewLineAtEof(true);

			CPreferencesDialog dialog(odd, this);
			const Core::CAppSettings out = dialog.GetSettings();
			Require(out.EditorFontName() == "A Font This Machine Does Not Have",
				QStringLiteral("preferences: a font this machine lacks SURVIVES, got '%1'")
					.arg(QString::fromStdString(out.EditorFontName())));
			Require(out.EditorFontPointSize() == 9 && out.EditorTabWidth() == 3
					&& out.UseCustomTabSettings() && !out.ProcessIndentationTab()
					&& out.EditorZoomFactor() == 2 && out.EnableCaretBlink()
					&& !out.EnableMultipleCursor() && out.DefaultFileEol() == 2
					&& out.AutoAddNewLineAtEof(),
				QStringLiteral("preferences: and every other editor setting round-trips "
					"untouched"));

			// THE SPIN BOXES CLAMP, and that is the same #45 bug the combos
			// above are guarded against. QSpinBox silently pulls setValue
			// inside its range, so a fixed range rewrites a stored value the
			// moment Preferences is opened and OK'd - even untouched. Found in
			// review: the guard was applied to the combos and not to these.
			{
				Core::CAppSettings wide = before;
				wide.SetEditorFontPointSize(200);	// beyond a 6-72 box
				wide.SetEditorTabWidth(64);			// beyond a 1-16 box
				wide.SetEditorZoomFactor(-40);		// beyond a -10..20 box
				CPreferencesDialog wideDialog(wide, this);
				const Core::CAppSettings kept = wideDialog.GetSettings();
				Require(kept.EditorFontPointSize() == 200,
					QStringLiteral("preferences: an out-of-range font size survives OK, "
						"got %1").arg(kept.EditorFontPointSize()));
				Require(kept.EditorTabWidth() == 64,
					QStringLiteral("preferences: an out-of-range tab width survives OK, "
						"got %1").arg(kept.EditorTabWidth()));
				Require(kept.EditorZoomFactor() == -40,
					QStringLiteral("preferences: an out-of-range zoom survives OK, got %1")
						.arg(kept.EditorZoomFactor()));
			}

			// An out-of-range EOL leaves the combo at -1, and writing that back
			// would give the Windows build a line ending it cannot map.
			Core::CAppSettings badEol = before;
			badEol.SetDefaultFileEol(9);
			CPreferencesDialog eolDialog(badEol, this);
			Require(eolDialog.GetSettings().DefaultFileEol() == 9,
				QStringLiteral("preferences: an unknown default EOL survives OK, got %1")
					.arg(eolDialog.GetSettings().DefaultFileEol()));
		}

		// An applied change must reach an open document, not merely the file.
		CEditorWidget* pEditor = GetCurrentEditor();
		if (pEditor != nullptr)
		{
			Core::CAppSettings changed = before;
			changed.SetLongLineColumnLimit(37);
			QString strSaveError;
			m_Data.ApplySettings(changed, strSaveError);	// may fail to write; fine
			ReapplySettings();
			Require(pEditor->Send(SCI_GETEDGECOLUMN) == 37,
				QStringLiteral("preferences: an applied change reaches an open editor, "
					"got %1").arg(pEditor->Send(SCI_GETEDGECOLUMN)));

			// Put it back, so nothing after this sees a modified configuration.
			m_Data.ApplySettings(before, strSaveError);
			ReapplySettings();
			Require(pEditor->Send(SCI_GETEDGECOLUMN) == before.LongLineColumnLimit(),
				QStringLiteral("preferences: and restoring it takes effect too"));
		}
	}

	// The corpus has to REACH the interesting path. A suite where every file had
	// exactly one match would pass every check above by never running one.
	Require(nWalkChecked > 0,
		QStringLiteral("at least one file had 2+ matches to walk (%1 did)")
			.arg(nWalkChecked));
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
		// IN REGEX MODE, so the preset list's button is in the picture. It is
		// hidden in plain-text mode by design, so a screenshot of the default
		// bar would show nothing of this feature at all.
		m_pFindBar->SetRegex(true);
		OnPatternChanged();
		// AND STANDING ON A MATCH, so the picture shows the difference between
		// the one you are on and the rest. Without this the shot renders the
		// highlight-all state only - which is the state in which this looked
		// correct while being unusable.
		OnFind(false);
		// And the goto bar below it, for the same reason: D10 asks for a
		// screenshot of the shallow visible tail, and a feature that never
		// appears in one has not been shown to anybody. Both bars at once is
		// also the arrangement the MFC cannot produce - they are pages of one
		// tab control there - so the picture doubles as the evidence for that
		// divergence.
		OnShowGoto();
	}

	// Bookmarks in the picture: a few markers in the margin and the pane
	// showing them. A marker margin nobody can see in a screenshot is the
	// thing this change exists to fix.
	if (CEditorWidget* pShot = GetCurrentEditor())
	{
		pShot->ToggleBookmark(3);
		pShot->ToggleBookmark(9);
		pShot->ToggleBookmark(17);
		RefreshBookmarks();
		m_pBookmarkPane->show();
		m_pBookmarkPane->raise();
	}

	show();
	QApplication::processEvents();

	// The window manager, in its own picture. It is MODAL, so it cannot appear
	// in the main shot - and D10 asks for a screenshot of the shallow visible
	// tail, which a feature that never appears in one has not had.
	{
		CWindowListDialog windows(this);
		windows.SetEntries(CollectWindowList());
		windows.show();
		QApplication::processEvents();
		const QString strWindows = QStringLiteral("%1/vinatext-windows.png").arg(strDirectory);
		if (windows.grab().save(strWindows))
		{
			qInfo("wrote %s", qPrintable(strWindows));
		}
		else
		{
			qWarning("could not write %s", qPrintable(strWindows));
		}
		windows.close();
	}

	int nFailures = 0;

	// The preset list, in its own picture, for the same reason: it is a POPUP,
	// so it cannot appear in the main shot either, and the whole feature is the
	// list. It is also the evidence for a review finding - the pattern sits in
	// QMenu's shortcut column because the label carries a tab, and the two
	// renderings were compared here before keeping it (PORTING.md 6s).
	{
		m_pFindBar->Activate(QString());
		m_pFindBar->SetRegex(true);
		QMenu* pPresets = m_pFindBar->PresetMenu();
		pPresets->popup(QPoint(0, 0));
		QApplication::processEvents();
		const QString strPresets = QStringLiteral("%1/vinatext-regex-presets.png")
			.arg(strDirectory);
		if (pPresets->grab().save(strPresets))
		{
			qInfo("wrote %s", qPrintable(strPresets));
		}
		else
		{
			qWarning("could not write %s", qPrintable(strPresets));
		}
		// Hidden again before the main shots: a popup left open would sit on top
		// of the window in both of them.
		pPresets->hide();
		QApplication::processEvents();
		// CHECKED, not assumed. A popup still up would sit on top of the window
		// in both theme shots below, and the shots would still be "written" -
		// this is the failure that reports itself instead of shipping a picture
		// of a menu covering the editor.
		if (pPresets->isVisible())
		{
			qWarning("the preset popup is still up; the theme shots would be wrong");
			++nFailures;
		}
	}

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
