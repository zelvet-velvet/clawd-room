#!/usr/bin/env python3
"""Linked rooms share the sessions without Clawds hopping between them (a session a running room shows stays there):
  A  one room with the sessions, then three more open one after another: no session changes room, at any moment
  B  new sessions start: each is shown by exactly one room
  C  one room freezes for 7 s (like a room stuck drawing): nobody takes its Clawds
  D  the first room closes: only its Clawds go to other rooms, each to exactly one

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

FIRST, MORE = 6, 8


def shown(log):
    try:
        lines = open(log).read().splitlines()
    except OSError:
        return set()
    return set(lines[-1].split()[1:]) if lines else set()


def watch(rooms, logs, secs, seen):  # pump the rooms; note every session each room showed at any moment
    end = time.time() + secs
    while time.time() < end:
        for nm, r in rooms.items():
            r.pump(0.05)
            seen.setdefault(nm, set()).update(shown(logs[nm]))


def add_sessions(events, first, n):
    with open(events, "a") as f:
        for i in range(first, first + n):
            f.write(json.dumps({"t": time.time(), "sid": "sess%02d" % i, "ev": "SessionStart", "cwd": "p%d" % i, "tool": ""}) + "\n")


with tempfile.TemporaryDirectory() as tmp:
    link, events = os.path.join(tmp, "link"), os.path.join(tmp, "events.jsonl")
    open(events, "w").close()
    add_sessions(events, 0, FIRST)
    rooms, logs, bad = {}, {}, []

    def start(nm):
        logs[nm] = os.path.join(tmp, nm + ".shown")
        rooms[nm] = Proc("c", ["--room", nm, "--theme", "office", "--events", events], {"CLAWD_LINK_DIR": link, "CLAWD_SHOWN_LOG": logs[nm]})

    def snap():
        return {nm: shown(logs[nm]) for nm in rooms}

    def once_each(label, s, total):
        everyone = [x for v in s.values() for x in v]
        if len(everyone) != total or len(set(everyone)) != total:
            bad.append("%s: %d shown, %d different (want %d, each once)" % (label, len(everyone), len(set(everyone)), total))

    start("alpha")
    watch(rooms, logs, 3, {})
    first = snap()["alpha"]
    if len(first) != FIRST:
        bad.append("A: alpha alone shows %d of %d" % (len(first), FIRST))
    seen = {}
    for nm in ("beta", "gamma", "delta"):  # (A) the others open one by one
        start(nm)
        watch(rooms, logs, 1.5, seen)
    watch(rooms, logs, 2, seen)
    for nm in ("beta", "gamma", "delta"):
        if seen.get(nm, set()) & first:
            bad.append("A: %s opened and took %s from alpha" % (nm, sorted(seen[nm] & first)))
    if snap()["alpha"] != first:
        bad.append("A: alpha's Clawds changed")
    add_sessions(events, FIRST, MORE)  # (B)
    watch(rooms, logs, 3, {})
    sb = snap()
    once_each("B", sb, FIRST + MORE)
    spread = sum(1 for nm in sb if sb[nm] - first)
    os.kill(rooms["beta"].pid, signal.SIGSTOP)  # (C)
    seen = {}
    watch({n: r for n, r in rooms.items() if n != "beta"}, logs, 7, seen)
    os.kill(rooms["beta"].pid, signal.SIGCONT)
    for nm in seen:
        if seen[nm] != sb[nm]:
            bad.append("C: beta frozen, %s changed (%s)" % (nm, sorted(seen[nm] ^ sb[nm])))
    watch(rooms, logs, 2, {})
    if snap() != sb:
        bad.append("C: beta back, the split changed")
    rooms["alpha"].stop(); del rooms["alpha"]  # (D)
    watch(rooms, logs, 3, {})
    sd = snap()
    once_each("D", sd, FIRST + MORE)
    for nm in sd:
        if not sd[nm] >= sb[nm]:
            bad.append("D: alpha closed, %s lost %s" % (nm, sorted(sb[nm] - sd[nm])))
    for r in rooms.values():
        r.stop()
    print("%s  A alpha kept its %d while 3 rooms opened; B new sessions in %d rooms %s; D after alpha closed %s" % (
        "OK  " if not bad else "FAIL", len(first), spread, [len(sb[n]) for n in sorted(sb)], [len(sd[n]) for n in sorted(sd)]))
    for b in bad:
        print("   ", b)
    sys.exit(1 if bad else 0)
