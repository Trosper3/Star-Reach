#!/usr/bin/env python3
"""Runs every structural check. Use this locally before pushing:

    python tools/ci/check_all.py

CI runs the same three scripts as separate jobs so a failure names itself in the Actions UI.
Running them here in one process is purely for the local loop -- the checks are a few hundred
milliseconds, which is deliberate. An enforcement step people skip because it is slow is an
enforcement step that does not exist.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

CHECKS = [
    "check_sizes.py",
    "check_layers.py",
    "check_content_pipeline.py",
    "check_dead_dirs.py",
]


def main() -> int:
    here = Path(__file__).resolve().parent
    failed = []

    for check in CHECKS:
        print(f"--- {check} ---")
        result = subprocess.run([sys.executable, str(here / check)], check=False)
        if result.returncode != 0:
            failed.append(check)
        print()

    if failed:
        print(f"FAILED: {', '.join(failed)}")
        return 1

    print("All structural checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
