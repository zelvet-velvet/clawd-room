#!/usr/bin/env python3
"""Runs `room_tui.py --dump` and `c/clawd-room --dump` on the same inputs and checks the text frames are identical."""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
EVENTS = [(os.path.join(ROOT, "tools", "fixtures", "events-odd.jsonl"), 1010.0)]
if len(sys.argv) > 1:
    EVENTS.append((sys.argv[1], float(sys.argv[2])))
CASES = [["--dump", "100x30"], ["--dump", "120x40", "--theme", "coffee"], ["--dump", "140x50", "--theme", "park"],
         ["--dump", "80x24", "--ratio", "16:9"], ["--dump", "160x60", "--theme", "office", "--room", "alpha"], ["--dump", "200x70", "--scale", "2"]]
bad = 0
for events, at in EVENTS:
    for case in CASES:
        args = case + ["--events", events, "--at", str(at)]
        c = subprocess.run([os.path.join(ROOT, "c", "clawd-room")] + args, capture_output=True, text=True)
        p = subprocess.run([sys.executable, os.path.join(ROOT, "room_tui.py"), "--no-detect"] + args, capture_output=True, text=True)
        same = c.stdout == p.stdout and c.returncode == p.returncode == 0
        bad += not same
        print("%-4s %s %s (%d lines)" % ("OK" if same else "DIFF", os.path.basename(events), " ".join(case), c.stdout.count("\n")))
        if not same:
            print(c.stderr[:300], p.stderr[:300])
sys.exit(1 if bad else 0)
