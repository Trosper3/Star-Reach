"""Minimal C++ scanning shared by the enforcement checks.

This is deliberately not a parser. The checks it feeds are structural limits, not semantics --
"is this function over 80 lines" does not need a type system, it needs balanced braces with
comments and string literals removed. A real parser would be a dependency, a build step, and a
thing that breaks; a hundred lines of brace counting is a thing that runs everywhere Python does.

The tradeoff is accepted knowingly: these checks may miscount pathological code (macros that
open braces, raw string literals containing braces). Both are rare, both are visible in review,
and both are worth catching some other way rather than paying for a parser here.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".inl"}


def iter_sources(root: Path) -> list[Path]:
    """Every C++ source under `root`, skipping build output and vendored trees."""
    skip_parts = {"build", "vcpkg_installed", ".git", "out", "CMakeFiles"}
    files = []
    for path in sorted(root.rglob("*")):
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        if skip_parts & set(path.parts):
            continue
        files.append(path)
    return files


def strip_noise(text: str) -> str:
    """Blank out comments and string/char literals, preserving line structure.

    Newlines are kept so reported line numbers still match the original file.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if ch == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue

        if ch == "/" and nxt == "*":
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] == "\n":
                    out.append("\n")
                i += 1
            i += 2
            continue

        if ch in "\"'":
            quote = ch
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 1
                if i < n and text[i] == "\n":
                    out.append("\n")
                i += 1
            i += 1
            out.append('""' if quote == '"' else "''")
            continue

        out.append(ch)
        i += 1
    return "".join(out)


@dataclass
class FunctionSpan:
    name: str
    start_line: int  # 1-indexed, the line the signature starts on
    end_line: int    # 1-indexed, the line holding the closing brace

    @property
    def lines(self) -> int:
        return self.end_line - self.start_line + 1


# A function body's '{' is preceded by a signature: something ending in ')' plus optional
# trailing specifiers. A constructor initializer list also ends in ')', so this covers both.
# Class/struct/enum/namespace bodies have no parens and are measured by the file cap instead.
_ENDS_SIGNATURE = re.compile(
    r"\)\s*(?:const|noexcept|override|final|volatile|mutable|&&?|\s)*(?:->[^{;]+)?$",
    re.DOTALL,
)

# The declared name is the identifier immediately before the FIRST paren in the header. Taking
# the first rather than the last is what makes `Foo::Foo(args) : member_(x)` report "Foo"
# instead of "member_".
_NAME_BEFORE_PAREN = re.compile(r"([~\w]+)\s*(?:<[^<>()]*>)?\s*\(")

_NOT_A_FUNCTION = {
    "if", "for", "while", "switch", "catch", "return", "else", "do",
    "class", "struct", "enum", "union", "namespace", "requires", "sizeof",
    "static_assert", "decltype", "alignas", "noexcept",
}


def find_functions(text: str) -> list[FunctionSpan]:
    """Locate function bodies by brace matching over comment-stripped source."""
    clean = strip_noise(text)
    line_starts = _line_start_offsets(clean)

    spans: list[FunctionSpan] = []
    depth = 0
    open_stack: list[tuple[str, int] | None] = []
    segment_start = 0

    for pos, ch in enumerate(clean):
        if ch == "{":
            candidate = None
            if depth >= 0:
                header = clean[segment_start:pos]
                candidate = _match_signature(header, segment_start, line_starts)
            open_stack.append(candidate)
            depth += 1
            segment_start = pos + 1
        elif ch == "}":
            depth = max(0, depth - 1)
            candidate = open_stack.pop() if open_stack else None
            if candidate is not None:
                name, start_line = candidate
                spans.append(FunctionSpan(name, start_line, _line_of(pos, line_starts)))
            segment_start = pos + 1
        elif ch in ";":
            segment_start = pos + 1

    return spans


def _match_signature(header: str, header_offset: int, line_starts: list[int]):
    stripped = header.strip()
    if not stripped or "(" not in stripped:
        return None
    if not _ENDS_SIGNATURE.search(stripped):
        return None
    # `= delete` / `= default` never open a body; a trailing '=' is an initializer.
    if stripped.endswith("=") or "= delete" in stripped or "= default" in stripped:
        return None

    name_match = _NAME_BEFORE_PAREN.search(stripped)
    if not name_match:
        # Most commonly a lambda: `[](auto x) {`. Skipped knowingly -- see the module docstring.
        return None

    name = name_match.group(1)
    if name in _NOT_A_FUNCTION:
        return None

    signature_start = header_offset + (len(header) - len(header.lstrip()))
    return name, _line_of(signature_start, line_starts)


def _line_start_offsets(text: str) -> list[int]:
    offsets = [0]
    for i, ch in enumerate(text):
        if ch == "\n":
            offsets.append(i + 1)
    return offsets


def _line_of(offset: int, line_starts: list[int]) -> int:
    lo, hi = 0, len(line_starts) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if line_starts[mid] <= offset:
            lo = mid
        else:
            hi = mid - 1
    return lo + 1
