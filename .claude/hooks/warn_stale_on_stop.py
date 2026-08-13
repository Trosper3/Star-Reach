"""Stop: warn if C++ source changed this session but sr_tests was never (re)run after.

CLAUDE.md's workflow requires a clean sr_tests.exe run before handoff -- this makes
that check a systemMessage instead of relying on remembering to do it.
"""
import json
import os

if os.path.exists(".claude/build-stale.flag"):
    print(json.dumps({
        "systemMessage": (
            "C++ source changed but sr_tests hasn't been (re)run since -- confirm "
            "build + sr_tests.exe before treating this as done (CLAUDE.md build/test step)."
        )
    }))
