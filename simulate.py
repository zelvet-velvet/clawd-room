#!/usr/bin/env python3
"""Write fake events so you can watch the room without real Claude sessions.

  python simulate.py                 # live: 6 fake sessions for 3 minutes
  python simulate.py --fixture 1700000000 --out /tmp/e.jsonl   # static file for screenshots
"""
import argparse
import json
import os
import random
import time

EVENTS = os.environ.get("CLAWD_EVENTS", os.path.expanduser("~/.clawd-room/events.jsonl"))
NAMES = ["web-shop", "api-server", "notes_app", "pixel-gui", "scratch", "garden"]
TOOLS = ["Bash", "Edit", "Read", "Grep", "WebFetch", "Task", "mcp__x__y"]


def emit(path, t, sid, ev, cwd="", tool=""):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a") as f:
        f.write(json.dumps({"t": t, "sid": sid, "ev": ev, "cwd": cwd, "tool": tool}) + "\n")


def fixture(path, base):
    """Each session ends in a different state at base+10."""
    open(path, "w").close()
    plan = [
        ("a", "web-shop", ["UserPromptSubmit"]),
        ("b", "api-server", ["UserPromptSubmit", ("PreToolUse", "Bash")]),
        ("c", "notes_app", ["UserPromptSubmit", ("PreToolUse", "Edit")]),
        ("d", "pixel-gui", ["UserPromptSubmit", "Notification"]),
        ("e", "scratch", ["UserPromptSubmit", "Stop"]),
        ("f", "garden", ["SessionStart"]),
    ]
    for i, (sid, name, evs) in enumerate(plan):
        emit(path, base - 100 + i, sid, "SessionStart", name)
        t = base + 8 + i * 0.1
        for e in evs:
            ev, tool = (e, "") if isinstance(e, str) else e
            emit(path, t, sid, ev, name, tool)
            t += 0.3


def live(path, seconds):
    sessions = {s: n for s, n in zip("abcdef", NAMES)}
    for sid, name in sessions.items():
        emit(path, time.time(), sid, "SessionStart", name)
        time.sleep(0.4)
    end = time.time() + seconds
    while time.time() < end:
        sid = random.choice(list(sessions))
        name = sessions[sid]
        emit(path, time.time(), sid, "UserPromptSubmit", name)
        for _ in range(random.randint(2, 5)):
            time.sleep(random.uniform(0.5, 1.5))
            emit(path, time.time(), sid, "PreToolUse", name, random.choice(TOOLS))
            time.sleep(random.uniform(0.6, 1.8))
            emit(path, time.time(), sid, "PostToolUse", name, "x")
        if random.random() < 0.3:
            emit(path, time.time(), sid, "Notification", name)
            time.sleep(3)
        emit(path, time.time(), sid, "Stop", name)
        time.sleep(random.uniform(1, 3))
    for sid, name in sessions.items():
        emit(path, time.time(), sid, "SessionEnd", name)
        time.sleep(0.5)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=EVENTS)
    ap.add_argument("--fixture", type=float, help="write a static file ending at this base timestamp")
    ap.add_argument("--seconds", type=int, default=180)
    a = ap.parse_args()
    if a.fixture is not None:
        fixture(a.out, a.fixture)
    else:
        live(a.out, a.seconds)
