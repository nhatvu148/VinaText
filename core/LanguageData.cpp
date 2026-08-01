/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "LanguageData.h"

#include "json/picojson.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace Core
{
	namespace
	{
		// picojson stores every number as double; guard the narrowing explicitly so a
		// malformed file fails loudly instead of silently truncating to 0.
		bool ReadInt(const picojson::object& obj, const std::string& strKey, int& result)
		{
			const picojson::object::const_iterator it = obj.find(strKey);
			if (it == obj.end() || !it->second.is<double>())
			{
				return false;
			}
			result = static_cast<int>(it->second.get<double>());
			return true;
		}

		// Absent means false; present means it must actually be a bool.
		//
		// Reporting the wrong type here rather than folding it into false matters
		// because the fold is silent exactly when it is least visible: a style with
		// "bold": true and "italic": "yes" keeps its bold, passes every other
		// check, and simply never renders italic.
		bool ReadBool(const picojson::object& obj, const std::string& strKey, bool& bOut,
			std::string& strError)
		{
			const picojson::object::const_iterator it = obj.find(strKey);
			if (it == obj.end())
			{
				bOut = false;
				return true;
			}
			if (!it->second.is<bool>())
			{
				strError = "\"" + strKey + "\" is not true or false";
				return false;
			}
			bOut = it->second.get<bool>();
			return true;
		}

		std::string ReadString(const picojson::object& obj, const std::string& strKey)
		{
			const picojson::object::const_iterator it = obj.find(strKey);
			if (it == obj.end() || !it->second.is<std::string>())
			{
				return std::string();
			}
			return it->second.get<std::string>();
		}

		// "#RRGGBB" -> SColor. Returns false on anything else.
		bool ParseHexColor(const std::string& strHex, SColor& color)
		{
			if (strHex.size() != 7 || strHex[0] != '#')
			{
				return false;
			}
			unsigned int r = 0, g = 0, b = 0;
			if (std::sscanf(strHex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) != 3)
			{
				return false;
			}
			color._Red = static_cast<int>(r);
			color._Green = static_cast<int>(g);
			color._Blue = static_cast<int>(b);
			return true;
		}

		// CString::CompareNoCase is _tcsicmp, which follows the C runtime locale -
		// and CommandLine.cpp installs the user's via _tsetlocale(LC_ALL, ""). This
		// is deliberately ASCII-only instead, because every extension in the table
		// is ASCII: for ASCII input the two agree exactly, and any input where they
		// could disagree matches no row either way and falls through to plain text.
		bool EqualsNoCaseAscii(const std::string& strLeft, const std::string& strRight)
		{
			if (strLeft.size() != strRight.size())
			{
				return false;
			}
			for (std::string::size_type i = 0; i < strLeft.size(); ++i)
			{
				char a = strLeft[i];
				char b = strRight[i];
				if (a >= 'A' && a <= 'Z') { a = static_cast<char>(a - 'A' + 'a'); }
				if (b >= 'A' && b <= 'Z') { b = static_cast<char>(b - 'A' + 'a'); }
				if (a != b)
				{
					return false;
				}
			}
			return true;
		}

		bool ParseDocument(const std::string& strJson, picojson::value& value, std::string& strError)
		{
			const std::string strParseError = picojson::parse(value, strJson);
			if (!strParseError.empty())
			{
				strError = strParseError;
				return false;
			}
			if (!value.is<picojson::object>())
			{
				strError = "document root is not an object";
				return false;
			}
			return true;
		}
	}

	bool ReadFileToString(const std::string& strPath, std::string& strOut, std::string& strError)
	{
		std::ifstream file(strPath.c_str(), std::ios::binary);
		if (!file.is_open())
		{
			strError = "cannot open " + strPath;
			return false;
		}
		std::ostringstream buffer;
		buffer << file.rdbuf();
		strOut = buffer.str();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// CLanguageTable

	bool CLanguageTable::LoadFromFile(const std::string& strPath, std::string& strError)
	{
		std::string strJson;
		if (!ReadFileToString(strPath, strJson, strError))
		{
			return false;
		}
		return LoadFromString(strJson, strError);
	}

	bool CLanguageTable::LoadFromString(const std::string& strJson, std::string& strError)
	{
		picojson::value root;
		if (!ParseDocument(strJson, root, strError))
		{
			return false;
		}

		const picojson::object& obj = root.get<picojson::object>();
		const picojson::object::const_iterator itLanguages = obj.find("languages");
		if (itLanguages == obj.end() || !itLanguages->second.is<picojson::array>())
		{
			strError = "missing \"languages\" array";
			return false;
		}

		m_PlainTextLabel = ReadString(obj, "plainTextLabel");

		std::vector<SLanguageInfo> languages;
		const picojson::array& arr = itLanguages->second.get<picojson::array>();
		for (picojson::array::const_iterator it = arr.begin(); it != arr.end(); ++it)
		{
			if (!it->is<picojson::object>())
			{
				strError = "language entry is not an object";
				return false;
			}
			const picojson::object& entry = it->get<picojson::object>();

			SLanguageInfo info;
			info._Id = ReadString(entry, "id");
			if (info._Id.empty())
			{
				strError = "language entry with empty \"id\"";
				return false;
			}
			info._Name = ReadString(entry, "name");
			info._Extension = ReadString(entry, "extension");
			info._Extensions = ReadString(entry, "extensions");
			info._LexerName = ReadString(entry, "lexer");
			info._FoldMarker = ReadString(entry, "foldMarker");
			info._IndentGuides = ReadString(entry, "indentGuides");
			info._StyleTable = ReadString(entry, "styleTable");
			if (!ReadBool(entry, "tagMatch", info._TagMatch, strError))
			{
				strError = "language \"" + info._Id + "\": " + strError;
				return false;
			}
			if (info._StyleTable.empty())
			{
				info._StyleTable = info._Id;
			}
			info._CommentLine = ReadString(entry, "commentLine");
			info._CommentStart = ReadString(entry, "commentStart");
			info._CommentEnd = ReadString(entry, "commentEnd");
			info._Keywords = ReadString(entry, "keywords");

			const picojson::object::const_iterator itAttributes = entry.find("styleAttributes");
			if (itAttributes != entry.end())
			{
				if (!itAttributes->second.is<picojson::array>())
				{
					strError = "\"styleAttributes\" of \"" + info._Id + "\" is not an array";
					return false;
				}
				const picojson::array& attributes = itAttributes->second.get<picojson::array>();
				for (picojson::array::const_iterator itAttribute = attributes.begin();
					itAttribute != attributes.end(); ++itAttribute)
				{
					if (!itAttribute->is<picojson::object>())
					{
						strError = "style attribute of \"" + info._Id + "\" is not an object";
						return false;
					}
					const picojson::object& attribute = itAttribute->get<picojson::object>();

					SStyleAttribute style;
					style._Style = ReadString(attribute, "style");
					if (!ReadInt(attribute, "value", style._Value))
					{
						strError = "style attribute \"" + style._Style + "\" of \"" + info._Id
							+ "\" has no numeric \"value\"";
						return false;
					}
					// A key this does not know is a typo - "Italic" for "italic" -
					// and picojson would hand it over without complaint. The data
					// is generated, so any hand edit is exactly where that happens.
					static const char* const KNOWN_KEYS[] = {
						"style", "value", "bold", "italic", "underline" };
					for (picojson::object::const_iterator itKey = attribute.begin();
						itKey != attribute.end(); ++itKey)
					{
						bool bKnown = false;
						for (size_t i = 0; i < sizeof(KNOWN_KEYS) / sizeof(KNOWN_KEYS[0]); ++i)
						{
							bKnown = bKnown || itKey->first == KNOWN_KEYS[i];
						}
						if (!bKnown)
						{
							strError = "style attribute \"" + style._Style + "\" of \""
								+ info._Id + "\" has an unknown key \"" + itKey->first + "\"";
							return false;
						}
					}

					std::string strAttributeError;
					if (!ReadBool(attribute, "bold", style._Bold, strAttributeError)
						|| !ReadBool(attribute, "italic", style._Italic, strAttributeError)
						|| !ReadBool(attribute, "underline", style._Underline, strAttributeError))
					{
						strError = "style attribute \"" + style._Style + "\" of \"" + info._Id
							+ "\": " + strAttributeError;
						return false;
					}
					// An entry that sets nothing is data that cannot do anything -
					// far more likely a typo in a key name than an intent.
					if (!style._Bold && !style._Italic && !style._Underline)
					{
						strError = "style attribute \"" + style._Style + "\" of \"" + info._Id
							+ "\" sets no attribute at all";
						return false;
					}
					info._StyleAttributes.push_back(style);
				}
			}
			languages.push_back(info);
		}

		m_Languages.swap(languages);
		return true;
	}

	const SLanguageInfo* CLanguageTable::FindById(const std::string& strId) const
	{
		for (std::vector<SLanguageInfo>::const_iterator it = m_Languages.begin();
			it != m_Languages.end(); ++it)
		{
			if (it->_Id == strId)
			{
				return &(*it);
			}
		}
		return nullptr;
	}

	std::string CLanguageTable::ExtensionOf(const std::string& strFileName)
	{
		const std::string::size_type nDot = strFileName.rfind('.');
		if (nDot == std::string::npos)
		{
			return std::string();
		}
		return strFileName.substr(nDot + 1);
	}

	const SLanguageInfo* CLanguageTable::FindByExtension(const std::string& strExtension) const
	{
		if (strExtension.empty())
		{
			return nullptr;
		}
		for (std::vector<SLanguageInfo>::const_iterator it = m_Languages.begin();
			it != m_Languages.end(); ++it)
		{
			const std::string& strList = it->_Extensions;
			if (strList.empty())
			{
				continue;			// selected by file name only, e.g. makefile
			}
			// Splits on '|' as CLexingParser did at the MFC call site. An empty
			// token - from "|py", or from the trailing '|' the user override file
			// writes - can never match here, because an empty strExtension was
			// rejected above.
			std::string::size_type nStart = 0;
			while (nStart <= strList.size())
			{
				std::string::size_type nEnd = strList.find('|', nStart);
				if (nEnd == std::string::npos)
				{
					nEnd = strList.size();
				}
				if (EqualsNoCaseAscii(strList.substr(nStart, nEnd - nStart), strExtension))
				{
					return &(*it);
				}
				nStart = nEnd + 1;
			}
		}
		return nullptr;
	}

	const SLanguageInfo* CLanguageTable::DetectForFileName(const std::string& strFileName) const
	{
		// Both special cases are transcribed from CEditorCtrl::DetectFileLexer,
		// including its inconsistency: CMakeLists.txt is matched case-sensitively
		// (operator==) and Makefile case-insensitively (CompareNoCase). Preserved
		// rather than tidied, so the two frontends agree about "makefile" vs
		// "MAKEFILE" vs "cmakelists.txt" - all three of which reach this code.
		if (strFileName == "CMakeLists.txt")
		{
			return FindById("cmake");
		}
		if (EqualsNoCaseAscii(strFileName, "Makefile"))
		{
			return FindById("makefile");
		}
		return FindByExtension(ExtensionOf(strFileName));
	}

	//////////////////////////////////////////////////////////////////////////
	// CEditorTheme

	bool CEditorTheme::LoadFromFile(const std::string& strPath, std::string& strError)
	{
		std::string strJson;
		if (!ReadFileToString(strPath, strJson, strError))
		{
			return false;
		}
		return LoadFromString(strJson, strError);
	}

	bool CEditorTheme::LoadFromString(const std::string& strJson, std::string& strError)
	{
		picojson::value root;
		if (!ParseDocument(strJson, root, strError))
		{
			return false;
		}
		const picojson::object& obj = root.get<picojson::object>();

		m_Name = ReadString(obj, "name");

		std::map<std::string, SColor> palette;
		const picojson::object::const_iterator itPalette = obj.find("palette");
		if (itPalette == obj.end() || !itPalette->second.is<picojson::object>())
		{
			strError = "missing \"palette\" object";
			return false;
		}
		const picojson::object& paletteObj = itPalette->second.get<picojson::object>();
		for (picojson::object::const_iterator it = paletteObj.begin(); it != paletteObj.end(); ++it)
		{
			SColor color;
			if (!it->second.is<std::string>() || !ParseHexColor(it->second.get<std::string>(), color))
			{
				strError = "palette entry \"" + it->first + "\" is not a #RRGGBB string";
				return false;
			}
			palette[it->first] = color;
		}

		// "roles" is generated from src/Editor.cpp rather than hand-written, and a
		// role naming a key the palette does not define would resolve to nothing
		// and paint an unset colour - so it is rejected here, exactly as a style
		// referencing an undefined colour is below.
		std::map<std::string, std::string> roles;
		const picojson::object::const_iterator itRoles = obj.find("roles");
		if (itRoles == obj.end() || !itRoles->second.is<picojson::object>())
		{
			strError = "missing \"roles\" object";
			return false;
		}
		const picojson::object& rolesObj = itRoles->second.get<picojson::object>();
		for (picojson::object::const_iterator it = rolesObj.begin(); it != rolesObj.end(); ++it)
		{
			if (!it->second.is<std::string>())
			{
				strError = "role \"" + it->first + "\" does not name a palette key";
				return false;
			}
			const std::string& strKey = it->second.get<std::string>();
			if (palette.find(strKey) == palette.end())
			{
				strError = "role \"" + it->first + "\" references undefined colour \""
					+ strKey + "\"";
				return false;
			}
			roles[it->first] = strKey;
		}

		std::map<std::string, std::vector<SStyleMapping> > styles;
		const picojson::object::const_iterator itLanguages = obj.find("languages");
		if (itLanguages == obj.end() || !itLanguages->second.is<picojson::object>())
		{
			strError = "missing \"languages\" object";
			return false;
		}
		const picojson::object& languagesObj = itLanguages->second.get<picojson::object>();
		for (picojson::object::const_iterator it = languagesObj.begin(); it != languagesObj.end(); ++it)
		{
			if (!it->second.is<picojson::array>())
			{
				strError = "style table for \"" + it->first + "\" is not an array";
				return false;
			}
			std::vector<SStyleMapping> mappings;
			const picojson::array& arr = it->second.get<picojson::array>();
			for (picojson::array::const_iterator itEntry = arr.begin(); itEntry != arr.end(); ++itEntry)
			{
				if (!itEntry->is<picojson::object>())
				{
					strError = "style entry in \"" + it->first + "\" is not an object";
					return false;
				}
				const picojson::object& entry = itEntry->get<picojson::object>();

				SStyleMapping mapping;
				mapping._Style = ReadString(entry, "style");
				mapping._Color = ReadString(entry, "color");
				if (!ReadInt(entry, "value", mapping._Value))
				{
					strError = "style \"" + mapping._Style + "\" in \"" + it->first
						+ "\" has no numeric \"value\"";
					return false;
				}
				// A style may only reference a colour the palette actually defines,
				// otherwise the editor would silently paint it black.
				if (palette.find(mapping._Color) == palette.end())
				{
					strError = "style \"" + mapping._Style + "\" in \"" + it->first
						+ "\" references undefined colour \"" + mapping._Color + "\"";
					return false;
				}
				mappings.push_back(mapping);
			}
			styles[it->first] = mappings;
		}

		m_Palette.swap(palette);
		m_Roles.swap(roles);
		m_Styles.swap(styles);
		return true;
	}

	const std::vector<SStyleMapping>* CEditorTheme::FindStyles(const std::string& strLanguageId) const
	{
		const std::map<std::string, std::vector<SStyleMapping> >::const_iterator it
			= m_Styles.find(strLanguageId);
		return it == m_Styles.end() ? nullptr : &it->second;
	}

	bool CEditorTheme::ResolveColor(const std::string& strKey, SColor& color) const
	{
		const std::map<std::string, SColor>::const_iterator it = m_Palette.find(strKey);
		if (it == m_Palette.end())
		{
			return false;
		}
		color = it->second;
		return true;
	}

	bool CEditorTheme::ResolveRole(const std::string& strRole, SColor& color) const
	{
		const std::map<std::string, std::string>::const_iterator it = m_Roles.find(strRole);
		if (it == m_Roles.end())
		{
			return false;
		}
		// LoadFromString rejects a role whose key is undefined, so this second
		// lookup cannot fail on a theme that loaded - but it is a lookup, not an
		// assumption, because a default-constructed theme has neither.
		return ResolveColor(it->second, color);
	}
}
