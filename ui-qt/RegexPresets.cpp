/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "RegexPresets.h"

const std::vector<RegexPresets::SPreset>& RegexPresets::All()
{
	static const std::vector<SPreset> presets = {
		{ "Any character",						".",			"a" },
		{ "Any character, zero or more times",	".*",			"abc" },
		{ "Any character, one or more times",	".+",			"abc" },
		{ "Any character in the set abc",		"[abc]",		"xbz" },

		// THE ORIGINAL HAS `^[abc]` HERE, under the label "not in the set".
		// That is a different pattern entirely - start-of-line followed by one
		// of a, b, c. The negated set is [^abc], and a preset that does not do
		// what its own label says is worth fixing rather than transcribing.
		{ "Any character NOT in the set abc",	"[^abc]",		"abcz" },

		{ "Any character in the range a-f",		"[a-f]",		"xez" },
		{ "Any word character",					"\\w",			"a" },
		{ "Any decimal digit",					"\\d",			"7" },

		// The original's "any whitespace" is [^\S\r\n], which is horizontal
		// whitespace - it deliberately excludes line breaks. The label is
		// corrected to say so rather than the pattern changed, because the
		// pattern is the useful one when searching within a line.
		{ "Any whitespace except line breaks",	"[^\\S\\r\\n]",	"a b" },

		{ "Preceding character, zero or one time",	"be?",		"be" },
		{ "Preceding character, zero or more times",	"be*",	"bee" },
		{ "Preceding character, one or more times",	"be+",		"bee" },
		{ "Three consecutive digits",			"\\d\\d\\d",	"x123x" },
		// \b IS NOT SUPPORTED by Scintilla's basic engine - measured, 0
		// matches on "hello". Its word boundaries are \< and \>, which do
		// work, so the preset offers those and says which end it means.
		{ "Start of a word",					"\\<",			"hello" },
		{ "End of a word",						"\\>",			"hello" },
		{ "Start of line",						"^",			"abc" },
		// "Match a line break" IS DROPPED. Scintilla's regex is LINE-ORIENTED -
		// a search never spans one - so no pattern for a line break can match,
		// in either frontend. Offering it would be a promise the engine cannot
		// keep. ^ and $ below are the usable part of the same idea.
		{ "End of line",						"$",			"abc" },

		// "Capture and implicitly number" in the original - and its own label
		// carries the typo "impicitily".
		// The original illustrates capture with 'dog|cat', but ALTERNATION IS
		// NOT SUPPORTED either - measured, 0 matches for |, \|, and (a|b)
		// alike. The group itself works, so the preset keeps the capture and
		// drops the alternation its engine cannot run.
		{ "Capture a subexpression",			"\\(dog\\)",	"a dog" },
		{ "Backreference the first capture",	"\\(.\\)\\1",	"aa" },

		{ "Space or tab",						"[ \\t]",		"a b" },
		{ "Any numeric character",				"\\d",			"7" },
		{ "A quoted string",					"\"[^\"]*\"",	"say \"hi\"" },
		{ "A hexadecimal number",				"0[xX][0-9a-fA-F]+",	"0xBEEF" },
		{ "An integer or decimal",				"[0-9]*\\.*[0-9]+",	"3.14" },
		{ "A C identifier",						"[A-Za-z_][A-Za-z0-9_]*",	"my_var" },
	};
	return presets;
}
