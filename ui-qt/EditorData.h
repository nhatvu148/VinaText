/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Everything the editor needs from Packages/data-packages, loaded once.
//
// The same JSON the MFC build reads (doc/PORTING.md 6d). Both themes are held at
// once because the alpha switches between them live, and re-reading a file to
// change a colour would be silly.

#pragma once

#include "AppSettings.h"		// core/
#include "LanguageData.h"		// core/

#include <QString>

enum class EEditorTheme
{
	Light,
	Dark,
};

class CEditorData final
{
public:
	// Returns false and fills strErrorOut with the first file that failed.
	bool Load(const QString& strDataDir, QString& strErrorOut);

	// The editor settings, read from the file the MFC application writes. A
	// separate call from Load because a missing or unreadable settings file is
	// NOT fatal - every setting keeps its shipped default and the editor runs -
	// whereas missing language data leaves every file unlexed and is.
	//
	// strPath empty means "the usual place for this platform".
	void LoadSettings(const QString& strPath, QString& strWarningOut);
	// Where the settings were read from, and whether anything was there. Shown
	// in the message pane at startup, because a reader that gives the user no
	// way to see what it read is half a feature - and until EditorSettingDlg
	// exists this is the only way to tell defaults from a file.
	const QString& GetSettingsPath() const { return m_strSettingsPath; }
	const Core::CAppSettings& GetSettings() const { return m_Settings; }
	// Replaces the settings and writes them back. Returns false and fills
	// strErrorOut when the file cannot be written; the in-memory settings are
	// updated either way, so a failed save does not also discard the edit.
	bool ApplySettings(const Core::CAppSettings& settings, QString& strErrorOut);

	// Where the MFC keeps it: %AppData%/VinaText on Windows, and the platform
	// equivalent elsewhere. QStandardPaths with no organisation name yields
	// AppData/Roaming/VinaText on Windows, which is the same directory - so the
	// two frontends genuinely share one file rather than each having their own.
	static QString DefaultSettingsPath();

	const Core::CLanguageTable& GetLanguages() const { return m_Languages; }
	const Core::CEditorTheme& GetTheme(EEditorTheme theme) const
	{
		return theme == EEditorTheme::Dark ? m_Dark : m_Light;
	}

	// nullptr for plain text, which is not a language: no metadata, no keywords,
	// no lexer. core/ owns the rules, so ui-mfc/ and ui-qt/ agree.
	const Core::SLanguageInfo* DetectLanguage(const QString& strFileName) const
	{
		return m_Languages.DetectForFileName(strFileName.toStdString());
	}

	// What the status bar shows when DetectLanguage returns nullptr.
	QString GetPlainTextLabel() const
	{
		return QString::fromStdString(m_Languages.GetPlainTextLabel());
	}

private:
	Core::CLanguageTable	m_Languages;
	Core::CEditorTheme		m_Light;
	Core::CEditorTheme		m_Dark;
	Core::CAppSettings		m_Settings;
	QString					m_strSettingsPath;
};
