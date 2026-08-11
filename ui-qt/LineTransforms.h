/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The seventeen text-transform commands, as data.
//
// src/EditorView.cpp spends 1,174 lines on these because every one repeats the
// same skeleton: validate, fetch text or selection, split on the EOL, map a
// per-line function, rebuild, write back. CEditorWidget::ApplyLineTransform is
// that skeleton, so what is left here is the part that actually differs - a
// menu label, a prompt, and a few lines of per-line logic.
//
// Kept out of MainWindow.cpp because it is a table, and because the per-line
// logic is the only part worth reading side by side. See doc/PORTING.md 6p.

#pragma once

#include "EditorWidget.h"
#include "TransformDialog.h"

#include <QString>
#include <vector>

namespace LineTransforms
{
	struct SCommand
	{
		const char* _MenuLabel;
		CTransformDialog::SSpec _Prompt;
		// Builds the per-line transform from what the user typed. Returns an
		// empty function to refuse the input, having already explained why via
		// strErrorOut - which is the MFC's "[Error] Inputs are empty!" box.
		CEditorWidget::FLineTransform (*_Build)(const CTransformDialog& dialog,
			QString& strErrorOut);
	};

	// Every command, in the order the MFC's menus present them.
	const std::vector<SCommand>& All();

	// Roman numerals, transcribed from AppUtils::DecimalToRomanNumerals
	// (src/AppUtil.cpp:138). Public so the self-test can check it directly
	// rather than through a document.
	QString ToRoman(int nValue);
}
