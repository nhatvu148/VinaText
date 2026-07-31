#!/usr/bin/env python3
"""
Extract editor language + theme data out of the C++ static tables into JSON.

Source of truth (before this change):
    src/EditorColorLight.h    namespace EditorColorLight
    src/EditorColorDark.h     namespace EditorColorDark
    src/EditorCommonDef.h     namespace EditorLanguageDef   (file extensions)
    src/EditorLexer{Light,Dark}.cpp                         (Lexilla lexer names)

Both headers duplicate an identical block of per-language metadata (name,
extension, comment delimiters, keyword blob) and differ only in colour data.
This script deduplicates the shared half into data/languages.json and emits the
per-theme colour data into data/theme-light.json and data/theme-dark.json.

It also emits the two fields that decide which lexer a file gets - "extensions"
and "lexer" - which live in neither colour header. See parse_lexer_dispatch().

It is also the verification tool: --verify re-reads the generated JSON and
asserts it reproduces the C++ tables exactly. That is the only correctness
signal available until the Windows build runs in CI, since the MFC app cannot
be compiled on macOS/Linux.

Usage:
    python3 tools/extract_language_data.py            # generate + verify
    python3 tools/extract_language_data.py --verify   # verify only
"""

import ast
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIGHT_H = os.path.join(ROOT, "src", "EditorColorLight.h")
DARK_H = os.path.join(ROOT, "src", "EditorColorDark.h")
COMMON_DEF_H = os.path.join(ROOT, "src", "EditorCommonDef.h")
LEXER_SOURCES = [os.path.join(ROOT, "src", "EditorLexerLight.cpp"),
                 os.path.join(ROOT, "src", "EditorLexerDark.cpp")]
EDITOR_CPP = os.path.join(ROOT, "src", "Editor.cpp")
SCILEXER_H = os.path.join(ROOT, "include", "scintilla", "SciLexer.h")
# Runtime data lives under Packages/, matching the existing layout the app already
# uses (PathUtils::GetVinaTextPackagePath -> "Packages\"). Today that tree only
# exists per-configuration under bin/x64/{Debug,Release}/Packages and the two copies
# have already diverged; this root-level copy is the single source of truth, and
# Phase 0's CMake is expected to copy it to the output directory.
DATA_DIR = os.path.join(ROOT, "Packages", "data-packages")

META_FIELDS = {
    "language": "name",
    "extention": "extension",       # sic - the original spelling
    "commentline": "commentLine",
    "commentStart": "commentStart",
    "commentEnd": "commentEnd",
}

C_ESCAPES = {
    "\\\\": "\\", '\\"': '"', "\\'": "'", "\\n": "\n",
    "\\t": "\t", "\\r": "\r", "\\0": "\0",
}


