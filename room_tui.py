#!/usr/bin/env python3
"""clawd-room, terminal edition: one little pixel character per running Claude Code session.

Reads the events written by hook.py (~/.clawd-room/events.jsonl) and draws the room in the
terminal. Each terminal cell shows two pixels with the half-block trick: "▀" with the top pixel
as foreground colour and the bottom pixel as background colour (truecolor).

Run:   python3 room_tui.py
Keys:  q / Esc quit,  p pause,  ?  help
Test:  python3 room_tui.py --dump 100x30 --events /tmp/e.jsonl --at 1800000010
Python 3 standard library only.
"""
import argparse
import json
import math
import os
import random
import re
import select
import shutil
import signal
import sys
import termios
import time
import tty
import zlib

import roomlink
import themes
import windetect

EVENTS = os.environ.get("CLAWD_EVENTS", os.path.expanduser("~/.clawd-room/events.jsonl"))
FPS = 16

REPLAY_SECONDS = 600  # at start, show sessions that were active in the last 10 min
SLEEP_AFTER = 90  # idle seconds before a character falls asleep
CHEER_FOR = 2.5
HOLD_WORK = 0.8  # keep the typing animation a moment after a tool call ends
STALE_AFTER = 1800  # drop a session with no events for 30 min (closed without SessionEnd)
LEAVE_MAX = 12.0  # a session that ended is removed after this long even if it never got out
ARRIVE_WALK = 3.0  # a session that started this recently walks in through the door
SEAT_DY = 7  # a Clawd stands in front of its desk, this far below the desk's reference line
AWAY_MAX = 150.0  # a Clawd that went to another window is brought home after this long
OPPOSITE = {"left": "right", "right": "left", "up": "down", "down": "up"}
GATE_DELAY = 1.0  # a gate shows (and moves) only after the windows have been still for this long
MAX_GUESTS = 4  # visitors from other windows that one room shows at a time
WALK_SPEED = 26.0  # pixels per second
LEAVE_SPEED = 50.0
MAX_HISTORY_BYTES = 2_000_000  # only the end of a huge events file is read at start

# ---- layout (all sizes in canvas pixels; one terminal cell = 1 wide x 2 high) -----------
SLOT_H = 40  # height of one character with its desk and labels
SLOT_MIN_W = 30
SLOT_MAX_W = 62
PER_ROW = 5
BANNER_H = 8
MIN_COLS = 44
# The standard room for each shape, in pixels (width, height). A terminal cell is 1 px wide and
# 2 px tall, so on screen the shape is the same as these numbers. Every window of the same shape
# shows the same room, which is what lets Clawds walk from one into the next.
RATIOS = {"3:1": (144, 48), "2:1": (120, 60), "16:9": (128, 72), "4:3": (128, 96), "1:1": (120, 120), "3:4": (96, 128)}
RATIO_CHOICES = ["auto"] + list(RATIOS)  # "auto": the room is as big and as shaped as the window
DEFAULT_RATIO = "auto"
DEFAULT_SIZE = "110x32"  # window size in cells when a room opens (WSL only), at DEFAULT_FONT; "off" leaves the window alone
DEFAULT_FONT = 8  # terminal font size in points the room asks for when it opens (0 leaves the font alone)
PROFILE_FONT = 14  # the terminal profile's own font size: what Ctrl+0 gives, the zoom steps count from it
SCALE_CAP = 3  # the automatic TUI scale never goes above this (the [ and ] keys go up to MAX_SCALE)
MAX_SCALE = 8


def std_grid(ratio):  # (not for "auto")
    """Terminal cells (columns, rows) that hold the standard room of this shape at scale 1."""
    w, h = RATIOS[ratio]
    return w, h // 2
MIN_ROWS = SLOT_H // 2

# ---- palette (same as room.py) ----------------------------------------------------------
MARGIN = (14, 14, 18)  # terminal area around the room
WALL = (34, 36, 46)
WALL2 = (38, 41, 52)
FLOOR = (62, 58, 58)
FLOOR2 = (57, 53, 53)
BASEBOARD = (48, 45, 46)
DESK = (112, 92, 76)
DESK_TOP = (128, 106, 88)
CLAWD = (205, 110, 88)  # #CD6E58, the colour of the official pixel Clawd, flat, no shading
BLUSH = (250, 200, 216)
PINK = (240, 96, 144)
BUBBLE_EDGE = (136, 136, 136)
INK = (20, 20, 30)
WHITE = (235, 235, 245)
GLINT = (242, 190, 168)  # pale peach, not white: the lit part of a happy eye (eye is 1 wide, 2 high)
YELLOW = (250, 210, 70)
GREY = (150, 155, 185)
GREEN, BLUE, PINK, ORANGE = (110, 230, 120), (110, 200, 240), (220, 120, 230), (240, 150, 80)
TOOL_KINDS = {
    "Bash": ("BASH", GREEN),
    "Edit": ("EDIT", YELLOW), "Write": ("EDIT", YELLOW),
    "MultiEdit": ("EDIT", YELLOW), "NotebookEdit": ("EDIT", YELLOW),
    "Read": ("READ", BLUE), "Grep": ("READ", BLUE), "Glob": ("READ", BLUE),
    "WebFetch": ("WEB", PINK), "WebSearch": ("WEB", PINK),
    "Task": ("AGENT", ORANGE), "Agent": ("AGENT", ORANGE),
}

# ---- editable sprites (sprites/*.txt, see sprites.py): a frame found there is drawn instead of the code's drawing ----
import sprites
CLAWD_SPRITES = sprites.SpriteFile(os.path.join(sprites.DIR, "clawd.txt"))
ITEM_SPRITES = sprites.SpriteFile(os.path.join(sprites.DIR, "items.txt"))


def pose_key(p):
    """What picks a Clawd frame: everything in the pose except the bob (that only moves the whole sprite)."""
    return "arms=%d,%d legs=%d eyes=%s look=%d,%d lean=%d" % (*p["arms"], p["legs"], p["eyes"], *p["look"], p["lean"])


def blit_sprite(cv, sp, x, y, body=None):
    for dy, row in enumerate(sp.rows):
        for dx, c in enumerate(row):
            if c is not None:
                cv.px(x + dx, y + dy, (body or CLAWD) if c == "body" else c)


def item(name, cv, x, y):
    """Draws the item sprite `name` with its top left at (x, y); False if the files have no such item."""
    sp = ITEM_SPRITES.by_name.get(name)
    if sp:
        blit_sprite(cv, sp, x, y)
    return bool(sp)


# ---- tiny 3x5 pixel font (same glyphs as room.py) ---------------------------------------
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
    ":": "000/010/000/010/000", "<": "001/010/100/010/001", "^": "010/101/000/000/000", ">": "100/010/001/010/100", "+": "000/010/111/010/000", " ": "000/000/000/000/000",
}
FONT = {c: [[int(b) for b in row] for row in g.split("/")] for c, g in _G.items()}


def text_width(s):
    return len(s) * 4 - 1 if s else 0


