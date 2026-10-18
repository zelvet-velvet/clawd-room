#!/usr/bin/env python3
"""Two linked rooms in ptys: pick a Clawd in alpha (Tab), move it to beta (m + right arrow), and check the move message,
that beta's heartbeat claims it, and that alpha's does not.

    python3 tools/move_smoke.py
"""
import glob
import json
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from link_smoke import Proc  # noqa: E402  (the pty wrapper)


def heartbeat(link, room):
    for path in glob.glob(os.path.join(link, room + "-*.peer")) + glob.glob(os.path.join(link, room + ".peer")):
        try:
            return json.load(open(path))
        except (OSError, ValueError):
            pass
    return {}


with tempfile.TemporaryDirectory() as tmp:
    link = os.path.join(tmp, "link")
    events = os.path.join(tmp, "events.jsonl")
    now = time.time()
    with open(events, "w") as f:
        for i in range(8):
            f.write(json.dumps({"t": now - 5, "sid": "sess%d" % i, "ev": "SessionStart", "cwd": "proj%d" % i, "tool": ""}) + "\n")
    env = {"CLAWD_LINK_DIR": link}
    a = Proc("c", ["--room", "alpha", "--theme", "office", "--right", "beta", "--events", events], env)
    b = Proc("c", ["--room", "beta", "--theme", "coffee", "--left", "alpha", "--events", events], env)
    for _ in range(40):  # both start, see each other, show their gates
        a.pump(0.1); b.pump(0.1)
    a.key(b"\t"); a.pump(0.3); b.pump(0.1)
    a.key(b"m"); a.pump(0.3); b.pump(0.1)
    a.key(b"\x1b[C"); a.pump(0.3)
    for _ in range(100):  # it walks to the door and through it; beta claims it; alpha lets go
        a.pump(0.1); b.pump(0.1)
    walks = []
    for path in glob.glob(os.path.join(link, "beta*.inbox")):
        walks += [m for m in (json.loads(x) for x in open(path).read().splitlines() if x.strip()) if m["type"] == "walk"]
    moves = [m for m in walks if m["data"].get("move")]
    hb_a, hb_b = heartbeat(link, "alpha"), heartbeat(link, "beta")
    sid = moves[0]["data"]["sid"] if moves else None
    sa, sb = a.stop(), b.stop()
    ok = (sa == 0 and sb == 0 and len(moves) == 1 and sid in hb_b.get("homes", [])
          and sid not in hb_a.get("homes", []))
    print("%-4s moved %s: move messages %d, beta homes %s, alpha homes %s, exit %s/%s" % (
        "OK" if ok else "FAIL", sid, len(moves), hb_b.get("homes"), hb_a.get("homes"), sa, sb))
    if not ok:
        print("   walks to beta:", len(walks), "| alpha said PICK A CLAWD:", b"PICK A CLAWD" in a.out)
    sys.exit(0 if ok else 1)
