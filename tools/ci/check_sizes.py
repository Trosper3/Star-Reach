#!/usr/bin/env python3
"""Size caps -- architecture.md section 2.2.

    File               600 lines      StarReach2's worst: SpaceFlight.cpp at 12,947
    Function            80 lines      StarReach2's worst: Update() at 2,379
    Mode-class members  25            StarReach2's worst: SpaceFlight.h at 466

Hard failure, never a warning. A warning is a document, and documents are exactly what this
project has already established do not hold a boundary.

The point of the caps is not that 601 lines is worse than 599. It is that extraction happens at
line 601, when it is a twenty-minute job, instead of at line 12,000, when it becomes a
twelve-phase refactor plan that never gets started.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from srlint.cxx import find_functions, iter_sources  # noqa: E402

MAX_FILE_LINES = 600
MAX_FUNCTION_LINES = 80
MAX_MODE_MEMBERS = 25

# Mode orchestrators: the classes Law 6 exists to keep empty. Matches src/modes/<mode>/<Name>.h.
MODE_HEADER = re.compile(r"src/modes/[^/]+/[A-Z]\w*\.h$")

# A data member: a declaration with no parens, ending in ';', inside a class body. Excludes
# using-aliases, typedefs, static constants, and nested type declarations.
MEMBER = re.compile(
    r"^\s*(?!using |typedef |friend |static |return |public:|private:|protected:)"
    r"[\w:<>,\s\*&\[\]]+\s[\*&]?(\w+)\s*(?:=[^;]*)?;\s*$"
)


def check_file_length(path: Path, text: str, failures: list[str]) -> None:
    count = len(text.splitlines())
    if count > MAX_FILE_LINES:
        failures.append(
            f"{path}:1: file is {count} lines, cap is {MAX_FILE_LINES}. "
            f"Split it now -- this is the twenty-minute version of the job."
        )


def check_function_lengths(path: Path, text: str, failures: list[str]) -> None:
    for span in find_functions(text):
        if span.lines > MAX_FUNCTION_LINES:
            failures.append(
                f"{path}:{span.start_line}: function '{span.name}' is {span.lines} lines, "
                f"cap is {MAX_FUNCTION_LINES}."
            )


def check_mode_members(path: Path, text: str, failures: list[str]) -> None:
    """Law 6 -- a mode class holds a registry handle, a camera, UI view state, and nothing else."""
    posix = path.as_posix()
    if not MODE_HEADER.search(posix):
        return

    members: list[str] = []
    depth = 0
    in_class = False
    for line in text.splitlines():
        stripped = line.strip()
        if re.match(r"^(class|struct)\s+\w+", stripped) and not stripped.endswith(";"):
            in_class = True
            depth = 0
        if in_class:
            depth += line.count("{") - line.count("}")
            match = MEMBER.match(line)
            if match and "(" not in line and depth >= 1:
                members.append(match.group(1))
            if depth <= 0 and "}" in line:
                in_class = False

    if len(members) > MAX_MODE_MEMBERS:
        failures.append(
            f"{path}:1: mode class declares {len(members)} members, cap is {MAX_MODE_MEMBERS}. "
            f"Gameplay state belongs in the registry or in core/, not in a mode class (Law 6). "
            f"Members: {', '.join(members[:10])}..."
        )


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    failures: list[str] = []

    for path in iter_sources(root / "src"):
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(root)
        check_file_length(rel, text, failures)
        check_function_lengths(rel, text, failures)
        check_mode_members(rel, text, failures)

    if failures:
        print("Size cap violations (architecture.md section 2.2):\n")
        for failure in failures:
            print(f"  {failure}")
        print(f"\n{len(failures)} violation(s).")
        return 1

    print("Size caps OK.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
