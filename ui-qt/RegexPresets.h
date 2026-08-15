/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// The regex helper list from src/ComboboxRegexHelper.cpp, in the find bar.
//
// THE ONE PIECE OF FindDlg WORTH PORTING. Everything else that dialog adds over
// the find bar is find-in-files - Search Scope, Search All, File Filter,
// Specific Path, Exclude Sub Folder, and two results-pane options - and
// find-in-files needs SearchResultWindow and PathResultWindow, both deferred by
// D10. This list is independent of all of it. See doc/PORTING.md 6s.
//
// EVERY PRESET CARRIES A SAMPLE IT MUST MATCH, and the self-test runs each one
// through the editor's own search. Offering a pattern is a promise that it
// works here, and both frontends use plain SCFIND_REGEXP - Scintilla's basic
// engine - which does not have lookahead, named groups, or .NET character-class
// subtraction. Several of the originals use all three.

#pragma once

// NO Qt HEADER. Every field is a const char* - the labels are marked with
// QT_TRANSLATE_NOOP in the .cpp, which expands to the bare literal, and the
// lookup happens at the call site. <QString> was here and unused.
#include <vector>

namespace RegexPresets
{
	struct SPreset
	{
		const char* _Label;
		const char* _Pattern;
		// A string this pattern must find something in. Empty means the preset
		// is offered but not checkable that way - there are none today, and a
		// new one with an empty sample should be justified rather than waved
		// through.
		const char* _Sample;
	};

	// In the original's order, minus what its own regex engine cannot run.
	const std::vector<SPreset>& All();
}
