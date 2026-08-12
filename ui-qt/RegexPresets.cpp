/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "RegexPresets.h"

// QT_TRANSLATE_NOOP MARKS A LABEL FOR lupdate WITHOUT TRANSLATING IT HERE. The
// table is built once and lives for the process; the lookup happens where the
// menu is filled, or a language change would never reach it. The macro expands
// to the bare literal, so it costs nothing at run time and exists purely to be
// seen by the extractor - which cannot follow a const char* into tr().
//
// SPELLED OUT AT EVERY ENTRY, and not behind a tidier local macro, because
// lupdate is a parser and does not expand user macros. A `#define PRESET(x)`
// wrapping this reads better and extracts NOTHING - measured, 156 strings
// before and 156 after. It is the same way LineTransforms' own Tr() helper
// hides 53 strings from the extractor (doc/PORTING.md 6s). The context must
// match the one CFindBar looks up under.

const std::vector<RegexPresets::SPreset>& RegexPresets::All()
{
	static const std::vector<SPreset> presets = {
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any character"),
		  ".",                      "a" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any character, zero or more times"),
		  ".*",                     "abc" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any character, one or more times"),
		  ".+",                     "abc" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any character in the set abc"),
		  "[abc]",                  "xbz" },

		// THE ORIGINAL HAS `^[abc]` HERE, under the label "not in the set".
		// That is a different pattern entirely - start-of-line followed by one
		// of a, b, c. The negated set is [^abc], and a preset that does not do
		// what its own label says is worth fixing rather than transcribing.
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any character NOT in the set abc"),
		  "[^abc]",                 "abcz" },

		{ QT_TRANSLATE_NOOP("RegexPresets", "Any character in the range a-f"),
		  "[a-f]",                  "xez" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any word character"),
		  "\\w",                    "a" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any decimal digit"),
		  "\\d",                    "7" },

		// The original's "any whitespace" is [^\S\r\n], which is horizontal
		// whitespace - it deliberately excludes line breaks. The label is
		// corrected to say so rather than the pattern changed, because the
		// pattern is the useful one when searching within a line.
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any whitespace except line breaks"),
		  "[^\\S\\r\\n]",           "a b" },

		{ QT_TRANSLATE_NOOP("RegexPresets", "Preceding character, zero or one time"),
		  "be?",                    "be" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Preceding character, zero or more times"),
		  "be*",                    "bee" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Preceding character, one or more times"),
		  "be+",                    "bee" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Three consecutive digits"),
		  "\\d\\d\\d",              "x123x" },
		// \b IS NOT SUPPORTED by Scintilla's basic engine - measured, 0
		// matches on "hello". Its word boundaries are \< and \>, which do
		// work, so the preset offers those and says which end it means.
		{ QT_TRANSLATE_NOOP("RegexPresets", "Start of a word"),
		  "\\<",                    "hello" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "End of a word"),
		  "\\>",                    "hello" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Start of line"),
		  "^",                      "abc" },
		// "Match a line break" IS DROPPED. Scintilla's regex is LINE-ORIENTED -
		// a search never spans one - so no pattern for a line break can match,
		// in either frontend. Offering it would be a promise the engine cannot
		// keep. ^ and $ below are the usable part of the same idea.
		{ QT_TRANSLATE_NOOP("RegexPresets", "End of line"),
		  "$",                      "abc" },

		// "Capture and implicitly number" in the original - and its own label
		// carries the typo "impicitily".
		// The original illustrates capture with 'dog|cat', but ALTERNATION IS
		// NOT SUPPORTED either - measured, 0 matches for |, \|, and (a|b)
		// alike. The group itself works, so the preset keeps the capture and
		// drops the alternation its engine cannot run.
		{ QT_TRANSLATE_NOOP("RegexPresets", "Capture a subexpression"),
		  "\\(dog\\)",              "a dog" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Backreference the first capture"),
		  "\\(.\\)\\1",             "aa" },

		{ QT_TRANSLATE_NOOP("RegexPresets", "Space or tab"),
		  "[ \\t]",                 "a b" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "Any numeric character"),
		  "\\d",                    "7" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "A quoted string"),
		  "\"[^\"]*\"",             "say \"hi\"" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "A hexadecimal number"),
		  "0[xX][0-9a-fA-F]+",      "0xBEEF" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "An integer or decimal"),
		  "[0-9]*\\.*[0-9]+",       "3.14" },
		{ QT_TRANSLATE_NOOP("RegexPresets", "A C identifier"),
		  "[A-Za-z_][A-Za-z0-9_]*", "my_var" },
	};
	return presets;
}
