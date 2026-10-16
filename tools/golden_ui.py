#!/usr/bin/env python3
"""Prints what the UI helpers of room_tui.py give, like `c/clawd-room --golden-ui FILE.jsonl`.

    python3 tools/golden_ui.py tools/fixtures/events-odd.jsonl
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


def main(path):
    for ev in r.Tail(path).history(1000.0):
        print("tail %.17g %s %s %s %s" % (ev["t"], ev["sid"], ev["ev"], ev["cwd"], ev["tool"]))
    for paused in (0, 1):
        for help_on in (0, 1):
            for note, flash in (("", ""), ("+3 MORE", ""), ("", "SCALE 2X"), ("+1 MORE", "FLASH")):
                print("overlay %d %d [%s]" % (paused, help_on, r.overlay_text(bool(paused), bool(help_on), note, flash)))
    for cols, rows in ((30, 10), (44, 19), (10, 5), (200, 80)):
        print("small %d %d %r" % (cols, rows, r.too_small(cols, rows)))
    for pw in (110, 160, 220, 300):
        for ph in (64, 100, 140, 200):
            print("auto %d %d %s" % (pw, ph, " ".join(str(r.auto_scale(pw, ph, n)) for n in (0, 1, 3, 6, 12, 40))))
    cache = {}
    for w, h in ((144, 84), (110, 64), (60, 40), (200, 120), (44, 30), (46, 56)):
        for pick in range(3):
            for solo in (False, True):
                for t in (0.0, 0.4):
                    cv = r.Canvas(w, h, r.MARGIN)
                    r.draw_menu(cv, pick, solo, t, cache)
                    print("menu %d %d %d %d %.1f %016x" % (w, h, pick, int(solo), t, fnv(cv)))
    for cols, rows in ((110, 32), (200, 60), (300, 100), (90, 30)):  # the menu in a terminal, as choose_scene builds it
        pw, ph = cols, rows * 2
        k = max(1, min(pw // (r.MENU_W * 5 // 4), ph // (r.MENU_H * 5 // 4)))
        small = r.Canvas(pw // k, ph // k // 2 * 2, r.MARGIN)
        r.draw_menu(small, 1, False, 0.4, cache)
        big = small.scaled(k)
        full = r.Canvas(pw, ph, r.MARGIN)
        ox, oy = (pw - big.w) // 2, (ph - big.h) // 4 * 2
        for y in range(max(0, -oy), min(big.h, ph - oy)):
            src = big.rows[y]
            lo = max(0, -ox)
            full.rows[oy + y][ox + lo:ox + min(big.w, pw - ox)] = src[lo:min(big.w, pw - ox)]
        print("menuterm %d %d %016x" % (cols, rows, fnv(full)))


main(sys.argv[1])
