#!/usr/bin/env python3
"""Runs scripted scenes (tools/scenarios/*.sc) with room_tui.py and prints what `c/clawd-room --script` prints.

Per frame: events, messages, prune, render (which moves everybody). Every few frames: a hash of the canvas and one of
everybody's state; messages for other windows are printed when they appear.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import room_tui as r  # noqa: E402
import themes  # noqa: E402


def fnv(h, s):
    for b in s.encode():
        h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def canvas_hash(cv):
    h = 1469598103934665603
    for row in cv.rows:
        for c in row:
            for b in c:
                h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def fmt(v):
    return "-" if v is None else "%.3f,%.3f" % v


def digest(room):
    h = 1469598103934665603
    for group in (list(room.sessions.values()), list(room.guests.values())):
        for s in group:
            h = fnv(h, "%s:%s:%s:%s:%s:%s:%d:%d:%d:" % (s.sid, s.state, s.stage, s.chat_with or "", s.emote[0] if s.emote else "", s.part, s.chat_kind, s.moving, s.face))
            h = fnv(h, (fmt((s.px, s.py)) if s.px is not None else "-") + ":")
            h = fnv(h, (fmt(s.goal) if s.goal else "-") + ":")
            h = fnv(h, "%d:%d:%d:%d:%.3f;" % (s.left_at is not None, bool(s.away), bool(s.travel), bool(s.peek), s.react_until))
    return fnv(h, "P%d" % len(room.particles))


def run(path):
    room = r.Room()
    cols, rows, frames, fps, every, start = 100, 24, 160, 16, 8, 1000.0
    items = []
    for line in open(path):
        tok = line.split()
        if not tok or tok[0].startswith("#"):
            continue
        k = tok[0]
        if k == "theme": room.theme = themes.pick(None, tok[1])
        elif k == "title": room.title = tok[1]
        elif k == "terminal": cols, rows = int(tok[1]), int(tok[2])
        elif k == "scale": room.scale = int(tok[1])
        elif k == "run": frames, fps, every = int(tok[1]), int(tok[2]), int(tok[3])
        elif k == "start": start = float(tok[1])
        elif k == "neighbor":
            room.neighbors[tok[1]] = "id-" + tok[1]
            room.neighbor_themes[tok[1]] = tok[2]
        elif k == "linkpos": room.link_pos[tok[1]] = float(tok[2])
        elif k in ("ev", "msg", "gone", "key"):
            items.append((float(tok[1]), k, tok[2:]))
    print("script", path)
    nxt = 0
    for f in range(frames):
        t = f / fps
        now = start + t
        while nxt < len(items) and items[nxt][0] <= t:
            _, kind, a = items[nxt]
            nxt += 1
            if kind == "ev":
                room.apply({"t": start + items[nxt - 1][0], "sid": a[0], "ev": a[1], "cwd": "" if a[2] == "-" else a[2], "tool": "" if a[3] == "-" else a[3]})
            elif kind == "msg":  # msg T from walk|recall sid name side home y
                d = {"sid": a[2], "side": a[4]}
                if a[1] == "walk":
                    d["name"] = "" if a[3] == "-" else a[3]
                    if float(a[5]) > 0:
                        d["home"] = 1
                room.receive({"from": a[0], "type": a[1], "data": d})
            elif kind == "gone":
                room.peer_gone(a[0])
            elif kind == "key":
                print("key %d %s" % (f, room.send_one(now)))
        room.prune(now)
        cv, note = r.render(cols, rows, room, now, t)
        for peer, mtype, data in room.outbox:
            if mtype == "walk":
                print("out %d %s walk %s %s %s %d %d" % (f, peer, data["sid"], data.get("name") or "-", data["side"], 1 if data.get("home") else 0, data["y"]))
            else:
                print("out %d %s recall %s" % (f, peer, data["sid"]))
        room.outbox = []
        if os.environ.get("CLAWD_DUMP") and f == int(os.environ["CLAWD_DUMP"]):
            for grp in (list(room.sessions.values()), list(room.guests.values())):
                for s in grp:
                    print("  %s state=%s stage=%s chat=%s emote=%s part=%s kind=%d moving=%d face=%d pos=%d %.3f,%.3f goal=%d %.3f,%.3f left=%d away=%d travel=%d peek=%d react=%.3f" % (
                        s.sid, s.state, s.stage, s.chat_with or "", s.emote[0] if s.emote else "", s.part, s.chat_kind, s.moving, s.face, s.px is not None, s.px or 0, s.py or 0,
                        s.goal is not None, s.goal[0] if s.goal else 0, s.goal[1] if s.goal else 0, s.left_at is not None, bool(s.away), bool(s.travel), bool(s.peek), s.react_until))
        if f % every == 0:
            print("f %d %016x %016x %s" % (f, canvas_hash(cv), digest(room), note))


for p in sys.argv[1:]:
    run(p)
