/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#pragma once

#include "stdafx.h"

#include <scintilla\ILexer.h>
#include <scintilla\Lexilla.h>
#include <scintilla\Scintilla.h>
#include <scintilla\SciLexer.h>
#include <scintilla\ILoader.h>

namespace EditorColorLight
{
	//----------------------------------------------------------------------------
	// Color definitions
	//----------------------------------------------------------------------------
	struct SScintillaColors
	{
		int			iItem;
		COLORREF	rgb;
	};

	// reference color
	const COLORREF black = RGB(0, 0, 0);
	const COLORREF editorTextColor = RGB(0, 0, 0);
	const COLORREF editorMarginBarColor = RGB(220, 220, 220);
	const COLORREF editorFolderBackColor = RGB(128, 128, 128);
	const COLORREF editorFolderForeColor = RGB(220, 220, 220);
	const COLORREF editorCaretColor = RGB(0, 0, 0);
	const COLORREF editorIndicatorColor = RGB(20, 135, 226);
	const COLORREF editorSpellCheckColor = RGB(0, 0, 255);
	const COLORREF editorTagMatchColor = RGB(0, 0, 0);
	const COLORREF linenumber = RGB(0, 0, 0);
	const COLORREF white = RGB(255, 255, 255);
	const COLORREF green = RGB(0, 255, 0);
	const COLORREF red = RGB(255, 0, 0);
	const COLORREF orange = RGB(255, 69, 0);
	const COLORREF blue = RGB(0, 0, 255);
	const COLORREF yellow = RGB(235, 245, 0);
	const COLORREF olive = RGB(128, 128, 0);
	const COLORREF magenta = RGB(255, 0, 255);
	const COLORREF peachpuff = RGB(255, 200, 185);
	const COLORREF cyan = RGB(0, 255, 255);
	const COLORREF darkorange = RGB(255, 140, 0);
	const COLORREF yellowgreen = RGB(154, 205, 50);

	// language lexer color
	const COLORREF keyword = RGB(255, 30, 110); // Red
	const COLORREF definition = RGB(0, 255, 0); // Green
	const COLORREF comment = RGB(10, 103, 4);
	const COLORREF preprocessor = RGB(18, 107, 241);
	const COLORREF string = RGB(158, 66, 5); // orange
	const COLORREF currentline = RGB(249, 38, 114);
	const COLORREF builtin = RGB(10, 103, 175); // Blue
	const COLORREF number = RGB(49, 3, 99); // Purple
	const COLORREF instance = RGB(146, 73, 0);
	const COLORREF Orange = RGB(253, 151, 31);
	const COLORREF light_orange = RGB(255, 140, 0);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//----------------------------------------------------------------------------
	//---- plain text
	//----------------------------------------------------------------------------