def unescape(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            pair = s[i:i + 2]
            if pair in C_ESCAPES:
                out.append(C_ESCAPES[pair])
                i += 2
                continue
        out.append(s[i])
        i += 1
    return "".join(out)


def join_literals(blob):
    """Concatenate adjacent C string literals, as the compiler would."""
    return "".join(unescape(m) for m in re.findall(r'"((?:[^"\\]|\\.)*)"', blob))


def read(path):
    with open(path, encoding="utf-8", errors="strict") as f:
        return f.read()


def parse_sce_constants():
    """SCE_* symbol -> integer value, from Scintilla's own header."""
    out = {}
    for m in re.finditer(r"^#define\s+(SCE_[A-Z0-9_]+)\s+(\d+)", read(SCILEXER_H), re.M):
        out[m.group(1)] = int(m.group(2))
    return out


def parse_header(path):
    src = read(path)

    palette = {}
    for m in re.finditer(
        r"const\s+COLORREF\s+(\w+)\s*=\s*RGB\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\s*;", src
    ):
        name, r, g, b = m.group(1), *map(int, m.groups()[1:])
        palette[name] = (r, g, b)

    meta = {}
    for m in re.finditer(
        r'static\s+CString\s+g_str_(\w+?)_(language|extention|commentline|commentStart|commentEnd)'
        r'\s*=\s*_T\(\s*"((?:[^"\\]|\\.)*)"\s*\)\s*;',
        src,
    ):
        lang, field, value = m.group(1), m.group(2), unescape(m.group(3))
        meta.setdefault(lang, {})[META_FIELDS[field]] = value

    keywords = {}
    for m in re.finditer(
        r'static\s+const\s+char\s*\*\s*g_(\w+?)_KeyWords\s*=\s*((?:\s*"(?:[^"\\]|\\.)*")+)\s*;', src
    ):
        keywords[m.group(1)] = join_literals(m.group(2))

    styles = {}
    for m in re.finditer(
        r"static\s+SScintillaColors\s+g_rgb_Syntax_(\w+)\s*\[\]\s*=\s*\{(.*?)\}\s*;", src, re.S
    ):
        lang, body = m.group(1), m.group(2)
        entries = []
        for sm in re.finditer(r"\{\s*(SCE_[A-Z0-9_]+|-1)\s*,\s*(\w+)\s*\}", body):
            symbol, colour = sm.group(1), sm.group(2)
            if symbol == "-1":          # sentinel terminator, not data
                continue
            entries.append((symbol, colour))
        styles[lang] = entries

    # g_str_plantext is a lone UI label, not a language block
    plaintext = None
    pm = re.search(r'static\s+CString\s+g_str_plantext\s*=\s*_T\(\s*"([^"]*)"\s*\)\s*;', src)
    if pm:
        plaintext = pm.group(1)

    return dict(palette=palette, meta=meta, keywords=keywords, styles=styles,
                plaintext=plaintext)


def parse_lexer_dispatch():
    """How the MFC app decides which lexer a file gets — three name spaces deep.

    Reading it takes three files, because a language is written three different
    ways along the path from a filename to a styled document:

      1. src/EditorCommonDef.h    arrLangExtensions[i]  <->  arrLexerNames[i]
         "json"                   maps to the VinaText lexer token "kix"
      2. src/EditorLexerDark.cpp  LoadLexer() dispatches that token to an
         Init_<x>_Editor function                       "kix" -> Init_json_Editor
      3. that function names two more things:
              SetLexer("cpp")                  <- the LEXILLA lexer name
              ApplyLanguageMetadata(_, "json") <- the languages.json id

    Only (3) is of any use to a second frontend: ui-qt/ needs the Lexilla name to
    call CreateLexer, and the id to find keywords and a theme style table. The
    VinaText token in (1) is an MFC dispatch detail and is deliberately not
    emitted — it would be a fourth name for a thing that already has three.

    Returns {language id: {"extensions": "py|pyw", "lexer": "python"}} plus a list
    of notes about what the C++ says that a reader would not expect.
    """
    common = read(COMMON_DEF_H)

    def array_body(name):
        m = re.search(r"\b%s\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;" % name, common, re.S)
        if m is None:
            raise SystemExit("%s: cannot find %s[]" % (COMMON_DEF_H, name))
        return m.group(1)

    extensions = re.findall(r'_T\(\s*"((?:[^"\\]|\\.)*)"\s*\)', array_body("arrLangExtensions"))
    tokens = re.findall(r'"((?:[^"\\]|\\.)*)"', array_body("arrLexerNames"))
    if len(tokens) < len(extensions):
        raise SystemExit("arrLexerNames has %d entries, fewer than arrLangExtensions' %d - "
                         "they are indexed in parallel" % (len(tokens), len(extensions)))

    # Both theme files carry the same dispatch; parse each and require agreement.
    per_source = {}
    for path in LEXER_SOURCES:
        src = read(path)
        chain = re.search(r"::LoadLexer\(.*?\n\{(.*?)\n\}", src, re.S)
        if chain is None:
            raise SystemExit("%s: cannot find LoadLexer" % path)
        # token -> Init function stem
        dispatch = dict(re.findall(r'czLexer\s*==\s*"([^"]+)"\s*\)\s*\n\s*Init_(\w+)_Editor',
                                   chain.group(1)))
        # Init function stem -> (Lexilla lexer name, languages.json id, colour table)
        initialisers = {}
        for m in re.finditer(r"void\s+\w+::Init_(\w+)_Editor\([^)]*\)\s*\n\{(.*?)\n\}", src, re.S):
            stem, body = m.group(1), strip_comments(m.group(2))
            lexers = re.findall(r'SetLexer\(\s*"([^"]*)"\s*\)', body)
            ids = re.findall(r'ApplyLanguageMetadata\(\s*\w+\s*,\s*"([^"]*)"\s*\)', body)
            if len(lexers) != 1 or len(ids) > 1:
                raise SystemExit("%s: Init_%s_Editor has %d SetLexer and %d "
                                 "ApplyLanguageMetadata calls; expected 1 and 0-1"
                                 % (os.path.basename(path), stem, len(lexers), len(ids)))
            # Which g_rgb_Syntax_* table it walks. Usually the language's own, but
            # NOT always - Init_xml_Editor walks html's, because Scintilla's xml
            # lexer emits the SCE_H_* family. A frontend that assumes the table is
            # named after the language colours .xml files from a table the
            # shipping app never applies.
            tables = set(re.findall(r"g_rgb_Syntax_(\w+)\s*\[", body))
            if len(tables) > 1:
                raise SystemExit("%s: Init_%s_Editor walks %d different colour tables (%s)"
                                 % (os.path.basename(path), stem, len(tables), sorted(tables)))
            initialisers[stem] = (lexers[0], ids[0] if ids else None,
                                  tables.pop() if tables else None)
        per_source[os.path.basename(path)] = (dispatch, initialisers)

    (name_a, a), (name_b, b) = sorted(per_source.items())
    if a != b:
        raise SystemExit("%s and %s do not dispatch identically - the two themes have "
                         "drifted apart" % (name_a, name_b))
    dispatch, initialisers = a

    notes = []
    out = {}

    # Every language with an initialiser gets its Lexilla name, whether or not any
    # file extension reaches it: `makefile` is selected by filename, not extension.
    for stem, (lexer, lang_id, table) in sorted(initialisers.items()):
        if lang_id is None:
            continue                        # Init_text_Editor - the plain-text fallback
        if table is None:
            raise SystemExit("Init_%s_Editor sets language metadata but walks no colour "
                             "table" % stem)
        if lang_id in out and out[lang_id]["lexer"] != lexer:
            # Two initialisers, one language id, two different Lexilla lexers. One
            # field cannot hold both, and silently keeping either would change how
            # half of that language's files are highlighted.
            raise SystemExit("language %r is initialised twice with different lexers "
                             "(%r and %r); languages.json cannot represent that"
                             % (lang_id, out[lang_id]["lexer"], lexer))
        out[lang_id] = {"extensions": "", "lexer": lexer, "styleTable": table}
        if table != lang_id:
            notes.append("%s is coloured from the %r table, not its own" % (lang_id, table))

    for index, ext in enumerate(extensions):
        token = tokens[index]
        stem = dispatch.get(token)
        if stem is None:
            raise SystemExit("arrLexerNames[%d] = %r reaches no branch of LoadLexer, so "
                             "files matching %r fall through to plain text"
                             % (index, token, ext))
        lang_id = initialisers[stem][1]
        if lang_id is None:
            raise SystemExit("extensions %r dispatch to Init_%s_Editor, which sets no "
                             "language metadata" % (ext, stem))
        # MERGED, not overwritten. GetLexerNameFromExtension walks every row and
        # returns on the first token that matches, so two rows reaching one
        # language means both rows' extensions select it. Overwriting would drop
        # the first row's extensions, and nothing downstream could tell: --verify
        # would compare the JSON against the same overwritten mapping and pass.
        if out[lang_id]["extensions"]:
            notes.append("%s claims two extension rows, %r and %r - merged, which is "
                         "what the MFC scan does" % (lang_id, out[lang_id]["extensions"], ext))
            out[lang_id]["extensions"] += "|" + ext
        else:
            out[lang_id]["extensions"] = ext
        # Kept only so parse_fold_markers can re-key its chain; never emitted.
        out[lang_id]["token"] = token
        if token != lang_id:
            notes.append("%s is written %r in arrLexerNames" % (lang_id, token))

    # Worth surfacing, and deliberately NOT corrected here: the JSON records what
    # the shipping app does, so a second frontend reproduces it rather than
    # quietly diverging. See doc/PORTING.md §6d.
    for lang_id, entry in sorted(out.items()):
        if entry["lexer"] != lang_id and entry["extensions"]:
            notes.append("%s files are lexed by Lexilla's %r lexer"
                         % (lang_id, entry["lexer"]))
    return out, notes


def strip_comments(text):
    """Remove // and /* */ comments, leaving string literals alone.

    Needed because Init_html_Editor carries a commented-out attribute rule that
    ends `else */if (...)`. A scanner that sees that `else` reads the chain
    structure wrongly; worse, a scanner that sees the commented-out block would
    make SCE_H_ATTRIBUTE bold and italic in ui-qt/ when Windows renders it plain.
    The disabled rule must stay disabled.
    """
    out = []
    i = 0
    while i < len(text):
        two = text[i:i + 2]
        if two == "//":
            i = text.find("\n", i)
            if i < 0:
                break
        elif two == "/*":
            end = text.find("*/", i + 2)
            i = len(text) if end < 0 else end + 2
        elif text[i] in "\"'":
            quote = text[i]
            out.append(text[i])
            i += 1
            while i < len(text) and text[i] != quote:
                if text[i] == "\\":
                    out.append(text[i])
                    i += 1
                if i < len(text):
                    out.append(text[i])
                    i += 1
            if i < len(text):
                out.append(text[i])
                i += 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def _split_top_level_branches(body):
    """The if / else if / else chains of a loop body, as (kind, condition, block).

    A brace scanner rather than a regex: the blocks nest, and a regex that
    "works" on today's 13 functions is a regex that silently mis-reads the
    fourteenth. Returns branches in source order; `kind` is 'if', 'elseif' or
    'else', and a new 'if' starts a new chain.
    """
    branches = []
    i = 0
    while i < len(body):
        m = re.compile(r"\b(else\s+if|else|if)\b").search(body, i)
        if m is None:
            break
        kind = {"if": "if", "else": "else", "else if": "elseif"}[
            re.sub(r"\s+", " ", m.group(1))]

        cursor = m.end()
        condition = ""
        if kind in ("if", "elseif"):
            open_paren = body.find("(", cursor)
            if open_paren < 0:
                break
            depth, j = 0, open_paren
            while j < len(body):
                if body[j] == "(":
                    depth += 1
                elif body[j] == ")":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            condition = body[open_paren + 1:j]
            cursor = j + 1

        open_brace = body.find("{", cursor)
        if open_brace < 0 or body[cursor:open_brace].strip():
            # A braceless branch body. None exist today; refusing beats guessing.
            raise SystemExit("unbraced %s branch in a lexer initialiser - this parser "
                             "only handles braced blocks" % kind)
        depth, j = 0, open_brace
        while j < len(body):
            if body[j] == "{":
                depth += 1
            elif body[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        branches.append((kind, condition, body[open_brace + 1:j]))
        i = j + 1
    return branches


ATTRIBUTE_MESSAGES = {"SCI_STYLESETBOLD": "bold",
                      "SCI_STYLESETITALIC": "italic",
                      "SCI_STYLESETUNDERLINE": "underline"}


def parse_style_attributes(styles_by_language, sce):
    """Which styles the MFC lexer initialisers render bold, italic or underlined.

    This lives in neither colour header: it is an if-chain inside each
    Init_<x>_Editor, applied per style constant as the colour table is walked.
    The JSON carries colour and nothing else, so ui-qt/ renders Python keywords
    at normal weight where the Windows build renders them bold.

    Extraction SIMULATES the chains rather than pattern-matching them, because
    the shapes differ: Init_python_Editor has two independent `if` chains, so its
    SCE_P_WORD takes the first chain's bold and then falls to the second chain's
    else. Anything that assumed one chain would get that wrong and look right.

    Attributes are theme-independent - the 13 functions are byte-identical
    between EditorLexerLight.cpp and EditorLexerDark.cpp once the namespace is
    normalised - so they belong with the language, not with a theme.

    Matching is NUMERIC, because `iItem == SCE_H_TAG` is a comparison of numbers
    and the initialisers exploit that. Init_xml_Editor tests SCE_H_* constants
    while walking a table written in SCE_C_* names: at runtime Scintilla's xml
    lexer emits the H family, so the H names are the meaningful ones and the
    table's C names are labels on the same integers. Matching by name instead
    would attribute nine styles that the running program never touches, and miss
    the three it does. See doc/PORTING.md 6e.
    """
    per_source = {}
    for path in LEXER_SOURCES:
        src = read(path)
        found = {}
        for m in re.finditer(r"void\s+\w+::Init_(\w+)_Editor\([^)]*\)\s*\n\{(.*?)\n\}", src, re.S):
            stem, body = m.group(1), m.group(2)
            if not any(msg in body for msg in ATTRIBUTE_MESSAGES):
                continue

            language = re.search(r"ApplyLanguageMetadata\(\s*\w+\s*,\s*\"([^\"]*)\"\s*\)", body)
            table = re.search(r"g_rgb_Syntax_(\w+)\s*\[", body)
            if language is None or table is None:
                raise SystemExit("Init_%s_Editor applies style attributes but names no "
                                 "language or no colour table" % stem)

            loop = re.search(r"for\s*\([^)]*\)\s*\{(.*)\n\t\}", strip_comments(body), re.S)
            if loop is None:
                raise SystemExit("Init_%s_Editor: cannot find the colour-table loop" % stem)
            branches = _split_top_level_branches(loop.group(1))

            # Group the branches back into chains: a new 'if' starts one.
            chains, current = [], []
            for branch in branches:
                if branch[0] == "if" and current:
                    chains.append(current)
                    current = []
                current.append(branch)
            if current:
                chains.append(current)

            # Now run every style constant of this language's table through them.
            symbols = [symbol for symbol, _ in styles_by_language.get(table.group(1), [])]
            # NB: table.group(1), not the language id - see parse_lexer_dispatch.
            if not symbols:
                raise SystemExit("Init_%s_Editor walks g_rgb_Syntax_%s, which has no styles"
                                 % (stem, table.group(1)))
            for symbol in symbols:
                if symbol not in sce:
                    raise SystemExit("style %r in g_rgb_Syntax_%s is not defined in "
                                     "SciLexer.h" % (symbol, table.group(1)))
                applied = set()
                for chain in chains:
                    for kind, condition, block in chain:
                        wanted = set()
                        for name in re.findall(r"\bSCE_[A-Z0-9_]+\b", condition):
                            if name not in sce:
                                raise SystemExit("Init_%s_Editor tests %r, which is not "
                                                 "defined in SciLexer.h" % (stem, name))
                            wanted.add(sce[name])
                        matches = (kind == "else" or sce[symbol] in wanted)
                        if not matches:
                            continue
                        for message, attribute in ATTRIBUTE_MESSAGES.items():
                            # `..., iItem, 1)` sets it; the codebase never passes 0,
                            # and a 0 would mean the opposite, so check the value.
                            for value in re.findall(
                                    r"%s\s*,\s*iItem\s*,\s*(\d+)" % message, block):
                                if value == "1":
                                    applied.add(attribute)
                                else:
                                    raise SystemExit(
                                        "Init_%s_Editor sets %s to %s, which this "
                                        "extraction does not model" % (stem, message, value))
                        break                   # first matching branch of the chain wins
                if applied:
                    found.setdefault(language.group(1), {})[symbol] = sorted(applied)
        per_source[os.path.basename(path)] = found

    (name_a, a), (name_b, b) = sorted(per_source.items())
    if a != b:
        raise SystemExit("%s and %s apply different style attributes - the two themes have "
                         "drifted apart" % (name_a, name_b))
    return a


def parse_indent_guides(dispatch):
    """Which indentation-guide mode a language gets: "lookforward" or "lookboth".

    CEditorCtrl::LoadEditorSettings gives Python SC_IV_LOOKFORWARD and everything
    else SC_IV_LOOKBOTH (src/Editor.cpp:209-215) - guides that stop at a blank
    line suit a language with no closing brace.

    Keyed by the VinaText TOKEN again, and the trap is not hypothetical here:
    `flexlicense` is lexed by Lexilla's python lexer but its token is "FLEXlm",
    so it takes LOOKBOTH. Keying on the Lexilla name would silently change how
    .lic files are drawn.
    """
    src = strip_comments(read(EDITOR_CPP))
    # Anchored BACKWARDS from the call, not forwards from an `if`: the same
    # function has an earlier m_strLexerName chain (the fold markers), and a
    # forward scan swallows it and everything between.
    call = src.find("SCI_SETINDENTATIONGUIDES")
    if call < 0:
        raise SystemExit("%s: cannot find SCI_SETINDENTATIONGUIDES" % EDITOR_CPP)
    start = src.rfind("if", 0, call)
    if start < 0:
        raise SystemExit("%s: SCI_SETINDENTATIONGUIDES has no enclosing if" % EDITOR_CPP)
    end = src.find("SC_IV_", src.find("else", call))
    end = src.find("}", src.find("\n", end)) + 1

    branches = _split_top_level_branches(src[start:end])
    if len(branches) != 2 or branches[1][0] != "else":
        raise SystemExit("the indentation-guide chain is no longer one if/else - this "
                         "extraction models exactly that shape")

    def mode_of(block_text):
        m = re.search(r"SCI_SETINDENTATIONGUIDES\s*,\s*SC_IV_(\w+)", block_text)
        if m is None:
            raise SystemExit("an indentation-guide branch sets no SC_IV_ mode")
        return m.group(1).lower()

    special = mode_of(branches[0][2])
    default = mode_of(branches[1][2])
    tokens = set(re.findall(r'_T\(\s*"([^"]*)"\s*\)', branches[0][1]))

    out = {}
    for lang_id, entry in dispatch.items():
        out[lang_id] = special if entry.get("token") in tokens else default
    return out, default


def parse_fold_markers(dispatch):
    """What a folded block shows when it is collapsed: " { ... } ", " < ... > " or " --- ".

    CEditorCtrl::LoadEditorSettings picks one with an if-chain over the VinaText
    lexer TOKEN (src/Editor.cpp:193-207) - the third of the three name spaces in
    6d, the one deliberately not carried into the JSON. So the chain is read here
    and re-keyed onto language ids, which is the only name a second frontend has.

    Keying on the Lexilla lexer name instead would be wrong and would look right:
    `go`, `protobuf`, `autoit`, `resource` and `vcxproject` are all lexed as cpp,
    and none of them is in the chain's list - they fold with " --- ".

    Returns {language id: marker string}, omitting the default.
    """
    src = strip_comments(read(EDITOR_CPP))
    markers = {}
    for m in re.finditer(r'#define\s+(FOLDED_MARKER_\w+)\s+"([^"]*)"', read(COMMON_DEF_H)):
        markers[m.group(1)] = m.group(2)
    if len(markers) != 3:
        raise SystemExit("expected 3 FOLDED_MARKER_* defines in EditorCommonDef.h, found %d"
                         % len(markers))

    # The one chain that sets the DEFAULT fold text; SCI_TOGGLEFOLDSHOWTEXT
    # elsewhere is a per-fold override and not what a newly opened file uses.
    block = re.search(r"(if\s*\(\s*m_strLexerName\s*==.*?SCI_SETDEFAULTFOLDDISPLAYTEXT.*?"
                      r"FOLDED_MARKER_TEXT[^;]*;\s*\})", src, re.S)
    if block is None:
        raise SystemExit("%s: cannot find the SCI_SETDEFAULTFOLDDISPLAYTEXT chain" % EDITOR_CPP)

    token_to_marker = {}
    for branch in re.finditer(r"(if\s*\((.*?)\)|else)\s*\{([^}]*)\}", block.group(1), re.S):
        condition, body = branch.group(2) or "", branch.group(3)
        used = re.search(r"(FOLDED_MARKER_\w+)", body)
        if used is None:
            continue
        for token in re.findall(r'_T\(\s*"([^"]*)"\s*\)', condition):
            token_to_marker[token] = markers[used.group(1)]

    # dispatch maps id -> {..., "token": ...}; invert it to re-key.
    out = {}
    for lang_id, entry in dispatch.items():
        token = entry.get("token")
        if token is not None and token in token_to_marker:
            out[lang_id] = token_to_marker[token]
    if not out:
        raise SystemExit("the fold-marker chain matched no language token at all")
    return out, markers["FOLDED_MARKER_TEXT"]


_DISPATCH_CACHE = []


def cached_dispatch():
    """parse_lexer_dispatch(), parsed once - it reads three files."""
    if not _DISPATCH_CACHE:
        _DISPATCH_CACHE.append(parse_lexer_dispatch())
    return _DISPATCH_CACHE[0]


def dispatch_fields_for(lang_id):
    mapping, _ = cached_dispatch()
    entry = mapping.get(lang_id, {"extensions": "", "lexer": "", "styleTable": ""})
    fold, default_marker = cached_fold_markers()
    guides, _ = cached_indent_guides()
    return {"extensions": entry["extensions"], "lexer": entry["lexer"],
            "styleTable": entry.get("styleTable") or "",
            "indentGuides": guides.get(lang_id, "lookboth"),
            # Emitted for every language, default included: a frontend should not
            # have to know a hidden default to render a folded block.
            "foldMarker": fold.get(lang_id, default_marker)}


_FOLD_CACHE = []


def cached_fold_markers():
    if not _FOLD_CACHE:
        mapping, _ = cached_dispatch()
        _FOLD_CACHE.append(parse_fold_markers(mapping))
    return _FOLD_CACHE[0]


_GUIDES_CACHE = []


def cached_indent_guides():
    if not _GUIDES_CACHE:
        mapping, _ = cached_dispatch()
        _GUIDES_CACHE.append(parse_indent_guides(mapping))
    return _GUIDES_CACHE[0]


_ATTRIBUTE_CACHE = []


def cached_style_attributes():
    """{language id: [{"style", "value", "bold"/"italic"/"underline"}]}, from the C++."""
    if not _ATTRIBUTE_CACHE:
        sce = parse_sce_constants()
        parsed = parse_style_attributes(parse_header(LIGHT_H)["styles"], sce)
        out = {}
        for lang, styles in parsed.items():
            rows = []
            for symbol, attributes in styles.items():
                if symbol not in sce:
                    raise SystemExit("style %r in %r is not defined in SciLexer.h"
                                     % (symbol, lang))
                row = {"style": symbol, "value": sce[symbol]}
                for attribute in attributes:
                    row[attribute] = True
                rows.append(row)
            out[lang] = sorted(rows, key=lambda r: (r["value"], r["style"]))
        _ATTRIBUTE_CACHE.append(out)
    return _ATTRIBUTE_CACHE[0]


def dispatch_sources_present():
    """True while the extension table and the lexer initialisers are still C++.

    They are, and are expected to stay that way until Phase 4 rewrites the lexer
    initialisers - so unlike the metadata tables, this data has a live source to
    round-trip against.
    """
    return all(os.path.exists(p) for p in [COMMON_DEF_H] + LEXER_SOURCES)


def sync_dispatch_fields(languages_path):
    """Rewrite only "extensions" and "lexer" in languages.json, from the C++.

    Not a full regeneration: the metadata and keyword blobs left the C++ in an
    earlier change and the JSON is their only source now, so this must not touch
    them. It edits the two fields that still have a C++ original and leaves every
    other byte of the document alone.
    """
    mapping, notes = cached_dispatch()
    for note in notes:
        print("note: lexer dispatch: %s" % note)

    with open(languages_path, encoding="utf-8") as f:
        doc = json.load(f)

    attributes = cached_style_attributes()
    changed = []
    for entry in doc.get("languages", []):
        for field, value in sorted(dispatch_fields_for(entry["id"]).items()):
            if entry.get(field) != value:
                changed.append("%s.%s -> %r" % (entry["id"], field, value))
            entry[field] = value
        # Bold/italic/underline live in the Init_* if-chains, in neither colour
        # header, and are identical between the two themes - so they belong to
        # the language rather than to a theme. Absent means "nothing special".
        rows = attributes.get(entry["id"], [])
        if entry.get("styleAttributes", []) != rows:
            changed.append("%s.styleAttributes -> %d style(s)" % (entry["id"], len(rows)))
        if rows:
            entry["styleAttributes"] = rows
        else:
            entry.pop("styleAttributes", None)

    unknown = sorted(set(mapping) - {e["id"] for e in doc.get("languages", [])})
    if unknown:
        raise SystemExit("the lexer initialisers name languages absent from languages.json: %s"
                         % unknown)

    with open(languages_path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print("synced extensions/lexer from the C++: %d field(s) changed" % len(changed))
    for line in changed[:10]:
        print("  " + line)
    return changed


def check_lexer_dispatch(languages_doc):
    """The two dispatch fields must still reproduce the C++ exactly.

    ui-qt/ picks a lexer from these; ui-mfc/ picks one from arrLangExtensions and
    the Init_*_Editor functions. Two frontends highlighting the same file
    differently is precisely the drift this file exists to prevent, and nothing
    else in CI would notice - the Windows job compiles the MFC app but never runs
    it, and no job runs the Qt one against a real .cpp.
    """
    problems = []
    by_id = {e["id"]: e for e in languages_doc.get("languages", []) if isinstance(e, dict)}

    if dispatch_sources_present():
        mapping, _ = cached_dispatch()
        for lang in sorted(mapping):
            want = dispatch_fields_for(lang)
            got = by_id.get(lang, {})
            for field in ("extensions", "lexer", "styleTable", "foldMarker",
                          "indentGuides"):
                if got.get(field) != want[field]:
                    problems.append("%s.%s: JSON says %r, the C++ says %r"
                                    % (lang, field, got.get(field), want[field]))
        for lang, entry in sorted(by_id.items()):
            if lang not in mapping and (entry.get("extensions") or entry.get("lexer")):
                problems.append("%s carries extensions/lexer but no C++ initialiser sets them"
                                % lang)

        for lang, rows in sorted(cached_style_attributes().items()):
            got = by_id.get(lang, {}).get("styleAttributes", [])
            if got != rows:
                problems.append("%s.styleAttributes: JSON has %d style(s), the C++ has %d"
                                % (lang, len(got), len(rows)))
        for lang, entry in sorted(by_id.items()):
            if entry.get("styleAttributes") and lang not in cached_style_attributes():
                problems.append("%s carries styleAttributes but the C++ sets none" % lang)

        # Re-counted from the raw header with a different expression than
        # parse_lexer_dispatch() uses. Counting TOKENS rather than rows is the
        # point: a row that gets dropped or merged away changes the token count,
        # while a row count can be matched by a table that lost an extension.
        raw_array = re.search(r"arrLangExtensions\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;",
                              read(COMMON_DEF_H), re.S)
        cpp_tokens = []
        if raw_array is None:
            problems.append("cannot find arrLangExtensions[] in EditorCommonDef.h")
        else:
            for row in re.findall(r'_T\(\s*"([^"]*)"\s*\)', raw_array.group(1)):
                cpp_tokens.extend(t for t in row.split("|") if t)
        json_tokens = [t for e in by_id.values() for t in e.get("extensions", "").split("|") if t]
        if sorted(cpp_tokens) != sorted(json_tokens):
            missing = sorted(set(cpp_tokens) - set(json_tokens))
            extra = sorted(set(json_tokens) - set(cpp_tokens))
            problems.append("extension tokens differ from the C++: %d in the header, %d in "
                            "the JSON; missing from the JSON=%s, not in the header=%s"
                            % (len(cpp_tokens), len(json_tokens), missing, extra))
        n_mapped = sum(1 for e in by_id.values() if e.get("extensions"))
    else:
        print("note: the C++ extension table is gone - languages.json is now its only source")

    # A token claimed twice is not a parse error: the lookup is first-match-wins,
    # so the later claim is simply unreachable. Say which one loses.
    seen_token = {}
    for entry in languages_doc.get("languages", []):
        for token in entry.get("extensions", "").split("|"):
            if not token:
                continue
            if token in seen_token:
                problems.append("extension %r is claimed by both %r and %r; only %r can win"
                                % (token, seen_token[token], entry["id"], seen_token[token]))
            else:
                seen_token[token] = entry["id"]
        if entry.get("extensions") and not entry.get("lexer"):
            problems.append("%s matches file extensions but names no lexer" % entry["id"])

    # Read by hand out of the C++, independent of every parser above.
    hand_checked = {
        "cpp":      ("cpp|cxx|h|hh|hpp|hxx|cc", "cpp"),
        "python":   ("py|pyw", "python"),
        "html":     ("htm|html|shtml|htt|cfm|tpl|hta", "hypertext"),
        # .json really is lexed as C++ by the shipping app - see doc/PORTING.md §6d.
        "json":     ("json", "cpp"),
        # Selected by filename, never by extension.
        "makefile": ("", "makefile"),
    }
    for lang, want in sorted(hand_checked.items()):
        got = by_id.get(lang)
        if got is None:
            problems.append("hand-checked language %r missing" % lang)
            continue
        actual = (got.get("extensions"), got.get("lexer"))
        if actual != want:
            problems.append("hand-checked %r dispatch: %r != %r" % (lang, actual, want))

    if not problems:
        n_attr_styles = sum(len(e.get("styleAttributes", [])) for e in by_id.values())
        n_attr_langs = sum(1 for e in by_id.values() if e.get("styleAttributes"))
        print("lexer dispatch: %d file extensions over %d languages, matching the C++"
              % (len(seen_token), n_mapped if dispatch_sources_present() else
                 sum(1 for e in by_id.values() if e.get("extensions"))))
        print("style attributes: %d bold/italic style(s) over %d languages"
              % (n_attr_styles, n_attr_langs))
    return problems


def hexcolor(rgb):
    return "#%02X%02X%02X" % rgb


def build():
    light, dark = parse_header(LIGHT_H), parse_header(DARK_H)
    sce = parse_sce_constants()
    problems = []

    # --- the shared half must genuinely be identical, or dedup is unsound ---
    if set(light["meta"]) != set(dark["meta"]):
        problems.append("language sets differ: light-only=%s dark-only=%s" % (
            sorted(set(light["meta"]) - set(dark["meta"])),
            sorted(set(dark["meta"]) - set(light["meta"]))))
    for lang in sorted(set(light["meta"]) & set(dark["meta"])):
        if light["meta"][lang] != dark["meta"][lang]:
            problems.append("metadata differs for %r" % lang)
    if set(light["keywords"]) != set(dark["keywords"]):
        problems.append("keyword language sets differ")
    for lang in sorted(set(light["keywords"]) & set(dark["keywords"])):
        if light["keywords"][lang] != dark["keywords"][lang]:
            problems.append("keywords differ for %r" % lang)
    # --- nothing in the C++ may be silently dropped on the way out ---
    for name, parsed in (("light", light), ("dark", dark)):
        orphan_kw = sorted(set(parsed["keywords"]) - set(parsed["meta"]))
        if orphan_kw:
            problems.append("%s: keyword blobs with no language metadata (would be "
                            "dropped from languages.json): %s" % (name, orphan_kw))

    if problems:
        raise SystemExit("Refusing to deduplicate:\n  " + "\n  ".join(problems))

    # Style tables with no language metadata are preserved in the theme files but
    # have no entry in languages.json. Faithful to the C++, but worth surfacing.
    orphan_styles = sorted(set(light["styles"]) - set(light["meta"]))
    no_keywords = sorted(set(light["meta"]) - set(light["keywords"]))
    empty_keywords = sorted(k for k, v in light["keywords"].items() if not v)
    for label, langs in (("style table but no language metadata", orphan_styles),
                         ("language metadata but no keyword blob", no_keywords),
                         ("keyword blob present but empty", empty_keywords)):
        if langs:
            print("note: %s: %s" % (label, ", ".join(langs)))

    languages = []
    for lang in sorted(light["meta"]):
        entry = {"id": lang}
        entry.update(light["meta"][lang])
        entry.update(dispatch_fields_for(lang))
        entry["keywords"] = light["keywords"].get(lang, "")
        languages.append(entry)

    languages_doc = {
        "_generatedBy": "tools/extract_language_data.py",
        "_source": "src/EditorColorLight.h, src/EditorColorDark.h (identical halves)",
        "plainTextLabel": light["plaintext"] or dark["plaintext"],
        "languages": languages,
    }

    themes = {}
    for name, parsed in (("light", light), ("dark", dark)):
        unknown = sorted({c for entries in parsed["styles"].values()
                          for _, c in entries if c not in parsed["palette"]})
        if unknown:
            raise SystemExit("theme %s references undefined colours: %s" % (name, unknown))
        themes[name] = {
            "_generatedBy": "tools/extract_language_data.py",
            "_source": "src/EditorColor%s.h" % name.capitalize(),
            "name": name,
            "palette": {k: hexcolor(v) for k, v in sorted(parsed["palette"].items())},
            "languages": {
                lang: [{"style": sym, "value": sce[sym], "color": col}
                       for sym, col in parsed["styles"][lang]]
                for lang in sorted(parsed["styles"])
            },
        }

    return languages_doc, themes, light, dark


def verify(languages_doc, themes, light, dark):
    """Re-derive the C++ tables from the JSON and compare, field by field."""
    failures = []
    by_id = {e["id"]: e for e in languages_doc["languages"]}


    for name, parsed in (("light", light), ("dark", dark)):
        theme = themes[name]

        if set(by_id) != set(parsed["meta"]):
            failures.append("%s: language set mismatch" % name)

        for lang, cpp in parsed["meta"].items():
            got = by_id.get(lang, {})
            for field, want in cpp.items():
                if got.get(field) != want:
                    failures.append("%s/%s.%s: %r != %r" % (name, lang, field, got.get(field), want))
            want_kw = parsed["keywords"].get(lang, "")
            if got.get("keywords", "") != want_kw:
                failures.append("%s/%s.keywords: %d chars != %d chars"
                                % (name, lang, len(got.get("keywords", "")), len(want_kw)))

        for lang, entries in parsed["styles"].items():
            got = theme["languages"].get(lang)
            if got is None:
                failures.append("%s/%s: missing style table" % (name, lang))
                continue
            if len(got) != len(entries):
                failures.append("%s/%s: %d styles != %d" % (name, lang, len(got), len(entries)))
                continue
            for i, (sym, col) in enumerate(entries):
                g = got[i]
                if g["style"] != sym or g["color"] != col:
                    failures.append("%s/%s[%d]: (%s,%s) != (%s,%s)"
                                    % (name, lang, i, g["style"], g["color"], sym, col))
                # the resolved RGB must match the C++ palette entry
                if theme["palette"].get(col) != hexcolor(parsed["palette"][col]):
                    failures.append("%s/%s[%d]: colour %s resolves differently" % (name, lang, i, col))
    return failures


def check_deployed_copies():
    """Packages/ is the single source; the build copies it next to the executable.

    This used to compare Packages/data-packages against hand-maintained copies
    under bin/x64/<config>/Packages. Those copies are gone: src/VinaText.vcxproj
    now has a PostBuildEvent that xcopies Packages/ into $(OutDir), so there is
    one source and no copy to drift.

    What remains worth checking is that the source tree still holds the files the
    application loads by name, because a rename or a deletion here is silent until
    something fails to find its data at runtime.
    """
    problems = []
    required = ["data-packages/languages.json",
                "data-packages/theme-light.json",
                "data-packages/theme-dark.json",
                "data-packages/syntax-highlight-file-extension.dat",
                "data-packages/all-file-extension.dat",
                "data-packages/file-format-description.dat"]
    pkg = os.path.join(ROOT, "Packages")
    for rel in required:
        if not os.path.exists(os.path.join(pkg, rel)):
            problems.append("Packages/%s is missing - the app loads it by name" % rel)

    # The old bin/ copies must not be COMMITTED again, or the drift returns. Test
    # what git tracks, not what is on disk: after a build the output directory
    # legitimately contains this data - that is the point of the PostBuildEvent.
    try:
        tracked = subprocess.run(
            ["git", "ls-files", "bin/x64/*/Packages/*"],
            cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
    except (OSError, subprocess.CalledProcessError):
        tracked = []                      # not a git checkout; nothing to assert
    if tracked:
        problems.append(
            "%d files under bin/x64/*/Packages are committed - that tree is a build "
            "output now, written by the PostBuildEvent. Untrack them: %s%s"
            % (len(tracked), ", ".join(tracked[:3]), " ..." if len(tracked) > 3 else ""))

    if not problems:
        print("Packages/: %d files, single source, deployed by the build"
              % sum(len(f) for _, _, f in os.walk(pkg)))
    return problems


def json_only_checks(languages_doc, themes):
    """Checks that still hold once the C++ tables are gone and JSON is authoritative.

    These no longer compare against anything - there is nothing left to compare
    against - so they assert shape and known-good values instead.
    """
    # Nothing below may subscript unvalidated input. This function is the validator
    # that runs once the JSON is authoritative and there is no C++ left to compare
    # against, so it has to name the bad entry - dying with KeyError: 'id' hides the
    # very thing it exists to find.
    problems = []
    entries = languages_doc.get("languages")
    if not isinstance(entries, list):
        return problems + ["languages.json has no \"languages\" array"]

    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            problems.append("language entry #%d is %s, not an object"
                            % (index, type(entry).__name__))
            continue
        lang_id = entry.get("id") or "<entry #%d, no id>" % index
        if not entry.get("id"):
            problems.append("%s: language entry with empty or missing id" % lang_id)
        for field in ("name", "extension", "extensions", "lexer", "styleTable",
                      "foldMarker", "indentGuides", "commentLine", "commentStart",
                      "commentEnd", "keywords"):
            if field not in entry:
                problems.append("%s: missing field %r" % (lang_id, field))
                continue
            value = entry.get(field)
            if not isinstance(value, str):
                problems.append("%s.%s is %s, expected a string"
                                % (lang_id, field, type(value).__name__))
                continue
            for needle, label in (("_T(", "_T( macro"), ("\n", "newline")):
                if needle in value:
                    problems.append("%s.%s contains a stray %s" % (lang_id, field, label))

    expected = {"cpp": ("cpp", "//", "/*", "*/"), "python": ("py", "#", "", ""),
                "ada": ("ada", "--", "", ""), "bash": ("bash", "#", "", ""),
                "r": ("r", "#", '"', '"')}
    by_id = {e["id"]: e for e in entries if isinstance(e, dict) and e.get("id")}
    for lang, want in expected.items():
        got = by_id.get(lang)
        if got is None:
            problems.append("expected language %r missing" % lang)
            continue
        actual = tuple(got.get(f) for f in
                       ("extension", "commentLine", "commentStart", "commentEnd"))
        if actual != want:
            problems.append("%r: %r != %r" % (lang, actual, want))

    for name, key, want in (("light", "black", "#000000"), ("light", "comment", "#0A6704"),
                            ("dark", "editorTextColor", "#FFFFFF"),
                            ("light", "editorBackground", "#FFFFFF"),
                            ("dark", "editorBackground", "#272822")):
        palette = (themes.get(name) or {}).get("palette")
        if not isinstance(palette, dict):
            problems.append("theme-%s.json has no \"palette\" object" % name)
            continue
        if palette.get(key) != want:
            problems.append("%s palette %r: %s != %s" % (name, key, palette.get(key), want))
    return problems


def source_tables_present():
    """True while src/EditorColor{Light,Dark}.h still declare the metadata tables.

    Once the MFC lexers load their metadata from JSON those declarations are
    deleted, the JSON becomes the source of truth, and every check below that
    compares against the C++ becomes meaningless rather than merely redundant.
    """
    return bool(re.search(r"static\s+CString\s+g_str_\w+", read(LIGHT_H)))


def independent_checks(languages_doc, themes):
    """Checks that deliberately do NOT reuse parse_header() or unescape().

    verify() re-parses the C++ with the very functions used to generate the JSON, so
    a systematic bug shared by both sides - an unhandled escape sequence, say - would
    cancel out and pass. These checks look only at the emitted JSON, plus a handful of
    counts taken from the raw source with independent expressions and a few values
    decoded by hand.
    """
    problems = []

    # 1. Re-decode every _T("...") literal with a DIFFERENT implementation than
    #    unescape() - Python's own string-literal parser - and compare. This is the
    #    check that can actually catch a systematic escape-handling bug, which
    #    re-running our own unescape() on both sides never could.
    #
    #    Note some values legitimately contain a double quote: the C++ really does
    #    say g_str_r_commentStart = _T("\""). Banning quotes outright would be wrong;
    #    decoding independently and comparing is the honest check.
    if not source_tables_present():
        return problems + json_only_checks(languages_doc, themes)

    raw_light = read(LIGHT_H)
    by_id_local = {e["id"]: e for e in languages_doc["languages"]}
    field_of = {"language": "name", "extention": "extension", "commentline": "commentLine",
                "commentStart": "commentStart", "commentEnd": "commentEnd"}
    n_crosschecked = 0
    for m in re.finditer(
        r'g_str_(\w+?)_(language|extention|commentline|commentStart|commentEnd)'
        r'\s*=\s*_T\(\s*("(?:[^"\\]|\\.)*")\s*\)', raw_light
    ):
        lang, field, literal = m.group(1), field_of[m.group(2)], m.group(3)
        try:
            independently_decoded = ast.literal_eval(literal)
        except (ValueError, SyntaxError) as exc:
            problems.append("could not independently decode %s.%s literal %s: %s"
                            % (lang, field, literal, exc))
            continue
        emitted = by_id_local.get(lang, {}).get(field)
        if emitted != independently_decoded:
            problems.append("escape mismatch %s.%s: emitted %r, independently decoded %r"
                            % (lang, field, emitted, independently_decoded))
        n_crosschecked += 1
    if n_crosschecked < 200:
        problems.append("only %d literals cross-checked - the independent decode is not "
                        "covering the file" % n_crosschecked)

    # Structural residue that should never survive regardless of escaping.
    for entry in languages_doc["languages"]:
        for field in ("name", "extension", "commentLine", "commentStart", "commentEnd", "keywords"):
            value = entry.get(field, "")
            for needle, label in (("_T(", "_T( macro"), ("\n", "newline"), ("\r", "carriage return")):
                if needle in value:
                    problems.append("languages.json %s.%s contains a stray %s: %r"
                                    % (entry["id"], field, label, value[:80]))

    # 2. Counts re-derived from the raw source with different expressions than the
    #    ones parse_header() uses.
    raw = read(LIGHT_H)
    n_language_decls = len(re.findall(r"g_str_\w+?_language\s*=", raw))
    if n_language_decls != len(languages_doc["languages"]):
        problems.append("counted %d '_language' declarations but emitted %d languages"
                        % (n_language_decls, len(languages_doc["languages"])))

    n_keyword_decls = len(re.findall(r"g_\w+?_KeyWords\s*=", raw))
    n_keyword_blobs = sum(1 for e in languages_doc["languages"] if e["keywords"])
    # markdown and xml declare a blob but it is empty, so declarations >= non-empty.
    if n_keyword_decls < n_keyword_blobs:
        problems.append("counted %d keyword declarations but emitted %d non-empty blobs"
                        % (n_keyword_decls, n_keyword_blobs))

    for name, path in (("light", LIGHT_H), ("dark", DARK_H)):
        src = read(path)
        n_palette_decls = len(re.findall(r"const\s+COLORREF\s+\w+\s*=", src))
        if n_palette_decls != len(themes[name]["palette"]):
            problems.append("%s: counted %d COLORREF declarations but emitted %d palette entries"
                            % (name, n_palette_decls, len(themes[name]["palette"])))
        # Every '{ SCE_..., colour }' pair in the source, minus the '{ -1, 0 }' terminators.
        n_pairs = len(re.findall(r"\{\s*SCE_[A-Z0-9_]+\s*,", src))
        n_emitted = sum(len(v) for v in themes[name]["languages"].values())
        if n_pairs != n_emitted:
            problems.append("%s: counted %d SCE_ style pairs but emitted %d"
                            % (name, n_pairs, n_emitted))

    # 3. Values decoded by hand, independent of the parser entirely.
    expected = {
        "cpp":    ("cpp", "//", "/*", "*/"),
        "python": ("py", "#", "", ""),
        "ada":    ("ada", "--", "", ""),
        "bash":   ("bash", "#", "", ""),
    }
    by_id = {e["id"]: e for e in languages_doc["languages"]}
    for lang, (ext, line, start, end) in expected.items():
        got = by_id.get(lang)
        if got is None:
            problems.append("hand-checked language %r missing" % lang)
            continue
        actual = (got["extension"], got["commentLine"], got["commentStart"], got["commentEnd"])
        if actual != (ext, line, start, end):
            problems.append("hand-checked %r: %r != %r" % (lang, actual, (ext, line, start, end)))

    # 4. Palette values decoded by hand from the RGB() literals.
    for name, key, want in (("light", "black", "#000000"),
                            ("light", "editorTextColor", "#000000"),
                            ("dark", "editorTextColor", "#FFFFFF"),
                            ("light", "editorIndicatorColor", "#1487E2")):
        got = themes[name]["palette"].get(key)
        if got != want:
            problems.append("hand-checked %s palette %r: %s != %s" % (name, key, got, want))

    return problems


def check_lexer_call_sites(languages_doc):
    """Every language id the MFC lexer initialisers ask for must exist in the JSON.

    EditorLexerLight.cpp / EditorLexerDark.cpp look languages up by string literal:

        pEditorCtrl->SetKeywords(EditorLanguageData::GetKeywords("python"));
        EditorLanguageData::ApplyLanguageMetadata(pDatabase, "python");

    A typo in one of those - or a language dropped from languages.json - would
    compile cleanly and only show up as a silently unhighlighted file at runtime.
    Nothing else in CI can catch that, because CI never runs the application.
    """
    problems = []
    ids = {e["id"] for e in languages_doc["languages"]}
    per_file = {}

    for path in LEXER_SOURCES:
        if not os.path.exists(path):
            continue                        # not on this branch yet
        src = read(path)
        used = set(re.findall(r'EditorLanguageData::GetKeywords\(\s*"([^"]*)"\s*\)', src))
        used |= set(re.findall(
            r'EditorLanguageData::ApplyLanguageMetadata\(\s*\w+\s*,\s*"([^"]*)"\s*\)', src))
        if not used:
            continue
        per_file[os.path.basename(path)] = used
        unknown = sorted(used - ids)
        if unknown:
            problems.append("%s references language ids absent from languages.json: %s"
                            % (os.path.basename(path), unknown))

    # The two theme files are mirrors of each other; a language handled in one but
    # not the other means a lost initialiser.
    if len(per_file) == 2:
        (na, a), (nb, b) = sorted(per_file.items())
        if a != b:
            problems.append("%s and %s disagree: only-%s=%s only-%s=%s"
                            % (na, nb, na, sorted(a - b), nb, sorted(b - a)))
        else:
            print("lexer call sites: %d language ids, identical in both theme files, "
                  "all present in languages.json" % len(a))

    return problems


def main():
    verify_only = "--verify" in sys.argv

    # Once the MFC lexers read their metadata from JSON, the C++ declarations are
    # deleted and the JSON becomes the source of truth. There is then nothing to
    # extract from and nothing to round-trip against, so the tool drops to
    # verifying the data files and their call sites.
    if not source_tables_present():
        print("src/EditorColor{Light,Dark}.h no longer declare the metadata tables -\n"
              "JSON in %s is now the source of truth.\n"
              "Extraction and C++ round-trip are skipped; verifying the data files "
              "and their call sites instead.\n" % os.path.relpath(DATA_DIR, ROOT))
        languages_path = os.path.join(DATA_DIR, "languages.json")
        if not verify_only:
            # Two fields do still have a C++ original, so there is still something
            # to generate; everything else in this file must be edited by hand.
            print("Metadata and keywords: edit the JSON directly.")
            sync_dispatch_fields(languages_path)
            print()
        on_disk = json.load(open(languages_path, encoding="utf-8"))
        on_disk_themes = {n: json.load(open(os.path.join(DATA_DIR, "theme-%s.json" % n),
                                            encoding="utf-8")) for n in ("light", "dark")}
        failures = json_only_checks(on_disk, on_disk_themes)
        failures += check_lexer_call_sites(on_disk)
        failures += check_lexer_dispatch(on_disk)
        failures += check_deployed_copies()
        n_styles = sum(len(v) for t in on_disk_themes.values() for v in t["languages"].values())
        print("languages: %d   keyword blobs: %d   style mappings: %d   palette entries: %d/%d"
              % (len(on_disk["languages"]),
                 sum(1 for e in on_disk["languages"] if e["keywords"]), n_styles,
                 len(on_disk_themes["light"]["palette"]), len(on_disk_themes["dark"]["palette"])))
        if failures:
            print("\nFAILED (%d):" % len(failures))
            for f in failures[:25]:
                print("  " + f)
            return 1
        print("data files and lexer call sites verified")
        return 0

    languages_doc, themes, light, dark = build()

    if not verify_only:
        os.makedirs(DATA_DIR, exist_ok=True)
        outputs = [("languages.json", languages_doc),
                   ("theme-light.json", themes["light"]),
                   ("theme-dark.json", themes["dark"])]
        for fname, doc in outputs:
            path = os.path.join(DATA_DIR, fname)
            with open(path, "w", encoding="utf-8") as f:
                json.dump(doc, f, indent=2, ensure_ascii=False)
                f.write("\n")
            print("wrote %-40s %7d bytes"
                  % (os.path.relpath(path, ROOT), os.path.getsize(path)))

    # verify against what is actually on disk, not the in-memory objects
    on_disk = json.load(open(os.path.join(DATA_DIR, "languages.json"), encoding="utf-8"))
    on_disk_themes = {n: json.load(open(os.path.join(DATA_DIR, "theme-%s.json" % n), encoding="utf-8"))
                      for n in ("light", "dark")}
    failures = verify(on_disk, on_disk_themes, light, dark)
    failures += independent_checks(on_disk, on_disk_themes)
    failures += check_lexer_call_sites(on_disk)
    failures += check_lexer_dispatch(on_disk)

    n_styles = sum(len(v) for t in on_disk_themes.values() for v in t["languages"].values())
    print("\nlanguages: %d   keyword blobs: %d   style mappings: %d   palette entries: %d/%d"
          % (len(on_disk["languages"]),
             sum(1 for e in on_disk["languages"] if e["keywords"]),
             n_styles,
             len(on_disk_themes["light"]["palette"]), len(on_disk_themes["dark"]["palette"])))

    if failures:
        print("\nFAILED (%d):" % len(failures))
        for f in failures[:25]:
            print("  " + f)
        return 1
    print("round-trip verified: JSON reproduces both C++ tables exactly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
