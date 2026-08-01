#!/usr/bin/env python3
"""
Write the encoding/line-ending fixtures ui-qt/'s --selftest opens.

These exist for one assertion: that opening a file and saving it unmodified
produces the same bytes. That is not a nicety. src/Textfile.cpp detects encodings
with uchardet and is a Wave 2 file nobody has pulled yet, so the Qt frontend
trusts only a byte-order mark and falls back to Latin-1 for bytes that are not
valid UTF-8. An editor that guesses wrong and then WRITES is the failure a user
notices after their file is already overwritten.

Generated rather than committed: the interesting part of each file is its exact
bytes, and a repository is the one place those are most likely to be quietly
normalised - by an editor, by .gitattributes, by a linter.

Usage:
    python3 tools/make_selftest_fixtures.py <output-directory>
"""

import os
import sys


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip())
        return 2
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)

    files = {
        # CRLF line endings behind a UTF-8 BOM. Both must survive a save.
        "crlf-bom.cpp": b"\xef\xbb\xbf" + (
            "// comment\r\n"
            "int main(int argc, char** argv)\r\n"
            "{\r\n"
            # 1234 is load-bearing: autocomplete skips an all-digit prefix
            # (m_bAutoCompleteIgnoreNumbers ships TRUE), and without a word in
            # the corpus that actually STARTS with a digit, the check for that
            # passes whether the rule is implemented or not.
            "\tint value = 1234;\r\n"
            "\treturn 0;\r\n"
            "}\r\n").encode("utf-8"),

        # UTF-16 LE with a BOM, and non-ASCII text - saving this as UTF-8 would
        # double its size and change every byte.
        "utf16.py": b"\xff\xfe" + (
            "# tiếng Việt comment\n"
            "def main():\n"
            "    return 'xin chào'\n").encode("utf-16-le"),

        # Not valid UTF-8. The fallback has to be lossless, or these bytes change.
        "latin1.md": "# Caf\xe9 heading\n\nSome text with \xa9 and \xf1.\n".encode("latin-1"),

        # No trailing newline: the classic off-by-one in a save path.
        "no-trailing-newline.py": b"def f():\n    return 1",

        # Tag matching. Every line here is a case the algorithm's own comments
        # call out, so the file is small and every part of it is load-bearing:
        #
        #   - <item> nested inside <item>, same name, so a matcher that takes the
        #     first "</item>" it finds pairs the wrong two;
        #   - note="a>b", where the '>' is data. Scintilla's lexer styles it as
        #     SCE_H_DOUBLESTRING and the search has to skip it, or the open tag
        #     appears to end four characters early;
        #   - <empty />, self-closing, which matches itself and has no close tag;
        #   - <![CDATA[ ... ]]> containing text that looks exactly like a tag.
        "tags.xml": (
            "<root>\n"
            "    <item id=\"a\">\n"
            "        <name>first</name>\n"
            "    </item>\n"
            "    <item id=\"b\" note=\"a>b\">\n"
            "        <item>\n"
            "            <name>nested</name>\n"
            "        </item>\n"
            "    </item>\n"
            "    <empty />\n"
            "    <data><![CDATA[<item>not a tag</item>]]></data>\n"
            "</root>\n").encode("utf-8"),

        # URL hotspots. Each line is one rule from src/StringHelper.cpp's
        # scanner: the five supported schemes, a scheme that is not supported, a
        # scheme that is not at a delimiter, the trailing punctuation that is
        # stripped, and the two bracket cases that differ. The accented word puts
        # multi-byte UTF-8 before a URL, which is where a byte offset and a
        # character offset stop agreeing.
        "urls.md": (
            "Visit https://example.com/docs for details.\n"
            "Mail mailto:someone@example.com about it.\n"
            "Fetch ftp://files.example.com/pub/x.tar.gz now.\n"
            "Open file:///etc/hosts here.\n"
            "Not gopher://old.example.com and not nothttp://x.\n"
            "Bracketed (see http://example.com/x) but kept http://example.com/a_(b)\n"
            "Sau khi chào: http://example.com/á xong.\n").encode("utf-8"),
    }

    for name, payload in sorted(files.items()):
        path = os.path.join(out, name)
        with open(path, "wb") as f:
            f.write(payload)
        print("wrote %-28s %5d bytes" % (path, len(payload)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
