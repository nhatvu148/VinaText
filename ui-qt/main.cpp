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
#include <QCommandLineParser>
#include <QMessageBox>

namespace
{
	// Where the JSON extracted in PR #3 lives. The MFC app resolves it next to the
	// executable via PathUtils::GetVinaTextPackagePath(); a build-tree run cannot,
	// so the path is compiled in and --data overrides it.
	QString DefaultDataDir()
	{
		return QStringLiteral(VINATEXT_DATA_DIR);
	}
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("VinaText"));
	QCoreApplication::setApplicationVersion(QStringLiteral(VINATEXT_VERSION));

	QCommandLineParser parser;
	parser.setApplicationDescription(
		QStringLiteral("VinaText — a lightweight text and source editor (Qt alpha)"));
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption dataOption(QStringList() << "d" << "data",
		QStringLiteral("Directory holding languages.json and theme-*.json"),
		QStringLiteral("dir"), DefaultDataDir());
	parser.addOption(dataOption);
	QCommandLineOption selfTestOption(QStringLiteral("selftest"),
		QStringLiteral("Run the headless checklist over the given files, then exit"));
	parser.addOption(selfTestOption);
	QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
		QStringLiteral("Render the window in both themes into <dir>, then exit"),
		QStringLiteral("dir"));
	parser.addOption(screenshotOption);
	parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Files to open"),
		QStringLiteral("[file...]"));
	parser.process(app);

	// Every mode that never shows a window belongs in here. A modal box with
	// nobody to dismiss it does not fail, it HANGS - and under
	// QT_QPA_PLATFORM=offscreen in CI that is a job that runs until its timeout
	// with no useful output. Add new headless modes to this line.
	const bool bHeadless = parser.isSet(selfTestOption) || parser.isSet(screenshotOption);

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

	for (const QString& strPath : files)
	{
		window.OpenFile(strPath);
	}
	window.show();
	return app.exec();
}
