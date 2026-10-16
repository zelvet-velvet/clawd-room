#!/usr/bin/env python3
"""Writes the scenes in tools/fixtures/*.fx that the C and Python drawings are compared on (desks placed like the real room)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import room_tui as r  # noqa: E402
import themes  # noqa: E402

OUT = os.path.join(HERE, "fixtures")
STATES = ["work", "think", "wait", "idle", "cheer", "sleep", "work", "think"]
NAMES = ["clawd-room", "park", "api", "web", "docs", "infra", "tests", "notes", "alpha", "beta"]


def desks(theme, w, h, n, now, extra=None, states=None):
    """Session lines with every Clawd seated at its desk (as the real room places them)."""
    sess = [r.Session("s%02d" % i, 900 + i * 10) for i in range(n)]
    geo = r.Geo(w, h, sess, themes.THEMES[theme]())
    lines = []
    for i, s in enumerate(sess):
        st = (states or STATES)[i % len(STATES)] if not states else states[i]
        home = geo.homes.get(s.sid)
        px, py = (home[0] - 5, home[1] + r.SEAT_DY) if home else (5 + i, 20)
        line = "session %s name=%s created=%d state=%s tool=%s px=%d py=%d" % (
            s.sid, NAMES[i % len(NAMES)], 900 + i * 10, st, ["Bash", "Edit", "mcp__x__y", "Read", "Foo", "Task"][i % 6], px, py)
        if st == "think":
            line += " hold=%.1f" % (now + 0.4)
        if st == "cheer":
            line += " state_t=%.1f" % (now - 1.0)
        if st == "sleep":
            line += " state=idle last=%.1f" % (now - 200)
        line += (" " + extra(i, geo)) if extra else ""
        lines.append(line)
    return lines


def write(name, lines):
    with open(os.path.join(OUT, name + ".fx"), "w") as f:
        f.write("\n".join(lines) + "\n")


def scene(name, theme, cols, rows, n, scale=1, extra=None, head=(), tail=(), states=None, ratio=None):
    k = max(1, scale)
    w, h = cols // k, rows * 2 // k // 2 * 2
    if ratio:
        w, h = r.RATIOS[ratio]
    now = 1000.0
    lines = ["theme " + theme, "title room-%s" % name, "terminal %d %d" % (cols, rows), "time %.1f 3.4" % now, "scale %d" % scale]
    if ratio:
        lines.append("ratio " + ratio)
    lines += list(head) + desks(theme, w, h, n, now, extra, states) + list(tail)
    write(name, lines)


os.makedirs(OUT, exist_ok=True)
for th in ("office", "coffee", "park"):
    scene(th + "-basic", th, 110, 32, 3)
    scene(th + "-many", th, 110, 32, 8)
    scene(th + "-overflow", th, 110, 24, 12)
scene("office-scale2", "office", 160, 60, 4, scale=2)
scene("coffee-ratio", "coffee", 200, 60, 3, ratio="16:9")
scene("office-empty", "office", 110, 32, 0)
scene("office-gates", "office", 110, 32, 3,
      head=["peer beta right", "peer gamma up", "peer delta -", "neighbor left coffee", "neighbor right park", "neighbor up office", "neighbor down park",
            "linkpos left 40.5", "linkpos right 33", "linkpos up 70", "linkpos down 25.5", "gate left 1.0", "gate right 1.6", "gate up 2.0", "gate down 1.3"])
scene("office-gates-defaults", "office", 110, 32, 2, head=["neighbor left office", "neighbor right coffee", "neighbor up park", "neighbor down office"])


def acts(i, geo):
    return ["", "cup=1010", "chat_with=s03 stage=talk chat_kind=0", "chat_with=s02 stage=talk chat_kind=2", "emote=jump@990", "react=1010", "peek=1", ""][i % 8]


scene("office-props", "office", 120, 40, 8, extra=acts, states=["idle"] * 8,
      tail=["particle 30.5 20 0 0 0.9 240 96 144 1", "particle 31 22 0 0 0.9 250 210 70 0", "particle 60 22 0 0 0.1 250 210 70 0",
            "guest g1 name=visitor px=50 py=45 face=-1", "guest g2 name=wanderer px=70.5 py=46 face=1 moving=1 walk_phase=4 chat_with=g1 stage=talk chat_kind=1"])
scene("coffee-visit", "coffee", 120, 40, 4, states=["idle"] * 4,
      extra=lambda i, g: ["chat_with=s01 stage=talk chat_kind=6 part=host", "chat_with=s00 stage=talk chat_kind=6 part=visitor", "", "cup=1100"][i])
scene("park-playground", "park", 120, 40, 4, states=["idle"] * 4,
      extra=lambda i, g: ["emote=swing@990 px=22 py=12", "emote=slide@990", "moving=1 walk_phase=7 face=-1", ""][i].replace("px=22 py=12", ""))
print("fixtures in", OUT, len(os.listdir(OUT)))
