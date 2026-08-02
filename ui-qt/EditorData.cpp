/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "EditorData.h"

#include <QStandardPaths>

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

QString CEditorData::DefaultSettingsPath()
{
	// AppDataLocation with no organisation name is AppData/Roaming/VinaText on
	// Windows - the same directory PathUtils::GetVinaTextAppDataPath() uses - so
	// the MFC build and this one read and write one file rather than two.
	const QString strDirectory =
		QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	return strDirectory + QLatin1Char('/')
		+ QLatin1String(Core::CAppSettings::FileName());
}

void CEditorData::LoadSettings(const QString& strPath, QString& strWarningOut)
{
	const QString strActual = strPath.isEmpty() ? DefaultSettingsPath() : strPath;
	m_strSettingsPath = strActual;
	std::string strError;
	if (!m_Settings.LoadFromFile(strActual.toStdString(), strError))
	{
		// A warning, never a failure. A settings file that exists and cannot be
		// parsed is worth saying out loud - it means the user's choices are
		// being ignored - but it is not a reason to refuse to edit text.
		strWarningOut = QStringLiteral("%1: %2")
			.arg(strActual, QString::fromStdString(strError));
	}
}
