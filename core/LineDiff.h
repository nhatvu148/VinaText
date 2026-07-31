/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Line-by-line alignment of two files: the algorithm behind Path Comparator.
//
// core/ code: UI-free and portable, no MFC, no Win32, no Qt.
//
// WHAT MOVED, AND WHAT DID NOT
//
// src/DiffEngine.cpp's CDiffEngine was two things sharing a class: this
// alignment algorithm, and an HTML report renderer (Serialize / SetTitles /
// SetColorStyles / Escape, plus seven CString colour members). Only the
// algorithm is here. The renderer is presentation and stays in ui-mfc/; the Qt
// frontend will want a different one.
//
// Option filtering - "ignore case", "ignore indentation" - deliberately did NOT
// move either, even though it is only ~20 lines of string work. It is applied
// with CString::MakeLower, and every plausible implementation of that (Win32
// CharLowerBuff, or _wcslwr_s under the locale CommandLine.cpp installs with
// _tsetlocale(LC_ALL, "")) is Unicode- or locale-aware. Core::ToLower is the
// classic locale, so it is ASCII-only by construction - see the long note in
// core/TextTransform.h. Swapping one for the other would silently change which
// lines compare equal in any file containing non-ASCII text with "ignore case"
// enabled. So the frontend filters, and hands the result in.
//
// That is what SDiffSide's two vectors are for: _Compare drives matching,
// _Raw is what the report shows.
//
// The file is LineDiff.h and not DiffEngine.h because src/DiffEngine.h still
// exists and still declares the renderer. A quoted #include resolves against the
// including file's own directory first, so two headers of that name would have
// meant src/DiffEngine.cpp silently including itself instead of this.
//
// WHY THIS IS NOT AN LCS DIFF
//
// It is not Myers, and it is not minimal. It is a single forward pass that, for
// each line of f1, searches forward in f2 for the first line with an equal
// token AND an equal string. That is what shipped, and the output of a minimal
// diff would differ on real files, so the algorithm is preserved rather than
// improved. core/tests/TestLineDiff.cpp pins its actual behaviour, including
// the asymmetry between how it reports adds and deletes.

#pragma once

#include <string>
#include <vector>

namespace Core
{
	enum ELineStatus
	{
		LineNormal = 0,
		LineChanged = 1,
		LineAdded = 2,
		LineDeleted = 3
	};

	struct SDiffLine
	{
		std::wstring	_Text;
		ELineStatus		_Status;

		SDiffLine() : _Status(LineNormal) {}
		SDiffLine(const std::wstring& strText, ELineStatus eStatus)
			: _Text(strText), _Status(eStatus) {}
	};

	// One side of a comparison.
	//
	//   _Raw      the lines as they should appear in the report
	//   _Compare  the same lines after the frontend has applied its ignore-case /
	//             ignore-indentation options; these drive matching
	//
	// The two must be the same length. When no options are set they are equal,
	// which is what CFilePartition::GetRawLine and GetLine already do.
	struct SDiffSide
	{
		std::vector<std::wstring> _Raw;
		std::vector<std::wstring> _Compare;

		bool IsWellFormed() const { return _Raw.size() == _Compare.size(); }
	};

	// Aligns f1 (the old version) against f2 (the new one), producing two
	// sequences of equal length: blanks are inserted on the side that is missing
	// a line, so the caller can render them side by side.
	//
	// Returns false only if a side is malformed (_Raw and _Compare of different
	// lengths). CDiffEngine::Diff returned BOOL and could only ever return TRUE.
	bool DiffLines(const SDiffSide& f1, const SDiffSide& f2,
		std::vector<SDiffLine>& f1Out, std::vector<SDiffLine>& f2Out);
}
