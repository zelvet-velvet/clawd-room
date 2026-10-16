#!/usr/bin/env python3
"""Renders fixture scenes (tools/fixtures/*.fx) with room_tui.py and prints "fixture NAME HASH NOTE" like `c/clawd-room --fixture`.

    python3 tools/golden_fixture.py tools/fixtures/*.fx
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import room_tui as r  # noqa: E402
import themes  # noqa: E402


def fnv(cv):
    h = 1469598103934665603
    for row in cv.rows:
        for c in row:
            for b in c:
                h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def load(path):
    room = r.Room()
    room.step = lambda geo, now: None  # the scene is fixed: nobody moves
    cols, rows, now, t = 100, 24, 1000.0, 0.0
    guests = []
    for line in open(path):
        tok = line.split()
        if not tok or tok[0].startswith("#"):
            continue
        k = tok[0]
        if k == "theme":
            room.theme = themes.pick(None, tok[1])
        elif k == "title":
            room.title = tok[1]
        elif k == "terminal":
            cols, rows = int(tok[1]), int(tok[2])
        elif k == "time":
            now, t = float(tok[1]), float(tok[2])
        elif k == "scale":
            room.scale = int(tok[1])
        elif k == "ratio":
            room.ratio = tok[1]
        elif k == "peer":
            room.peers.append((tok[1], None if tok[2] == "-" else tok[2]))
        elif k == "neighbor":
            room.neighbors[tok[1]] = "id-" + tok[1]
            room.neighbor_themes[tok[1]] = tok[2]
        elif k == "linkpos":
            room.link_pos[tok[1]] = float(tok[2])
        elif k == "gate":
            room.gate_scale[tok[1]] = float(tok[2])
        elif k == "particle":
            room.particles.append([float(tok[1]), float(tok[2]), float(tok[3]), float(tok[4]), float(tok[5]), (int(tok[6]), int(tok[7]), int(tok[8])), bool(int(tok[9]))])
        elif k in ("session", "guest"):
            s = r.Session(tok[1], 0.0)
            s.guest = k == "guest"
            s.px = s.py = None
            for kv in tok[2:]:
                key, val = kv.split("=", 1)
                if key == "name": s.name = val
                elif key == "state": s.state = val
                elif key == "tool": s.tool = val
                elif key == "px": s.px = float(val)
                elif key == "py": s.py = float(val)
                elif key == "created": s.created = s.last = s.state_t = float(val)
                elif key == "last": s.last = float(val)
                elif key == "state_t": s.state_t = float(val)
                elif key == "hold": s.hold_work_until = float(val)
                elif key == "moving": s.moving = bool(int(val))
                elif key == "vclimb": s.vclimb = bool(int(val))
                elif key == "face": s.face = int(val)
                elif key == "stage": s.stage = val
                elif key == "chat_with": s.chat_with = val
                elif key == "chat_kind": s.chat_kind = int(val)
                elif key == "part": s.part = val
                elif key == "emote": kind, st = val.split("@"); s.emote = (kind, float(st), float(st) + 10)
                elif key == "peek": s.peek = ["y", 1.0] if int(val) else None
                elif key == "react": s.react_until = float(val)
                elif key == "cup": s.cup_until = float(val)
                elif key == "left_at": s.left_at = float(val)
                elif key == "seed": s.seed = float(val)
                elif key == "away": s.away = {"peer": "x"} if int(val) else None
                elif key == "walk_phase": s.walk_phase = float(val)
            if s.guest:
                guests.append(s)
            else:
                room.sessions[s.sid] = s
    for g in guests:
        room.guests[g.sid] = g
    return room, cols, rows, now, t


for path in sys.argv[1:]:
    room, cols, rows, now, t = load(path)
    cv, note = r.render(cols, rows, room, now, t)
    print("fixture %s %016x %s" % (os.path.basename(path), fnv(cv), note))
