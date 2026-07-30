/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Phase 3 spike: the smallest thing that proves the port's riskiest assumption.
//
// This is NOT the Qt frontend. There are no dock widgets, no tabs, no dialogs,
// no menus beyond File. It exists to answer one question that has been open
// since the brief was written and never tested:
//
//     Does upstream Scintilla's qt/ScintillaEditBase (D2) actually work for
//     this project, on a platform that is not Windows?
//
// What it does prove, when it runs:
//   - Scintilla and Lexilla build from source on macOS/Linux via CMake
//   - ScintillaEditBase hosts and renders in a QMainWindow
//   - the SCI_* message API is reachable the same way ui-mfc/ uses it
//   - core/ links into a Qt binary, and the theme/language JSON extracted in
//     PR #3 drives real syntax highlighting outside the MFC app

#include "LanguageData.h"        // core/

// Order matters: Lexilla.h uses Scintilla::ILexer5 without declaring it, so
// ILexer.h has to come first. Neither header is self-contained.
#include <ScintillaEditBase.h>
#include <Scintilla.h>
#include <SciLexer.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextStream>

#include <set>
#include <string>

namespace
{
	// Where the JSON extracted in PR #3 lives, relative to the source tree.
	// The MFC app resolves this next to the executable via
	// PathUtils::GetVinaTextPackagePath(); a spike run from the build directory
	// cannot, so the path is passed in or defaulted.
	QString DefaultDataDir()
	{
		return QStringLiteral(VINATEXT_DATA_DIR);
	}

	long SendScintilla(ScintillaEditBase* pEditor, unsigned int iMessage,
		uptr_t wParam = 0, sptr_t lParam = 0)
	{
		return static_cast<long>(pEditor->send(iMessage, wParam, lParam));
	}

	// COLORREF ordering is 0x00BBGGRR, which is what Scintilla expects - the same
	// convention ui-mfc/ already relies on.
	sptr_t ToScintillaColour(const Core::SColor& c)
	{
		return static_cast<sptr_t>((c._Blue << 16) | (c._Green << 8) | c._Red);
	}
}

class CSpikeWindow final : public QMainWindow
{
public:
	explicit CSpikeWindow(const QString& strDataDir)
		: m_strDataDir(strDataDir)
	{
		m_pEditor = new ScintillaEditBase(this);
		setCentralWidget(m_pEditor);
		resize(1100, 750);

		QMenu* pFile = menuBar()->addMenu(tr("&File"));
		pFile->addAction(tr("&Open..."), QKeySequence::Open, this, &CSpikeWindow::OnOpen);
		pFile->addSeparator();
		pFile->addAction(tr("E&xit"), QKeySequence::Quit, qApp, &QApplication::quit);

		LoadEditorData();
	}

	bool LoadFile(const QString& strPath)
	{
		QFile file(strPath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			QMessageBox::warning(this, tr("VinaText"),
				tr("Cannot open %1").arg(strPath));
			return false;
		}
		const QByteArray content = file.readAll();
		SendScintilla(m_pEditor, SCI_SETTEXT, 0,
			reinterpret_cast<sptr_t>(content.constData()));
		SendScintilla(m_pEditor, SCI_EMPTYUNDOBUFFER);

		ApplyLanguageFor(QFileInfo(strPath).suffix());
		setWindowTitle(QStringLiteral("%1 — VinaText Qt spike").arg(QFileInfo(strPath).fileName()));
		return true;
	}

	// Reads SCI_GETSTYLEAT back across the document. If Lexilla ran, the text
	// carries more than one distinct style value; if it did not, every byte is
	// style 0 and this fails.
	bool RunSelfTest()
	{
		const long length = SendScintilla(m_pEditor, SCI_GETLENGTH);
		if (length <= 0)
		{
			qWarning("selftest: document is empty - nothing to lex");
			return false;
		}
		SendScintilla(m_pEditor, SCI_COLOURISE, 0, -1);

		std::set<int> styles;
		for (long i = 0; i < length; ++i)
		{
			styles.insert(static_cast<int>(SendScintilla(m_pEditor, SCI_GETSTYLEAT, i)));
		}

		QStringList seen;
		for (int st : styles)
		{
			seen << QString::number(st);
		}
		qInfo("selftest: %ld bytes, %zu distinct styles [%s]",
			length, styles.size(), qPrintable(seen.join(QStringLiteral(", "))));
		qInfo("selftest: core/ loaded %zu languages, %zu palette entries",
			m_Languages.GetLanguages().size(), m_Theme.GetPalette().size());

		if (styles.size() < 2)
		{
			qWarning("selftest: FAILED - every byte is style %d, so the lexer did not run",
				styles.empty() ? -1 : *styles.begin());
			return false;
		}
		if (m_Languages.GetLanguages().empty() || m_Theme.GetPalette().empty())
		{
			qWarning("selftest: FAILED - core/ data did not load");
			return false;
		}
		qInfo("selftest: PASSED");
		return true;
	}

private:
	void OnOpen()
	{
		const QString strPath = QFileDialog::getOpenFileName(this, tr("Open File"));
		if (!strPath.isEmpty())
		{
			LoadFile(strPath);
		}
	}

