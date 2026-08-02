/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "AppSettings.h"
#include "LanguageData.h"		// ReadFileToString

#include "json/picojson.h"

#include <fstream>

namespace Core
{
	namespace
	{
		// A key that is present but of the wrong type is IGNORED rather than
		// treated as false or zero. The MFC's own reader does the same - its
		// ReadBOOL leaves the member alone unless the value is a bool - and the
		// alternative is worse: a corrupt entry silently turning a feature off
		// looks exactly like the user having turned it off.
		void ReadBool(const picojson::object& obj, const char* szKey, bool& bOut)
		{
			const picojson::object::const_iterator it = obj.find(szKey);
			if (it != obj.end() && it->second.is<bool>())
			{
				bOut = it->second.get<bool>();
			}
		}

		void ReadInt(const picojson::object& obj, const char* szKey, int& nOut)
		{
			const picojson::object::const_iterator it = obj.find(szKey);
			if (it != obj.end() && it->second.is<double>())
			{
				nOut = static_cast<int>(it->second.get<double>());
			}
		}
	}

	bool CAppSettings::LoadFromFile(const std::string& strPath, std::string& strError)
	{
		// An absent file is the normal state, not a failure: a fresh install has
		// never saved, and a macOS or Linux user may never have run the MFC
		// application at all. Every setting keeps its shipped default and the
		// frontend carries on.
		std::ifstream probe(strPath.c_str());
		if (!probe.good())
		{
			strError.clear();
			return true;
		}
		probe.close();

		std::string strContent;
		if (!ReadFileToString(strPath, strContent, strError))
		{
			return false;
		}
		return LoadFromString(strContent, strError);
	}

	bool CAppSettings::LoadFromString(const std::string& strJson, std::string& strError)
	{
		picojson::value root;
		const std::string strParseError = picojson::parse(root, strJson);
		if (!strParseError.empty())
		{
			strError = strParseError;
			return false;
		}
		if (!root.is<picojson::object>())
		{
			strError = "settings root is not an object";
			return false;
		}

		// Everything sits under the writer's root name, not at the top level.
		const picojson::object& outer = root.get<picojson::object>();
		const picojson::object::const_iterator itRoot = outer.find(RootName());
		if (itRoot == outer.end() || !itRoot->second.is<picojson::object>())
		{
			strError = std::string("no \"") + RootName() + "\" object";
			return false;
		}
		const picojson::object& settings = itRoot->second.get<picojson::object>();

		// EVERY KEY BELOW WAS READ OUT OF CAppSettings::SaveSettingData, not
		// inferred from the member name. LongLineColumnLimitation is why: it
		// stores m_nLongLineMaximum, and guessing "LongLineMaximum" would parse
		// cleanly, find nothing, and silently keep the default - the failure
		// mode that leaves no trace.
		ReadBool(settings, "EnableUrlHighlight", m_bEnableUrlHighlight);
		ReadBool(settings, "DrawFoldingLineUnderLineStyle", m_bDrawFoldingLineUnderLineStyle);
		ReadBool(settings, "DrawCaretLineFrame", m_bDrawCaretLineFrame);
		ReadBool(settings, "EnableHightLightFolder", m_bEnableHightLightFolder);
		ReadBool(settings, "EnableAutoComplete", m_bEnableAutoComplete);
		ReadBool(settings, "AutoCompleteIgnoreNumbers", m_bAutoCompleteIgnoreNumbers);
		ReadBool(settings, "AutoCompleteIgnoreCase", m_bAutoCompleteIgnoreCase);
		ReadBool(settings, "UseFolderMarginClassic", m_bUseFolderMarginClassic);
		ReadInt(settings, "FolderMarginStyle", m_nFolderMarginStyle);
		ReadInt(settings, "LongLineColumnLimitation", m_nLongLineMaximum);

		m_bLoaded = true;
		return true;
	}
}
