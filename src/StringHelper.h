/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#pragma once

#include "TextTransform.h"		// core/ - replaces boost/algorithm/string/join
#include <wininet.h>		// INTERNET_SCHEME / HINTERNET
#include "StringUtil.h"		// core/

//////////////////////////////////
// c++ 11 string buffer format

template<typename ... Args> std::string variadic_string_format(const std::string& format, Args ... args)
{
	int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	if (size_s <= 0) { throw std::runtime_error("Error during formatting string."); }
	auto size = static_cast<size_t>(size_s);
	auto buf = std::make_unique<char[]>(size);
	std::snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

//////////////////////////////////
// Multiple byte buffer allocator

template <class T> class CMultiByteBuffer final
{
public:
	~CMultiByteBuffer() { if (_allocLen) delete[] _str; }

	void sizeTo(size_t size)
	{
		if (_allocLen < size)
		{
			if (_allocLen)
				delete[] _str;
			_allocLen = max(size, initSize);
			_str = new T[_allocLen];
		}
	}

	void empty()
	{
		static T nullStr = 0;
		if (_allocLen == 0)
			_str = &nullStr;
		else
			_str[0] = 0;
	}

	operator T* () { return _str; }
	operator const T* () const { return _str; }

protected:
	static const int initSize = 1024;
	size_t _allocLen = 0;
	T* _str = NULL;
};

//////////////////////////////////
// C String Helper

namespace StringHelper
{
	int xisalnum(wint_t c);
	int xisspecial(wint_t c);
	int xisalpha(wint_t c);
	int xisalnum(wint_t c);
	int xisspace(wint_t c);
	bool IsMBSTrail(const TCHAR *pszChars, int nCol);
	bool isUrlSchemeStartChar(TCHAR const c);
	bool isUrlSchemeDelimiter(TCHAR const c);
	bool isUrlTextChar(TCHAR const c);
	bool isUrlQueryDelimiter(TCHAR const c);
	bool isUrlSchemeSupported(INTERNET_SCHEME s);
	bool scanToUrlStart(TCHAR * text, int textLen, int start, int * distance, int * schemeLength);
	void scanToUrlEnd(TCHAR * text, int textLen, int start, int * distance);
	bool removeUnwantedTrailingCharFromUrl(TCHAR const * text, int * length);
	// join string
	template <class T>
	T JoinStdString(const T & strDelimiters, const std::vector<T>& arFields)
	{
		return Core::Join(arFields, strDelimiters);
	}
}

//////////////////////////////////
// C++ STD String Helper

// The portable majority of this class now lives in core/StringUtil.h. Deriving from
// it keeps every existing call site working unchanged - STDStringHelper::trim and
// STDStringHelper::Format both still resolve. Only the two members that cannot move
// remain here.
class STDStringHelper : public Core::CStringUtil
{
public:
	// REQUIRED - do not remove.
	//
	// to_lower is the one name declared on both sides of this split: the narrow
	// to_lower(const std::string&) stayed in Core::CStringUtil, the wide
	// to_lower(const std::wstring&) had to stay here because it uses Win32 NLS.
	// C++ member-name hiding means the derived declaration hides the ENTIRE base
	// overload set for that name, so without this using-declaration
	// STDStringHelper::to_lower(std::string) stops resolving - silently, because
	// nothing calls it today. This restores the overload set the class had before
	// the split.
	using Core::CStringUtil::to_lower;

	// Win32 NLS (LCMapStringEx / LOCALE_NAME_INVARIANT) - stays here.
	/// converts a string to lowercase
	/// note: please use only where absolutely necessary!
	/// better use stricmp functions if possible since for non ANSI strings there just are too many exceptions
	/// and special cases to handle.
	static inline std::wstring to_lower(const std::wstring& s)
	{
		auto len = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, s.c_str(), -1, nullptr, 0, nullptr, nullptr, 0);
		auto outBuf = std::make_unique<wchar_t[]>(len + 1LL);
		LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, s.c_str(), -1, outBuf.get(), len, nullptr, nullptr, 0);
		return outBuf.get();
	}

	// MSVC CRT extension (_wcsnicmp) - stays here.
	static size_t find_caseinsensitive(const std::wstring& haystack, const std::wstring& needle)
	{
		auto ret = std::wstring::npos;
		for (size_t i = 0; i < haystack.size(); ++i)
		{
			if (_wcsnicmp(&haystack[i], needle.c_str(), needle.size()) == 0)
			{
				ret = i;
				break;
			}
		}
		return ret;
	}

	// MSVC-only CRT extensions (_vscwprintf / _vsnwprintf_s / _vscprintf /
	// _vsnprintf_s). Porting this means rewriting it on vsnprintf/vswprintf, which
	// shifts printf edge-case behaviour - a separate change.
	static std::wstring Format(const wchar_t* frmt, ...);
	static std::string  Format(const char* frmt, ...);

	static std::wstring ExpandEnvironmentStrings(const std::wstring& s)
	{
		DWORD len = ::ExpandEnvironmentStrings(s.c_str(), nullptr, 0);
		if (len == 0)
			return s;

		auto buf = std::make_unique<wchar_t[]>(len + 1ULL);
		if (::ExpandEnvironmentStrings(s.c_str(), buf.get(), len) == 0)
			return s;

		return buf.get();
	}
};

//////////////////////////////////
// Text alignment

typedef std::list<std::wstring> WordList;

class CTextAlignment
{
public:
	CTextAlignment() = default;
	~CTextAlignment() = default;

	enum class Alignment : unsigned int
	{
		Left = 0,
		Right,
		Center,
		Justify
	};

	std::wstring AlignText(const std::wstring& text, Alignment align, const std::wstring& eol);
private:
	WordList SplitTextIntoWords(const std::wstring& text);
};