/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "ResourcePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace
{
	// A leaf is only "there" if it holds what it is supposed to hold. An empty
	// directory that happens to exist next to the binary would otherwise win the
	// search and the app would report missing languages rather than a missing
	// directory - the failure one step removed from its cause.
	bool HasContent(const QString& strDir, const QString& strLeaf)
	{
		if (strDir.isEmpty() || !QFileInfo::exists(strDir))
		{
			return false;
		}
		const QString strWitness = (strLeaf == QLatin1String("data"))
			? QStringLiteral("languages.json")
			: QStringLiteral("License-VinaText.txt");
		return QFileInfo::exists(strDir + QLatin1Char('/') + strWitness);
	}

	QString CompiledIn(const QString& strLeaf)
	{
		return (strLeaf == QLatin1String("data"))
			? QStringLiteral(VINATEXT_DATA_DIR)
			: QStringLiteral(VINATEXT_LICENSE_DIR);
	}

	QString Resolve(const QString& strLeaf)
	{
		const QStringList candidates = ResourcePaths::Candidates(strLeaf);
		for (const QString& strCandidate : candidates)
		{
			if (HasContent(strCandidate, strLeaf))
			{
				return strCandidate;
			}
		}
		// NOT an empty string. Returning the compiled-in path means the caller
		// reports "cannot read <path>" naming somewhere real, which is the
		// difference between a diagnosable failure and a silent one.
		return CompiledIn(strLeaf);
	}
}

QStringList ResourcePaths::Candidates(const QString& strLeaf)
{
	const QString strExeDir = QCoreApplication::applicationDirPath();
	return QStringList()
		// A macOS bundle: Contents/MacOS/vinatext-qt -> Contents/Resources/<leaf>.
		<< strExeDir + QStringLiteral("/../Resources/") + strLeaf
		// Next to the binary - an AppImage's AppDir, or an unpacked tarball.
		<< strExeDir + QLatin1Char('/') + strLeaf
		// A Linux prefix install: bin/vinatext-qt -> share/vinatext/<leaf>.
		<< strExeDir + QStringLiteral("/../share/vinatext/") + strLeaf
		// The build tree, LAST, so a developer running out of qtbuild/ still
		// works and a packaged copy never prefers the builder's source tree.
		<< CompiledIn(strLeaf);
}

QString ResourcePaths::DataDir()
{
	return Resolve(QStringLiteral("data"));
}

QString ResourcePaths::LicenseDir()
{
	return Resolve(QStringLiteral("license"));
}
