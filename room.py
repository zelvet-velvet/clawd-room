#!/usr/bin/env python3
"""clawd-room: a pixel room where every running Claude Code session is a little character.

Reads events written by hook.py and animates one character per session:
  thinking -> dots, working -> typing at a desk, needs OK -> sign, done -> cheer,
  idle for a while -> asleep, session ended -> walks out.

Run:  .venv/bin/python room.py            (window via WSLg)
      .venv/bin/python room.py --shot a.png --frames 120   (headless screenshot)
"""
import argparse
import json
import math
import os
import time
import zlib

import pygame

EVENTS = os.environ.get("CLAWD_EVENTS", os.path.expanduser("~/.clawd-room/events.jsonl"))
W, H = 320, 180  # low-res canvas, scaled up with nearest neighbour
FPS = 30

REPLAY_SECONDS = 600  # at start, show sessions that were active in the last 10 min
SLEEP_AFTER = 90  # idle seconds before a character falls asleep
CHEER_FOR = 2.5
HOLD_WORK = 0.8  # keep the typing animation a moment after a tool call ends
STALE_AFTER = 1800  # drop a session with no events for 30 min (closed without SessionEnd)
LEAVE_FOR = 1.4
ARRIVE_FOR = 1.0

# ---- palette ---------------------------------------------------------------
WALL = (38, 42, 66)
WALL2 = (46, 51, 78)
FLOOR = (92, 64, 52)
FLOOR2 = (80, 55, 45)
DESK = (140, 100, 66)
DESK_TOP = (170, 126, 84)
INK = (20, 20, 30)
WHITE = (235, 235, 245)
YELLOW = (250, 210, 70)
BODIES = [
    (232, 120, 70), (96, 170, 230), (130, 200, 120), (200, 130, 220),
    (240, 190, 80), (110, 210, 200), (230, 110, 140), (170, 170, 240),
]
TOOL_KINDS = {
    "Bash": ("BASH", (110, 230, 120)),
    "Edit": ("EDIT", (250, 210, 70)), "Write": ("EDIT", (250, 210, 70)),
    "MultiEdit": ("EDIT", (250, 210, 70)), "NotebookEdit": ("EDIT", (250, 210, 70)),
    "Read": ("READ", (110, 200, 240)), "Grep": ("READ", (110, 200, 240)),
    "Glob": ("READ", (110, 200, 240)),
    "WebFetch": ("WEB", (220, 120, 230)), "WebSearch": ("WEB", (220, 120, 230)),
    "Task": ("AGENT", (240, 150, 80)), "Agent": ("AGENT", (240, 150, 80)),
}

# ---- tiny 3x5 pixel font ---------------------------------------------------
_G = {
    "A": "010/101/111/101/101", "B": "110/101/110/101/110", "C": "011/100/100/100/011",
    "D": "110/101/101/101/110", "E": "111/100/110/100/111", "F": "111/100/110/100/100",
    "G": "011/100/101/101/011", "H": "101/101/111/101/101", "I": "111/010/010/010/111",
    "J": "001/001/001/101/010", "K": "101/101/110/101/101", "L": "100/100/100/100/111",
    "M": "101/111/111/101/101", "N": "110/101/101/101/101", "O": "010/101/101/101/010",
    "P": "110/101/110/100/100", "Q": "010/101/101/110/011", "R": "110/101/110/101/101",
    "S": "011/100/010/001/110", "T": "111/010/010/010/010", "U": "101/101/101/101/111",
    "V": "101/101/101/101/010", "W": "101/101/111/111/101", "X": "101/101/010/101/101",
    "Y": "101/101/010/010/010", "Z": "111/001/010/100/111",
    "0": "111/101/101/101/111", "1": "010/110/010/010/111", "2": "110/001/010/100/111",
    "3": "110/001/010/001/110", "4": "101/101/111/001/001", "5": "111/100/110/001/110",
    "6": "011/100/111/101/111", "7": "111/001/010/010/010", "8": "111/101/111/101/111",
    "9": "111/101/111/001/110",
    "-": "000/000/111/000/000", "_": "000/000/000/000/111", ".": "000/000/000/000/010",
    "!": "010/010/010/000/010", "?": "110/001/010/000/010", "/": "001/001/010/100/100",
    ":": "000/010/000/010/000", " ": "000/000/000/000/000",
}
FONT = {c: [[int(b) for b in row] for row in g.split("/")] for c, g in _G.items()}