# ---- canvas -----------------------------------------------------------------------------
class Canvas:
    """A grid of RGB pixels. rows[y][x] is an (r, g, b) tuple."""

    def __init__(self, w, h, bg):
        self.w, self.h = w, h
        self.rows = [[bg] * w for _ in range(h)]

    def px(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.rows[y][x] = c

    def rect(self, x, y, w, h, c):
        x0, x1 = max(0, int(x)), min(self.w, int(x + w))
        if x1 <= x0:
            return
        fill = [c] * (x1 - x0)
        for yy in range(max(0, int(y)), min(self.h, int(y + h))):
            self.rows[yy][x0:x1] = fill

    def line(self, x0, y0, x1, y1, c):
        """A one-pixel line."""
        x0, y0, x1, y1 = int(x0), int(y0), int(x1), int(y1)
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1):
            self.px(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, c)

    def scaled(self, k):
        """A copy with every pixel drawn k times wider and k times taller."""
        out = Canvas(self.w * k, self.h * k, WALL)
        out.rows = [[c for c in row for _ in range(k)] for row in self.rows for _ in range(k)]
        return out

    def blit(self, other, x, y):
        for yy, row in enumerate(other.rows):
            if 0 <= y + yy < self.h:
                self.rows[y + yy][x:x + other.w] = row[:max(0, self.w - x)] if x + other.w > self.w else row

    def text(self, x, y, s, c):
        for ch in s.upper():
            for ry, row in enumerate(FONT.get(ch, FONT["?"])):
                for rx, bit in enumerate(row):
                    if bit:
                        self.px(x + rx, y + ry, c)
            x += 4

    def text_c(self, cx, y, s, c):
        self.text(cx - text_width(s) // 2, y, s, c)


# ---- sessions (state machine ported from room.py) ---------------------------------------
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
        self.gone = False  # walked out of the room
        self.rng = random.Random(zlib.crc32(sid.encode()))
        self.px = self.py = None  # body centre x and body top y, in room pixels (None: not placed)
        self.face = 1  # which way it looks: 1 right, -1 left
        self.goal = None  # where it is walking to
        self.moving = False
        self.walk_phase = 0.0
        self.next_plan = 0.0  # when an idle one decides what to do next
        self.chat_with = None  # sid of the friend it is visiting or talking to
        self.stage = ""  # "walk" (going to the friend), "wait" (being visited), "talk", "chase" or "flee"
        self.chat_until = 0.0
        self.chat_kind = 0
        self.walk_t = 0.0  # seconds of walking so far (for easing)
        self.burst_t = None  # state_t of the cheer that already threw its sparks
        self.next_heart = 0.0
        self.run = 1.0  # walking speed factor (running in a game of tag)
        self.emote = None  # (kind, start, end): a little solo act
        self.peek = None  # [sid, end or None]: watching a busy friend work
        self.react_until = 0.0  # applauding a friend who just finished
        self.flee_next = 0.0
        self.catch_ok = 0.0  # no tagging before this time
        self.last_hf = -1
        self.part = ""  # in a visit: "visitor" or "host"
        self.then = None  # (emote kind, seconds) to start when it gets where it is walking to
        self.cup_until = 0.0  # carrying a coffee until then
        self.guest = False  # a visitor from another window
        self.origin = self.origin_sid = self.enter_side = None  # where a visitor comes from, and how it came in
        self.returning = False  # a visitor on its way home
        self.stay_until = 0.0
        self.travel = None  # (peer id, edge) : walking to the door of the next window
        self.route = []  # more goals to walk to after the current one (to the rope or stairs)
        self.vclimb = False  # climbing the rope or the stairs (affects the pose)
        self.leaving = False  # a visitor that already set off for the exit
        self.away = None  # while visiting another window: {"peer", "side", "since", "recalled"}
        self.seed = (zlib.crc32(sid.encode()) % 1000) / 1000.0

    def set(self, state, t):
        self.state = state
        self.state_t = t

    def shown(self, now):
        """The state to draw, after the little delays that make it look calm."""
        if self.guest:
            return "idle"
        if self.state == "think" and now < self.hold_work_until:
            return "work"  # no flicker between two tool calls
        if self.state == "cheer" and now - self.state_t > CHEER_FOR:
            return "sleep" if now - self.state_t > SLEEP_AFTER else "idle"
        if self.state == "idle" and now - self.last > SLEEP_AFTER:
            return "sleep"
        return self.state


class Room:
    def __init__(self):
        self.sessions = {}
        self.last_now = None
        self.geo_key = None
        self.particles = []  # [x, y, vx, vy, life, colour, is_heart]
        self.title = ""  # name of this room, shown in the banner
        self.ratio = DEFAULT_RATIO  # shape of the room: "auto" (follow the window) or a key of RATIOS
        self.size = (0, 0)  # size of the room in pixels as last drawn
        self.theme = themes.Theme()
        self.peers = []  # (name, side) of the other rooms linked to this one; side: "left"/"right"/None
        self.neighbors = {}  # {"left"/"right"/"up"/"down": peer room id} for windows that are close enough
        self.neighbor_themes = {}  # the same keys: the theme name of that room
        self.link_pos = {}  # the same keys: where the passage goes in room pixels (x for up/down, y for left/right)
        self.gate_scale = {}  # the same keys: how many times bigger than normal its gate is drawn (for a smaller font)
        self.scale = 1  # TUI scale: room pixels are scale x scale terminal cells (auto shape only)
        self.view = (0, 0, 1)  # where the room sits in the terminal canvas: x offset, y offset, scale
        self.owns = lambda sid: True  # which sessions this room shows (when several rooms share the sessions)
        self.geo = None
        self.guests = {}  # visitors from other windows, by key
        self.outbox = []  # (peer id, message type, data) waiting to be sent by the link
        self.incoming = []  # messages from other rooms waiting to be handled in step()
        self.gone_peers = set()  # rooms that closed

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
        if r["cwd"]:
            s.name = r["cwd"]
        if ev == "SessionStart":
            s.set("idle", t)
        elif ev == "UserPromptSubmit":
            s.set("think", t)
        elif ev == "PreToolUse":
            s.tool = r["tool"]
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
            if s.left_at is not None and (s.gone or now - s.left_at > LEAVE_MAX):
                del self.sessions[sid]
            elif now - s.last > STALE_AFTER and s.left_at is None:
                s.left_at = now  # walk out, then get removed

    def ordered(self):
        return sorted((s for s in self.sessions.values() if self.owns(s.sid)), key=lambda s: (s.created, s.sid))

    # ---- movement: busy ones sit at their desk, idle ones wander and visit each other ----
    def everyone(self):
        return list(self.sessions.values()) + list(self.guests.values())

    def find(self, sid):
        return self.sessions.get(sid) or self.guests.get(sid)

    def step(self, geo, now):
        """Move everybody a little. Called once per frame with the room geometry."""
        dt = 0.0 if self.last_now is None else min(0.25, max(0.0, now - self.last_now))
        self.last_now = now
        if geo.key != self.geo_key:  # the room changed size: everybody goes back to their desk
            self.geo_key = geo.key
            for s in self.everyone():
                s.px = s.goal = s.chat_with = s.peek = s.emote = s.travel = None
                s.run = 1.0
            self.particles = []
        self.geo = geo
        self.handle_messages(geo, now)
        for s in self.ordered():
            slot = geo.homes.get(s.sid)
            if slot is None:
                continue  # does not fit in the room
            if s.away:
                self.steer_away(s, geo, now)
                continue
            seat = (slot[0] - 5, slot[1] + SEAT_DY)
            if s.travel and s.travel[1] not in geo.neighbors:  # the gate is gone (windows moved): stay
                self.drop_social(s)
                s.goal = None
            if s.px is None:
                s.px, s.py = (-8.0, float(geo.ymax)) if now - s.created < ARRIVE_WALK else seat
                s.next_plan = now + s.rng.uniform(1.0, 4.0)
            self.steer(s, seat, geo, now)
            self.move(s, geo, dt, now)
            self.emit(s, now)
            if s.travel:
                self.cross(s, geo, now)
        for g in list(self.guests.values()):
            if g.px is None:  # (re)appear at the door it came through
                self.enter(g, g.enter_side, 500, geo)
            self.steer_guest(g, geo, now)
            self.move(g, geo, dt, now)
            self.emit(g, now)
            self.cross(g, geo, now)
        self.fly(dt)

    # ---- crossing between windows -------------------------------------------------------------
    @staticmethod
    def frac(s, geo):
        """How far down the walking strip a Clawd is, 0..1000, so the other room can match it."""
        return int(1000 * min(1.0, max(0.0, (s.py - geo.ymin) / max(1, geo.ymax - geo.ymin))))

    @staticmethod
    def spawn_y(frac, geo):
        try:
            frac = min(1000, max(0, int(frac)))
        except (TypeError, ValueError):
            frac = 500
        return geo.ymin + (geo.ymax - geo.ymin) * frac / 1000.0

    def handle_messages(self, geo, now):
        """Clawds arriving from another window, calls to come home, and rooms that closed."""
        for m in self.incoming:
            d, kind = m["data"], m["type"]
            side = d.get("side")
            sid = str(d.get("sid", ""))[:64]
            if kind == "walk" and side in OPPOSITE and sid:
                if d.get("home"):  # one of ours coming back
                    s = self.sessions.get(sid)
                    if s and s.away and s.away["peer"] == m["from"]:
                        self.come_home(s, geo, now, side, d.get("y", 500))
                else:  # a visitor
                    key = "~%s~%s" % (m["from"], sid)
                    if key in self.guests or len(self.guests) >= MAX_GUESTS:
                        continue
                    g = Session(key, now)
                    g.guest, g.origin, g.origin_sid, g.enter_side = True, m["from"], sid, side
                    g.name = str(d.get("name", ""))[:32]
                    g.stay_until = now + g.rng.uniform(25.0, 45.0)
                    g.next_plan = now + g.rng.uniform(1.0, 3.0)
                    self.enter(g, side, d.get("y", 500), geo)
                    self.guests[key] = g
            elif kind == "recall":
                g = self.guests.get("~%s~%s" % (m["from"], sid))
                if g:
                    g.returning = True
        self.incoming = []
        for peer in self.gone_peers:  # a window closed: its Clawds are brought home, its visitors vanish
            for s in self.sessions.values():
                if s.away and s.away["peer"] == peer:
                    self.come_home(s, geo, now, s.away["side"], 500)
            for key in [k for k, g in self.guests.items() if g.origin == peer]:
                del self.guests[key]
        self.gone_peers = set()

    def enter(self, s, side, frac, geo):
        """Put `s` just outside the edge it comes in by and send it into the room."""
        y = self.spawn_y(frac, geo)
        s.route, s.goal, s.vclimb = [], None, False
        if side in ("left", "right"):  # in through the door, which lines up with the other window's door
            s.px, s.py = (-10.0 if side == "left" else geo.w + 10.0), geo.door_y[side]
        elif side == "up":  # slides down the rope hanging from the top
            s.px, s.py, s.goal, s.vclimb = geo.rope_x, -12.0, (geo.rope_x, float(geo.ymin)), True  # down to the rope's end
        else:  # comes up the stairs from the bottom
            s.px, s.py, s.goal, s.vclimb = geo.stairs_x, geo.h + 8.0, (geo.stairs_x, float(geo.ymax)), True  # up to the top of the stairs

    @staticmethod
    def exit_route(s, edge, geo, y=None):
        """Send `s` out through `edge`: straight out sideways, up the rope, or down the stairs."""
        y = s.py if y is None else y
        s.leaving = True
        if edge in ("left", "right"):
            s.goal, s.route = (-12.0 if edge == "left" else geo.w + 12.0, geo.door_y[edge]), []
        else:
            x, out = (geo.rope_x, -14.0) if edge == "up" else (geo.stairs_x, geo.h + 8.0)
            s.goal, s.route = (x, float(geo.ymin if edge == "up" else geo.ymax)), [(x, out)]

    @staticmethod
    def outside(s, geo):
        return not (-6 < s.px < geo.w + 6 and -9 < s.py < geo.h + 1)

    def come_home(self, s, geo, now, side, frac):
        """A Clawd that was in another window walks back in through the door."""
        s.away = None
        self.enter(s, side, frac, geo)
        s.next_plan = now + s.rng.uniform(1.0, 3.0)

    def steer_away(self, s, geo, now):
        """A Clawd that is in another window: ask for it back when its session gets busy or ends."""
        state = s.shown(now)
        if (state in ("work", "think") or s.left_at is not None) and not s.away["recalled"]:
            s.away["recalled"] = True
            self.outbox.append((s.away["peer"], "recall", {"sid": s.sid}))
        if s.left_at is not None:
            s.gone = True
        elif now - s.away["since"] > AWAY_MAX:  # nobody answered: come home anyway
            self.come_home(s, geo, now, s.away["side"], 500)

    def cross(self, s, geo, now):
        """A Clawd that walked out of the room's edge goes to the next window (or home)."""
        if not self.outside(s, geo):
            return
        if s.guest:
            if s.returning:  # back through the door it came in by
                other = OPPOSITE[s.enter_side]
                self.outbox.append((s.origin, "walk", {"sid": s.origin_sid, "home": 1, "side": other,
                                                       "y": self.frac(s, geo)}))
                self.guests.pop(s.sid, None)
        elif s.travel and s.left_at is None:
            peer, edge = s.travel
            other = OPPOSITE[edge]  # it enters by the opposite edge there
            self.outbox.append((peer, "walk", {"sid": s.sid, "name": s.name, "side": other,
                                               "y": self.frac(s, geo)}))
            self.drop_social(s)
            s.travel, s.goal, s.moving, s.route, s.leaving = None, None, False, [], False
            s.away = {"peer": peer, "side": edge, "since": now, "recalled": False}

    def send_one(self, now):
        """The g key: send an idle (or sleeping) Clawd to the neighbouring window right now."""
        geo = self.geo
        if not geo or not geo.neighbors:
            return "NO WINDOW NEXT TO THIS ONE"
        for s in self.ordered():
            if s.away or s.travel or s.px is None or s.left_at is not None or s.guest:
                continue
            if s.shown(now) in ("idle", "sleep"):
                self.drop_social(s)
                s.set("idle", now)
                s.last = now  # wake it up
                edge = sorted(geo.neighbors)[0]
                s.travel = (geo.neighbors[edge], edge)
                self.exit_route(s, edge, geo, min(max(s.py, geo.ymin), geo.ymax))
                return "SENDING %s %s" % ((s.name or "CLAWD").upper()[:10], edge.upper())
        return "NO IDLE CLAWD TO SEND"

    def receive(self, m):
        """Queue a message from another room (handled in the next step)."""
        if m["type"] in ("walk", "recall"):
            self.incoming.append(m)

    def peer_gone(self, peer_id):
        self.gone_peers.add(peer_id)

    @staticmethod
    def drop_social(s):
        """Stop whatever social thing `s` was doing."""
        s.chat_with = s.peek = s.emote = s.travel = s.then = None
        s.route, s.vclimb = [], False
        s.stage = ""
        s.run = 1.0

    def steer(self, s, seat, geo, now):
        """Decide where `s` wants to go and what it wants to do."""
        state = s.shown(now)
        if s.left_at is not None:  # leave through the nearest side
            self.drop_social(s)
            s.goal = (-12.0 if s.px < geo.w / 2 else geo.w + 12.0, s.py)
        elif state in ("work", "think", "sleep"):  # busy or asleep: be at the desk
            self.drop_social(s)
            far = abs(s.px - seat[0]) + abs(s.py - seat[1]) > 0.5
            s.goal = seat if far else None
        elif state != "idle":  # needs OK or just done: free to walk around, no visits
            self.drop_social(s)
            if s.goal is None and now >= s.next_plan:
                self.plan(s, seat, geo, now, social=False)
        else:
            self.idle_step(s, seat, geo, now)

    def steer_guest(self, s, geo, now):
        """A visitor wanders and socialises for a while, then walks out the way it came in."""
        if s.returning or now >= s.stay_until:
            s.returning = True
            if not s.leaving:
                self.drop_social(s)
                self.exit_route(s, s.enter_side, geo)
        else:
            self.idle_step(s, None, geo, now)

    def idle_step(self, s, seat, geo, now):
        """What a free Clawd (idle, or a visitor) does next."""
        if now < s.react_until:  # applauding a friend who just finished
            s.goal = None
            s.peek = s.emote = None
        elif s.chat_with:
            self.chat(s, geo, now)
        elif s.peek:
            self.watch(s, geo, now)
        elif s.emote:
            if now >= s.emote[2]:
                s.emote = None
                s.next_plan = now + s.rng.uniform(1.0, 3.0)
            elif s.emote[0] in ("swing", "slide"):
                self.play(s, geo, now)
        elif s.then:  # walking to a play spot
            if s.goal is None:
                kind, dur = s.then
                s.then, s.emote = None, (kind, now, now + dur)
        elif s.goal is None and not s.travel and now >= s.next_plan:
            self.plan(s, seat, geo, now)

    def plan(self, s, seat, geo, now, social=True):
        """An idle one visits a friend, watches someone work, does a little act, walks or sits."""
        rng = s.rng
        others = [o for o in self.everyone()
                  if o is not s and o.left_at is None and o.px is not None and (o.guest or o.sid in geo.homes)]
        pals = sorted((o for o in others if o.chat_with is None and o.shown(now) == "idle"
                       and o.py >= geo.ymin - 6), key=lambda o: o.sid)
        busy = sorted((o for o in others if o.shown(now) in ("work", "think")), key=lambda o: o.sid)
        roll = rng.random()
        if social and not s.guest and geo.neighbors and rng.random() < 0.2:  # go and see the next window
            edge = rng.choice(sorted(geo.neighbors))
            s.travel = (geo.neighbors[edge], edge)
            self.exit_route(s, edge, geo, rng.uniform(geo.ymin, geo.ymax))
        elif social and pals and roll < 0.35:  # visit: chat, wave, hop, high-five, dance or tag
            o = rng.choice(pals)
            kind = rng.choice(geo_theme_kinds(geo))
            if kind == 6:  # ordering coffee needs a customer and the barista
                baristas = [p for p in pals if geo.roles.get(p.sid) == "barista"]
                if baristas and geo.roles.get(s.sid) != "barista":
                    o = baristas[0]
                else:
                    kind = 0
            s.chat_with, o.chat_with = o.sid, s.sid
            s.stage, o.stage = "walk", "wait"
            s.part, o.part = "visitor", "host"
            s.chat_kind = o.chat_kind = kind
            o.goal = None  # the friend stops where it is
            o.peek = o.emote = None
        elif social and busy and roll < 0.47:
            s.peek = [rng.choice(busy).sid, None]
        elif roll < 0.67:
            s.goal = (rng.uniform(8, geo.w - 8), rng.uniform(geo.ymin, geo.ymax))
        elif social and roll < 0.87:
            kind, dur = rng.choice(geo.emotes)
            if kind in geo.spots:  # play equipment: walk there first
                sp = geo.spots[kind]
                s.goal = (sp["x"], sp["y"]) if kind == "swing" else sp["base"]
                s.then = (kind, dur)
            else:
                s.emote = (kind, now, now + dur)
        else:
            s.goal = seat or (rng.uniform(8, geo.w - 8), rng.uniform(geo.ymin, geo.ymax))

    @staticmethod
    def play(s, geo, now):
        """Scripted movement on the playground: swing back and forth, climb the ladder and slide down."""
        u = now - s.emote[1]
        if s.emote[0] == "swing":
            sp = geo.spots["swing"]
            ph = u * 3.2
            s.px, s.py = sp["x"] + 6 * math.sin(ph), sp["y"] - 2 * abs(math.sin(ph))
        else:
            sp = geo.spots["slide"]
            (bx, by), (tx, ty), (ex, ey) = sp["base"], sp["top"], sp["bottom"]
            c = u % 2.2
            if c < 1.0:  # climb
                s.px, s.py = tx, by + (ty - by) * c
            elif c < 1.9:  # slide
                k = (c - 1.0) / 0.9
                s.px, s.py = tx + (ex - tx) * k, ty + (ey - ty) * k * k
            else:  # walk back to the ladder
                k = (c - 1.9) / 0.3
                s.px, s.py = ex + (bx - ex) * k, ey + (by - ey) * k
            s.face = 1 if c < 1.9 else -1
        s.goal = None
        s.moving = False

    def watch(self, s, geo, now):
        """Walk over to a busy friend, look at their screen for a few seconds, wander off."""
        o = self.find(s.peek[0])
        if o is None or o.left_at is not None or o.px is None or o.shown(now) not in ("work", "think"):
            s.peek = None
            s.next_plan = now + 1.0
            return
        s.face = 1 if o.px >= s.px else -1
        if s.peek[1] is None:
            s.goal = (o.px - 17, min(max(o.py + 3, geo.ymin), geo.ymax))
            if abs(s.px - s.goal[0]) + abs(s.py - s.goal[1]) < 1.0:
                s.goal = None
                s.peek[1] = now + s.rng.uniform(3.0, 5.0)
        elif now >= s.peek[1]:
            s.peek = None
            s.next_plan = now + s.rng.uniform(1.0, 3.0)

    def chat(self, s, geo, now):
        """One step of a visit: walk over, do something together, part."""
        o = self.find(s.chat_with)
        if o is None or o.chat_with != s.sid or o.left_at is not None or o.shown(now) != "idle":
            self.drop_social(s)  # the friend got busy or left
            s.next_plan = now + 1.0
            return
        if s.stage != "flee":
            s.face = 1 if o.px >= s.px else -1
        if s.stage == "walk":
            s.goal = (o.px + (18 if s.px > o.px else -18), min(max(o.py, geo.ymin), geo.ymax))
            if abs(s.px - s.goal[0]) + abs(s.py - s.goal[1]) < 1.0:
                s.goal = None
                if s.chat_kind == 5:  # tag: this one chases, the friend runs
                    s.stage, o.stage = "chase", "flee"
                    s.chat_until = o.chat_until = now + 6.0
                    s.catch_ok = o.catch_ok = now + 1.0
                else:
                    s.stage = o.stage = "talk"
                    s.chat_until = o.chat_until = now + s.rng.uniform(3.0, 5.0)
        elif s.stage == "chase":
            s.run = 1.7
            s.goal = (o.px, o.py)
            if now >= s.catch_ok and math.hypot(o.px - s.px, o.py - s.py) < 9:  # tagged: swap roles
                s.stage, o.stage = "flee", "chase"
                s.catch_ok = o.catch_ok = now + 1.0
                self.sparks(s.rng, o.px, o.py - 1, 6)
        elif s.stage == "flee":
            s.run = 1.5
            if s.goal is None or now >= s.flee_next:  # run away from the chaser
                s.flee_next = now + 0.7
                ang = math.atan2(s.py - o.py, s.px - o.px) + s.rng.uniform(-0.8, 0.8)
                dist = s.rng.uniform(25, 50)
                s.goal = (min(max(s.px + math.cos(ang) * dist, 8), geo.w - 8),
                          min(max(s.py + math.sin(ang) * dist * 0.5, geo.ymin), geo.ymax))
        if s.stage in ("talk", "chase", "flee") and now > s.chat_until and s.sid < o.sid:  # one side ends it
            kind = s.chat_kind
            for x, pause in ((s, (2.0, 5.0)), (o, (1.0, 3.0))):
                self.drop_social(x)
                x.goal = None
                x.next_plan = now + x.rng.uniform(*pause)
                slot = geo.homes.get(x.sid)
                if kind == 6 and x.part == "visitor":  # got the coffee
                    x.cup_until = now + 25.0
                    if slot:  # ... and takes it back to the table
                        x.goal = (slot[0] - 5, slot[1] + SEAT_DY)

    def sparks(self, rng, x, y, n):
        for i in range(n):
            self.particles.append([x + rng.uniform(-3, 3), y, rng.uniform(-0.4, 0.4), -rng.uniform(0.3, 1.1),
                                   1.0, (YELLOW, WHITE)[i % 2], False])

    def emit(self, s, now):
        """Sparks when a Clawd is done (its idle neighbours applaud), hearts, notes, high-five sparks."""
        if s.px is None:
            return
        if s.shown(now) == "cheer" and s.burst_t != s.state_t:
            s.burst_t = s.state_t
            self.sparks(s.rng, s.px, s.py - 1, 8)
            for o in self.everyone():
                if (o is not s and o.px is not None and o.left_at is None and o.chat_with is None
                        and o.shown(now) == "idle" and abs(o.px - s.px) < 90):
                    o.react_until = now + 1.6
                    o.face = 1 if s.px >= o.px else -1
        o = self.find(s.chat_with) if s.chat_with else None
        if not o or s.stage != "talk":
            return
        if s.chat_kind == 1 and now >= s.next_heart:
            s.next_heart = now + 0.7
            self.particles.append([s.px - 1, s.py - 4, s.rng.uniform(-0.1, 0.1), -0.25, 1.0, PINK, True])
        elif s.chat_kind == 4 and now >= s.next_heart:  # dancing throws out little notes of colour
            s.next_heart = now + 0.35
            self.particles.append([s.px + s.rng.uniform(-5, 5), s.py - 1, s.rng.uniform(-0.2, 0.2), -0.5, 1.0,
                                   s.rng.choice((PINK, YELLOW, BLUE)), False])
        elif s.chat_kind == 3 and s.sid < o.sid:  # high-five: sparks where the hands meet
            cycle = int(now / 1.6)
            if cycle != s.last_hf and now % 1.6 > 0.9:
                s.last_hf = cycle
                self.sparks(s.rng, (s.px + o.px) / 2, min(s.py, o.py) - 3, 8)

    def fly(self, dt):
        """Move the particles (the official Clawd's: a little gravity, fading out)."""
        k = dt * 30
        for q in self.particles:
            q[0] += q[2] * k
            q[1] += q[3] * k
            q[3] += (0 if q[6] else 0.04) * k
            q[4] -= (0.02 if q[6] else 0.025) * k
        self.particles = [q for q in self.particles if q[4] > 0]

    @staticmethod
    def move(s, geo, dt, now):
        was_moving, s.moving = s.moving, False
        if s.goal is None:
            s.walk_t = 0.0
            return
        dx, dy = s.goal[0] - s.px, s.goal[1] - s.py
        d = math.hypot(dx, dy)
        s.walk_t = s.walk_t + dt if was_moving else 0.0
        if s.left_at is not None:
            step = LEAVE_SPEED * dt
        else:  # ease in when setting off and ease out when arriving
            step = WALK_SPEED * s.run * min(1.0, 0.3 + s.walk_t / 0.5, 0.3 + d / 10.0) * dt
        if d <= max(step, 0.5):  # arrived
            s.px, s.py = s.goal
            if s.route:  # on to the next point (up the rope, down the stairs)
                s.goal, s.vclimb = s.route.pop(0), True
                return
            s.goal, s.vclimb = None, False
            s.next_plan = now + s.rng.uniform(1.5, 4.0)
            return
        s.px += dx / d * step
        s.py += dy / d * step
        s.moving = True
        if abs(dx) > 0.5:
            s.face = 1 if dx > 0 else -1
        s.walk_phase += step
        if s.left_at is not None and not -10 < s.px < geo.w + 10:
            s.gone = True


# ---- following the events file ----------------------------------------------------------
class Tail:
    """Follow the events file. Never writes to it; tolerates missing, truncated, bad lines."""

    def __init__(self, path):
        self.path = path
        self.pos = 0
        self.buf = b""

    def history(self, since):
        """Events newer than `since`; afterwards poll() continues from the end of the file."""
        try:
            with open(self.path, "rb") as f:
                size = os.fstat(f.fileno()).st_size
                start = max(0, size - MAX_HISTORY_BYTES)
                f.seek(start)
                data = f.read()
        except OSError:
            self.pos = 0
            return []
        if start:
            data = data.partition(b"\n")[2]  # first line is probably cut in half
        end = data.rfind(b"\n") + 1  # a half-written last line is left for poll()
        self.pos = size - (len(data) - end)
        out = (self._parse(line) for line in data[:end].splitlines())
        return [r for r in out if r and r["t"] >= since]

    def poll(self):
        try:
            size = os.path.getsize(self.path)
            if size < self.pos:  # file was truncated or replaced
                self.pos, self.buf = 0, b""
            if size == self.pos:
                return []
            with open(self.path, "rb") as f:
                f.seek(self.pos)
                data = f.read()
        except OSError:
            return []
        self.pos += len(data)
        *lines, self.buf = (self.buf + data).split(b"\n")
        return [r for r in map(self._parse, lines) if r]

    @staticmethod
    def _parse(line):
        """Keep only the five fields the hook writes; anything else is dropped."""
        try:
            r = json.loads(line)
            return {
                "t": float(r["t"]),
                "sid": str(r["sid"]),
                "ev": str(r["ev"]),
                "cwd": str(r.get("cwd") or ""),
                "tool": str(r.get("tool") or ""),
            }
        except (ValueError, KeyError, TypeError, AttributeError):
            return None


# ---- drawing ----------------------------------------------------------------------------
def geo_theme_kinds(geo):
    return geo.visit_kinds


def draw_background(cv, banner, count, title, peers, theme):
    theme.background(cv, banner)
    if banner:
        cv.text(2, 1, title.upper() or "CLAWD ROOM", YELLOW)
        label = "%d SESSION%s" % (count, "" if count == 1 else "S")
        cv.text(cv.w - 2 - text_width(label), 1, label, WHITE)
        if peers:  # linked to other windows; an arrow shows which side one is close enough
            link = "LINK " + " ".join(n + {"right": " >", "left": " <", "up": " ^", "down": " V"}.get(side, "") for n, side in peers)
            cv.text_c(cv.w // 2, 1, fit(link, cv.w - 2 * (max(text_width(label), text_width(title)) + 6)), GREEN)


WOOD = (78, 62, 50)
ROPE, RUNG = (186, 166, 124), (150, 130, 96)
HOLE = (18, 16, 20)


def sc(v, g):
    """A length of the gate drawings (in room pixels) at scale g, at least one pixel."""
    return max(1, round(v * g))


def draw_door(cv, side, door_y, theme_name, g=1.0):
    """A wooden doorframe in the side wall for a Clawd whose body top is at door_y, with a doormat
    in the colour of the next room's floor so you can tell where it leads. g > 1 draws it bigger
    (when the room next door has a bigger font, so that both doors are the same size on screen)."""
    theme = themes.THEMES.get(theme_name, themes.Theme)
    w = sc(8, g)
    x = 0 if side == "left" else cv.w - w
    cy = int(door_y + 5.5)  # middle of the doorway (the Clawd's middle)
    up, down = sc(7, g), sc(7, g)  # the posts go this far above and below the middle
    lint, post = sc(2, g), sc(1, g)
    outer, inner = (x, x + w - post) if side == "left" else (x + w - post, x)
    cv.rect(x, cy - up - lint, w, lint, WOOD)  # lintel
    cv.rect(inner, cy - up, post, up + down, WOOD)  # post on the room side
    cv.rect(outer, cy - up, post, up + down, (54, 42, 34))  # dark gap at the edge of the room
    cv.rect(x, cy + down, w, post, WOOD)  # sill
    mat_x = x + post if side == "left" else x
    cv.rect(mat_x, cy + down - sc(3, g), w - post, sc(3, g), theme.floor)  # doormat
    cv.rect(mat_x, cy + down - sc(2, g), w - post, post, theme.floor2)


def draw_rope(cv, x, top, end, g=1.0):
    """A rope ladder hanging from a hatch in the ceiling: the way up to the window above."""
    half, rail, thick = sc(9, g), sc(7, g), sc(1, g)
    cv.rect(x - half, top, 2 * half + 1, sc(3, g), WOOD)  # hatch frame
    cv.rect(x - half + thick, top + thick, 2 * half + 1 - 2 * thick, thick, HOLE)
    for rx in (x - rail, x + rail):  # the two ropes
        cv.rect(rx, top + sc(3, g), thick, end - top - sc(3, g), ROPE)
    for y in range(top + sc(6, g), end, sc(5, g)):  # rungs
        cv.rect(x - rail + thick, y, 2 * rail - thick, thick, RUNG)


def draw_stairs(cv, x, h, g=1.0):
    """A hatch in the floor with a ladder going down: the way down to the window below."""
    half, rail, thick, depth = sc(9, g), sc(6, g), sc(1, g), sc(6, g)
    cv.rect(x - half, h - depth - thick, 2 * half + 1, thick, WOOD)  # frame
    cv.rect(x - half, h - depth, sc(2, g), depth, WOOD)
    cv.rect(x + half - sc(2, g) + 1, h - depth, sc(2, g), depth, WOOD)
    cv.rect(x - half + sc(2, g), h - depth, 2 * (half - sc(2, g)) + 1, depth, HOLE)  # the dark shaft
    for rx in (x - rail, x + rail):  # the ladder goes down into it
        cv.rect(rx, h - depth, thick, depth, ROPE)
    for y in range(h - sc(1, g), h - depth, -sc(2, g)):
        cv.rect(x - rail + thick, y, 2 * rail - thick, thick, RUNG)


def draw_clawd(cv, cx, y, p, theme=None, role="", body=CLAWD):
    """The Claude Code mascot, built like the official pixel Clawd (14 wide): an 8-wide torso,
    two 2x2 stub arms at its sides, four 1-pixel legs, two 1x1 eyes, no mouth, one flat colour.
    (cx, y) = body centre and top; the sprite is 8 rows high.
    """
    x = int(cx) - 7
    y = int(y) + p["bob"]
    lean = p["lean"]  # the upper body leans a pixel
    sp = CLAWD_SPRITES.get(pose_key(p))
    if sp:  # an edited / exported frame (15 wide, from one row above the head)
        blit_sprite(cv, sp, x, y - 1, body)
        if theme:
            theme.accessory(cv, x + lean, y, role)
        return
    for i, lx in enumerate((3, 5, 8, 10)):  # legs; while walking one pair is lifted
        lifted = (p["legs"] == 1 and i % 2 == 0) or (p["legs"] == 2 and i % 2 == 1)
        cv.rect(x + lx, y + 6, 1, 1 if lifted else 2, body)
    cv.rect(x + 3 + lean, y, 8, 4, body)
    cv.rect(x + 3, y + 4, 8, 2, body)
    for k, ax in enumerate((x + 1, x + 11)):  # arms: 0 down, 1 raised beside the head, 2 half raised
        mode = p["arms"][k]
        cv.rect(ax + lean, y + {0: 2, 1: -1, 2: 1}[mode], 2, 2, body)
    if theme:
        theme.accessory(cv, x + lean, y, role)
    ex, ey = x + lean + p["look"][0], y + 1 + p["look"][1]
    if p["eyes"] != "shut":  # a blink (or sleep) simply hides the eyes, as in the official one
        cv.px(ex + 4, ey, INK)
        cv.px(ex + 9, ey, INK)
    if p["eyes"] == "happy":  # the eye grows to 1x2: a pale peach (not white) top over the dark pupil; plus blush
        for c in (4, 9):
            cv.px(x + lean + c, y, GLINT)
        cv.px(x + lean + 3, y + 2, BLUSH)
        cv.px(x + lean + 10, y + 2, BLUSH)


def pose_for(s, state, t, now):
    """How the body moves in this state: bob, lean, arms, legs, eyes."""
    p = {"bob": 0, "lean": 0, "arms": (0, 0), "legs": 0, "eyes": "open", "look": (0, 0)}
    ph = t + s.seed * 10  # each Clawd has its own rhythm
    if s.moving and s.vclimb:  # up the rope or the stairs
        k = int(s.walk_phase / 2) % 2
        p.update(arms=(1, 2) if k else (2, 1), legs=1 + k, look=(0, -1))
    elif s.moving:
        k = int(s.walk_phase / 3) % 2
        arms = (2, 0) if k else (0, 2)
        if s.stage in ("chase", "flee"):  # running: arms up and happy / arms pumping
            arms = (1, 1) if s.stage == "chase" else (2, 2)
        p.update(bob=-k, lean=s.face, legs=1 + k, arms=arms, look=(s.face, 0),
                 eyes="happy" if s.stage == "chase" else "open")
    elif s.chat_with and s.stage == "talk":
        kind = s.chat_kind
        side = 1 if s.sid < s.chat_with else -1  # the two of a pair mirror each other
        if kind == 0:  # chatting: little nods
            p.update(bob=-1 if int(ph * 3) % 2 else 0, look=(s.face, 0))
        elif kind == 1:  # waving at each other (hearts come from Room.step)
            p.update(arms=(0, 1) if int(ph * 4) % 2 else (1, 0), look=(s.face, 0), eyes="happy")
        elif kind == 2:  # hopping together
            p.update(bob=-int(abs(math.sin(now * 6)) * 3), arms=(1, 1), eyes="happy")
        elif kind == 3:  # high-five: a jump with the near arm out, every 1.6 s
            u = now % 1.6
            if 0.75 < u < 1.05:
                up = -int(math.sin((u - 0.75) / 0.3 * math.pi) * 3)
                p.update(bob=up, arms=(0, 1) if s.face > 0 else (1, 0), eyes="happy", lean=s.face)
            else:
                p.update(look=(s.face, 0))
        elif kind == 6:  # ordering coffee: the host (barista) works the machine, the visitor waits
            if s.part == "host":
                p.update(arms=(2, 0) if int(now * 6) % 2 else (0, 2), look=(s.face, 0), eyes="happy")
            else:
                p.update(look=(s.face, 0), bob=-1 if int(ph * 2) % 2 else 0)
        else:  # dancing together
            step = int(now * 2.5) % 4
            p.update(lean=(-1, 0, 1, 0)[step] * side, arms=(1, 2) if step % 2 else (2, 1),
                     bob=-(step % 2), eyes="happy")
    elif state == "work":  # typing: lean to the screen, look down at the keys, arms tap
        p.update(lean=1, arms=(2, 0) if int(t * 12) % 2 else (0, 2), look=(0, 1))
    elif state == "think":  # sways, scratches its head, looks from side to side
        p.update(lean=(-1, 0, 1, 0)[int(ph * 1.5) % 4], arms=(0, 2 if int(ph * 0.7) % 2 else 0),
                 bob=-1 if int(ph * 2) % 2 else 0, look=((-1, 0), (0, 0), (1, 0), (0, 0))[int(ph * 1.2) % 4])
    elif state == "wait":  # waves an arm for attention
        p.update(arms=(0, 1) if int(t * 3) % 2 else (0, 0), look=(1, 0))
    elif state == "cheer":
        p.update(bob=-int(abs(math.sin((now - s.state_t) * 8)) * 4), arms=(1, 1), eyes="happy",
                 lean=1 if int(ph * 3) % 2 else -1)
    elif state == "sleep":
        p.update(bob=1 if int(t * 0.8) % 2 else 0, eyes="shut")
    elif now < s.react_until:  # applauding a friend who just finished
        p.update(bob=-int(abs(math.sin(now * 9)) * 3), arms=(1, 1), eyes="happy", look=(s.face, 0))
    elif s.peek and s.peek[1] is not None:  # watching a friend work
        p.update(lean=s.face, look=(s.face, 0), arms=(0, 2), bob=-1 if int(ph * 2) % 2 else 0)
    elif s.emote:  # a little solo act
        kind = s.emote[0]
        step = int(ph * 2.5) % 4
        if kind == "stretch":
            up = int(ph * 1.5) % 2
            p.update(arms=(1, 1) if up else (2, 2), bob=-up, eyes="shut")
        elif kind == "dance":
            p.update(lean=(-1, 0, 1, 0)[step], arms=(1, 2) if step % 2 else (2, 1), bob=-(step % 2), eyes="happy")
        elif kind == "swing":  # holding the ropes, legs up at the top of each swing
            p.update(arms=(1, 1), eyes="happy", legs=1 + int((now - s.emote[1]) * 3.2 / 3) % 2)
        elif kind == "slide":
            u = (now - s.emote[1]) % 2.2
            if 1.0 <= u < 1.9:  # sliding down
                p.update(arms=(1, 1), eyes="happy")
            else:  # climbing or walking back
                p.update(arms=(2, 0) if int(now * 6) % 2 else (0, 2), legs=1 + int(now * 6) % 2)
        elif kind == "jump":
            p.update(bob=-int(abs(math.sin((now - s.emote[1]) * 6)) * 4), arms=(1, 1), eyes="happy")
        else:  # wave at nobody in particular
            p.update(arms=(0, 1) if int(ph * 4) % 2 else (0, 0), eyes="happy")
    else:  # idle: looks around, shifts its weight
        p.update(look=((0, 0), (-1, 0), (0, 0), (1, 0))[int(ph * 0.8) % 4], bob=-1 if int(ph) % 3 == 0 else 0)
    if p["eyes"] == "open" and (t + s.seed * 7) % 3.2 < 0.12:
        p["eyes"] = "shut"  # blink
    return p


def tool_kind(tool):
    if tool.startswith("mcp__"):
        return "MCP", (150, 220, 160)
    return TOOL_KINDS.get(tool, (tool[:5].upper() or "TOOL", (170, 170, 190)))


def draw_desk(cv, cx, by, state, tool, t, theme, role):
    """The workstation of one session in the room's style; returns the tool label."""
    kind, color = tool_kind(tool)
    theme.furniture(cv, cx, by, state == "work", color, t, role)
    return kind


HEART = ((1, 0, 1), (1, 1, 1), (0, 1, 0))  # the official 3x3 heart


def draw_bubble(cv, cx, y, kind, ph):
    """Speech bubble above a head, grey outline with cut corners like the official one."""
    name = {"!": "bubble-excl", "?": "bubble-ques", "cup": "bubble-cup"}.get(kind) or "bubble-dots-%d" % (int(ph * 3) % 4)
    if item(name, cv, cx - 4, y):  # (an edited bubble from sprites/items.txt)
        return
    cv.rect(cx - 3, y, 7, 1, BUBBLE_EDGE)
    cv.rect(cx - 3, y + 6, 7, 1, BUBBLE_EDGE)
    cv.rect(cx - 4, y + 1, 1, 5, BUBBLE_EDGE)
    cv.rect(cx + 4, y + 1, 1, 5, BUBBLE_EDGE)
    cv.rect(cx - 3, y + 1, 7, 5, WHITE)
    cv.px(cx - 2, y + 7, BUBBLE_EDGE)
    if kind == "dots":
        for i in range(3):
            if i < int(ph * 3) % 4:
                cv.px(cx - 2 + i * 2, y + 3, INK)
    elif kind == "cup":
        cv.rect(cx - 1, y + 2, 3, 2, (190, 120, 80))
        cv.px(cx + 2, y + 2, (190, 120, 80))
        cv.px(cx, y + 1, (160, 160, 170))
    else:  # "!" or "?"
        cv.text(cx - 1, y + 1, kind, INK)


def draw_extras(cv, s, state, t, now, seated, label_rects, geo):
    """Props around a character: thinking dots, "!" sign, Zs, sparkles, bubbles, name tag."""
    cx, y = int(s.px), int(s.py)
    if now < s.cup_until:  # carrying a coffee, with a little steam
        if not item("cup-held", cv, cx + 7, y + 4):
            cv.rect(cx + 7, y + 4, 3, 2, WHITE)
            cv.px(cx + 10, y + 4, WHITE)
        cv.px(cx + 8, y + 3 - int(t * 2) % 3, (200, 200, 210))
    if s.emote and s.emote[0] == "swing" and "swing" in geo.spots:  # ropes from the bar to the hands
        sp = geo.spots["swing"]
        cv.line(sp["x"] - 6, sp["bar"] + 2, cx - 6, y + 3, (150, 140, 120))
        cv.line(sp["x"] + 6, sp["bar"] + 2, cx + 6, y + 3, (150, 140, 120))
        cv.rect(cx - 6, y + 8, 13, 1, (112, 88, 64))  # the seat
    if state == "think":
        for i in range(3):
            c = WHITE if i < int(t * 3) % 4 else (90, 90, 120)
            cv.rect(cx + 8 + i * 3, y - 5 - i, 2, 2, c)
    elif state == "wait":
        if not item("sign-wait", cv, cx - 4, y - 11):
            cv.rect(cx - 4, y - 11, 9, 9, YELLOW)
            cv.rect(cx - 3, y - 10, 7, 7, (250, 235, 150))
            cv.text(cx - 1, y - 9, "!", INK)
            cv.rect(cx, y - 2, 1, 1, (120, 90, 40))
    elif state == "sleep":
        for i in range(3):
            k = (t * 0.7 + i / 3.0) % 1.0
            cv.text(int(cx + 8 + k * 4), int(y - 2 - k * 7), "Z", (180, 190, 240) if k < 0.7 else (110, 115, 150))
    if s.chat_with and s.stage == "talk":
        ph = t + s.seed * 10
        if s.chat_kind == 0:  # only one of the two "speaks" at a time
            if int(t * 0.7) % 2 == (0 if s.sid < s.chat_with else 1):
                draw_bubble(cv, cx, y - 10, "dots", ph)
        elif s.chat_kind in (2, 3):
            draw_bubble(cv, cx, y - 10, "!", ph)
        elif s.chat_kind == 6 and s.part == "visitor":  # ordering a coffee
            draw_bubble(cv, cx, y - 10, "cup", ph)
    elif now < s.react_until and state == "idle":
        draw_bubble(cv, cx, y - 10, "!", t)
    elif s.peek and s.peek[1] is not None:
        draw_bubble(cv, cx, y - 10, "?", t)
    elif not seated and s.left_at is None and state != "wait":  # away from the desk: say who it is
        name = fit(s.name or s.sid[:8], 19)
        x0, y0 = cx - text_width(name) // 2, y - 8
        # skip the tag where it would sit on top of a desk label
        if not any(x0 < r[2] and x0 + text_width(name) > r[0] and y0 < r[3] and y0 + 5 > r[1] for r in label_rects):
            cv.text(x0, y0, name, BLUE if s.guest else GREY)


LABEL = {"idle": "IDLE", "think": "THINKING", "wait": "NEED OK", "cheer": "DONE!", "sleep": "ZZZ"}


def fit(s, width):
    """Longest start of s (upper case) whose pixel text fits in `width` pixels."""
    s = s.upper()
    n = max(0, (width + 1) // 4)
    return s[:n]


def layout(w, h, n):
    """Where the characters go: (banner_h, per_row, slot_w, rows_shown, slot_top_ys)."""
    per_row = max(1, min(PER_ROW, w // SLOT_MIN_W))
    slot_w = min(SLOT_MAX_W, w // per_row)
    rows_needed = max(1, -(-n // per_row))
    banner = BANNER_H if (h - BANNER_H) // SLOT_H >= rows_needed else 0
    rows_shown = min(rows_needed, (h - banner) // SLOT_H)
    gap = (h - banner - rows_shown * SLOT_H) // (rows_shown + 1)  # spread spare height evenly
    ys = [banner + gap * (r + 1) + SLOT_H * r for r in range(rows_shown)]
    return banner, per_row, slot_w, rows_shown, ys


class Geo:
    """Where the desks are, in room pixels, and the strip of floor where Clawds may walk."""

    def __init__(self, w, h, sessions, theme, neighbors=None, neighbor_themes=None, link_pos=None, gate_scale=None):
        self.w, self.h = w, h
        self.neighbors = neighbors or {}  # {"left"/"right"/"up"/"down": peer room id}
        self.neighbor_themes = neighbor_themes or {}
        link_pos = link_pos or {}
        self.gate_scale = gate_scale or {}
        self.rope_x = min(max(round(link_pos.get("up", w * 0.72)), 10), w - 10)  # (the hatch is drawn clipped at the edges)  # rope up to the window above
        self.stairs_x = min(max(round(link_pos.get("down", w * 0.28)), 10), w - 10)  # stairs down to the window below
        self.key = (w, h)
        self.roles = theme.roles(sessions)  # sid -> role (the barista in the coffee shop)
        self.spots = theme.spots(w, h)  # playground spots in the park
        self.floor_y = int(h * theme.horizon)
        self.visit_kinds = theme.visit_kinds
        self.emotes = theme.emotes
        self.banner, per_row, self.slot_w, rows_shown, ys = layout(w, h, len(sessions))
        slots = per_row * rows_shown
        keep = len(sessions)
        if keep > slots:
            keep = slots - 1 if slots > 1 else slots  # the last slot says "+N MORE"
        self.shown = sessions[:keep]
        self.more = len(sessions) - keep
        self.more_pos = None  # centre of the "+N MORE" slot (only when there is a spare slot)
        self.homes = {}  # sid -> (slot centre x, body top y)
        for i in range(min(len(sessions), keep + (1 if self.more and slots > 1 else 0))):
            r, c = divmod(i, per_row)
            n_in_row = min(per_row, keep + (1 if self.more and slots > 1 else 0) - r * per_row)
            cx = (w - self.slot_w * n_in_row) // 2 + c * self.slot_w + self.slot_w // 2
            if i < keep:
                self.homes[sessions[i].sid] = (cx, ys[r] + 8)
            else:
                self.more_pos = (cx, ys[r] + 14)
        self.ymin = (ys[-1] + 8 + SEAT_DY + 4) if ys else 0
        self.ymax = max(self.ymin, h - 8)
        self.rope_end = self.ymin + 8  # the rope hangs down to where the Clawds' feet are
        # doors to the left and right: the body-top height of a Clawd walking through (lined up on screen)
        self.door_y = {side: float(min(max(link_pos.get(side, (self.ymin + self.ymax) / 2), self.ymin), self.ymax))
                       for side in ("left", "right")}


def draw_room(cv, room, now, t):
    """Draw the whole room. Returns a note ("+N MORE") when it did not fit in a slot."""
    sessions = room.ordered()
    geo = Geo(cv.w, cv.h, sessions, room.theme, room.neighbors, room.neighbor_themes, room.link_pos, room.gate_scale)
    room.step(geo, now)
    active = len([s for s in sessions if s.left_at is None])
    draw_background(cv, geo.banner, active, room.title, room.peers, room.theme)
    for side in geo.neighbors:
        theme_name = geo.neighbor_themes.get(side, "")
        if side in ("left", "right"):
            draw_door(cv, side, geo.door_y[side], theme_name, geo.gate_scale.get(side, 1.0))
        elif side == "up":
            draw_rope(cv, geo.rope_x, geo.banner, geo.rope_end, geo.gate_scale.get(side, 1.0))
        else:
            draw_stairs(cv, geo.stairs_x, cv.h, geo.gate_scale.get(side, 1.0))
    if not sessions:
        mid = (geo.banner + cv.h) // 2
        cv.text_c(cv.w // 2, mid - 7, "NO SESSIONS", (150, 150, 180))
        cv.text_c(cv.w // 2, mid, "START CLAUDE", (110, 110, 140))
        return ""
    seated = []
    for s in geo.shown:
        if s.away:
            continue
        cx, by = geo.homes[s.sid]
        at_desk = not s.moving and s.left_at is None and abs(s.px - (cx - 5)) < 1 and abs(s.py - (by + SEAT_DY)) < 1
        if at_desk:
            seated.append(s)
    guests = [g for g in room.guests.values() if g.px is not None]
    people = [s for s in geo.shown if not s.away] + guests  # everybody who is in the room right now
    states = {s.sid: s.shown(now) for s in geo.shown + guests}

    def clawd(s):
        draw_clawd(cv, s.px, s.py, pose_for(s, states[s.sid], t, now), room.theme, geo.roles.get(s.sid, ""))

    kinds = {}
    for s in geo.shown:
        cx, by = geo.homes[s.sid]
        kinds[s.sid] = draw_desk(cv, cx, by, states[s.sid], s.tool, t, room.theme, geo.roles.get(s.sid, ""))
    name_w = geo.slot_w - 3
    label_rects = []
    for s in geo.shown:  # desk labels; Clawds walking past are drawn over them
        if s.left_at is None:
            cx, by = geo.homes[s.sid]
            cv.text_c(cx, by + 18, fit(s.name or s.sid[:8], name_w), WHITE)
            state = states[s.sid]
            label = kinds[s.sid] if state == "work" else LABEL.get(state, "")
            if s.away and state == "idle":
                label = "AWAY"
            if text_width(label) > name_w:
                label = label[:5]  # "THINKING" -> "THINK" in narrow slots
            cv.text_c(cx, by + 24, label, GREY)
            label_rects.append((cx - name_w // 2, by + 18, cx + name_w // 2 + 1, by + 29))
    for s in sorted(people, key=lambda s: s.py):  # nearer to the viewer = drawn later
        clawd(s)
    for q in room.particles:
        if q[4] > 0.15:
            if q[6]:
                for ry, row in enumerate(HEART):
                    for rx, bit in enumerate(row):
                        if bit:
                            cv.px(q[0] - 1 + rx, q[1] + ry, q[5])
            else:
                cv.px(q[0], q[1], q[5])
    for s in people:
        draw_extras(cv, s, states[s.sid], t, now, s in seated, label_rects, geo)
    if geo.more_pos:
        cv.text_c(geo.more_pos[0], geo.more_pos[1], "+%d MORE" % geo.more, WHITE)
    return "+%d MORE" % geo.more if geo.more and not geo.more_pos else ""


# ---- terminal output --------------------------------------------------------------------
def cells_from_canvas(cv, overlay):
    """Rows of (char, fg, bg) cells. The last row may be replaced by plain overlay text."""
    rows = []
    for r in range(cv.h // 2):
        rows.append([("▀", a, b) for a, b in zip(cv.rows[2 * r], cv.rows[2 * r + 1])])
    if overlay and rows:
        text = (" " + overlay)[: cv.w].ljust(cv.w)
        rows[-1] = [(ch, (230, 230, 240), (20, 20, 30)) for ch in text]
    return rows


def diff_frame(cells, prev):
    """ANSI string that updates the screen from `prev` to `cells` (all of it if prev is None)."""
    out = []
    fg = bg = None
    for r, row in enumerate(cells):
        old = prev[r] if prev else None
        if old == row:
            continue
        x, n = 0, len(row)
        while x < n:
            if old and old[x] == row[x]:
                x += 1
                continue
            out.append("\x1b[%d;%dH" % (r + 1, x + 1))
            while x < n and not (old and old[x] == row[x]):  # one run of changed cells
                ch, f, b = row[x]
                if f != fg:
                    out.append("\x1b[38;2;%d;%d;%dm" % f)
                    fg = f
                if b != bg:
                    out.append("\x1b[48;2;%d;%d;%dm" % b)
                    bg = b
                out.append(ch)
                x += 1
    return "".join(out)


def too_small(cols, rows):
    """A centred one-line message on a cleared screen."""
    msg = "need %dx%d, have %dx%d" % (MIN_COLS, MIN_ROWS, cols, rows)
    msg = msg[:cols]
    return "\x1b[0m\x1b[2J\x1b[%d;%dH%s" % (max(1, rows // 2 + 1), max(1, (cols - len(msg)) // 2 + 1), msg)


def overlay_text(paused, help_on, note, flash=""):
    parts = []
    if paused:
        parts.append("PAUSED")
    if help_on:
        parts.append("q/esc quit   p pause   r room shape   [ ] scale   ctrl +/- font   f auto font+scale   g send a Clawd   ? help")
    if note:
        parts.append(note)
    if flash:
        parts.append(flash)
    return "   ".join(parts)


def render(cols, rows, room, now, t):
    """Canvas for the whole terminal with the room centred in it.

    When the terminal has at least std_grid(room.ratio) cells the room is the standard one of that
    shape, drawn k times bigger if the terminal allows (a bigger window = chunkier pixels). A
    smaller terminal gets the biggest room of that shape that fits, or the whole screen if that
    would be too small to hold even one Clawd.
    """
    pw, ph = cols, rows * 2  # terminal size in pixels
    if room.ratio == "auto":  # the room is the whole window, whatever its shape (room pixels k x k cells at scale k)
        k = max(1, room.scale)
        cv = Canvas(pw // k, ph // k // 2 * 2, MARGIN)
        note = draw_room(cv, room, now, t)
        room.size = (cv.w, cv.h)
        if k == 1:
            room.view = (0, 0, 1)
            return cv, note
        big = cv.scaled(k)
        full = Canvas(pw, ph, MARGIN)
        ox, oy = (pw - big.w) // 2, (ph - big.h) // 2 // 2 * 2
        full.blit(big, ox, oy)
        room.view = (ox, oy, k)
        return full, note
    sw, sh = RATIOS[room.ratio]
    k = min(pw // sw, ph // sh)
    full = Canvas(pw, ph, MARGIN)
    if k >= 1:
        room_cv = Canvas(sw, sh, WALL)
        note = draw_room(room_cv, room, now, t)
        room.size = (sw, sh)
        big = room_cv.scaled(k)
        bx, by = (pw - big.w) // 2, (ph - big.h) // 2 // 2 * 2
        room.view = (bx, by, k)
        full.blit(big, bx, by)
        return full, note
    rw = min(pw, ph * sw // sh)
    rh = rw * sh // sw // 2 * 2  # whole terminal rows
    if rh < SLOT_H:
        rw, rh = pw, ph
    room_cv = Canvas(rw, rh, WALL)
    note = draw_room(room_cv, room, now, t)
    room.size = (rw, rh)
    bx, by = (pw - rw) // 2, (rows - rh // 2) // 2 * 2
    room.view = (bx, by, 1)
    full.blit(room_cv, bx, by)
    return full, note


def dump(cols, rows, room, now, t):
    """Plain-text frame for tests: lit pixels as block characters, no colour codes."""
    cv, _ = render(cols, rows, room, now, t)

    def lit(c):
        return sum(c) > 330  # sprites, text and screens; not the wall, floor or desk

    lines = []
    for r in range(rows):
        row = zip(cv.rows[2 * r], cv.rows[2 * r + 1])
        lines.append("".join({(0, 0): " ", (1, 0): "▀", (0, 1): "▄", (1, 1): "█"}[(lit(a), lit(b))] for a, b in row).rstrip())
    return "\n".join(lines)


# ---- keys and main loop -----------------------------------------------------------------
REPORT_RE = re.compile(rb"\x1b\[([46]);([0-9]+);([0-9]+)t")  # the terminal's answer to CSI 14 t / CSI 16 t


def read_keys(fd, reports):
    """Non-blocking read of whatever is waiting. Returns the useful keys as a string; answers to our
    size questions (text area and cell size in pixels) are stored in `reports`."""
    try:
        data = os.read(fd, 256)
    except OSError:
        return ""
    for kind, h, w in REPORT_RE.findall(data):
        reports["text" if kind == b"4" else "cell"] = (int(w), int(h))
    data = REPORT_RE.sub(b"", data)
    if data == b"\x1b":
        return "q"  # a lone Esc quits
    if data.startswith(b"\x1b"):
        return ""  # arrow keys and other escape sequences are ignored
    return "".join(c for c in data.decode("ascii", "ignore") if c in "qpfgr?[]")


def cell_px(area, dpi, reports, cols, rows):
    """Size of one terminal cell on screen in pixels. The terminal reports it in 96-dpi units;
    if that does not fit the window (not answered yet, or stale) we divide the window instead."""
    cw, ch = area_w(area) / max(1, cols), area_h(area) / max(1, rows)
    cell = reports.get("cell")
    if cell:
        rw, rh = cell[0] * dpi / 96.0, cell[1] * dpi / 96.0
        if abs(rw * cols - area_w(area)) < 0.15 * area_w(area) and abs(rh * rows - area_h(area)) < 0.15 * area_h(area):
            cw, ch = rw, rh
    return cw, ch


def area_w(a):
    return max(1, a[2] - a[0])


def area_h(a):
    return max(1, a[3] - a[1])


DOOR_H = 14  # room pixels a door needs: a Clawd and a little headroom
HATCH_W = 21  # room pixels a ladder hatch needs (the drawing is 19 wide)


def grid_params(room, area, dpi, reports, cols, rows):
    """Where this room's cells are on screen: (ox, oy, cw, ch, k, x0, y0). Room pixel c, r then covers
    screen x = ox + (x0 + c*k)*cw and y = oy + (y0 + r*k)/2*ch."""
    cw, ch = cell_px(area, dpi, reports, cols, rows)
    x0, y0, k = room.view
    return (float(area[0]), float(area[1]), cw, ch, float(k), float(x0), float(y0))


def my_band(room, g):
    """Screen heights (top, bottom) of the strip of floor where this room's Clawds walk, or None."""
    geo = room.geo
    if not geo or not g:
        return None
    ox, oy, cw, ch, k, x0, y0 = g
    return int(oy + (y0 + geo.ymin * k) / 2 * ch), int(oy + (y0 + (geo.ymax + 8) * k) / 2 * ch)


def snap_both(lo, hi, grids):
    """The screen position in [lo, hi] that every grid (A + B*n, n a whole number) can hit most closely,
    and the index n each one uses for it. Rooms with different fonts have different grids, so the
    best common position is searched for rather than taken as the middle."""
    best = None
    steps = max(1, int(hi - lo))
    for i in range(steps + 1):
        y = lo + (hi - lo) * i / steps
        err = max(abs(A + B * round((y - A) / B) - y) for A, B in grids)
        key = (err, abs(y - (lo + hi) / 2))
        if best is None or key < best[0]:
            best = (key, y)
    y = best[1]
    return [round((y - A) / B) for A, B in grids]


def plan_passages(room, watcher, win_id, my_grid, peers):
    """Where the gates to the attached windows go, so that both ends line up on screen, and which
    attached windows get no gate (the floors or the overlap are too small for a door or a hatch).
    Also returns the scale each gate is drawn at: both windows draw it the same size on screen (the
    size it needs in the window with the bigger cells), whatever their font sizes.
    Returns ({side: index}, set of closed sides, {side: scale}). The windows themselves are never moved."""
    mine = watcher.area_of(win_id) if watcher else None
    if not mine or not my_grid:
        return {}, set(), {}
    out, closed, scales = {}, set(), {}
    ox, oy, cw, ch, k, x0, y0 = my_grid
    for side, peer in room.neighbors.items():
        other = watcher.area_of(peer)
        pinfo = peers.get(peer) or {}
        pg = pinfo.get("grid") or my_grid  # (until it tells us, assume it looks like us)
        if not other:
            continue
        if side in ("up", "down"):
            lo, hi = max(mine[0], other[0]), min(mine[2], other[2])
            scales[side] = max(k * cw, pg[4] * pg[2]) / (k * cw)  # a room pixel is this much smaller here than next door
            half = HATCH_W * max(k * cw, pg[4] * pg[2]) / 2  # the hatch must fit in both
            lo, hi = lo + half, hi - half
            if lo > hi:
                closed.add(side)
                continue
            grids = [(ox + (x0 + 0.5 * k) * cw, k * cw), (pg[0] + (pg[5] + 0.5 * pg[4]) * pg[2], pg[4] * pg[2])]
            out[side] = snap_both(lo, hi, grids)[0]
        else:
            lo, hi = max(mine[1], other[1]), min(mine[3], other[3])  # overlap on screen
            for band in (my_band(room, my_grid), pinfo.get("band")):  # both need floor to walk on
                if band:
                    lo, hi = max(lo, band[0]), min(hi, band[1])
            scales[side] = max(k * ch, pg[4] * pg[3]) / (k * ch)
            half = DOOR_H * max(k * ch, pg[4] * pg[3]) / 2 / 2  # the door must fit in both
            lo, hi = lo + half, hi - half
            if lo > hi:
                closed.add(side)
                continue
            grids = [(oy + (y0 + 5.5 * k) / 2 * ch, k * ch / 2), (pg[1] + (pg[6] + 5.5 * pg[4]) / 2 * pg[3], pg[4] * pg[3] / 2)]
            out[side] = snap_both(lo, hi, grids)[0]
    return out, closed, scales


ROAM_W, ROAM_H = 80, 62  # a room at least this big (room pixels) leaves floor to walk on beside the seats


def auto_scale(pw, ph, n):
    """The biggest TUI scale (1..SCALE_CAP) at which a room of pw x ph terminal pixels still seats n Clawds
    and has floor for them to move around (1 if no scale does both)."""
    for k in range(SCALE_CAP, 0, -1):
        w, h = pw // k, ph // k
        if w < SLOT_MIN_W or h < SLOT_H:
            continue
        _, per_row, _, rows_shown, _ = layout(w, h, n)
        if per_row * rows_shown >= max(1, n):
            if w >= ROAM_W and h >= ROAM_H:
                return k
    return 1


class OpenSetup:
    """When a room opens (or on the f key): the terminal font goes to `font` points and the window to `size`.

    The terminal only zooms by steps, so the font is reset (Ctrl+0) and then moved by the right number of
    steps in one go. The window is sized once, in pixels, from the cell size at the profile font, so the
    window and the font change together and nothing has to be corrected afterwards (if the terminal does not
    tell its sizes, the slower Fitter does it). Used by the start menu and the room (the menu tells the room it is done).
    """

    def __init__(self, tool, title, font, size, reports, ask, window=True, profile_font=PROFILE_FONT):
        self.tool, self.title, self.font, self.reports, self.ask = tool, title, font, reports, ask
        self.profile_font = profile_font
        self.size = size if window else "off"
        self.stage, self.wait_until, self.fitter, self.started, self.asked = "reset", 0.0, None, None, 0.0

    def window_px(self, rect, area, cols, rows):
        """Outer window size (w, h) in pixels that holds `size` cells at the new font, or None if not known yet.

        Everything is measured on the window itself (outer rectangle, terminal area, cells now): the terminal's
        own pixel reports are in other units on a scaled screen and must not be mixed with the window's.
        """
        if not (rect and area and cols and rows):
            return None
        want_cols, want_rows = (int(v) for v in self.size.lower().split("x"))
        k = (self.font or self.profile_font) / self.profile_font
        cw, ch = (area[2] - area[0]) / cols, (area[3] - area[1]) / rows
        return (rect[2] - rect[0] - (area[2] - area[0]) + want_cols * cw * k, rect[3] - rect[1] - (area[3] - area[1]) + want_rows * ch * k)

    def update(self, now, cols, rows, rect, area=None):
        """Returns True while it is still working."""
        if self.started is None:
            self.started = now
        if self.stage == "reset":
            if self.font:
                self.tool.reset_zoom(self.title)
            self.reports.pop("cell", None)
            self.reports.pop("text", None)
            self.stage, self.wait_until = "probe", now + 0.6
        elif self.stage == "probe":
            if self.size.lower() == "off":
                self.stage = "font"
            elif now >= self.wait_until:
                px = self.window_px(rect, area, cols, rows)
                want = tuple(int(v) for v in self.size.lower().split("x"))
                if self.font in (0, self.profile_font) and abs(cols - want[0]) <= 2 and abs(rows - want[1]) <= 1:
                    self.stage = "font"  # the window was opened at that size already (wt --size): leave it
                elif px:
                    self.tool.resize(self.title, int(px[0]), int(px[1]))
                    self.stage = "font"
                elif now - self.started > 3.0:  # no answer from the terminal: size the window the slow way
                    self.fitter, self.stage = start_fitter(self.tool, self.title, self.size), "fit"
                elif now - self.asked > 0.5:
                    self.asked = now
                    self.ask()
        elif self.stage == "fit":
            self.fitter.update(now, cols, rows, rect)
            if self.fitter.done:
                self.stage = "font"
        elif self.stage == "font":
            if self.font and self.font != self.profile_font:
                self.tool.zoom(self.title, self.font - self.profile_font, only_if_focused=True)
            self.stage, self.wait_until = "wait", now + 1.0
        elif self.stage == "wait" and now >= self.wait_until:
            self.stage = "done"
        return self.stage != "done"


def run(args):
    fd = sys.stdin.fileno()
    old_attrs = termios.tcgetattr(fd)
    resized = [True]
    signal.signal(signal.SIGWINCH, lambda *_: resized.__setitem__(0, True))
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))  # still runs the cleanup below
    out = sys.stdout

    def write(s):
        out.write(s)
        out.flush()

    room = Room()
    room.theme = themes.pick(args.room, args.theme)
    room.ratio = args.ratio
    tail = Tail(args.events)
    start = args.at if args.at is not None else time.time()
    for r in tail.history(start - REPLAY_SECONDS):
        room.apply(r)

    link = roomlink.Link(args.room) if args.room else None
    win_id = link.id if link else "solo-%d" % os.getpid()  # our window title is "clawd-room <win_id>"
    tool = fitter = setup = None  # tool changes the window's font/size (WSL only)
    watcher = None  # finds out where the windows are (for neighbours, auto scale and fitting)
    if not args.no_detect and windetect.available():
        watcher = windetect.WindowWatcher()
        tool = windetect.WindowTool()
    title = windetect.PREFIX + win_id
    reports = {}  # what the terminal tells us about its pixel sizes: "text" and "cell" as (width, height)
    if tool and not args.no_auto and not getattr(args, "opened", False):  # the start menu has done this already
        setup = OpenSetup(tool, title, args.font, args.size, reports, lambda: write("\x1b[14t\x1b[16t"), profile_font=args.profile_font)
    font_set = bool(tool and not args.no_auto and args.font)  # we changed the terminal font: put it back on exit
    auto = not args.scale  # the TUI scale follows the window until [ ] is pressed
    room.scale = args.scale or 1
    pend = None  # (time, cell size before) while we wait to learn whether a size change was the window or the font
    flash, flash_until = "", 0.0  # a short message about rooms coming and going
    last_closed = set()  # attached sides that got no gate last time
    held = set()  # rooms that were attached last frame
    still = {"sig": None, "since": time.time()}  # when the windows last moved (gates wait until they are still)

    t0 = time.monotonic()
    paused = help_on = False
    frozen = (0.0, 0.0)  # (now, t) shown while paused
    prev = None
    cols = rows = 0
    dirty = True
    try:
        tty.setcbreak(fd)
        if link:
            link.theme = room.theme.name
            link.start()
            room.title = link.name
        if watcher:
            watcher.start()
            write("\x1b[22;0t\x1b]2;%s%s\x07" % (windetect.PREFIX, win_id))  # window title = how we are found
        write("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[0m\x1b[2J")  # alt screen, hide cursor, no wrap
        while True:
            frame_start = time.monotonic()
            t = frame_start - t0
            now = (args.at + t) if args.at is not None else time.time()
            for k in read_keys(fd, reports) if select.select([fd], [], [], 0)[0] else "":
                if k == "q":
                    return
                if k == "p":
                    paused = not paused
                    frozen = (now, t)
                    dirty = True
                elif k == "?":
                    help_on = not help_on
                    dirty = True
                elif k == "r":  # next room shape: auto, 3:1, 2:1 ...
                    room.ratio = RATIO_CHOICES[(RATIO_CHOICES.index(room.ratio) + 1) % len(RATIO_CHOICES)]
                    flash, flash_until = "ROOM SHAPE %s  (f fits the window)" % room.ratio.upper(), time.time() + 4
                    prev, dirty = None, True
                    write("\x1b[0m\x1b[2J")
                elif k == "g":  # send a Clawd next door right now (to try crossing)
                    flash, flash_until = room.send_one(now), time.time() + 4
                    dirty = True
                elif k in "[]":
                    if room.ratio != "auto":
                        flash, flash_until = "SCALE NEEDS THE AUTO ROOM SHAPE (R)", time.time() + 4
                    else:
                        room.scale = min(MAX_SCALE, max(1, room.scale + (1 if k == "]" else -1)))
                        auto = False
                        flash, flash_until = "SCALE %dX  (F = AUTO)" % room.scale, time.time() + 4
                        prev = None
                        write("\x1b[0m\x1b[2J")
                elif k == "f":
                    if not watcher:
                        flash, flash_until = "FIT ONLY WORKS ON WSL", time.time() + 4
                    elif room.ratio == "auto":  # automatic font and scale again (the window is left as it is)
                        auto = True
                        setup = OpenSetup(tool, title, args.font, args.size, reports, lambda: None, window=False, profile_font=args.profile_font)
                        font_set = font_set or bool(args.font)
                        room.scale = auto_scale(cols, rows * 2, len(room.ordered()))
                        flash, flash_until = "AUTO FONT AND SCALE", time.time() + 3
                        prev = None
                        write("\x1b[0m\x1b[2J")
                    else:  # fixed shape: zoom the font, then size the window to the exact grid
                        fitter = windetect.Fitter(tool, title, *std_grid(room.ratio))
                        flash, flash_until = "FITTING WINDOW...", time.time() + 30
                    dirty = True
            size = shutil.get_terminal_size((80, 24))
            if resized[0] or (size.columns, size.lines) != (cols, rows):
                resized[0] = False
                cols, rows = size.columns, size.lines
                prev = None  # full redraw after a resize
                dirty = True
                prescale = room.scale
                if auto and room.ratio == "auto" and not args.no_auto:
                    room.scale = auto_scale(cols, rows * 2, len(room.ordered()))  # at once; undone below if it was the font
                pend = (time.time(), reports.pop("cell", None), prescale) if pend is None else pend
                write("\x1b[0m\x1b[2J\x1b[14t\x1b[16t")  # ... and ask how big the text area and a cell are in pixels
                if os.environ.get("CLAWD_SIZE_LOG"):  # test hook: note every size the terminal had
                    with open(os.environ["CLAWD_SIZE_LOG"], "a") as f:
                        f.write("%.1f %d %d\n" % (time.time(), cols, rows))
            for r in tail.poll():
                room.apply(r)
            room.prune(now)
            if link:
                for kind, x in link.update(time.time(), cols, rows):
                    if kind in ("joined", "left"):
                        flash, flash_until = "%s %s" % (x["name"].upper(), kind.upper()), time.time() + 4
                        dirty = True
                        if kind == "left":
                            room.peer_gone(x["id"])
                    elif kind == "msg":
                        room.receive(x)

                mine = watcher.rect_of(win_id) if watcher else None
                sides = {}  # peer id -> "left"/"right" when that room is next to this one
                for p in link.peers:
                    manual = {args.left: "left", args.right: "right", args.up: "up", args.down: "down"}.get(p["name"])
                    slack = windetect.HYSTERESIS_PX if p["id"] in room.neighbors.values() or p["id"] in held else 0  # stay attached a bit longer
                    sides[p["id"]] = manual or (windetect.neighbor_side(mine, watcher.rect_of(p["id"]), args.near, slack) if watcher else None)
                room.peers = [(p["name"], sides[p["id"]]) for p in link.peers]
                held = {pid for pid, side in sides.items() if side}  # attached now (also counts while its gate is closed)
                room.neighbors = {side: pid for pid, side in sides.items() if side}
                room.neighbor_themes = {side: next(p["theme"] for p in link.peers if p["id"] == pid)
                                        for side, pid in room.neighbors.items()}
                if not args.no_split:  # the linked rooms share the sessions: each shows its own part
                    ids = sorted([link.id] + [p["id"] for p in link.peers])
                    room.owns = lambda sid, ids=ids, me=link.id: ids[zlib.crc32(sid.encode()) % len(ids)] == me
                mine_area = watcher.area_of(win_id) if watcher else None
                my_grid = grid_params(room, mine_area, watcher.dpi_of(win_id), reports, cols, rows) if mine_area else None
                link.grid = my_grid
                link.band = my_band(room, my_grid)
                peer_info = {p["id"]: p for p in link.peers}
                sig = None
                if mine_area:  # where everything is, to about 4 pixels: if it changes the windows are being moved
                    sig = (tuple(v // 4 for v in mine_area), my_grid and tuple(round(v) for v in my_grid[2:5]),
                           tuple(sorted((pid, tuple(v // 4 for v in (watcher.area_of(pid) or ())), (peer_info.get(pid) or {}).get("grid") and
                                         tuple(round(v) for v in peer_info[pid]["grid"][2:5])) for pid in room.neighbors.values())))
                if sig != still["sig"]:
                    still["sig"], still["since"] = sig, time.time()
                if time.time() - still["since"] < GATE_DELAY:  # pending: no gates until the windows have been still
                    room.link_pos, closed, room.gate_scale = {}, set(), {}
                    room.neighbors = {}
                    room.peers = [(n, None) for n, side in room.peers]
                else:
                    room.link_pos, closed, room.gate_scale = plan_passages(room, watcher, win_id, my_grid, peer_info)
                room.neighbors = {side: pid for side, pid in room.neighbors.items() if side not in closed}  # no gate where it does not fit
                room.peers = [(n, side if side not in closed else None) for n, side in room.peers]
                if closed != last_closed:  # say why a window next to us has no gate
                    if closed:
                        flash, flash_until = "NO GATE: NOT ENOUGH FLOOR / OVERLAP", time.time() + 4
                    last_closed, dirty = set(closed), True
                if os.environ.get("CLAWD_SIZE_LOG") and room.link_pos != getattr(link, "_logged", None):  # test hook
                    link._logged = dict(room.link_pos)
                    with open(os.environ["CLAWD_SIZE_LOG"], "a") as f:
                        f.write("%.1f link %s\n" % (time.time(), {k: round(v, 1) for k, v in room.link_pos.items()}))
                        f.write("   cols=%d reports=%s area=%s dpi=%s\n" % (cols, reports, watcher.area_of(win_id) if watcher else None,
                                                                         watcher.dpi_of(win_id) if watcher else None))
                for to, mtype, data in room.outbox:  # Clawds crossing over and calls to come home
                    if not link.send(to, mtype, **data):
                        room.peer_gone(to)
                room.outbox = []
            if pend:  # did the window change, or the font (the cell size changed, the window did not)?
                cell = reports.get("cell")
                if cell or time.time() - pend[0] > 0.4:
                    if cell and pend[1] and cell != pend[1] and not setup and not fitter and auto:
                        auto, room.scale = False, pend[2]  # Ctrl +/- : the user took over, keep the scale
                        flash, flash_until = "FONT CHANGED: F = AUTO FONT AND SCALE", time.time() + 5
                    elif not cell and pend[1]:
                        reports["cell"] = pend[1]
                    pend, prev, dirty = None, None, True
            if setup:
                if not setup.update(time.time(), cols, rows, watcher.rect_of(win_id) if watcher else None, watcher.area_of(win_id) if watcher else None):
                    setup, dirty = None, True
            if fitter:
                fitter.update(time.time(), cols, rows, watcher.rect_of(win_id) if watcher else None)
                if fitter.done:
                    flash, flash_until, fitter = fitter.message, time.time() + 5, None
                    dirty = True
            if dirty or not paused:
                if cols < MIN_COLS or rows < MIN_ROWS:
                    write(too_small(cols, rows))
                    prev = None
                else:
                    shown_now, shown_t = frozen if paused else (now, t)
                    cv, note = render(cols, rows, room, shown_now, shown_t)
                    cells = cells_from_canvas(cv, overlay_text(paused, help_on, note, flash if time.time() < flash_until else ""))
                    # one write per frame, wrapped in "synchronized update" so it never tears
                    write("\x1b[?2026h" + diff_frame(cells, prev) + "\x1b[0m\x1b[?2026l")
                    prev = cells
                dirty = False
            wait = frame_start + 1.0 / FPS - time.monotonic()
            if wait > 0:
                select.select([fd], [], [], wait)  # sleeps, but wakes on a key press
    finally:
        if tool:
            if font_set:  # put the terminal font back to the profile's
                tool.reset_zoom(title)
                time.sleep(0.8)
            tool.stop()
        if watcher:
            watcher.stop()
            write("\x1b[23;0t")  # give the window title back (if the terminal supports it)
        if link:
            link.close()
        termios.tcsetattr(fd, termios.TCSADRAIN, old_attrs)
        write("\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l")


MENU = (("OFFICE", "office"), ("COFFEE", "coffee"), ("PARK", "park"))  # (label, scene); the scene is also the room name
MENU_W, MENU_H = 144, 84  # the menu picture, in room pixels (drawn k times bigger when the terminal allows)


def dim(c, f):
    return (int(c[0] * f), int(c[1] * f), int(c[2] * f))


def draw_menu(cv, pick, solo, t, theme_cache):
    """The start menu in the room's own style: a little picture of each scene, Clawd standing in it.
    It fills the canvas, whatever its size (like the room does): the cards take what space is left."""
    cv.rect(0, 0, cv.w, cv.h, MARGIN)
    cv.text_c(cv.w // 2, 3, "CLAWD ROOM", YELLOW)
    if cv.w < 46 or cv.h < 36:  # no room for the cards
        cv.text_c(cv.w // 2, cv.h // 2, "TOO SMALL", GREY)
        return
    tall = cv.h >= 56
    if tall:
        cv.text_c(cv.w // 2, 11, "PICK A ROOM", GREY)
    top = 19 if tall else 11
    bottom = cv.h - 22  # the link line and the key hints
    ch = max(10, min(90, bottom - top - 9))
    cw = max(14, min(ch * 4 // 3, (cv.w - 16) // 3))
    ch = min(ch, cw * 3 // 4 + 2)
    gap = (cv.w - 3 * cw) // 4
    y = top + max(0, (bottom - top - ch - 9) // 2)
    for i, (label, scene) in enumerate(MENU):
        x = gap + i * (cw + gap)
        theme = theme_cache.setdefault(scene, themes.THEMES[scene]())
        card = Canvas(cw, ch, WALL)
        theme.background(card, 0)
        chosen = i == pick
        if not chosen:
            card.rows = [[dim(c, 0.55) for c in row] for row in card.rows]
        bob = -1 if chosen and int(t * 3) % 2 else 0
        pose = {"bob": bob, "lean": 0, "arms": (1, 1) if chosen else (0, 0), "legs": 0,
                "eyes": "happy" if chosen else "open", "look": (0, 0)}
        draw_clawd(card, cw // 2, min(int(ch * theme.horizon) + 3, ch - 9), pose, None, "", dim(CLAWD, 1.0 if chosen else 0.6))
        if chosen:
            cv.rect(x - 2, y - 2, cw + 4, ch + 4, YELLOW)
        cv.blit(card, x, y)
        cv.text_c(x + cw // 2, y + ch + 4, label, YELLOW if chosen else GREY)
    cv.text_c(cv.w // 2, cv.h - 15, "S  SOLO" if solo else "S  LINKED", WHITE)
    hint = next((h for h in ("< > CHOOSE   ENTER OPEN   Q QUIT", "< >   ENTER   Q QUIT", "ENTER  Q") if text_width(h) <= cv.w - 4), "")
    cv.text_c(cv.w // 2, cv.h - 8, hint, GREY)


def start_fitter(tool, title, size):
    """A Fitter that only sizes the window to `size` (COLSxROWS) and leaves the font alone, or None for 'off'."""
    if size.lower() == "off":
        return None
    try:
        cols, rows = (int(v) for v in size.lower().split("x"))
    except ValueError:
        sys.exit("--size wants COLSxROWS, for example 128x36, or off")
    fitter = windetect.Fitter(tool, title, cols, rows)
    fitter.stage = "resize"  # skip the font zoom: OpenSetup sets the font
    return fitter


def choose_scene(args):
    """Ask which scene to open (arrows or 1-3, Enter; S toggles solo). Returns (scene, room name or None), or None to quit.

    Like the room it first sets the terminal font and the window size (OpenSetup); the room then skips that.
    """
    fd = sys.stdin.fileno()
    old_attrs = termios.tcgetattr(fd)
    pick, solo = 0, False
    watcher = tool = setup = None
    reports = {}
    chosen = False
    win_id = "menu-%d" % os.getpid()
    if not args.no_detect and windetect.available():
        watcher, tool = windetect.WindowWatcher(), windetect.WindowTool()
        if not args.no_auto:
            setup = OpenSetup(tool, windetect.PREFIX + win_id, args.font, args.size, reports,
                              lambda: (sys.stdout.write("\x1b[14t\x1b[16t"), sys.stdout.flush()), profile_font=args.profile_font)
    theme_cache = {}
    prev, size, t0 = None, None, time.monotonic()
    try:
        tty.setcbreak(fd)
        if watcher:
            watcher.start()
            sys.stdout.write("\x1b]2;%s%s\x07" % (windetect.PREFIX, win_id))  # window title = how we are found
        sys.stdout.write("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[0m\x1b[2J")
        while True:
            if select.select([fd], [], [], 0.08)[0]:
                raw = os.read(fd, 256)
                for kind, h, w in REPORT_RE.findall(raw):  # the terminal's answers about its pixel sizes
                    reports["text" if kind == b"4" else "cell"] = (int(w), int(h))
                data = REPORT_RE.sub(b"", raw).decode("utf-8", "ignore")
                if data in ("\x1b[D", "\x1bOD", "h", "k"):
                    pick = (pick - 1) % len(MENU)
                elif data in ("\x1b[C", "\x1bOC", "l", "j", "\t"):
                    pick = (pick + 1) % len(MENU)
                elif data in ("s", "S"):
                    solo = not solo
                elif data in ("q", "Q", "\x03", "\x1b"):
                    return None
                elif data in ("\r", "\n", " "):
                    chosen = True
                    return MENU[pick][1], None if solo else MENU[pick][1]
                elif data.isdigit() and 1 <= int(data) <= len(MENU):
                    pick = int(data) - 1
            term = shutil.get_terminal_size((80, 24))
            if (term.columns, term.lines) != size:
                size, prev = (term.columns, term.lines), None
                sys.stdout.write("\x1b[0m\x1b[2J")
            if setup and not setup.update(time.time(), term.columns, term.lines, watcher.rect_of(win_id), watcher.area_of(win_id)):
                setup, args.opened = None, True  # the room need not do it again
            pw, ph = term.columns, term.lines * 2
            k = max(1, min(pw // (MENU_W * 5 // 4), ph // (MENU_H * 5 // 4)))  # chunkier pixels in a big window
            small = Canvas(pw // k, ph // k // 2 * 2, MARGIN)
            draw_menu(small, pick, solo, time.monotonic() - t0, theme_cache)
            big = small.scaled(k)
            full = Canvas(pw, ph, MARGIN)
            ox, oy = (pw - big.w) // 2, (ph - big.h) // 4 * 2  # whole terminal rows
            for y in range(max(0, -oy), min(big.h, ph - oy)):
                src = big.rows[y]
                lo = max(0, -ox)
                full.rows[oy + y][ox + lo:ox + min(big.w, pw - ox)] = src[lo:min(big.w, pw - ox)]
            cells = cells_from_canvas(full, "")
            sys.stdout.write(diff_frame(cells, prev))
            sys.stdout.flush()
            prev = cells
    finally:
        if tool:
            if not chosen and args.font and not args.no_auto:  # leaving: put the terminal font back
                tool.reset_zoom(windetect.PREFIX + win_id)
                time.sleep(0.8)
            tool.stop()
        if watcher:
            watcher.stop()
        termios.tcsetattr(fd, termios.TCSADRAIN, old_attrs)
        sys.stdout.write("\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l")
        sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser(description="clawd-room in the terminal")
    ap.add_argument("--events", default=EVENTS, help="events file (default: ~/.clawd-room/events.jsonl)")
    ap.add_argument("--at", type=float, default=None, help="pretend 'now' is this timestamp")
    ap.add_argument("--room", metavar="NAME", help="name this room (park, coffee...) and link it to other rooms")
    ap.add_argument("--theme", choices=sorted(themes.THEMES), help="office, coffee or park (default: guessed from --room)")
    ap.add_argument("--left", metavar="NAME", help="treat the room called NAME as the neighbour on the left (instead of detecting windows)")
    ap.add_argument("--right", metavar="NAME", help="treat the room called NAME as the neighbour on the right")
    ap.add_argument("--up", metavar="NAME", help="treat the room called NAME as the one above (a rope leads up)")
    ap.add_argument("--down", metavar="NAME", help="treat the room called NAME as the one below (stairs lead down)")
    ap.add_argument("--no-split", action="store_true", help="show every session in every linked room (default: share them)")
    ap.add_argument("--ratio", choices=RATIO_CHOICES, default=DEFAULT_RATIO,
                    help="shape of the room, width:height: auto follows the window shape (default %(default)s); the r key cycles")
    ap.add_argument("--size", default=DEFAULT_SIZE, metavar="COLSxROWS",
                    help="set the window to this many cells when the room opens (font size untouched), or 'off' (default %(default)s)")
    ap.add_argument("--font", type=int, default=DEFAULT_FONT, metavar="PT",
                    help="terminal font size to ask for when the room opens, 0 = leave it (default %(default)s)")
    ap.add_argument("--profile-font", type=int, default=PROFILE_FONT, metavar="PT",
                    help="the terminal profile's own font size, what Ctrl+0 gives (default %(default)s; the Clawd Room profile says 8)")
    ap.add_argument("--scale", type=int, default=0, metavar="N",
                    help="start with this TUI scale (1x, 2x...) and no automatic scale (default: automatic)")
    ap.add_argument("--no-auto", action="store_true",
                    help="do not set the font or window size when opening, nor the scale when the window changes")
    ap.add_argument("--no-detect", action="store_true", help="do not look for the windows' positions")
    ap.add_argument("--near", type=int, default=windetect.NEAR_PX, metavar="PX",
                    help="windows with a gap smaller than this many screen pixels are attached (default %(default)s)")
    ap.add_argument("--dump", metavar="COLSxROWS", help="print one plain-text frame and exit")
    args = ap.parse_args()

    if args.dump:
        try:
            cols, rows = (int(v) for v in args.dump.lower().split("x"))
        except ValueError:
            ap.error("--dump wants COLSxROWS, for example 100x30")
        now = args.at if args.at is not None else time.time()
        room = Room()
        room.theme = themes.pick(args.room, args.theme)
        room.ratio = args.ratio
        for r in Tail(args.events).history(now - REPLAY_SECONDS):
            room.apply(r)
        room.prune(now)
        print(dump(cols, rows, room, now, 0.0))
        return
    if not (sys.stdin.isatty() and sys.stdout.isatty()):
        sys.exit("room_tui.py needs a terminal (stdin and stdout must be a tty)")
    if not (args.room or args.theme or args.at is not None):  # started bare (for example from a tab): ask
        choice = choose_scene(args)
        if choice is None:
            return
        args.theme, args.room = choice
    run(args)


if __name__ == "__main__":
    main()
