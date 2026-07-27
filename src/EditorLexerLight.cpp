#include "stdafx.h"
#include "Editor.h"
#include "EditorColorLight.h"
#include "EditorLanguageData.h"
#include "EditorLexerLight.h"
#include "EditorDatabase.h"
#include "AppUtil.h"
#include "AppSettings.h"

void EditorLexerLight::LoadLexer(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl, const CString& czLexer)
{
	if (!pDatabase) return;
	if (czLexer == "ada")
		Init_ada_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "asm")
		Init_asm_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "inno")
		Init_inno_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "bash")
		Init_bash_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "batch")
		Init_batch_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "cmake")
		Init_cmake_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "makefile")
		Init_makefile_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "cpp")
		Init_cpp_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "c")
		Init_c_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "css")
		Init_css_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "erlang")
		Init_erlang_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "fortran")
		Init_fortran_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "hypertext")
		Init_html_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "lua")
		Init_lua_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "matlab")
		Init_matlab_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "pascal")
		Init_pascal_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "perl")
		Init_perl_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "phpscript")
		Init_php_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "powershell")
		Init_powershell_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "python")
		Init_python_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "ruby")
		Init_ruby_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "rust")
		Init_rust_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "golang")
		Init_golang_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "sql")
		Init_sql_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "tcl")
		Init_tcl_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "vb")
		Init_vb_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "verilog")
		Init_verilog_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "vhdl")
		Init_vhdl_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "xml")
		Init_xml_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "kix")
		Init_json_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "java")
		Init_java_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "javascript")
		Init_javascript_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "typescript")
		Init_typescript_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "cs")
		Init_cshape_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "markdown")
		Init_markdown_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "protobuf")
		Init_protobuf_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "r")
		Init_r_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "FLEXlm")
		Init_flexlicense_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "Resource")
		Init_resource_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "autoit")
		Init_autoit_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "freebasic")
		Init_freebasic_Editor(pDatabase, pEditorCtrl);
	else if (czLexer == "vcxproj")
		Init_vcxproject_Editor(pDatabase, pEditorCtrl);
	else
		Init_text_Editor(pEditorCtrl);
}

