#!/usr/bin/env python3
"""Runs every check CI runs. Use this locally before pushing:

    python tools/ci/check_all.py

CI runs the same scripts as separate jobs so a failure names itself in the Actions UI.
Running them here in one process is purely for the local loop -- the checks are a few hundred
milliseconds, which is deliberate. An enforcement step people skip because it is slow is an
enforcement step that does not exist.

clang-format is included even though it is not one of our Python scripts, because it is a CI job
that can fail, and a pre-push script that reports "all checks passed" on a tree CI will reject is
worse than no script at all. That is not hypothetical -- it is exactly how three formatting
violations reached CI on 2026-08-12.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

CHECKS = [
    "check_sizes.py",
    "check_layers.py",
    "check_content_pipeline.py",
    "check_dead_dirs.py",
]

# Mirrors build.yaml's format job exactly: find src tests -name '*.h' -o -name '*.cpp'.
FORMAT_ROOTS = ["src", "tests"]
FORMAT_SUFFIXES = {".h", ".cpp"}

# Windows caps a command line at ~32k characters, and xargs (which CI uses) chunks for free.
# Here we chunk by hand rather than discover the cap the hard way on someone's machine.
FORMAT_BATCH = 100


def check_format(repo_root: Path) -> bool:
    exe = shutil.which("clang-format")
    if exe is None:
        print("clang-format was not found on PATH -- this check could not run.")
        print("CI runs it as a blocking job, so a green result here would be a false one.")
        print("Install clang-format (CI uses 18), or run build.yaml's format job command by hand.")
        return False

    # Printed because a version difference is a real source of disagreement with CI: clang-format
    # changes its output between major versions, so a tree that is clean under 15 can fail under 18.
    subprocess.run([exe, "--version"], check=False)

    files = sorted(
        str(path)
        for root in FORMAT_ROOTS
        for path in (repo_root / root).rglob("*")
        if path.suffix in FORMAT_SUFFIXES and path.is_file()
    )
    if not files:
        print(f"no sources found under {', '.join(FORMAT_ROOTS)} -- refusing to pass vacuously.")
        return False

    ok = True
    for start in range(0, len(files), FORMAT_BATCH):
        batch = files[start : start + FORMAT_BATCH]
        result = subprocess.run([exe, "--dry-run", "--Werror", *batch], check=False)
        if result.returncode != 0:
            ok = False

    if ok:
        print(f"{len(files)} files are correctly formatted.")
    return ok


def main() -> int:
    here = Path(__file__).resolve().parent
    repo_root = here.parent.parent
    failed = []

    for check in CHECKS:
        print(f"--- {check} ---", flush=True)
        result = subprocess.run([sys.executable, str(here / check)], check=False)
        if result.returncode != 0:
            failed.append(check)
        print()

    print("--- clang-format ---", flush=True)
    if not check_format(repo_root):
        failed.append("clang-format")
    print()

    if failed:
        print(f"FAILED: {', '.join(failed)}")
        return 1

    print("All checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
