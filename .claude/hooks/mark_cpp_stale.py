"""PostToolUse(Write|Edit): a C++ source or CMakeLists edit lands -> mark the build stale.

Paired with clear_stale_on_test.py (clears it once sr_tests runs) and
warn_stale_on_stop.py (surfaces it if it's still set when the turn ends).
"""
import json
import os
import sys

data = json.load(sys.stdin)
path = (data.get("tool_input") or {}).get("file_path") or ""

if path.endswith((".cpp", ".h", ".hpp")) or os.path.basename(path) == "CMakeLists.txt":
    os.makedirs(".claude", exist_ok=True)
    with open(".claude/build-stale.flag", "w", encoding="utf-8") as f:
        f.write(path)
