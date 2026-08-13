"""PreToolUse(Bash): block bare grep/find, push toward the Grep/Glob tools instead.

Only blocks when grep/find is the FIRST command in the string -- `build | grep error`
filters output from another command and is left alone; `grep -r foo src/` or
`find . -name '*.h'` (grep/find as the command being run) is what this blocks.
"""
import json
import re
import sys

data = json.load(sys.stdin)
cmd = (data.get("tool_input") or {}).get("command") or ""

if re.match(r"^\s*(grep|find)\b", cmd):
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": (
                "Bare grep/find in Bash is blocked in this repo -- use the Grep or Glob "
                "tool instead for targeted search. If neither tool can do what's needed, "
                "say so explicitly and ask before falling back to a shell search."
            ),
        }
    }))
