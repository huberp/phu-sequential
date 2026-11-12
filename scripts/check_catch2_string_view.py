#!/usr/bin/env python3
"""Guard against the Catch2 string_view linker error (see .github/instructions/cpp.instructions.md, item 11).

Catch2 v3 declares Catch::StringMaker<std::string_view>, but on MSVC the convert() definition is not
linked, so any assertion that stringifies a std::string_view fails with LNK2019/LNK1120 on Windows.

This script finds every std::string_view identifier declared under lib/ and tests/, then flags any
CHECK*/REQUIRE*/WARN*/INFO/CAPTURE assertion that uses one of those identifiers without wrapping the
operand in static_cast<std::string>. Usage: check_catch2_string_view.py [repo_root]
Exit status: 0 = clean, 1 = violations found.
"""

import pathlib
import re
import sys

SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}
# Data members and variables only (ending in ';', '=' or '{'); function parameters are not tracked.
DECLARATION = re.compile(r"std::string_view\s+(\w+)\s*[;={]")
ASSERTION_START = re.compile(r"\b((?:CHECK|REQUIRE|WARN|INFO|CAPTURE)\w*)\s*\(")
SAFE_CAST = "static_cast<std::string>"


def source_files(root: pathlib.Path, folder: str):
    base = root / folder
    if not base.is_dir():
        return []
    return sorted(p for p in base.rglob("*") if p.suffix in SOURCE_SUFFIXES)


def collect_string_view_names(files):
    names = set()
    for path in files:
        names.update(DECLARATION.findall(path.read_text(encoding="utf-8", errors="replace")))
    return names


def assertion_texts(text: str):
    """Yield (line_number, full_assertion_text) for each assertion macro call, across lines."""
    for match in ASSERTION_START.finditer(text):
        depth = 0
        end = None
        for index in range(match.end() - 1, len(text)):
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
                if depth == 0:
                    end = index + 1
                    break
        if end is None:
            continue
        line_number = text.count("\n", 0, match.start()) + 1
        yield line_number, text[match.start():end]


def find_violations(root: pathlib.Path):
    view_names = collect_string_view_names(source_files(root, "lib") + source_files(root, "tests"))
    if not view_names:
        return []
    identifier = re.compile(r"\b(" + "|".join(sorted(map(re.escape, view_names))) + r")\b")
    violations = []
    for path in source_files(root, "tests"):
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, assertion in assertion_texts(text):
            if identifier.search(assertion) and SAFE_CAST not in assertion:
                used = sorted(set(identifier.findall(assertion)))
                first_line = assertion.splitlines()[0].strip()
                violations.append(f"{path.relative_to(root)}:{line_number}: {first_line} (string_view: {', '.join(used)})")
    return violations


def main():
    root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent.parent
    violations = find_violations(root)
    if violations:
        print("Catch2 string_view guard: assertions stringify std::string_view (breaks Windows link).")
        print("Fix: wrap the operand, e.g. CHECK(static_cast<std::string>(obj.name) == \"value\").")
        for violation in violations:
            print("  " + violation)
        return 1
    print("Catch2 string_view guard: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
