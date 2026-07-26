#!/usr/bin/env python3
"""No empty scaffolding -- architecture.md section 2.4, applied to directories.

Section 0's headline evidence is a directory:

    src/modes/space_flight/entities/ -- created, correctly named, LEFT EMPTY.

while all 466 world-data members went into SpaceFlight.h instead. The directory was not the
cause, but it was the tell, and it is the cheapest possible thing to detect. Law 11's real
complaint was never junk drawers; it was FOUR PLAUSIBLE HOMES for a UI file, three of which
stayed empty. Ambiguity under fatigue resolves to "whatever I already have open."

So: a directory under src/ with no source beneath it is a home that does not exist yet, and it
is deleted rather than reserved. Create it in the commit that puts the first file in it -- which
is also when you learn whether you actually wanted it there (Law 11's promotion rule).

Note this check is mostly a LOCAL guard: git cannot track an empty directory, so a fresh clone
never has one. That is the point. The empty tree lives on the machine where the decision gets
made at 2am.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from srlint.cxx import SOURCE_SUFFIXES  # noqa: E402


def has_source_below(directory: Path) -> bool:
    return any(p.suffix in SOURCE_SUFFIXES and p.is_file() for p in directory.rglob("*"))


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    src = root / "src"
    if not src.exists():
        print("No src/ directory.")
        return 0

    dead = []
    for directory in sorted(p for p in src.rglob("*") if p.is_dir()):
        if has_source_below(directory):
            continue
        # Report the topmost dead directory only, so an empty five-level tree is one line.
        rel = directory.relative_to(root)
        if any(str(rel).startswith(str(existing) + "/") or
               rel.as_posix().startswith(existing.as_posix() + "/") for existing in dead):
            continue
        dead.append(rel)

    if dead:
        print("Empty scaffolding under src/ (architecture.md section 2.4):\n")
        for rel in dead:
            print(f"  {rel.as_posix()}/  -- contains no source")
        print(
            "\nDelete these. Create a directory in the same commit as the first file that goes\n"
            "in it. An empty, correctly-named directory is the exact tell that preceded\n"
            "StarReach2's god object -- see architecture.md section 0."
        )
        return 1

    print("No empty scaffolding.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
