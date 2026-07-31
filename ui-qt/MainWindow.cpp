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
	connect(m_pFindBar, &CFindBar::PatternChanged, this, &CMainWindow::OnPatternChanged);
	connect(m_pFindBar, &CFindBar::CloseRequested, this, &CMainWindow::OnHideFind);

	resize(1100, 750);
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
		return false;
	}
	m_strLastDirectory = QFileInfo(strPath).absolutePath();
	UpdateTabLabel(pEditor);
	UpdateStatusBar();
	UpdateWindowTitle();
	statusBar()->showMessage(tr("Saved %1").arg(strPath), 3000);
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

void CMainWindow::OnShowFind()
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
	m_pFindBar->Activate(strSelected);
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

	Require(nFoldClicksChecked > 0,
		QStringLiteral("the fold-margin click was exercised on at least one file"));

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
