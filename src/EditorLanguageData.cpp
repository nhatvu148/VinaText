/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "stdafx.h"
#include "EditorLanguageData.h"

#include "LanguageData.h"		// core/
#include "AppUtil.h"
#include "EditorDatabase.h"
#include "PathUtil.h"

namespace EditorLanguageData
{
	namespace
	{
		Core::CLanguageTable	g_LanguageTable;
		bool					g_bLoadAttempted = false;
		bool					g_bLoaded = false;

		CString GetLanguageDataPath()
		{
			return PathUtils::GetVinaTextPackagePath() + _T("data-packages\\languages.json");
		}
	}

	bool EnsureLoaded()
	{
		if (g_bLoadAttempted)
		{
			return g_bLoaded;
		}
		g_bLoadAttempted = true;

		const CString strPath = GetLanguageDataPath();
		const std::string strNarrowPath = AppUtils::CStringToStd(strPath);

		std::string strError;
		g_bLoaded = g_LanguageTable.LoadFromFile(strNarrowPath, strError);
		if (!g_bLoaded)
		{
			// Deliberately loud in a debug build and traced in release. Without the
			// data file every language loses its name, extension, comment delimiters
			// and keywords, which is very visible but hard to attribute - so say so
			// rather than degrading silently.
			const CString strMessage = _T("VinaText: failed to load ") + strPath
				+ _T(" - ") + AppUtils::StdToCString(strError) + _T("\n");
			::OutputDebugString(strMessage);
			ASSERT(FALSE);
		}
		return g_bLoaded;
	}

	void ApplyLanguageMetadata(CLanguageDatabase* pDatabase, const char* szLanguageId)
	{
		if (pDatabase == NULL || szLanguageId == NULL)
		{
			return;
		}
		if (!EnsureLoaded())
		{
			return;
		}
		const Core::SLanguageInfo* pInfo = g_LanguageTable.FindById(szLanguageId);
		if (pInfo == NULL)
		{
			return;
		}
		pDatabase->SetLanguageName(AppUtils::StdToCString(pInfo->_Name));
		pDatabase->SetLanguageExtension(AppUtils::StdToCString(pInfo->_Extension));
		pDatabase->SetLanguageCommentSymbol(AppUtils::StdToCString(pInfo->_CommentLine));
		pDatabase->SetLanguageCommentStart(AppUtils::StdToCString(pInfo->_CommentStart));
		pDatabase->SetLanguageCommentEnd(AppUtils::StdToCString(pInfo->_CommentEnd));
	}

	const char* GetKeywords(const char* szLanguageId)
	{
		if (szLanguageId == NULL || !EnsureLoaded())
		{
			return "";
		}
		const Core::SLanguageInfo* pInfo = g_LanguageTable.FindById(szLanguageId);
		if (pInfo == NULL)
		{
			return "";
		}
		// Points into the loaded table, which lives for the life of the process.
		return pInfo->_Keywords.c_str();
	}
}
