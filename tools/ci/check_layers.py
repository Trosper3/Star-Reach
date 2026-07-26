#!/usr/bin/env python3
"""Layer include rules -- architecture.md section 2.3.

The CMake target graph already turns most illegal dependencies into link errors. This check
covers the gap the linker cannot see: header-only violations, where an include compiles fine
because nothing in it needed a symbol at link time.

Rules mirror the target graph exactly, with the acyclic correction documented in section 2.1:

    shared/       is the BOTTOM layer -- POD blueprints, components, pure math.
                  May include nothing of ours except itself. No raylib.
    core/         may include shared/. No engine/, modes/, net/, raylib.
    engine/       hardware only. No core/, shared/, modes/, net/ -- zero game knowledge.
    shared/ui/    the one part of shared/ that may use engine/ and raylib.
    modes/        may include everything below it, with the intra-mode rules below.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from srlint.cxx import iter_sources  # noqa: E402

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')

# (directory prefix, [forbidden include prefixes], explanation)
RULES: list[tuple[str, list[str], str]] = [
    (
        "src/shared/ui/",
        ["modes/", "net/"],
        "shared/ui/ is cross-mode presentation; it may not know about a specific mode.",
    ),
    (
        "src/shared/",
        ["core/", "engine/", "modes/", "net/", "raylib.h"],
        "shared/ is the bottom layer. core/registries parses INTO these types, so a dependency "
        "the other way makes the layer graph cyclic and unlinkable. No raylib: every component "
        "must be constructible in a headless test and in tools/economy_sim.",
    ),
    (
        "src/core/",
        ["engine/", "modes/", "net/", "shared/ui/", "raylib.h"],
        "core/ is universe truth: mode-agnostic, registry-agnostic, render-agnostic (Law 8). "
        "sr_core does not link raylib, and that link constraint is what keeps it testable.",
    ),
    (
        "src/engine/",
        ["core/", "shared/", "modes/", "net/"],
        "engine/ is hardware abstraction with zero game knowledge. It is the layer a second "
        "game could be built on; nothing in it may know what a rig or a faction is.",
    ),
    (
        "src/modes/space/systems/",
        ["modes/space/factories/", "modes/space/ui/", "modes/space/render/"],
        "Systems never construct composite objects (Law 5) and never reach into presentation. "
        "Factories read blueprints and emit entities; systems simulate what already exists.",
    ),
    (
        "src/modes/space/ui/",
        ["modes/space/systems/"],
        "UI emits Intents into the queue; it never calls a system and never mutates game state "
        "directly (Law 9). This is what makes adding ENet later a change of intent source "
        "rather than a rewrite of every menu.",
    ),
    (
        "src/modes/space/",
        ["modes/planet/"],
        "modes/planet/ is a YAGNI boundary and is not built. It exists to keep core/ honest.",
    ),
    (
        "src/modes/planet/",
        ["modes/space/"],
        "Modes do not include each other. Shared behavior is promoted to shared/ (Law 11).",
    ),
]


def violations_for(rel_posix: str, includes: list[tuple[int, str]]) -> list[tuple[int, str, str]]:
    found = []
    for prefix, forbidden, reason in RULES:
        if not rel_posix.startswith(prefix):
            continue
        # A more specific rule already matched (shared/ui/ before shared/); both apply, and the
        # specific one is listed first so its allowance is not overridden.
        for line_no, header in includes:
            for bad in forbidden:
                if header == bad or header.startswith(bad):
                    if prefix == "src/shared/" and rel_posix.startswith("src/shared/ui/"):
                        continue  # shared/ui/ is exempt from the no-engine/no-raylib rule.
                    found.append((line_no, header, reason))
    return found


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    failures: list[str] = []

    for path in iter_sources(root / "src"):
        rel = path.relative_to(root)
        includes = []
        for line_no, line in enumerate(
            path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1
        ):
            match = INCLUDE.match(line)
            if match:
                includes.append((line_no, match.group(1)))

        for line_no, header, reason in violations_for(rel.as_posix(), includes):
            failures.append(f"{rel}:{line_no}: illegal include '{header}'\n      {reason}")

    if failures:
        print("Layer violations (architecture.md section 2.3):\n")
        for failure in failures:
            print(f"  {failure}\n")
        print(f"{len(failures)} violation(s).")
        return 1

    print("Layer rules OK.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
