#!/usr/bin/env python3
"""Prints the Windows geometry of room_tui.py / windetect.py on synthetic inputs, like `c/clawd-room --golden-win`."""
import os
import sys
import types

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import room_tui as r  # noqa: E402
import windetect as wd  # noqa: E402

R = [(0, 0, 800, 600), (830, 0, 1600, 600), (800, 100, 1600, 700), (-790, 0, 10, 600), (0, 620, 800, 1200), (0, -590, 800, 10), (850, 0, 1650, 600), (400, 300, 1200, 900), (810, 560, 1700, 1100)]
SIDE = {None: -1, "left": 0, "right": 1, "up": 2, "down": 3}
for a in range(9):
    for b in range(9):
        print("side %d %d %d %d %d" % (a, b, SIDE[wd.neighbor_side(R[a], R[b], 30, 0)], SIDE[wd.neighbor_side(R[a], R[b], 30, 15)], SIDE[wd.neighbor_side(R[a], R[b], 60, 0)]))
G = [[(10.5, 8), (3.25, 6)], [(0, 8.5), (4, 5)], [(7.3, 13), (1.1, 13)], [(100.25, 20), (200.5, 12)]]
for i, g in enumerate(G):
    x, y = r.snap_both(100.5, 230.25, g), r.snap_both(55, 56, g)
    print("snap %d %d %d %d %d" % (i, x[0], x[1], y[0], y[1]))
AR = [(10, 40, 890, 650), (0, 0, 1720, 880)]
for a in range(2):
    for rep in range(3):
        for dpi in (96, 120):
            reports = {} if rep == 0 else {"cell": (10, 20)} if rep == 1 else {"cell": (8, 16)}
            cw, ch = r.cell_px(AR[a], dpi, reports, 215 if a else 110, 55 if a else 32)
            print("cell %d %d %d %.6f %.6f" % (a, rep, dpi, cw, ch))
o = r.OpenSetup(None, "x", 8, "110x32", {}, lambda: None, True, 14)
px = o.window_px((183, 192, 1922, 1082), (192, 243, 1913, 1073), 215, 55)
print("px %d %.6f %.6f" % (px is not None, px[0], px[1]))
o = r.OpenSetup(None, "x", 11, "128X36", {}, lambda: None, True, 14)
px = o.window_px((183, 192, 1922, 1082), (192, 243, 1913, 1073), 215, 55)
print("px %d %.6f %.6f" % (px is not None, px[0], px[1]))
print("px %d" % (o.window_px((183, 192, 1922, 1082), None, 215, 55) is not None))
w = wd.WindowWatcher()
w.proc = types.SimpleNamespace(stdout=["clawd-room alpha-1|10|20|800|600|18|60|792|592|120|40\n", "clawd-room we|ird|-5|0|300|200|5|30|295|195|96|30\n", "clawd-room y|1|2|3|4|5|6|7|8|9|x|11\n", "--\n"])
w._read()
for t, rect in w.windows.items():
    a = w.areas[t]
    print("win %s %d %d %d %d %d %d %d %d %d" % (t, *rect, *a, w.dpis[t]))
SIDES = ["left", "right", "up", "down"]
for variant in range(4):
    room = r.Room()
    room.geo = types.SimpleNamespace(ymin=50, ymax=120 - 8 * (variant & 1))
    room.view = (4 * (variant & 2), 2 * (variant & 2), 1 + (variant >> 1))
    room.neighbors = {s: "peer-" + s for s in SIDES}
    wins = {"clawd-room me": (100, 100, 1000, 700), "clawd-room peer-left": (-700, 110, 90, 690), "clawd-room peer-right": (1010, 140, 1900, 760), "clawd-room peer-up": (150, -500, 900, 90), "clawd-room peer-down": (120, 710, 1050, 1300)}
    areas = {"clawd-room me": (108, 150, 992, 692), "clawd-room peer-left": (-692, 160, 82, 682), "clawd-room peer-right": (1018, 190, 1892, 752), "clawd-room peer-up": (158, -450, 892, 82), "clawd-room peer-down": (128, 760, 1042, 1292)}
    watcher = types.SimpleNamespace(area_of=lambda rid: areas.get(wd.PREFIX + rid))
    peers = {}
    for s in SIDES:
        p = {}
        if variant >= 1 and s != "up":
            p["grid"] = (1018.0, 190.0, 9.5, 19.0, 2.0, 3.0, 4.0)
        if variant >= 2 and s in ("left", "right"):
            p["band"] = (200, 600)
        peers["peer-" + s] = p
    g = r.grid_params(room, areas["clawd-room me"], 96, {}, 110, 32)
    pos, closed, scales = r.plan_passages(room, watcher, "me", g, peers)
    line = "gates %d %.6f %.6f %.6f %.6f" % (variant, g[2], g[3], g[4], g[6])
    for s in SIDES:
        line += " | %d %.1f %d %d %.6f" % (s in pos, pos.get(s, 0), s in closed, s in scales, scales.get(s, 0))
    print(line)