	//----------------------------------------------------------------------------
	//---- cpp Language
	//----------------------------------------------------------------------------
	// text color 
	static SScintillaColors g_rgb_Syntax_cpp[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,				instance },
	{ SCE_C_PREPROCESSOR,				preprocessor },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				instance },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				comment },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- python Language
	//----------------------------------------------------------------------------
	// color text 
	static SScintillaColors g_rgb_Syntax_python[] =
	{
		{ SCE_P_DEFAULT,		editorTextColor },
		{ SCE_P_COMMENTLINE,		preprocessor },
		{ SCE_P_NUMBER,				number },
		{ SCE_P_STRING,		    string },
		{ SCE_P_CHARACTER,			string },
		{ SCE_P_WORD,				keyword },
		{ SCE_P_TRIPLE,			preprocessor },
		{ SCE_P_TRIPLEDOUBLE,			preprocessor },
		{ SCE_P_CLASSNAME,			comment },
		{ SCE_P_DEFNAME,			comment },
		{ SCE_P_OPERATOR,			builtin },
		{ SCE_P_IDENTIFIER,		        editorTextColor },
		{ SCE_P_COMMENTBLOCK,		        preprocessor },
		{ SCE_P_STRINGEOL,		        editorTextColor },
		{ SCE_P_WORD2,				keyword },
		{ SCE_P_DECORATOR,		    editorTextColor },
		{ SCE_P_FSTRING,		    string },
		{ SCE_P_FCHARACTER,		    string },
		{ SCE_P_FTRIPLE,		    string },
		{ SCE_P_FTRIPLEDOUBLE,		string },
		{ -1,						0 }
	};

	static SScintillaColors g_rgb_Syntax_python_2[] =
	{
		{ SCE_P_COMMENTLINE,		comment },
		{ SCE_P_COMMENTBLOCK,		comment },
		{ SCE_P_NUMBER,				yellowgreen },
		{ SCE_P_DEFAULT,		    editorTextColor },
		{ SCE_P_CLASSNAME,			cyan },
		{ SCE_P_STRING,				string },
		{ SCE_P_CHARACTER,			string },
		{ SCE_P_IDENTIFIER,			editorTextColor },
		{ SCE_P_OPERATOR,			keyword },
		{ SCE_P_DEFNAME,			magenta },
		{ SCE_P_STRINGEOL,			editorTextColor },
		{ SCE_P_WORD,		        darkorange },
		{ SCE_P_WORD2,		        darkorange },
		{ SCE_P_TRIPLE,		        comment },
		{ SCE_P_TRIPLEDOUBLE,		comment },
		{ SCE_P_DECORATOR,		    builtin },
		{ -1,						0 }
	};

	//----------------------------------------------------------------------------
	//---- ada Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_ada[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };


	//----------------------------------------------------------------------------
	//---- asm Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_asm[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			keyword },
	{ SCE_C_COMMENTDOC,				keyword },
	{ SCE_C_NUMBER,				builtin },
	{ SCE_C_WORD,			editorTextColor },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				editorTextColor },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				keyword },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				keyword },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				keyword },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				keyword },
	{ SCE_C_TRIPLEVERBATIM,				keyword },
	{ SCE_C_HASHQUOTEDSTRING,				keyword },
	{ SCE_C_PREPROCESSORCOMMENT,				keyword },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				keyword },
	{ SCE_C_TASKMARKER,				keyword },
	{ SCE_C_ESCAPESEQUENCE,				keyword },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- bash Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_bash[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				builtin },
	{ SCE_C_NUMBER,				keyword },
	{ SCE_C_WORD,			string },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			number },
	{ SCE_C_UUID,		editorTextColor },
	{ SCE_C_PREPROCESSOR,				string },
	{ SCE_C_OPERATOR,				instance },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };


	//----------------------------------------------------------------------------
	//---- batch Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_batch[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		string },
	{ SCE_C_COMMENTLINE,			keyword },
	{ SCE_C_COMMENTDOC,				keyword },
	{ SCE_C_NUMBER,				keyword },
	{ SCE_C_WORD,			editorTextColor },
	{ SCE_C_STRING,				editorTextColor },
	{ SCE_C_CHARACTER,			builtin },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				definition },
	{ SCE_C_OPERATOR,				definition },
	{ SCE_C_IDENTIFIER,				definition },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				definition },
	{ SCE_C_WORD2,				keyword },
	{ SCE_C_COMMENTDOCKEYWORD,				definition },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				definition },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				editorTextColor },
	{ SCE_C_TRIPLEVERBATIM,				editorTextColor },
	{ SCE_C_HASHQUOTEDSTRING,				editorTextColor },
	{ SCE_C_PREPROCESSORCOMMENT,				editorTextColor },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				editorTextColor },
	{ SCE_C_TASKMARKER,				editorTextColor },
	{ SCE_C_ESCAPESEQUENCE,				editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- c Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_c[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,				instance },
	{ SCE_C_PREPROCESSOR,				preprocessor },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				instance },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				comment },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- cmake Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_cmake[] =
	{ { SCE_CMAKE_DEFAULT,		    editorTextColor },
	{ SCE_C_COMMENT,				comment },
	{ SCE_CMAKE_COMMENT,			comment },
	{ SCE_CMAKE_STRINGDQ,			string },
	{ SCE_CMAKE_STRINGLQ,			string },
	{ SCE_CMAKE_STRINGRQ,			string },
	{ SCE_CMAKE_COMMANDS,			keyword },
	{ SCE_CMAKE_PARAMETERS,			builtin },
	{ SCE_CMAKE_VARIABLE,			builtin },
	{ SCE_CMAKE_USERDEFINED,		builtin },
	{ SCE_CMAKE_WHILEDEF,			builtin },
	{ SCE_CMAKE_FOREACHDEF,			builtin },
	{ SCE_CMAKE_IFDEFINEDEF,		blue },
	{ SCE_CMAKE_MACRODEF,			blue },
	{ SCE_CMAKE_STRINGVAR,			builtin },
	{ SCE_CMAKE_NUMBER,				number },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- makefile Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_makefile[] =
	{ { SCE_MAKE_DEFAULT,		editorTextColor },
	{ SCE_MAKE_COMMENT,		    comment },
	{ SCE_MAKE_PREPROCESSOR,	string },
	{ SCE_MAKE_IDENTIFIER,		editorTextColor },
	{ SCE_MAKE_OPERATOR,		builtin },
	{ SCE_MAKE_TARGET,			number },
	{ SCE_MAKE_IDEOL,			editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- cs Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_cs[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- css Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_css[] =
	{ { SCE_CSS_DEFAULT,		editorTextColor },
	{ SCE_CSS_TAG,		keyword },
	{ SCE_CSS_CLASS,			comment },
	{ SCE_CSS_PSEUDOCLASS,				keyword },
	{ SCE_CSS_UNKNOWN_PSEUDOCLASS,				editorTextColor },
	{ SCE_CSS_OPERATOR,			editorTextColor },
	{ SCE_CSS_IDENTIFIER,				builtin },
	{ SCE_CSS_UNKNOWN_IDENTIFIER,			builtin },
	{ SCE_CSS_VALUE,		number },
	{ SCE_CSS_COMMENT,				preprocessor },
	{ SCE_CSS_ID,				builtin },
	{ SCE_CSS_IMPORTANT,				orange },
	{ SCE_CSS_DIRECTIVE,    keyword },
	{ SCE_CSS_DOUBLESTRING,				string },
	{ SCE_CSS_SINGLESTRING,				string },
	{ SCE_CSS_IDENTIFIER2,				keyword },
	{ SCE_CSS_ATTRIBUTE,				peachpuff },
	{ SCE_CSS_IDENTIFIER3,				keyword },
	{ SCE_CSS_PSEUDOELEMENT,				keyword },
	{ SCE_CSS_EXTENDED_IDENTIFIER,				keyword },
	{ SCE_CSS_EXTENDED_PSEUDOCLASS,				keyword },
	{ SCE_CSS_EXTENDED_PSEUDOELEMENT,				keyword },
	{ SCE_CSS_MEDIA,				keyword },
	{ SCE_CSS_VARIABLE,				editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- erlang Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_erlang[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- fortran Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_fortran[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- html Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_html[] =
	{ { SCE_H_DEFAULT,		editorTextColor },
	{ SCE_H_TAG,		keyword },
	{ SCE_H_TAGUNKNOWN,			editorTextColor },
	{ SCE_H_ATTRIBUTE,				comment },
	{ SCE_H_ATTRIBUTEUNKNOWN,				comment },
	{ SCE_H_NUMBER,			number },
	{ SCE_H_DOUBLESTRING,				editorTextColor },
	{ SCE_H_SINGLESTRING,			editorTextColor },
	{ SCE_H_OTHER,		builtin },
	{ SCE_H_COMMENT,				preprocessor },
	{ SCE_H_ENTITY,				builtin },
	{ SCE_H_TAGEND,				keyword },
	{ SCE_H_XMLSTART,    light_orange },
	{ SCE_H_XMLEND,				yellow },
	{ SCE_H_SCRIPT,				light_orange },
	{ SCE_H_ASP,				comment },
	{ SCE_H_ASPAT,				comment },
	{ SCE_H_CDATA,				comment },
	{ SCE_H_QUESTION,				comment },
	{ SCE_H_VALUE,				magenta },
	{ SCE_H_XCCOMMENT,				light_orange },
	{ SCE_H_SGML_DEFAULT,				light_orange },
	{ SCE_H_SGML_COMMAND,				light_orange },
	{ SCE_H_SGML_1ST_PARAM,				light_orange },
	{ SCE_H_SGML_DOUBLESTRING,				yellow },
	{ SCE_H_SGML_SIMPLESTRING,				light_orange },
	{ SCE_H_SGML_ERROR,				light_orange },
	{ SCE_H_SGML_SPECIAL,				light_orange },
	{ SCE_H_SGML_ENTITY,				light_orange },
	{ SCE_H_SGML_COMMENT,				light_orange },
	{ SCE_H_SGML_1ST_PARAM_COMMENT,				light_orange },
	{ SCE_H_SGML_BLOCK_DEFAULT,				light_orange },
	{ SCE_HJ_START,				light_orange },
	{ SCE_HJ_DEFAULT,				light_orange },
	{ SCE_HJ_COMMENT,				preprocessor },
	{ SCE_HJ_COMMENTLINE,				preprocessor },
	{ SCE_HJ_COMMENTDOC,				preprocessor },
	{ SCE_HJ_NUMBER,				number },
	{ SCE_HJ_WORD,				editorTextColor },
	{ SCE_HJ_KEYWORD,				keyword },
	{ SCE_HJ_DOUBLESTRING,				string },
	{ SCE_HJ_SINGLESTRING,				string },
	{ SCE_HJ_SYMBOLS,				builtin },
	{ SCE_HJ_STRINGEOL,				editorTextColor },
	{ SCE_HJ_REGEX,				string },
	{ SCE_HJA_START, orange },
	{ SCE_HJA_DEFAULT, builtin },
	{ SCE_HJA_COMMENT, preprocessor },
	{ SCE_HJA_COMMENTLINE, preprocessor },
	{ SCE_HJA_COMMENTDOC, preprocessor },
	{ SCE_HJA_NUMBER, number },
	{ SCE_HJA_WORD, keyword },
	{ SCE_HJA_KEYWORD, keyword },
	{ SCE_HJA_DOUBLESTRING , string },
	{ SCE_HJA_SINGLESTRING , string },
	{ SCE_HJA_SYMBOLS, comment },
	{ SCE_HJA_STRINGEOL, editorTextColor },
	{ SCE_HJA_REGEX, string },
	{ SCE_HB_START, string },
	{ SCE_HB_DEFAULT, builtin },
	{ SCE_HB_COMMENTLINE, preprocessor },
	{ SCE_HB_NUMBER, number },
	{ SCE_HB_WORD, comment },
	{ SCE_HB_STRING, string },
	{ SCE_HB_IDENTIFIER, builtin },
	{ SCE_HB_STRINGEOL, editorTextColor },
	{ SCE_HBA_START, string },
	{ SCE_HBA_DEFAULT, string },
	{ SCE_HBA_COMMENTLINE, preprocessor },
	{ SCE_HBA_NUMBER, number },
	{ SCE_HBA_WORD, comment },
	{ SCE_HBA_STRING, string },
	{ SCE_HBA_IDENTIFIER, builtin },
	{ SCE_HBA_STRINGEOL, editorTextColor },
	{ SCE_HP_START, string },
	{ SCE_HP_DEFAULT, string },
	{ SCE_HP_COMMENTLINE, preprocessor },
	{ SCE_HP_NUMBER, number },
	{ SCE_HP_STRING, string },
	{ SCE_HP_CHARACTER, string },
	{ SCE_HP_WORD, comment },
	{ SCE_HP_TRIPLE, string },
	{ SCE_HP_TRIPLEDOUBLE, string },
	{ SCE_HP_CLASSNAME, comment },
	{ SCE_HP_DEFNAME, comment },
	{ SCE_HP_OPERATOR, string },
	{ SCE_HP_IDENTIFIER, string },
	{ SCE_HPHP_COMPLEX_VARIABLE , string },
	{ SCE_HPA_START, string },
	{ SCE_HPA_DEFAULT, string },
	{ SCE_HPA_COMMENTLINE, preprocessor },
	{ SCE_HPA_NUMBER, number },
	{ SCE_HPA_STRING, string },
	{ SCE_HPA_CHARACTER, string },
	{ SCE_HPA_WORD, comment },
	{ SCE_HPA_TRIPLE, string },
	{ SCE_HPA_TRIPLEDOUBLE , string },
	{ SCE_HPA_CLASSNAME, comment },
	{ SCE_HPA_DEFNAME, string },
	{ SCE_HPA_OPERATOR, comment },
	{ SCE_HPA_IDENTIFIER, string },
	{ SCE_HPHP_DEFAULT, builtin },
	{ SCE_HPHP_HSTRING, string },
	{ SCE_HPHP_SIMPLESTRING , string },
	{ SCE_HPHP_WORD, comment },
	{ SCE_HPHP_NUMBER, number },
	{ SCE_HPHP_VARIABLE, string },
	{ SCE_HPHP_COMMENT, preprocessor },
	{ SCE_HPHP_COMMENTLINE, preprocessor },
	{ SCE_HPHP_HSTRING_VARIABLE, string },
	{ SCE_HPHP_OPERATOR, builtin },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- java Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_java[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- javascript Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_javascript[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- typescript Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_typescript[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- lua Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_lua[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- matlab Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_matlab[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- pascal Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_pascal[] =
	{ { SCE_PAS_DEFAULT,		editorTextColor },
	{ SCE_PAS_IDENTIFIER,		editorTextColor },
	{ SCE_PAS_COMMENT,			comment },
	{ SCE_PAS_COMMENT2,				comment },
	{ SCE_PAS_COMMENTLINE,				comment },
	{ SCE_PAS_PREPROCESSOR,			builtin },
	{ SCE_PAS_PREPROCESSOR2,				builtin },
	{ SCE_PAS_HEXNUMBER,			number },
	{ SCE_PAS_WORD,				keyword },
	{ SCE_PAS_STRING,				string },
	{ SCE_PAS_STRINGEOL,				red },
	{ SCE_PAS_CHARACTER,				red },
	{ SCE_PAS_OPERATOR,    builtin },
	{ SCE_PAS_ASM,				light_orange },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- perl Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_perl[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- php Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_php[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- ruby Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_ruby[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- rust Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_rust[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				builtin },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				editorTextColor },
	{ SCE_C_REGEX,				builtin },
	{ SCE_C_COMMENTLINEDOC,				editorTextColor },
	{ SCE_C_WORD2,				editorTextColor },
	{ SCE_C_COMMENTDOCKEYWORD,				builtin },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				editorTextColor },
	{ SCE_C_GLOBALCLASS,				builtin },
	{ SCE_C_STRINGRAW,				builtin },
	{ SCE_C_TRIPLEVERBATIM,				builtin },
	{ SCE_C_HASHQUOTEDSTRING,				builtin },
	{ SCE_C_PREPROCESSORCOMMENT,				builtin },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				editorTextColor },
	{ SCE_C_USERLITERAL,				editorTextColor },
	{ SCE_C_TASKMARKER,				editorTextColor },
	{ SCE_C_ESCAPESEQUENCE,				editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- sql Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_sql[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- tcl Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_tcl[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- visual basic Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_vb[] =
	{ { SCE_C_DEFAULT,		builtin },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			definition },
	{ SCE_C_COMMENTDOC,				keyword },
	{ SCE_C_NUMBER,				string },
	{ SCE_C_WORD,			comment },
	{ SCE_C_STRING,				orange },
	{ SCE_C_CHARACTER,			editorTextColor },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				comment },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- verilog Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_verilog[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- vhdl Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_vhdl[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- xml Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_xml[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		keyword },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			comment },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		builtin },
	{ SCE_C_PREPROCESSOR,				definition },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				definition },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				definition },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				definition },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				definition },
	{ SCE_C_TRIPLEVERBATIM,				definition },
	{ SCE_C_HASHQUOTEDSTRING,				definition },
	{ SCE_C_PREPROCESSORCOMMENT,				definition },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				definition },
	{ SCE_C_TASKMARKER,				definition },
	{ SCE_C_ESCAPESEQUENCE,				definition },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- json Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_json[] =
	{ { SCE_JSON_DEFAULT,		comment },
	{ SCE_JSON_NUMBER,		number },
	{ SCE_JSON_STRING,			comment },
	{ SCE_JSON_STRINGEOL,				keyword },
	{ SCE_JSON_PROPERTYNAME,				builtin },
	{ SCE_JSON_ESCAPESEQUENCE,			keyword },
	{ SCE_JSON_LINECOMMENT,				editorTextColor },
	{ SCE_JSON_BLOCKCOMMENT,			editorTextColor },
	{ SCE_JSON_OPERATOR,		builtin },
	{ SCE_JSON_URI,				editorTextColor },
	{ SCE_JSON_COMPACTIRI,				builtin },
	{ SCE_JSON_KEYWORD,				builtin },
	{ SCE_JSON_LDKEYWORD,    editorTextColor },
	{ SCE_JSON_ERROR,				editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- markdown Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_markdown[] =
	{ { SCE_MARKDOWN_DEFAULT,		editorTextColor },
	{ SCE_MARKDOWN_LINE_BEGIN,		red },
	{ SCE_MARKDOWN_STRONG1,			keyword },
	{ SCE_MARKDOWN_STRONG2,				keyword },
	{ SCE_MARKDOWN_EM1,				orange },
	{ SCE_MARKDOWN_EM2,			orange },
	{ SCE_MARKDOWN_HEADER1,				magenta },
	{ SCE_MARKDOWN_HEADER2,			builtin },
	{ SCE_MARKDOWN_HEADER3,		keyword },
	{ SCE_MARKDOWN_HEADER4,				orange },
	{ SCE_MARKDOWN_HEADER5,				red },
	{ SCE_MARKDOWN_HEADER6,				yellowgreen },
	{ SCE_MARKDOWN_PRECHAR,    string },
	{ SCE_MARKDOWN_ULIST_ITEM,				red },
	{ SCE_MARKDOWN_BLOCKQUOTE,				string },
	{ SCE_MARKDOWN_STRIKEOUT,				comment },
	{ SCE_MARKDOWN_HRULE,				orange },
	{ SCE_MARKDOWN_LINK,				comment },
	{ SCE_MARKDOWN_CODE,				string },
	{ SCE_MARKDOWN_CODE2,				string },
	{ SCE_MARKDOWN_CODEBK,				string },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- powershell Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_powershell[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		preprocessor },
	{ SCE_C_COMMENTLINE,			preprocessor },
	{ SCE_C_COMMENTDOC,				string },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			builtin },
	{ SCE_C_STRING,				editorTextColor },
	{ SCE_C_CHARACTER,			editorTextColor },
	{ SCE_C_UUID,		keyword },
	{ SCE_C_PREPROCESSOR,				builtin },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				editorTextColor },
	{ SCE_C_WORD2,				string },
	{ SCE_C_COMMENTDOCKEYWORD,				editorTextColor },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				editorTextColor },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				editorTextColor },
	{ SCE_C_TRIPLEVERBATIM,				editorTextColor },
	{ SCE_C_HASHQUOTEDSTRING,				editorTextColor },
	{ SCE_C_PREPROCESSORCOMMENT,				editorTextColor },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				editorTextColor },
	{ SCE_C_TASKMARKER,				editorTextColor },
	{ SCE_C_ESCAPESEQUENCE,				editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- go Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_go[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				editorTextColor },
	{ SCE_C_REGEX,				editorTextColor },
	{ SCE_C_COMMENTLINEDOC,				editorTextColor },
	{ SCE_C_WORD2,				editorTextColor },
	{ SCE_C_COMMENTDOCKEYWORD,				editorTextColor },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				editorTextColor },
	{ SCE_C_GLOBALCLASS,				editorTextColor },
	{ SCE_C_STRINGRAW,				editorTextColor },
	{ SCE_C_TRIPLEVERBATIM,				editorTextColor },
	{ SCE_C_HASHQUOTEDSTRING,				editorTextColor },
	{ SCE_C_PREPROCESSORCOMMENT,				editorTextColor },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				editorTextColor },
	{ SCE_C_USERLITERAL,				editorTextColor },
	{ SCE_C_TASKMARKER,				editorTextColor },
	{ SCE_C_ESCAPESEQUENCE,				editorTextColor },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- inno Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_inno[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		editorTextColor },
	{ SCE_C_COMMENTLINE,			keyword },
	{ SCE_C_COMMENTDOC,				keyword },
	{ SCE_C_NUMBER,				builtin },
	{ SCE_C_WORD,			editorTextColor },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				editorTextColor },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				keyword },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				keyword },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				keyword },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				keyword },
	{ SCE_C_TRIPLEVERBATIM,				keyword },
	{ SCE_C_HASHQUOTEDSTRING,				keyword },
	{ SCE_C_PREPROCESSORCOMMENT,				keyword },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				keyword },
	{ SCE_C_TASKMARKER,				keyword },
	{ SCE_C_ESCAPESEQUENCE,				keyword },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- protobuf Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_protobuf[] =
	{ { SCE_C_DEFAULT,		keyword },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				builtin },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		keyword },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				keyword },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				keyword },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				keyword },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				keyword },
	{ SCE_C_TRIPLEVERBATIM,				keyword },
	{ SCE_C_HASHQUOTEDSTRING,				keyword },
	{ SCE_C_PREPROCESSORCOMMENT,				keyword },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				keyword },
	{ SCE_C_TASKMARKER,				keyword },
	{ SCE_C_ESCAPESEQUENCE,				keyword },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- r Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_r[] =
	{ { SCE_R_DEFAULT,		editorTextColor },
	{ SCE_R_COMMENT,		comment },
	{ SCE_R_KWORD,			keyword },
	{ SCE_R_BASEKWORD,				comment },
	{ SCE_R_OTHERKWORD,				number },
	{ SCE_R_NUMBER,			number },
	{ SCE_R_STRING,				editorTextColor },
	{ SCE_R_STRING2,			editorTextColor },
	{ SCE_R_OPERATOR,		builtin },
	{ SCE_R_IDENTIFIER,				light_orange },
	{ SCE_R_INFIX,				yellow },
	{ SCE_R_INFIXEOL,				yellow },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- autoit Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_autoit[] =
	{ { SCE_C_DEFAULT,		keyword },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				builtin },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		keyword },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				keyword },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				keyword },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				keyword },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				keyword },
	{ SCE_C_TRIPLEVERBATIM,				keyword },
	{ SCE_C_HASHQUOTEDSTRING,				keyword },
	{ SCE_C_PREPROCESSORCOMMENT,				keyword },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				keyword },
	{ SCE_C_TASKMARKER,				keyword },
	{ SCE_C_ESCAPESEQUENCE,				keyword },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- flexlicense Language
	//----------------------------------------------------------------------------
	// color text 
	static SScintillaColors g_rgb_Syntax_flexlicense[] =
	{ { SCE_P_DEFAULT,		editorTextColor },
	{ SCE_P_COMMENTLINE,		preprocessor },
	{ SCE_P_NUMBER,				editorTextColor },
	{ SCE_P_STRING,		    string },
	{ SCE_P_CHARACTER,			string },
	{ SCE_P_WORD,				keyword },
	{ SCE_P_TRIPLE,			preprocessor },
	{ SCE_P_TRIPLEDOUBLE,			preprocessor },
	{ SCE_P_CLASSNAME,			comment },
	{ SCE_P_DEFNAME,			comment },
	{ SCE_P_OPERATOR,			builtin },
	{ SCE_P_IDENTIFIER,		        editorTextColor },
	{ SCE_P_COMMENTBLOCK,		        preprocessor },
	{ SCE_P_STRINGEOL,		        string },
	{ SCE_P_WORD2,				keyword },
	{ SCE_P_DECORATOR,		    editorTextColor },
	{ SCE_P_FSTRING,		    string },
	{ SCE_P_FCHARACTER,		    string },
	{ SCE_P_FTRIPLE,		    string },
	{ SCE_P_FTRIPLEDOUBLE,		string },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- resource Language
	//----------------------------------------------------------------------------
	// color text
	static SScintillaColors g_rgb_Syntax_resource[] =
	{ { SCE_C_DEFAULT,		keyword },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				builtin },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			string },
	{ SCE_C_UUID,		keyword },
	{ SCE_C_PREPROCESSOR,				keyword },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				keyword },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				keyword },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				keyword },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				keyword },
	{ SCE_C_TRIPLEVERBATIM,				keyword },
	{ SCE_C_HASHQUOTEDSTRING,				keyword },
	{ SCE_C_PREPROCESSORCOMMENT,				keyword },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				keyword },
	{ SCE_C_TASKMARKER,				keyword },
	{ SCE_C_ESCAPESEQUENCE,				keyword },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- freebasic Language
	//----------------------------------------------------------------------------
	    
	// color text
	static SScintillaColors g_rgb_Syntax_freebasic[] =
	{ { SCE_C_DEFAULT,		builtin },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			number },
	{ SCE_C_COMMENTDOC,				keyword },
	{ SCE_C_NUMBER,				string },
	{ SCE_C_WORD,			comment },
	{ SCE_C_STRING,				orange },
	{ SCE_C_CHARACTER,			editorTextColor },
	{ SCE_C_UUID,		instance },
	{ SCE_C_PREPROCESSOR,				comment },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,    editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				orange },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				yellow },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };

	//----------------------------------------------------------------------------
	//---- vcxproject Language
	//----------------------------------------------------------------------------
	// text color 
	static SScintillaColors g_rgb_Syntax_vcxproject[] =
	{ { SCE_C_DEFAULT,		editorTextColor },
	{ SCE_C_COMMENT,		comment },
	{ SCE_C_COMMENTLINE,			comment },
	{ SCE_C_COMMENTDOC,				comment },
	{ SCE_C_NUMBER,				number },
	{ SCE_C_WORD,			keyword },
	{ SCE_C_STRING,				string },
	{ SCE_C_CHARACTER,			definition },
	{ SCE_C_UUID,				instance },
	{ SCE_C_PREPROCESSOR,				comment },
	{ SCE_C_OPERATOR,				builtin },
	{ SCE_C_IDENTIFIER,				editorTextColor },
	{ SCE_C_STRINGEOL,			editorTextColor },
	{ SCE_C_VERBATIM,				yellow },
	{ SCE_C_REGEX,				yellow },
	{ SCE_C_COMMENTLINEDOC,				comment },
	{ SCE_C_WORD2,				instance },
	{ SCE_C_COMMENTDOCKEYWORD,				comment },
	{ SCE_C_COMMENTDOCKEYWORDERROR,				comment },
	{ SCE_C_GLOBALCLASS,				magenta },
	{ SCE_C_STRINGRAW,				comment },
	{ SCE_C_TRIPLEVERBATIM,				comment },
	{ SCE_C_HASHQUOTEDSTRING,				comment },
	{ SCE_C_PREPROCESSORCOMMENT,				comment },
	{ SCE_C_PREPROCESSORCOMMENTDOC,				comment },
	{ SCE_C_USERLITERAL,				comment },
	{ SCE_C_TASKMARKER,				comment },
	{ SCE_C_ESCAPESEQUENCE,				comment },
	{ -1,						0 } };
}