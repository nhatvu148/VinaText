/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Portable std::string / std::wstring helpers.
//
// This is core/ code (see doc/QT-PORT-BRIEF.md D5): UI-free and portable, with no
// MFC, no Win32 and no Qt dependency. Extracted verbatim from the STDStringHelper
// class that used to live entirely in src/StringHelper.h.
//
// Two members stayed behind in src/StringHelper.h because they are not portable:
//
//   Format()                   uses the MSVC-only CRT extensions _vscwprintf /
//                              _vsnwprintf_s / _vscprintf / _vsnprintf_s. Making it
//                              portable means rewriting it on vsnprintf/vswprintf,
//                              which changes printf edge-case behaviour and deserves
//                              its own change.
//   ExpandEnvironmentStrings() Win32, and called by nothing.
//
// STDStringHelper now derives from this class, so every existing call site -
// STDStringHelper::trim, STDStringHelper::Format - resolves exactly as before.
//
// NOTE the old header carried no #includes at all and relied on src/stdafx.h to have
// pulled in <string>, <algorithm> and the rest. That is precisely the god-header
// coupling the port has to undo, so the includes are spelled out here.

#pragma once

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <memory>
#include <string>

namespace Core
{
	class CStringUtil
	{
	public:
		// trim from both ends
		static inline std::string& trim(std::string& s)
		{
			return ltrim(rtrim(s));
		}
		static inline std::string& trim(std::string& s, const std::string& trimchars)
		{
			return ltrim(rtrim(s, trimchars), trimchars);
		}
		static inline std::string& trim(std::string& s, wint_t trimchar)
		{
			return ltrim(rtrim(s, trimchar), trimchar);
		}

		// trim from start
		static inline std::string& ltrim(std::string& s)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wint_t c) { return !iswspace(c); }));
			return s;
		}
		static inline std::string& ltrim(std::string& s, const std::string& trimchars)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&trimchars](wint_t c) { return trimchars.find(static_cast<char>(c)) == std::string::npos; }));
			return s;
		}
		static inline std::string& ltrim(std::string& s, wint_t trimchar)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&trimchar](wint_t c) { return c != trimchar; }));
			return s;
		}

		// trim from end
		static inline std::string& rtrim(std::string& s)
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [](wint_t c) { return !iswspace(c); }).base(), s.end());
			return s;
		}
		static inline std::string& rtrim(std::string& s, const std::string& trimchars)
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [&trimchars](wint_t c) { return trimchars.find(static_cast<char>(c)) == std::string::npos; }).base(), s.end());
			return s;
		}
		static inline std::string& rtrim(std::string& s, wint_t trimchar)
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [&trimchar](wint_t c) { return c != trimchar; }).base(), s.end());
			return s;
		}

		// trim from both ends
		static inline std::wstring& trim(std::wstring& s)
		{
			return ltrim(rtrim(s));
		}
		static inline std::wstring& trim(std::wstring& s, const std::wstring& trimchars)
		{
			return ltrim(rtrim(s, trimchars), trimchars);
		}
		static inline std::wstring& trim(std::wstring& s, wint_t trimchar)
		{
			return ltrim(rtrim(s, trimchar), trimchar);
		}

		// trim from start
		static inline std::wstring& ltrim(std::wstring& s)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wint_t c) { return !iswspace(c); }));
			return s;
		}
		static inline std::wstring& ltrim(std::wstring& s, const std::wstring& trimchars)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&trimchars](wint_t c) { return trimchars.find(c) == std::wstring::npos; }));
			return s;
		}
		static inline std::wstring& ltrim(std::wstring& s, wint_t trimchar)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&trimchar](wint_t c) { return c != trimchar; }));
			return s;
		}

		// trim from end
		static inline std::wstring& rtrim(std::wstring& s)
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [](wint_t c) { return !iswspace(c); }).base(), s.end());
			return s;
		}
		static inline std::wstring& rtrim(std::wstring& s, const std::wstring& trimchars)
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [&trimchars](wint_t c) { return trimchars.find(c) == std::wstring::npos; }).base(), s.end());
			return s;
		}
		static inline std::wstring& rtrim(std::wstring& s, wint_t trimchar)
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [&trimchar](wint_t c) { return c != trimchar; }).base(), s.end());
			return s;
		}



		[[deprecated("use case insensitive string comparison instead, or the ci_less container helper")]] static inline void emplace_to_lower(std::wstring& s)
		{
			std::transform(s.begin(), s.end(), s.begin(), ::towlower);
		}

		[[deprecated("use case insensitive string comparison instead, or the ci_less container helper")]] static inline void emplace_to_lower(std::string& s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](char c) { return static_cast<char>(::tolower(c)); });
		}


		[[deprecated("use case insensitive string comparison instead, or the ci_less container helper")]] static inline std::string to_lower(const std::string& s)
		{
			std::string ls(s);
			std::transform(ls.begin(), ls.end(), ls.begin(), [](char c) { return static_cast<char>(::tolower(c)); });
			return ls;
		}


		template <typename T, typename T2>
		static void TrimLeading(T& s, const T2& vals)
		{
			auto it = s.begin();
			while (it != s.end())
			{
				auto whereAt = std::find(vals.begin(), vals.end(), *it);
				if (whereAt == vals.end())
					break;
				++it;
				if (it == s.end())
					break;
			}
			s.erase(s.begin(), it);
		}

		template <typename T, typename T2>
		static void TrimTrailing(T& s, const T2& vals)
		{
			while (!s.empty())
			{
				auto whereAt = std::find(vals.begin(), vals.end(), s.back());
				if (whereAt == vals.end())
					break;
				s.pop_back();
			}
		}

		// Trim container T of values in T2.
		// T1 can at least be a string, wstring, vector,
		// T2 can be simiar but initializer_list is the typical type used.
		template <typename T, typename T2>
		static void TrimLeadingAndTrailing(T& s, const T2& vals)
		{
			TrimLeading(s, vals);
			TrimTrailing(s, vals);
		}
	};
}
