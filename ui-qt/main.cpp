/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// VinaText, Qt frontend - entry point.
//
// This was a 318-line spike whose only job was to prove D2: that upstream
// Scintilla's qt/ScintillaEditBase works for this project off Windows. It did,
// and under D9 it is now the alpha shell: MainWindow.cpp holds the window,
// EditorWidget.cpp holds a document, and this file only starts them.
//
// The alpha is fixed to D9's checklist - tabs, open/save, lexer + both themes,
// find, status bar - and to nothing else. No dialogs, no docking, no settings UI.

#include "EditorData.h"
#include "MainWindow.h"

#include <QApplication>
#include "SingleInstance.h"

#include "EditorWidget.h"
#include "ResourcePaths.h"

#include <QCommandLineParser>
#include <QIcon>
#include <QSettings>
#include <QTemporaryDir>
#include <QMessageBox>

namespace
{
	// Where the JSON extracted in PR #3 lives. The MFC app resolves it next to the
	// executable via PathUtils::GetVinaTextPackagePath(); this now does the same
	// where there IS something next to the executable, and falls back to the
	// compiled-in build-tree path otherwise. See ResourcePaths.h - without it
	// nothing that is copied off this machine can find its data. --data still
	// overrides everything.
	QString DefaultDataDir()
	{
		return ResourcePaths::DataDir();
	}
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("VinaText"));
	QCoreApplication::setApplicationVersion(QStringLiteral(VINATEXT_VERSION));
	// THE SAME ICON THE WINDOWS BUILD SHIPS - res/app.ico, which VinaText.rc
	// names as IDR_MAINFRAME - read straight out of the .ico rather than from a
	// PNG converted alongside it, so redrawing one redraws both. Without this
	// the app wears Qt's generic binary icon, which is what a user sees in the
	// Dock before they see anything else.
	QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

	// BEFORE ANY EDITOR EXISTS, because the first one applies its font in its
	// constructor - registering afterwards would leave the first tab using
	// whatever the machine happened to substitute. Failure is a warning rather
	// than fatal: the resolver still has the platform list to fall back to, and
	// refusing to start a text editor over a typeface would be absurd.
	if (CEditorWidget::RegisterBundledFont() == -1)
	{
		qWarning("bundled font: could not register :/fonts/DejaVuSansMono.ttf");
	}

	QCommandLineParser parser;
	parser.setApplicationDescription(
		QStringLiteral("VinaText — a lightweight text and source editor (Qt alpha)"));
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption dataOption(QStringList() << "d" << "data",
		QStringLiteral("Directory holding languages.json and theme-*.json"),
		QStringLiteral("dir"), DefaultDataDir());
	parser.addOption(dataOption);

	QCommandLineOption settingsOption(QStringLiteral("settings"),
		QStringLiteral("Path to vinatext-app-settings.json (the file the Windows "
			"build writes). Defaults to the platform's application-data location."),
		QStringLiteral("file"));
	parser.addOption(settingsOption);
	QCommandLineOption selfTestOption(QStringLiteral("selftest"),
		QStringLiteral("Run the headless checklist over the given files, then exit"));
	parser.addOption(selfTestOption);
	QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
		QStringLiteral("Render the window in both themes into <dir>, then exit"),
		QStringLiteral("dir"));
	parser.addOption(screenshotOption);
	QCommandLineOption newWindowOption(QStringLiteral("new-window"),
		QStringLiteral("Start a second window instead of reusing a running VinaText."));
	parser.addOption(newWindowOption);
	parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Files to open"),
		QStringLiteral("[file...]"));
	parser.process(app);

	// Every mode that never shows a window belongs in here. A modal box with
	// nobody to dismiss it does not fail, it HANGS - and under
	// QT_QPA_PLATFORM=offscreen in CI that is a job that runs until its timeout
	// with no useful output. Add new headless modes to this line.
	const bool bHeadless = parser.isSet(selfTestOption) || parser.isSet(screenshotOption);

	// The headless modes must not read or write the settings of a real install.
	// CMainWindow's constructor restores the dock layout from QSettings, so this
	// has to happen BEFORE the window exists - isolating it inside RunSelfTest is
	// already too late, and a --selftest that inherited a layout saved by an
	// earlier interactive session would fail on a pane the user had merely
	// closed. Demonstrated: seeding a hidden-pane layout makes a subsequent,
	// otherwise untouched self-test fail three checks.
	//
	// The QTemporaryDir has to outlive the window, hence the scope here: it
	// removes its tree when it is destroyed.
	QTemporaryDir settingsDirectory;
	if (bHeadless)
	{
		if (!settingsDirectory.isValid())
		{
			qWarning("could not create a scratch settings directory");
			return 2;
		}
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			settingsDirectory.path());
	}

	// Settings before the window, because CMainWindow builds editors in its
	// constructor and they read the settings as they are created.
	CEditorData data;
	QString strError;
	if (!data.Load(parser.value(dataOption), strError))
	{
		// Without this data every file loses its lexer, its keywords and its
		// colours. That is very visible and very hard to attribute, so refuse to
		// start rather than come up looking broken.
		qCritical("cannot load editor data from %s: %s",
			qPrintable(parser.value(dataOption)), qPrintable(strError));
		if (!bHeadless)
		{
			QMessageBox::critical(nullptr, QStringLiteral("VinaText"),
				QStringLiteral("Cannot load editor data from %1:\n%2")
					.arg(parser.value(dataOption), strError));
		}
		return 2;
	}

	// A settings file that cannot be parsed is a warning, never a failure: the
	// user's choices are being ignored, which is worth saying, but it is not a
	// reason to refuse to edit text. In a headless run the settings path is the
	// scratch directory set above, so --selftest and --screenshot read the
	// shipped defaults rather than whatever this machine happens to have.
	// An EXPLICIT --settings is honoured even headless: naming a file is a
	// deliberate choice, and being able to run --selftest against a real
	// settings file is the only way to check the wiring end to end. Without
	// one, a headless run reads from the scratch directory rather than this
	// machine's real settings, so CI sees the shipped defaults and cannot be
	// perturbed by whatever the developer happens to have configured.
	QString strSettingsWarning;
	const QString strSettingsPath = parser.isSet(settingsOption)
		? parser.value(settingsOption)
		: (bHeadless ? settingsDirectory.path() + QStringLiteral("/none.json") : QString());
	data.LoadSettings(strSettingsPath, strSettingsWarning);
	if (!strSettingsWarning.isEmpty())
	{
		qWarning("settings: %s", qPrintable(strSettingsWarning));
	}

	CMainWindow window(data);
	const QStringList files = parser.positionalArguments();

	if (parser.isSet(selfTestOption))
	{
		return window.RunSelfTest(files);
	}
	if (parser.isSet(screenshotOption))
	{
		return window.RenderScreenshots(files, parser.value(screenshotOption));
	}

	// ONE RUNNING VINATEXT. Tried after the headless modes have returned and
	// before the window is shown: a --selftest that handed its file list to a
	// running editor and exited would pass CI by not running, and bHeadless
	// alone would not have stopped it because those two return above.
	//
	// --new-window is the escape hatch, standing in for the MFC's three
	// (MOVE_TO_NEW_WINDOW, REOPEN_WITH_ADMIN_RIGHT, RESTART_APP) - the latter
	// two belong to features D10 defers.
	// NOT ON THE WEB. Qt's own documentation is flat about it - "All Q*Server
	// classes are not supported by the platform" - because a browser tab cannot
	// listen on a socket. There is also nothing for it to mean: a second tab is
	// a second process with its own sandbox, and handing files between them is
	// not a thing the platform offers. Guarded rather than left to fail at run
	// time, so the intent is legible instead of looking like a bug that nobody
	// noticed. See doc/PORTING.md 6z.
