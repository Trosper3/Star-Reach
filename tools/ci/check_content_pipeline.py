#!/usr/bin/env python3
"""Single content pipeline -- architecture.md Law 10.

JSON in data/ is the only source of authored content. No C++ definition tables.

StarReach2 ran two content pipelines simultaneously and never noticed: config/modules.json with
a real parser, AND 883 lines of hardcoded factories in data/modules/*Defs.h --

    inline ModuleDef Weapon_PulseCannon_I() {
        m.id = "pulse_cannon_i";  m.weapon.damage = 15.0f;  ...
    }

Both survived to the end because nothing declared which one won. That is the lesson underneath
the whole enforcement section: even a well-enforced boundary leaks where the RULE is ambiguous.
This script is the declaration.

If content is genuinely awkward to express in JSON, that is a schema conversation -- not an
escape hatch back into C++.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from srlint.cxx import iter_sources, strip_noise  # noqa: E402

# Types that describe authored content. Constructing one of these with a value is authoring.
CONTENT_TYPES = ["ModuleDef", "ShellDef", "ShipBlueprint", "RigBlueprint", "MountBlueprint"]

# Where authoring legitimately becomes C++:
#   core/registries/  the parser -- its entire job is turning JSON into these structs
#   tests/            fixtures must be able to build a blueprint without a data file
#   tools/            offline generators and the balancing sim
ALLOWED_PREFIXES = ("src/core/registries/", "tests/", "tools/")

# A definition with an initializer: `ModuleDef x = ...`, `ModuleDef x{...}`, `return ModuleDef{`.
# Plain declarations (`ModuleDef def;`), references, pointers, and parameters are all fine --
# passing content around is not authoring it.
PATTERNS = [
    re.compile(rf"\b{t}\s+[A-Za-z_]\w*\s*(?:=[^=]|\{{)") for t in CONTENT_TYPES
] + [
    re.compile(rf"\breturn\s+{t}\s*\{{") for t in CONTENT_TYPES
] + [
    # Container literals of content types -- the exact shape *Defs.h took.
    #
    # Requires the content type to be the container's VALUE type: `std::vector<ModuleDef> x = `.
    # A container of pointers (`std::vector<const MountBlueprint*>`) is traversal, not
    # authoring, and matching it was this check's first false positive.
    re.compile(rf"\b(?:vector|array|map)\s*<[^>]*\b{t}\s*>\s*[A-Za-z_]\w*\s*(?:=|\{{)")
    for t in CONTENT_TYPES
]


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    failures: list[str] = []

    search_roots = [root / "src", root / "tests", root / "tools"]
    for search_root in search_roots:
        if not search_root.exists():
            continue
        for path in iter_sources(search_root):
            rel = path.relative_to(root).as_posix()
            if rel.startswith(ALLOWED_PREFIXES):
                continue

            clean = strip_noise(path.read_text(encoding="utf-8", errors="replace"))
            for line_no, line in enumerate(clean.splitlines(), start=1):
                for pattern in PATTERNS:
                    if pattern.search(line):
                        failures.append(
                            f"{rel}:{line_no}: authored content constructed in C++\n"
                            f"      {line.strip()}\n"
                            f"      Content belongs in data/base_game/*.json (Law 10). "
                            f"Only core/registries/, tests/, and tools/ may build these types."
                        )
                        break

    if failures:
        print("Content pipeline violations (architecture.md Law 10):\n")
        for failure in failures:
            print(f"  {failure}\n")
        print(f"{len(failures)} violation(s).")
        return 1

    print("Content pipeline OK.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
