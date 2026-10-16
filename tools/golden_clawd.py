#!/usr/bin/env python3
"""Prints "action step hash" for every action, the same lines `c/clawd-room --golden` prints (FNV-1a over RGB bytes).

    python3 tools/golden_clawd.py [--code]      --code ignores sprites/clawd.txt (draws by the code)\n    python3 tools/golden_clawd.py --term        the terminal update strings instead
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import room_tui as r  # noqa: E402
import sprite_tool as t  # noqa: E402
import sprites  # noqa: E402


def fnv(cv):
    h = 1469598103934665603
    for row in cv.rows:
        for c in row:
            for b in c:
                h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


if "--code" in sys.argv:
    r.CLAWD_SPRITES = sprites.SpriteFile("-")
if "--term" not in sys.argv:
    for action, _, _ in t.ACTIONS:
        for i, (_, p) in enumerate(t.steps(action)):
            cv = r.Canvas(26, 18, (0, 0, 0))
            r.draw_clawd(cv, 13, 8, p)
            print("%s %d %016x" % (action, i, fnv(cv)))

if "--term" in sys.argv:  # the terminal update strings, same lines as `c/clawd-room --golden-term`
    def fnv_bytes(b):
        h = 1469598103934665603
        for x in b:
            h = ((h ^ x) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        return h
    for action, _, _ in t.ACTIONS:
        prev = None
        for i, (_, p) in enumerate(t.steps(action)):
            cv = r.Canvas(26, 18, (0, 0, 0))
            r.draw_clawd(cv, 13, 8, p)
            cells = r.cells_from_canvas(cv, action)
            full, d = r.diff_frame(cells, None), r.diff_frame(cells, prev)
            print("term %s %d %016x %016x" % (action, i, fnv_bytes(full.encode()), fnv_bytes(d.encode())))
            prev = cells
