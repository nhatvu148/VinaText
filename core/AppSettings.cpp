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

		// A string setting, which none existed before the font arrived. Same
		// shape as the others: absent or wrong-typed leaves the default in
		// place rather than clobbering it with an empty value.
		void ReadString(const picojson::object& obj, const char* szKey,
			std::string& strOut)
		{
			const auto it = obj.find(szKey);
			if (it != obj.end() && it->second.is<std::string>())
			{
				strOut = it->second.get<std::string>();
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

	bool CAppSettings::SaveToFile(const std::string& strPath, std::string& strError) const
	{
		// Start from whatever is already there. Everything this class does not
		// understand is carried through untouched - see the header for why that
		// is not merely polite.
		picojson::object outer;
		picojson::object settings;

		std::ifstream in(strPath.c_str(), std::ios::binary);
		if (in.good())
		{
			std::string strExisting((std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());
			in.close();

			picojson::value root;
			const std::string strParseError = picojson::parse(root, strExisting);
			if (!strParseError.empty())
			{
				// Refuse rather than overwrite. A file that exists and cannot be
				// parsed is far more likely to be someone's settings plus a
				// typo than something safe to replace with ten keys.
				strError = "refusing to overwrite an unparseable settings file: "
					+ strParseError;
				return false;
			}
			if (root.is<picojson::object>())
			{
				outer = root.get<picojson::object>();
				const picojson::object::const_iterator it = outer.find(RootName());
				if (it != outer.end() && it->second.is<picojson::object>())
				{
					settings = it->second.get<picojson::object>();
				}
			}
		}

		settings["EnableUrlHighlight"] = picojson::value(m_bEnableUrlHighlight);
		settings["EditorFontName"] = picojson::value(m_strEditorFontName);
		settings["EditorFontPointSize"] =
			picojson::value(static_cast<double>(m_nEditorFontPointSize));
		settings["EditorTabWidth"] = picojson::value(static_cast<double>(m_nEditorTabWidth));
		settings["UseCustomEditorTabSettings"] = picojson::value(m_bUseCustomTabSettings);
		settings["EnableProcessIndentationTab"] = picojson::value(m_bProcessIndentationTab);
		settings["EditorZoomFactor"] =
			picojson::value(static_cast<double>(m_nEditorZoomFactor));
		settings["EnableCaretBlink"] = picojson::value(m_bEnableCaretBlink);
		settings["EnableMultipleCursor"] = picojson::value(m_bEnableMultipleCursor);
		settings["DefaultFileEOL"] = picojson::value(static_cast<double>(m_nDefaultFileEol));
		settings["AutoAddNewLineAtTheEOF"] = picojson::value(m_bAutoAddNewLineAtEof);
		settings["DrawFoldingLineUnderLineStyle"] =
			picojson::value(m_bDrawFoldingLineUnderLineStyle);
		settings["DrawCaretLineFrame"] = picojson::value(m_bDrawCaretLineFrame);
		settings["EnableHightLightFolder"] = picojson::value(m_bEnableHightLightFolder);
		settings["EnableAutoComplete"] = picojson::value(m_bEnableAutoComplete);
		settings["AutoCompleteIgnoreNumbers"] =
			picojson::value(m_bAutoCompleteIgnoreNumbers);
		settings["AutoCompleteIgnoreCase"] = picojson::value(m_bAutoCompleteIgnoreCase);
		settings["UseFolderMarginClassic"] = picojson::value(m_bUseFolderMarginClassic);
		settings["FolderMarginStyle"] =
			picojson::value(static_cast<double>(m_nFolderMarginStyle));
		settings["LongLineColumnLimitation"] =
			picojson::value(static_cast<double>(m_nLongLineMaximum));

		outer[RootName()] = picojson::value(settings);

		std::ofstream out(strPath.c_str(), std::ios::binary | std::ios::trunc);
		if (!out.good())
		{
			strError = "cannot open " + strPath + " for writing";
			return false;
		}
		// serialize(true) is pretty-printed, matching JSonWriter::SaveFile - so
		// a file written by either frontend looks the same to the other and to
		// whoever opens it in an editor.
		out << picojson::value(outer).serialize(true);
		if (!out.good())
		{
			strError = "failed while writing " + strPath;
			return false;
		}
		out.close();
		return true;
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
		// The eight new ones. Keys derived from CAppSettings::SaveSettingData -
		// see the header for the three that do not match their member names.
		ReadString(settings, "EditorFontName", m_strEditorFontName);
		ReadInt(settings, "EditorFontPointSize", m_nEditorFontPointSize);
		ReadInt(settings, "EditorTabWidth", m_nEditorTabWidth);
		ReadBool(settings, "UseCustomEditorTabSettings", m_bUseCustomTabSettings);
		ReadBool(settings, "EnableProcessIndentationTab", m_bProcessIndentationTab);
		ReadInt(settings, "EditorZoomFactor", m_nEditorZoomFactor);
		ReadBool(settings, "EnableCaretBlink", m_bEnableCaretBlink);
		ReadBool(settings, "EnableMultipleCursor", m_bEnableMultipleCursor);
		ReadInt(settings, "DefaultFileEOL", m_nDefaultFileEol);
		ReadBool(settings, "AutoAddNewLineAtTheEOF", m_bAutoAddNewLineAtEof);
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