	void LoadEditorData()
	{
		std::string strError;
		const std::string strLangs = (m_strDataDir + "/languages.json").toStdString();
		const std::string strTheme = (m_strDataDir + "/theme-dark.json").toStdString();

		if (!m_Languages.LoadFromFile(strLangs, strError))
		{
			statusBar()->showMessage(tr("languages.json: %1").arg(QString::fromStdString(strError)));
			return;
		}
		if (!m_Theme.LoadFromFile(strTheme, strError))
		{
			statusBar()->showMessage(tr("theme-dark.json: %1").arg(QString::fromStdString(strError)));
			return;
		}
		statusBar()->showMessage(tr("core/: %1 languages, %2 palette entries")
			.arg(m_Languages.GetLanguages().size())
			.arg(m_Theme.GetPalette().size()));
	}

	// Drives Scintilla from the JSON extracted in PR #3 - the same data the MFC
	// build now reads. This is the part that proves the extraction was worth
	// doing: identical bytes, two entirely different frontends.
	void ApplyLanguageFor(const QString& strExtension)
	{
		const Core::SLanguageInfo* pLang = nullptr;
		for (const Core::SLanguageInfo& info : m_Languages.GetLanguages())
		{
			if (QString::fromStdString(info._Extension) == strExtension)
			{
				pLang = &info;
				break;
			}
		}
		if (pLang == nullptr)
		{
			return;
		}

		// One CreateLexer call, not two - each allocates an ILexer5 and the
		// document only takes ownership of the one handed to SCI_SETILEXER.
		void* pLexer = CreateLexer(pLang->_Id.c_str());
		if (pLexer == nullptr)
		{
			statusBar()->showMessage(
				tr("Lexilla has no lexer named '%1'").arg(QString::fromStdString(pLang->_Id)));
			return;
		}
		SendScintilla(m_pEditor, SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(pLexer));
		SendScintilla(m_pEditor, SCI_SETKEYWORDS, 0,
			reinterpret_cast<sptr_t>(pLang->_Keywords.c_str()));

		const std::vector<Core::SStyleMapping>* pStyles = m_Theme.FindStyles(pLang->_Id);
		if (pStyles != nullptr)
		{
			for (const Core::SStyleMapping& style : *pStyles)
			{
				Core::SColor colour;
				if (m_Theme.ResolveColor(style._Color, colour))
				{
					SendScintilla(m_pEditor, SCI_STYLESETFORE, style._Value,
						ToScintillaColour(colour));
				}
			}
		}
		statusBar()->showMessage(
			tr("%1 — %2 styles from theme-dark.json")
				.arg(QString::fromStdString(pLang->_Id))
				.arg(pStyles ? pStyles->size() : 0));
	}

	ScintillaEditBase*   m_pEditor = nullptr;
	QString              m_strDataDir;
	Core::CLanguageTable m_Languages;
	Core::CEditorTheme   m_Theme;
};

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("VinaText"));

	QCommandLineParser parser;
	parser.setApplicationDescription(
		QStringLiteral("VinaText Qt spike — proves D2 (upstream Scintilla + Qt) off Windows"));
	parser.addHelpOption();
	QCommandLineOption dataOption(QStringList() << "d" << "data",
		QStringLiteral("Directory holding languages.json and theme-*.json"),
		QStringLiteral("dir"), DefaultDataDir());
	parser.addOption(dataOption);
	QCommandLineOption selfTestOption(QStringLiteral("selftest"),
		QStringLiteral("Load the file, run the lexer, assert it styled the text, then exit"));
	parser.addOption(selfTestOption);
	parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("File to open"));
	parser.process(app);

	CSpikeWindow window(parser.value(dataOption));
	const QStringList args = parser.positionalArguments();
	if (!args.isEmpty())
	{
		window.LoadFile(args.first());
	}

	// Headless verification. "It compiled" is not evidence that Scintilla is
	// lexing anything, and a GUI cannot be asserted on in CI - so this loads a
	// file, runs the lexer, and reads the style bytes back out of the document.
	// Distinct style values mean Lexilla actually classified the text.
	if (parser.isSet(selfTestOption))
	{
		return window.RunSelfTest() ? 0 : 1;
	}

	window.show();
	return app.exec();
}
