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
};
