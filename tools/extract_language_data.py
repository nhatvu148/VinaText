#!/usr/bin/env python3
"""
Extract editor language + theme data out of the C++ static tables into JSON.

Source of truth (before this change):
    src/EditorColorLight.h    namespace EditorColorLight
    src/EditorColorDark.h     namespace EditorColorDark

Both headers duplicate an identical block of per-language metadata (name,
extension, comment delimiters, keyword blob) and differ only in colour data.
This script deduplicates the shared half into data/languages.json and emits the
per-theme colour data into data/theme-light.json and data/theme-dark.json.

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
        for field in ("name", "extension", "commentLine", "commentStart", "commentEnd", "keywords"):
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


LEXER_SOURCES = [os.path.join(ROOT, "src", "EditorLexerLight.cpp"),
                 os.path.join(ROOT, "src", "EditorLexerDark.cpp")]


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
        if not verify_only:
            print("Nothing to regenerate. Edit the JSON directly.\n")
        on_disk = json.load(open(os.path.join(DATA_DIR, "languages.json"), encoding="utf-8"))
        on_disk_themes = {n: json.load(open(os.path.join(DATA_DIR, "theme-%s.json" % n),
                                            encoding="utf-8")) for n in ("light", "dark")}
        failures = json_only_checks(on_disk, on_disk_themes)
        failures += check_lexer_call_sites(on_disk)
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
