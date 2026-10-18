#!/usr/bin/env python3
"""Linked rooms share the sessions without Clawds hopping between them: three rooms, then a fourth opens (it only takes
its own share), one room freezes for 7 s like a room stuck drawing (nobody takes its Clawds), the fourth closes (only its
Clawds go elsewhere). Every session is shown by exactly one room once things settle.

    python3 tools/split_smoke.py
"""
import json
import os
import signal
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from link_smoke import Proc  # noqa: E402

N = 16


def shown(log):
    try:
        lines = open(log).read().splitlines()
    except OSError:
        return set()
    return set(lines[-1].split()[1:]) if lines else set()


def pump(rooms, secs):
    end = time.time() + secs
    while time.time() < end:
        for r in rooms:
            r.pump(0.05)


with tempfile.TemporaryDirectory() as tmp:
    link, events = os.path.join(tmp, "link"), os.path.join(tmp, "events.jsonl")
    now = time.time()
    with open(events, "w") as f:
        for i in range(N):
            f.write(json.dumps({"t": now - 5, "sid": "sess%02d" % i, "ev": "SessionStart", "cwd": "p%d" % i, "tool": ""}) + "\n")
    names, rooms, logs = ["alpha", "beta", "gamma", "delta"], {}, {}

    def start(nm):
        logs[nm] = os.path.join(tmp, nm + ".shown")
        rooms[nm] = Proc("c", ["--room", nm, "--theme", "office", "--events", events], {"CLAWD_LINK_DIR": link, "CLAWD_SHOWN_LOG": logs[nm]})

    for nm in names[:3]:
        start(nm)
    pump(rooms.values(), 4)
    bad = []
    def snap():
        return {nm: shown(logs[nm]) for nm in rooms}
    def check_partition(label, s):
        everyone = [x for v in s.values() for x in v]
        if len(everyone) != N or len(set(everyone)) != N:
            bad.append("%s: %d shown, %d different (want %d each)" % (label, len(everyone), len(set(everyone)), N))
    s3 = snap(); check_partition("three rooms", s3)
    start("delta"); pump(rooms.values(), 4)
    s4 = snap(); check_partition("four rooms", s4)
    for nm in names[:3]:
        if not s4[nm] <= s3[nm]:
            bad.append("delta opened: %s got %s from another room" % (nm, sorted(s4[nm] - s3[nm])))
    os.kill(rooms["beta"].pid, signal.SIGSTOP)  # beta stuck: its heartbeat goes stale
    seen = {nm: set() for nm in ("alpha", "gamma", "delta")}
    end = time.time() + 7
    while time.time() < end:
        pump([rooms[n] for n in seen], 0.5)
        for nm in seen:
            seen[nm] |= shown(logs[nm])
    os.kill(rooms["beta"].pid, signal.SIGCONT)
    for nm in seen:
        if seen[nm] - s4[nm]:
            bad.append("beta frozen: %s took %s" % (nm, sorted(seen[nm] - s4[nm])))
    pump(rooms.values(), 3)
    s4b = snap(); check_partition("beta back", s4b)
    if s4b != s4:
        bad.append("beta back: the split changed")
    rooms["delta"].stop(); del rooms["delta"]
    pump(rooms.values(), 3)
    s3b = snap(); check_partition("delta closed", s3b)
    for nm in names[:3]:
        if not s3b[nm] >= s4[nm]:
            bad.append("delta closed: %s lost %s" % (nm, sorted(s4[nm] - s3b[nm])))
    for r in rooms.values():
        r.stop()
    print("%s  shares 3 rooms %s, 4 rooms %s, after delta closed %s" % ("OK  " if not bad else "FAIL",
          [len(s3[n]) for n in names[:3]], [len(s4[n]) for n in names], [len(s3b[n]) for n in names[:3]]))
    for b in bad:
        print("   ", b)
    sys.exit(1 if bad else 0)
