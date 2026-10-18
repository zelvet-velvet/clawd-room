#!/usr/bin/env python3
"""Made-up Claude Code sessions for trying the room (clawd-room.sh --try --demo).

Writes events in the hook's format (time, session id, event, folder, tool) with the current time to EVENTS, so the room
shows Clawds working, thinking, needing you, finishing and idling, without real sessions. Stops when STOP exists.

    demo_events.py EVENTS STOP [SESSIONS]
"""
import json
import os
import random
import sys
import time

TOOLS = ["Bash", "Edit", "Read", "Grep", "Write", "Task", "WebFetch"]
NAMES = ["api", "web", "docs", "infra", "tests", "notes", "app", "data", "ui", "cli"]


def main():
    events, stop = sys.argv[1], sys.argv[2]
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    rnd = random.Random(7)
    os.makedirs(os.path.dirname(events), exist_ok=True)
    out = open(events, "a", buffering=1)

    def emit(sid, ev, cwd, tool=""):
        out.write(json.dumps({"t": time.time(), "sid": sid, "ev": ev, "cwd": cwd, "tool": tool}) + "\n")

    sessions = [{"sid": "demo-%d" % i, "cwd": NAMES[i % len(NAMES)], "next": time.time() + rnd.uniform(0.2, 3.0), "busy": False}
                for i in range(n)]
    for s in sessions:
        emit(s["sid"], "SessionStart", s["cwd"])
    while not os.path.exists(stop):
        now = time.time()
        for s in sessions:
            if now < s["next"]:
                continue
            r = rnd.random()
            if not s["busy"]:  # idle: a new prompt now and then
                if r < 0.6:
                    emit(s["sid"], "UserPromptSubmit", s["cwd"])
                    s["busy"] = True
                s["next"] = now + rnd.uniform(2, 8)
            elif r < 0.55:  # a tool call
                tool = rnd.choice(TOOLS)
                emit(s["sid"], "PreToolUse", s["cwd"], tool)
                emit(s["sid"], "PostToolUse", s["cwd"], tool)
                s["next"] = now + rnd.uniform(1.5, 5)
            elif r < 0.7:  # asks for permission
                emit(s["sid"], "Notification", s["cwd"])
                s["next"] = now + rnd.uniform(4, 10)
            else:  # done
                emit(s["sid"], "Stop", s["cwd"])
                s["busy"] = False
                s["next"] = now + rnd.uniform(5, 15)
        time.sleep(0.25)
    for s in sessions:
        emit(s["sid"], "SessionEnd", s["cwd"])


if __name__ == "__main__":
    main()
