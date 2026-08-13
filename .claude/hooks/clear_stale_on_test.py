"""PostToolUse(Bash|PowerShell): a command that invokes sr_tests ran -> clear the stale flag.

Proxy for "tests were (re)run since the last C++ edit", not "tests passed" -- a build
or test failure surfaces in the same turn's tool output regardless, so this only needs
to catch the case where sr_tests was never invoked again after an edit.
"""
import json
import os
import sys

data = json.load(sys.stdin)
cmd = (data.get("tool_input") or {}).get("command") or ""

if "sr_tests" in cmd:
    try:
        os.remove(".claude/build-stale.flag")
    except FileNotFoundError:
        pass