def text_width(s):
    return len(s) * 4 - 1 if s else 0


def draw_text(surf, x, y, s, color):
    for ch in s.upper():
        g = FONT.get(ch, FONT["?"])
        for ry, row in enumerate(g):
            for rx, bit in enumerate(row):
                if bit:
                    surf.set_at((x + rx, y + ry), color)
        x += 4


def draw_text_c(surf, cx, y, s, color):
    draw_text(surf, cx - text_width(s) // 2, y, s, color)


def px(surf, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        surf.set_at((int(x), int(y)), c)


def rect(surf, x, y, w, h, c):
    surf.fill(c, (int(x), int(y), int(w), int(h)))


# ---- sessions --------------------------------------------------------------
class Session:
    def __init__(self, sid, t):
        self.sid = sid
        self.name = ""
        self.state = "idle"
        self.state_t = t
        self.last = t
        self.created = t
        self.tool = ""
        self.hold_work_until = 0.0
        self.left_at = None
        self.body = BODIES[zlib.crc32(sid.encode()) % len(BODIES)]
        self.seed = (zlib.crc32(sid.encode()) % 1000) / 1000.0

    def set(self, state, t):
        self.state = state
        self.state_t = t

    def shown(self, now):
        """The state to draw, after the little delays that make it look calm."""
        if self.state == "think" and now < self.hold_work_until:
            return "work"
        if self.state == "cheer" and now - self.state_t > CHEER_FOR:
            return "sleep" if now - self.state_t > SLEEP_AFTER else "idle"
        if self.state == "idle" and now - self.last > SLEEP_AFTER:
            return "sleep"
        return self.state


class Room:
    def __init__(self):
        self.sessions = {}

    def apply(self, r):
        t, sid, ev = r["t"], r["sid"], r["ev"]
        s = self.sessions.get(sid)
        if ev == "SessionEnd":
            if s and s.left_at is None:
                s.left_at = t
            return
        if s is None:
            s = Session(sid, t)
            self.sessions[sid] = s
        elif s.left_at is not None:  # the id came back (resume)
            s.left_at = None
        s.last = t
        if r.get("cwd"):
            s.name = r["cwd"]
        if ev == "SessionStart":
            s.set("idle", t)
        elif ev == "UserPromptSubmit":
            s.set("think", t)
        elif ev == "PreToolUse":
            s.tool = r.get("tool", "")
            s.set("work", t)
            s.hold_work_until = t + HOLD_WORK
        elif ev == "PostToolUse":
            s.set("think", t)
            s.hold_work_until = max(s.hold_work_until, t + HOLD_WORK)
        elif ev == "Notification":
            s.set("wait", t)
        elif ev == "Stop":
            s.set("cheer", t)

    def prune(self, now):
        for sid in list(self.sessions):
            s = self.sessions[sid]
            if s.left_at is not None and now - s.left_at > LEAVE_FOR:
                del self.sessions[sid]
            elif now - s.last > STALE_AFTER and s.left_at is None:
                s.left_at = now

    def ordered(self):
        return sorted(self.sessions.values(), key=lambda s: (s.created, s.sid))


class Tail:
    """Follow the events file; keep only the last lines if it grew large."""

    def __init__(self, path):
        self.path = path
        self.pos = 0
        self.buf = b""

    def history(self, since):
        out = []
        try:
            size = os.path.getsize(self.path)
            if size > 2_000_000:
                with open(self.path, "rb") as f:
                    lines = f.read().splitlines()[-500:]
                with open(self.path, "wb") as f:
                    f.write(b"\n".join(lines) + b"\n")
            with open(self.path, "rb") as f:
                data = f.read()
            self.pos = len(data)
            for line in data.splitlines():
                r = self._parse(line)
                if r and r["t"] >= since:
                    out.append(r)
        except OSError:
            self.pos = 0
        return out

    def poll(self):
        out = []
        try:
            size = os.path.getsize(self.path)
        except OSError:
            return out
        if size < self.pos:  # file was truncated
            self.pos, self.buf = 0, b""
        if size == self.pos:
            return out
        with open(self.path, "rb") as f:
            f.seek(self.pos)
            data = f.read()
        self.pos += len(data)
        self.buf += data
        *lines, self.buf = self.buf.split(b"\n")
        for line in lines:
            r = self._parse(line)
            if r:
                out.append(r)
        return out

    @staticmethod
    def _parse(line):
        try:
            r = json.loads(line)
            r["t"] = float(r["t"])
            r["sid"] = str(r["sid"])
            r["ev"] = str(r["ev"])
            return r
        except (ValueError, KeyError, TypeError):
            return None


# ---- drawing ---------------------------------------------------------------
def draw_background(surf, t, count):
    surf.fill(WALL)
    for y in range(12, 100, 8):  # faint wall panels
        rect(surf, 0, y, W, 1, WALL2)
    rect(surf, 0, 100, W, H - 100, FLOOR)
    for y in range(104, H, 8):
        rect(surf, 0, y, W, 1, FLOOR2)
    rect(surf, 0, 99, W, 2, (60, 42, 34))
    # window with a moon and twinkling stars
    rect(surf, 244, 18, 52, 38, (200, 200, 215))
    rect(surf, 246, 20, 48, 34, (24, 28, 58))
    rect(surf, 269, 20, 2, 34, (200, 200, 215))
    rect(surf, 246, 36, 48, 2, (200, 200, 215))
    for i, (sx, sy) in enumerate([(250, 24), (260, 30), (280, 26), (286, 44), (254, 46), (276, 50)]):
        if int(t * 1.5 + i) % 3:
            px(surf, sx, sy, WHITE)
    rect(surf, 284, 22, 5, 5, (240, 236, 200))
    rect(surf, 286, 22, 3, 3, (24, 28, 58))
    # banner
    draw_text(surf, 6, 4, "CLAWD ROOM", YELLOW)
    label = "%d SESSION%s" % (count, "" if count == 1 else "S")
    draw_text(surf, W - 6 - text_width(label), 4, label, WHITE)
    draw_text(surf, 6, H - 8, "Q QUIT   F FULLSCREEN", (120, 120, 150))


def draw_character(surf, cx, by, s, state, t, ox):
    """Body is 14x11 with its top-left at (cx-7, by); the desk is drawn by the caller."""
    walking = ox != 0
    bob = 0
    arm_l = arm_r = 0  # 0 down, 1 up
    x = int(cx - 7 + ox)
    phase = t * 6 + s.seed * 10

    if state == "cheer":
        bob = -int(abs(math.sin((t - s.state_t) * 8)) * 6)
        arm_l = arm_r = 1
    elif state == "wait":
        arm_r = 1 if int(t * 3) % 2 else 0
    elif state == "think":
        bob = -1 if int(t * 2 + s.seed * 5) % 2 else 0
    elif state == "idle":
        bob = -1 if int(t * 1 + s.seed * 5) % 3 == 0 else 0
    elif state == "sleep":
        bob = 1 if int(t * 0.8) % 2 else 0
    if walking:
        bob = -1 if int(t * 10) % 2 else 0
    y = by + bob
    body, dark = s.body, tuple(int(c * 0.7) for c in s.body)

    # legs
    shift = 1 if walking and int(t * 10) % 2 else 0
    rect(surf, x + 3 + shift, by + 11, 3, 3, dark)
    rect(surf, x + 8 - shift, by + 11, 3, 3, dark)
    # body (corners cut)
    rect(surf, x + 1, y, 12, 11, body)
    rect(surf, x, y + 1, 14, 9, body)
    rect(surf, x + 1, y + 9, 12, 2, dark)
    # arms
    ay = y + 2 if arm_l else y + 6
    rect(surf, x - 2, ay, 2, 4, body if not arm_l else body)
    ay = y + 2 if arm_r else y + 6
    rect(surf, x + 14, ay, 2, 4, body)
    # face
    blink = (t + s.seed * 7) % 3.2 < 0.12
    ey = y + 3
    if state == "sleep" or blink:
        rect(surf, x + 3, ey + 1, 3, 1, INK)
        rect(surf, x + 8, ey + 1, 3, 1, INK)
    elif state == "cheer":
        for dx in (3, 8):  # happy ^ ^
            px(surf, x + dx, ey + 1, INK)
            px(surf, x + dx + 1, ey, INK)
            px(surf, x + dx + 2, ey + 1, INK)
    else:
        look = -1 if state == "think" else 0
        rect(surf, x + 4, ey + look, 2, 3, INK)
        rect(surf, x + 9, ey + look, 2, 3, INK)
        px(surf, x + 4, ey + look, WHITE)
        px(surf, x + 9, ey + look, WHITE)
    # mouth
    if state == "cheer":
        rect(surf, x + 5, y + 7, 4, 2, INK)
    elif state == "wait":
        rect(surf, x + 6, y + 8, 2, 1, INK)
    elif state != "sleep":
        rect(surf, x + 5, y + 8, 4, 1, dark)
    return x, y


def draw_desk(surf, cx, by, state, tool, t):
    x = cx - 22
    rect(surf, x, by + 11, 44, 3, DESK_TOP)
    rect(surf, x, by + 14, 44, 8, DESK)
    rect(surf, x + 2, by + 22, 3, 4, DESK)
    rect(surf, x + 39, by + 22, 3, 4, DESK)
    # monitor on the right side of the desk
    mx, my = cx + 12, by
    rect(surf, mx, my, 12, 9, (60, 60, 70))
    kind, color = TOOL_KINDS.get(tool, (tool[:5].upper() or "TOOL", (170, 170, 190)))
    if tool.startswith("mcp__"):
        kind, color = "MCP", (150, 220, 160)
    on = state == "work"
    rect(surf, mx + 1, my + 1, 10, 7, (14, 18, 26) if not on else (10, 24, 20))
    if on:
        for i in range(4):  # scrolling lines
            ly = (i * 2 + int(t * 8)) % 8
            rect(surf, mx + 2, my + 1 + ly, 6 - (i % 2) * 2, 1, color)
    rect(surf, mx + 4, my + 9, 4, 2, (60, 60, 70))
    return kind


def draw_extras(surf, cx, by, x, y, state, s, t):
    if state == "think":
        for i in range(3):  # bubbling dots
            on = int(t * 3) % 4
            c = WHITE if i < on else (90, 90, 120)
            rect(surf, x + 16 + i * 4, y - 5 - i, 3, 3, c)
    elif state == "wait":
        rect(surf, cx - 5, by - 20, 11, 11, YELLOW)
        rect(surf, cx - 4, by - 19, 9, 9, (250, 235, 150))
        draw_text(surf, cx - 1, by - 17, "!", INK)
        rect(surf, cx, by - 9, 1, 3, (120, 90, 40))
    elif state == "sleep":
        for i in range(3):
            k = (t * 0.7 + i / 3.0) % 1.0
            draw_text(surf, int(x + 15 + k * 6), int(y - 2 - k * 14), "Z", (180, 190, 240) if k < 0.7 else (110, 115, 150))
    elif state == "cheer":
        for i in range(5):
            a = (t - s.state_t) * 6 + i * 1.26
            sx = int(cx + math.cos(a) * 14)
            sy = int(by + 4 + math.sin(a * 1.3) * 10 - 6)
            c = (YELLOW, WHITE)[i % 2]
            px(surf, sx, sy, c)
            px(surf, sx - 1, sy, c)
            px(surf, sx + 1, sy, c)
            px(surf, sx, sy - 1, c)
            px(surf, sx, sy + 1, c)
    elif state == "work":
        # hands on the desk, typing
        hx = x + 3 + (int(t * 12) % 2) * 4
        rect(surf, hx, by + 9, 3, 2, s.body)
        rect(surf, x + 8 - (int(t * 12) % 2) * 3, by + 9, 3, 2, s.body)


LABEL = {"idle": "IDLE", "think": "THINKING", "wait": "NEEDS OK", "cheer": "DONE!", "sleep": "ZZZ"}


def draw_room(surf, room, now, t):
    sessions = room.ordered()
    draw_background(surf, t, len([s for s in sessions if s.left_at is None]))
    if not sessions:
        draw_text_c(surf, W // 2, 86, "NO SESSIONS YET", (150, 150, 180))
        draw_text_c(surf, W // 2, 96, "START CLAUDE CODE", (110, 110, 140))
        return
    per_row = 5
    rows = (len(sessions) + per_row - 1) // per_row
    base = [84] if rows == 1 else [64, 124]
    for i, s in enumerate(sessions):
        r, c = divmod(i, per_row)
        if r >= len(base):
            break
        n = min(per_row, len(sessions) - r * per_row)
        cx = int(W / 2 + (c - (n - 1) / 2) * 62)
        by = base[r]
        ox = 0
        if s.left_at is not None:
            ox = int(((now - s.left_at) / LEAVE_FOR) ** 2 * 200) or 1
        elif now - s.created < ARRIVE_FOR:
            ox = -int((1 - (now - s.created) / ARRIVE_FOR) ** 2 * 120) or -1
        state = s.shown(now)
        x, y = draw_character(surf, cx, by, s, state, t, ox)
        if ox:
            continue  # walking: no desk props
        kind = draw_desk(surf, cx, by, state, s.tool, t)
        draw_extras(surf, cx, by, x, y, state, s, t)
        draw_text_c(surf, cx, by + 28, (s.name or s.sid[:8]).upper()[:14], WHITE)
        label = kind if state == "work" else LABEL.get(state, "")
        draw_text_c(surf, cx, by + 35, label, (150, 155, 185))
    if len(sessions) > per_row * len(base):
        draw_text_c(surf, W // 2, 160, "+%d MORE" % (len(sessions) - per_row * len(base)), WHITE)


# ---- main ------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--events", default=EVENTS)
    ap.add_argument("--shot", help="save a PNG after --frames frames and exit")
    ap.add_argument("--frames", type=int, default=90)
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--at", type=float, default=None, help="pretend 'now' is this timestamp (for --shot)")
    args = ap.parse_args()

    pygame.display.init()
    pygame.display.set_caption("Clawd Room")
    win = pygame.display.set_mode((W * args.scale, H * args.scale), pygame.RESIZABLE)
    canvas = pygame.Surface((W, H))
    clock = pygame.time.Clock()

    room = Room()
    tail = Tail(args.events)
    start = args.at if args.at is not None else time.time()
    for r in tail.history(start - REPLAY_SECONDS):
        room.apply(r)

    t0 = time.time()
    frame = 0
    fullscreen = False
    running = True
    while running:
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False
            elif e.type == pygame.KEYDOWN:
                if e.key in (pygame.K_q, pygame.K_ESCAPE):
                    running = False
                elif e.key == pygame.K_f:
                    fullscreen = not fullscreen
                    pygame.display.toggle_fullscreen()
        t = time.time() - t0  # animation clock
        now = (args.at + t) if args.at is not None else time.time()
        for r in tail.poll():
            room.apply(r)
        room.prune(now)
        draw_room(canvas, room, now, t)

        ww, wh = win.get_size()
        k = max(1, min(ww // W, wh // H))
        scaled = pygame.transform.scale(canvas, (W * k, H * k))
        win.fill((10, 10, 16))
        win.blit(scaled, ((ww - W * k) // 2, (wh - H * k) // 2))
        pygame.display.flip()
        frame += 1
        if args.shot and frame >= args.frames:
            pygame.image.save(pygame.transform.scale(canvas, (W * args.scale, H * args.scale)), args.shot)
            break
        clock.tick(FPS)
    pygame.quit()


if __name__ == "__main__":
    main()
