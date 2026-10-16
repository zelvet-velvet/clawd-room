#!/usr/bin/env python3
"""Claude Code hook for clawd-room.

Records WHICH event happened in WHICH session (session id, event name, tool name,
folder name). It never stores the prompt, tool input, tool output or file paths.
Always exits 0 and prints nothing, so it can never block or change Claude Code.
"""
import json
import os
import sys
import time

EVENTS_DIR = os.path.expanduser("~/.clawd-room")
EVENTS = os.environ.get("CLAWD_EVENTS", os.path.join(EVENTS_DIR, "events.jsonl"))
KNOWN = {
    "SessionStart", "SessionEnd", "UserPromptSubmit", "PreToolUse",
    "PostToolUse", "Notification", "Stop", "SubagentStop",
}


def main():
    d = json.load(sys.stdin)
    ev = d.get("hook_event_name", "")
    if ev not in KNOWN:
        return
    cwd = (d.get("cwd") or "").rstrip("/\\")
    rec = {
        "t": time.time(),
        "sid": str(d.get("session_id", "?"))[:64],
        "ev": ev,
        "cwd": os.path.basename(cwd)[:32],
        "tool": str(d.get("tool_name", ""))[:40],
    }
    os.makedirs(os.path.dirname(EVENTS), mode=0o700, exist_ok=True)
    fd = os.open(EVENTS, os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o600)
    try:
        os.write(fd, (json.dumps(rec) + "\n").encode())
    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
