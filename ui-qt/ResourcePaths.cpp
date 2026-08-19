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
			if (ResourcePaths::HoldsResources(strCandidate, strLeaf))
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

// A leaf is only "there" if it holds what it is supposed to hold. An empty
// directory that happens to exist next to the binary would otherwise win the
// search and the app would report missing languages rather than a missing
// directory - the failure one step removed from its cause.
bool ResourcePaths::HoldsResources(const QString& strDir, const QString& strLeaf)
{
	// NO SEPARATE exists(strDir) TEST. It reads as defensive and is unreachable:
	// if the directory is not there, neither is the witness inside it, so the
	// line below already returns false. Mutation proved it - removing that guard
	// changed no result. The isEmpty() check stays, because an empty string
	// would otherwise ask about "/languages.json" at the filesystem root.
	if (strDir.isEmpty())
	{
		return false;
	}
	const QString strWitness = (strLeaf == QLatin1String("data"))
		? QStringLiteral("languages.json")
		: QStringLiteral("License-VinaText.txt");
	return QFileInfo::exists(strDir + QLatin1Char('/') + strWitness);
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

QString ResourcePaths::DescribeSource(const QString& strDir, const QString& strLeaf)
{
	// Compared CLEANED, because the candidates carry "/../" segments and the
	// resolved path is one of them verbatim - but a caller passing a tidied or
	// symlink-resolved path would otherwise match nothing and be reported as
	// "--data", which is the one label that must not be wrong.
	const QString strWanted = QDir::cleanPath(strDir);
	const QStringList candidates = Candidates(strLeaf);
	// Same order as Candidates(), and that is load-bearing: the labels are
	// positional, so a candidate added there without one added here would
	// silently take its neighbour's name.
	const char* aLabels[] = { "bundle", "next to the app", "prefix install", "build tree" };
	const int nLabels = static_cast<int>(sizeof(aLabels) / sizeof(aLabels[0]));
	for (int i = 0; i < candidates.size() && i < nLabels; ++i)
	{
		if (QDir::cleanPath(candidates.at(i)) == strWanted)
		{
			return QString::fromLatin1(aLabels[i]);
		}
	}
	return QStringLiteral("--data");
}

QString ResourcePaths::ForDisplay(const QString& strPath)
{
	const QString strHome = QDir::homePath();
	if (!strHome.isEmpty() && strPath.startsWith(strHome + QLatin1Char('/')))
	{
		return QLatin1Char('~') + strPath.mid(strHome.size());
	}
	return strPath;
}

QString ResourcePaths::DataDir()
{
	return Resolve(QStringLiteral("data"));
}

QString ResourcePaths::LicenseDir()
{
	return Resolve(QStringLiteral("license"));
}