void EditorLexerLight::Init_ada_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("ada");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("ada"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_ada[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_ada[i].iItem, EditorColorLight::g_rgb_Syntax_ada[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "ada");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_asm_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("asm");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("asm"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_asm[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_asm[i].iItem, EditorColorLight::g_rgb_Syntax_asm[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "asm");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_inno_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("asm");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("inno"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_inno[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_inno[i].iItem, EditorColorLight::g_rgb_Syntax_inno[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "inno");
	CString strKeywords = AppUtils::StdToCString(EditorLanguageData::GetKeywords("inno"));
	pDatabase->SetLanguageAutoComplete(strKeywords);
}

void EditorLexerLight::Init_bash_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("bash");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("bash"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_bash[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageBashFontStyle(EditorColorLight::g_rgb_Syntax_bash[i].iItem, EditorColorLight::g_rgb_Syntax_bash[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "bash");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_batch_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("batch");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("batch"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_batch[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_batch[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_batch[i].rgb;
		if (iItem == SCE_C_COMMENTLINE || iItem == SCE_C_COMMENTDOC || iItem == SCE_C_WORD2)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_C_COMMENT)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "batch");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_c_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("c"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_c[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_c[i].iItem, EditorColorLight::g_rgb_Syntax_c[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "c");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_cmake_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cmake");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("cmake"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_cmake[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_cmake[i].iItem, EditorColorLight::g_rgb_Syntax_cmake[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "cmake");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_makefile_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("makefile");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("makefile"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_makefile[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_makefile[i].iItem, EditorColorLight::g_rgb_Syntax_makefile[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "makefile");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_cpp_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("cpp"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_cpp[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_cpp[i].iItem, EditorColorLight::g_rgb_Syntax_cpp[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "cpp");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_cshape_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("cs"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_cs[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_cs[i].iItem, EditorColorLight::g_rgb_Syntax_cs[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "cs");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_css_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("css");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("css"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_css[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_css[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_css[i].rgb;
		if (iItem == SCE_CSS_TAG || iItem == SCE_CSS_PSEUDOCLASS || iItem == SCE_CSS_OPERATOR || iItem == SCE_CSS_IMPORTANT)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_CSS_CLASS)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_CSS_IDENTIFIER2)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "css");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_erlang_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("erlang");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("erlang"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_erlang[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_erlang[i].iItem, EditorColorLight::g_rgb_Syntax_erlang[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "erlang");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_fortran_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("fortran");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("fortran"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_fortran[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_fortran[i].iItem, EditorColorLight::g_rgb_Syntax_fortran[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "fortran");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_html_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("hypertext");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("html"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_html[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_html[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_html[i].rgb;
		/*if (iItem == SCE_H_ATTRIBUTE || iItem == SCE_H_ATTRIBUTEUNKNOWN)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else */if (iItem == SCE_H_TAG || iItem == SCE_H_ENTITY
			|| iItem == SCE_HB_DEFAULT || iItem == SCE_HJA_DEFAULT
			|| iItem == SCE_HBA_IDENTIFIER || iItem == SCE_HB_IDENTIFIER
			|| iItem == SCE_HPHP_OPERATOR || iItem == SCE_HPHP_DEFAULT
			|| iItem == SCE_H_OTHER || iItem == SCE_H_XMLSTART)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "html");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_java_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("java"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_java[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_java[i].iItem, EditorColorLight::g_rgb_Syntax_java[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "java");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_javascript_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("javascript"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_javascript[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_javascript[i].iItem, EditorColorLight::g_rgb_Syntax_javascript[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "javascript");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_typescript_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("typescript"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_typescript[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_typescript[i].iItem, EditorColorLight::g_rgb_Syntax_typescript[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "typescript");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_lua_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("lua");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("lua"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_lua[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_lua[i].iItem, EditorColorLight::g_rgb_Syntax_lua[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "lua");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_matlab_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("matlab");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("matlab"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_matlab[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_matlab[i].iItem, EditorColorLight::g_rgb_Syntax_matlab[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "matlab");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_pascal_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("pascal");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("pascal"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_pascal[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_pascal[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_pascal[i].rgb;
		if (iItem == SCE_PAS_WORD)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_PAS_STRING)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else if (iItem == SCE_PAS_PREPROCESSOR || iItem == SCE_PAS_PREPROCESSOR2)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "pascal");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_perl_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("perl");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("perl"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_perl[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_perl[i].iItem, EditorColorLight::g_rgb_Syntax_perl[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "perl");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_php_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("php"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_php[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_php[i].iItem, EditorColorLight::g_rgb_Syntax_php[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "php");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_powershell_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("powershell");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("powershell"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_powershell[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_powershell[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_powershell[i].rgb;
		if (iItem == SCE_C_WORD || iItem == SCE_C_PREPROCESSOR || iItem == SCE_C_OPERATOR ||
			iItem == SCE_C_UUID || iItem == SCE_C_NUMBER)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_C_COMMENT || iItem == SCE_C_COMMENTLINE || iItem == SCE_C_COMMENTDOC || iItem == SCE_C_WORD2)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "powershell");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_python_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("python");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("python"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_python[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_python[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_python[i].rgb;
		if (iItem == SCE_P_WORD || iItem == SCE_P_WORD2 || iItem == SCE_P_OPERATOR)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		if (iItem == SCE_P_CLASSNAME || iItem == SCE_P_DEFNAME)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else if (iItem == SCE_P_STRING || iItem == SCE_P_CHARACTER)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "python");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_ruby_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("ruby");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("ruby"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_ruby[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_ruby[i].iItem, EditorColorLight::g_rgb_Syntax_ruby[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "ruby");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}
void EditorLexerLight::Init_rust_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("rust");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("rust"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_rust[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_rust[i].iItem, EditorColorLight::g_rgb_Syntax_rust[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "rust");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_golang_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("go"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_go[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_go[i].iItem, EditorColorLight::g_rgb_Syntax_go[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "go");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_sql_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("sql");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("sql"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_sql[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_sql[i].iItem, EditorColorLight::g_rgb_Syntax_sql[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "sql");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_tcl_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("tcl");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("tcl"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_tcl[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_tcl[i].iItem, EditorColorLight::g_rgb_Syntax_tcl[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "tcl");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_vb_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("vb");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("vb"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_vb[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_vb[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_vb[i].rgb;
		if (iItem == SCE_C_COMMENTDOC)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_C_NUMBER)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "vb");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_verilog_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("verilog");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("verilog"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_verilog[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_verilog[i].iItem, EditorColorLight::g_rgb_Syntax_verilog[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "verilog");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}
void EditorLexerLight::Init_vhdl_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("vhdl");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("vhdl"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_vhdl[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_vhdl[i].iItem, EditorColorLight::g_rgb_Syntax_vhdl[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "vhdl");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_xml_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("xml");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("xml"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_html[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_html[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_html[i].rgb;
		/*if (iItem == SCE_H_ATTRIBUTE || iItem == SCE_H_ATTRIBUTEUNKNOWN)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else */if (iItem == SCE_H_TAG || iItem == SCE_H_ENTITY
			|| iItem == SCE_HB_DEFAULT || iItem == SCE_HJA_DEFAULT
			|| iItem == SCE_HBA_IDENTIFIER || iItem == SCE_HB_IDENTIFIER
			|| iItem == SCE_HPHP_OPERATOR || iItem == SCE_HPHP_DEFAULT
			|| iItem == SCE_H_OTHER)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "xml");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_json_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("json"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_json[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_json[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_json[i].rgb;
		if (iItem == SCE_JSON_DEFAULT || iItem == SCE_JSON_ESCAPESEQUENCE || iItem == SCE_JSON_COMPACTIRI
			|| iItem == SCE_JSON_PROPERTYNAME || iItem == SCE_JSON_OPERATOR || iItem == SCE_JSON_KEYWORD)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "json");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_markdown_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("markdown");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("markdown"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_markdown[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_markdown[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_markdown[i].rgb;
		if (iItem == SCE_MARKDOWN_PRECHAR || iItem == SCE_MARKDOWN_BLOCKQUOTE || iItem == SCE_MARKDOWN_CODE
			|| iItem == SCE_MARKDOWN_CODE2 || iItem == SCE_MARKDOWN_CODEBK)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else if (iItem == SCE_MARKDOWN_STRIKEOUT || iItem == SCE_MARKDOWN_LINK)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "markdown");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_protobuf_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("protobuf"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_protobuf[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_protobuf[i].iItem, EditorColorLight::g_rgb_Syntax_protobuf[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "protobuf");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_r_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("r");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("r"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_r[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_r[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_r[i].rgb;
		if (iItem == SCE_R_KWORD || iItem == SCE_R_BASEKWORD)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		if (iItem == SCE_R_STRING || iItem == SCE_R_STRING2)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "r");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_flexlicense_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("python");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("flexlicense"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_flexlicense[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_flexlicense[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_flexlicense[i].rgb;
		if (iItem == SCE_P_WORD || iItem == SCE_P_WORD2 || iItem == SCE_P_OPERATOR)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_P_STRING || iItem == SCE_P_CHARACTER)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "flexlicense");
	CString strKeywords = AppUtils::StdToCString(EditorLanguageData::GetKeywords("resource"));
	pDatabase->SetLanguageAutoComplete(strKeywords);
}

void EditorLexerLight::Init_resource_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("resource"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_resource[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_resource[i].iItem, EditorColorLight::g_rgb_Syntax_resource[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "resource");
	CString strKeywords = AppUtils::StdToCString(EditorLanguageData::GetKeywords("resource"));
	pDatabase->SetLanguageAutoComplete(strKeywords);
}

void EditorLexerLight::Init_autoit_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("autoit"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_autoit[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_autoit[i].iItem, EditorColorLight::g_rgb_Syntax_autoit[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "autoit");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_freebasic_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("freebasic");
	pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("freebasic"));
	for (int i = 0; EditorColorLight::g_rgb_Syntax_freebasic[i].iItem != -1; i++)
	{
		auto iItem = EditorColorLight::g_rgb_Syntax_freebasic[i].iItem;
		auto rgb = EditorColorLight::g_rgb_Syntax_freebasic[i].rgb;
		if (iItem == SCE_C_COMMENTDOC)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETBOLD, iItem, 1);
		}
		else if (iItem == SCE_C_NUMBER)
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
			pEditorCtrl->DoCommand(SCI_STYLESETITALIC, iItem, 1);
		}
		else
		{
			pEditorCtrl->SetColorForStyle(iItem, rgb, AppSettingMgr.m_AppThemeColor);
		}
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "freebasic");
	pEditorCtrl->LoadExternalSettings(pDatabase);
}

void EditorLexerLight::Init_vcxproject_Editor(CLanguageDatabase* pDatabase, CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("cpp");
	for (int i = 0; EditorColorLight::g_rgb_Syntax_vcxproject[i].iItem != -1; i++)
	{
		pEditorCtrl->SetLanguageCFontStyle(EditorColorLight::g_rgb_Syntax_vcxproject[i].iItem, EditorColorLight::g_rgb_Syntax_vcxproject[i].rgb);
	}
	EditorLanguageData::ApplyLanguageMetadata(pDatabase, "vcxproject");
}

void EditorLexerLight::Init_text_Editor(CEditorCtrl* pEditorCtrl)
{
	pEditorCtrl->SetLexer("plaintext");
	pEditorCtrl->SetKeywords("");
}