#ifndef Q_OS_WASM
	CSingleInstance instance;
	if (!parser.isSet(newWindowOption))
	{
		// One call, because connect-then-listen has to happen under a single
		// lock: six simultaneous launches otherwise leave four windows, each
		// having unlinked the last winner's socket. See ui-qt/SingleInstance.h.
		bool bBecameServer = false;
		if (instance.TakeOverOrHandOff(files, bBecameServer))
		{
			return 0;
		}
		if (bBecameServer)
		{
			QObject::connect(&instance, &CSingleInstance::FilesReceived, &window,
				[&window](const QStringList& received)
			{
				for (const QString& strPath : received)
				{
					window.OpenFile(strPath);
				}
				// Come to the front even with nothing to open, which is what a
				// bare second launch means. The MFC notifies only when there is
				// a file, so double-clicking its icon while it runs does
				// nothing at all - a small, deliberate improvement.
				window.setWindowState((window.windowState() & ~Qt::WindowMinimized)
					| Qt::WindowActive);
				window.raise();
				window.activateWindow();
			});
		}
	}
#else
	// The option still parses on the web so the help text does not lie about
	// which build you have; it simply has nothing to switch off.
	(void)newWindowOption;
#endif

	for (const QString& strPath : files)
	{
		window.OpenFile(strPath);
	}
	window.show();
	return app.exec();
}
