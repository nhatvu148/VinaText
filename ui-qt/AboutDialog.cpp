/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace
{
	// thirdparty/CMakeLists.txt:27-28. Passed in by the build rather than
	// duplicated here, for the same reason the Qt version is read rather than
	// written down.
#ifndef VINATEXT_SCINTILLA_VERSION
#define VINATEXT_SCINTILLA_VERSION "unknown"
#endif
#ifndef VINATEXT_LEXILLA_VERSION
#define VINATEXT_LEXILLA_VERSION "unknown"
#endif

	const char* const HOME_PAGE = "https://www.vinatext.dev";
}

namespace About
{
	QString RuntimeQtVersion()
	{
		return QString::fromLatin1(qVersion());
	}

	QString CompiledQtVersion()
	{
		return QStringLiteral(QT_VERSION_STR);
	}

	QString QtSourceUrl()
	{
		// https://download.qt.io/archive/qt/<major>.<minor>/<full>/single/
		// Built from the running version so it points at the source for the Qt
		// the user actually has, which is what "corresponding source" means.
		const QString strVersion = RuntimeQtVersion();
		const QStringList parts = strVersion.split(QLatin1Char('.'));
		if (parts.size() < 2)
		{
			return QStringLiteral("https://download.qt.io/archive/qt/");
		}
		return QStringLiteral("https://download.qt.io/archive/qt/%1.%2/%3/single/")
			.arg(parts.at(0), parts.at(1), strVersion);
	}

	QString AttributionText()
	{
		QString strQt = QStringLiteral("Uses Qt %1 under the LGPL v3.")
			.arg(RuntimeQtVersion());
		// Only worth saying when they differ - and when they do, it matters:
		// the user may relink against the one they have.
		if (RuntimeQtVersion() != CompiledQtVersion())
		{
			strQt += QStringLiteral(" Built against Qt %1.").arg(CompiledQtVersion());
		}

		return strQt + QStringLiteral(
			"\nQt is linked dynamically and may be replaced with a compatible version."
			"\nCorresponding source for this Qt version:"
			"\n%1"
			"\nQt licence text: license/License-Qt.txt"
			"\n"
			"\nUses Scintilla %2 and Lexilla %3 (HPND / permissive), vendored in"
			"\nthirdparty/. Licence text: license/License-Scintilla.txt")
			.arg(QtSourceUrl(),
				QStringLiteral(VINATEXT_SCINTILLA_VERSION),
				QStringLiteral(VINATEXT_LEXILLA_VERSION));
	}

	QString AttributionHtml()
	{
		// Escape FIRST, then substitute the anchor, then break the lines. Doing
		// it in that order means no part of the attribution can be interpreted
		// as markup - the licence paths and version strings are data.
		//
		// UNTESTED AND KNOWN TO BE: no current input contains '<', '>' or '&',
		// so toHtmlEscaped() is a no-op and removing it fails nothing. Verified
		// by mutation. It stays because the version strings arrive from CMake
		// and the URL from whatever Qt reports, neither of which this file
		// controls - but nobody should mistake it for a covered property.
		QString strHtml = AttributionText().toHtmlEscaped();

		const QString strUrl = QtSourceUrl().toHtmlEscaped();
		strHtml.replace(strUrl, QStringLiteral("<a href=\"%1\">%1</a>").arg(strUrl));
		strHtml.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
		return strHtml;
	}
}

CAboutDialog::CAboutDialog(QWidget* pParent)
	: QDialog(pParent)
{
	setWindowTitle(tr("About VinaText"));
	setObjectName(QStringLiteral("AboutDialog"));

	// Wide enough that the corresponding-source URL sits on one line. It is
	// selectable either way, but a link the user is meant to ACT on should not
	// be broken across a wrap - copying half of it is the likely outcome.
	setMinimumWidth(480);

	QVBoxLayout* pLayout = new QVBoxLayout(this);

	// IDD_ABOUTBOX's own wording, minus its three hard-coded facts. The resource
	// pins "Release Version: 1.16", a commit hash and a release date as literal
	// text; 1.16 is already stale against a 1.17.0 project, which is exactly the
	// failure mode of writing a derivable value down. The version here comes
	// from the build.
	QLabel* pTitle = new QLabel(tr("<b>VinaText %1</b>").arg(
		QStringLiteral(VINATEXT_VERSION)), this);
	pTitle->setTextFormat(Qt::RichText);
	pLayout->addWidget(pTitle);

	pLayout->addWidget(new QLabel(
		tr("An opensource text editor and file viewer software."), this));
	pLayout->addWidget(new QLabel(tr("Licence: MIT (freeware)."), this));

	QLabel* pHome = new QLabel(tr("Home page: <a href=\"%1\">%1</a>")
		.arg(QString::fromLatin1(HOME_PAGE)), this);
	pHome->setTextFormat(Qt::RichText);
	pHome->setOpenExternalLinks(true);
	pLayout->addWidget(pHome);

	pLayout->addSpacing(8);

	// The attribution, with the corresponding-source URL as a real link.
	// TextBrowserInteraction is selection AND link following: the URL should be
	// both copyable and clickable, since either is a legitimate way for someone
	// to take up the offer.
	QLabel* pAttribution = new QLabel(About::AttributionHtml(), this);
	pAttribution->setTextFormat(Qt::RichText);
	pAttribution->setTextInteractionFlags(Qt::TextBrowserInteraction);
	pAttribution->setOpenExternalLinks(true);
	pAttribution->setWordWrap(true);
	pLayout->addWidget(pAttribution);

	QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	pLayout->addWidget(pButtons);
}
