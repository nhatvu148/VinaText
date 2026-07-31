/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "EditorData.h"

#include <string>

bool CEditorData::Load(const QString& strDataDir, QString& strErrorOut)
{
	struct SFile
	{
		const char*			_Name;
		Core::CEditorTheme*	_Theme;		// nullptr for the language table
	};

	std::string strError;
	if (!m_Languages.LoadFromFile((strDataDir + "/languages.json").toStdString(), strError))
	{
		strErrorOut = QStringLiteral("languages.json: %1").arg(QString::fromStdString(strError));
		return false;
	}

	const SFile themes[] = {
		{ "theme-light.json", &m_Light },
		{ "theme-dark.json", &m_Dark },
	};
	for (const SFile& file : themes)
	{
		const QString strPath = strDataDir + QLatin1Char('/') + QLatin1String(file._Name);
		if (!file._Theme->LoadFromFile(strPath.toStdString(), strError))
		{
			strErrorOut = QStringLiteral("%1: %2")
				.arg(QLatin1String(file._Name), QString::fromStdString(strError));
			return false;
		}
	}
	return true;
}
