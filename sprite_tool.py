#!/usr/bin/env python3
"""Exports Clawd's frames and items to editable text files, and draws the sheet and GIFs from them.

    python3 sprite_tool.py export          write sprites/clawd.txt and sprites/items.txt from the code's drawing
                                           (never overwrites a file that exists; --force does; --add appends frames the file lacks)
    python3 sprite_tool.py build           redraw Clawd Room Note/Animations/ (sheet.png, items.png, one GIF per action)

Edit the grids in sprites/*.txt (one character per pixel, legend at the top, see sprites.py), run `build` to look at the
result; the room reads the same files when it starts. The actions are sampled from `pose_for` with a stand-in Session,
so the GIFs show the same poses the room shows (timing is approximate). Standard library only.
"""
import os
import struct
import sys
import zlib

import room_tui as r
import sprites

ART = os.path.join(sprites.HERE, "Clawd Room Note", "Animations")
BG = (44, 44, 56)
STEPS, FPS = 41, 12  # one loop = 3.4 s, long enough for a blink

# (action, pose_for state, Session attributes); the order is the order in the files and the note
KNOWN = {r.INK: "K", r.BLUSH: "P", r.GLINT: "G", r.WHITE: "W", r.YELLOW: "Y", r.BUBBLE_EDGE: "E"}  # fixed letters in the files

ACTIONS = [
    ("idle", "idle", {}),
    ("work", "work", {}),
    ("think", "think", {}),
    ("wait", "wait", {}),
    ("cheer", "cheer", {}),
    ("sleep", "sleep", {}),
    ("walk", "idle", {"moving": True, "stage": "walk"}),
    ("climb", "idle", {"moving": True, "vclimb": True}),
    ("chase", "idle", {"moving": True, "stage": "chase"}),
    ("flee", "idle", {"moving": True, "stage": "flee"}),
    ("chat-nod", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 0}),
    ("chat-wave", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 1}),
    ("chat-hop", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 2}),
    ("chat-hi5", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 3}),
    ("chat-dance", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 4}),
    ("barista", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 6, "part": "host"}),
    ("visitor", "idle", {"chat_with": "y", "stage": "talk", "chat_kind": 6, "part": "visitor"}),
    ("stretch", "idle", {"emote": ("stretch", 1000.0, 1010.0)}),
    ("solo-dance", "idle", {"emote": ("dance", 1000.0, 1010.0)}),
    ("swing", "idle", {"emote": ("swing", 1000.0, 1010.0)}),
    ("slide", "idle", {"emote": ("slide", 1000.0, 1010.0)}),
    ("jump", "idle", {"emote": ("jump", 1000.0, 1010.0)}),
    ("wave", "idle", {"emote": ("wave", 1000.0, 1010.0)}),
    ("applaud", "idle", {"react_until": 9e9}),
    ("peek", "idle", {"peek": ["y", 1.0]}),
]
FACE = {"idle": "no events: looks around, blinks", "work": "typing at the desk", "think": "sways, scratches its head",
        "wait": "waves for attention", "cheer": "task done: jumps", "sleep": "90 s without events", "walk": "walking",
        "climb": "rope / stairs", "chase": "tag: chasing", "flee": "tag: running away"}


def steps(action):
    """[(pose key, pose)] for one loop of an action."""
    name, state, attrs = next(a for a in ACTIONS if a[0] == action)
    s = r.Session("x", 1000.0)
    s.seed = 0.0
    for k, v in attrs.items():
        setattr(s, k, v)
    out = []
    for i in range(STEPS):
        t = 0.2 + i / FPS
        s.walk_phase = t * 12
        p = r.pose_for(s, state, t, 1000.0 + t)
        out.append((r.pose_key(p), p))
    return out


def blank_pose(p):
    q = dict(p)
    q["bob"] = 0
    return q


def clawd_frames():
    """[(name, key, rows)] for every distinct frame of every action, drawn by the code (not the sprite files)."""
    saved, r.CLAWD_SPRITES = r.CLAWD_SPRITES, sprites.SpriteFile("-")
    names, frames = {}, []
    try:
        for action, _, _ in ACTIONS:
            n = 0
            for key, p in steps(action):
                if key in names:
                    continue
                n += 1
                names[key] = "%s-%d" % (action, n)
                cv = r.Canvas(15, 9, sprites.TRANSPARENT)
                r.draw_clawd(cv, 7, 1, blank_pose(p))
                frames.append((names[key], key, cv.rows))
    finally:
        r.CLAWD_SPRITES = saved
    return frames


def item_frames():
    saved, r.ITEM_SPRITES = r.ITEM_SPRITES, sprites.SpriteFile("-")
    out = []
    try:
        for n in range(4):
            cv = r.Canvas(9, 8, sprites.TRANSPARENT)
            r.draw_bubble(cv, 4, 0, "dots", n / 3.0 + 0.01)
            out.append(("bubble-dots-%d" % n, "bubble-dots-%d" % n, cv.rows))
        for name, kind in (("bubble-excl", "!"), ("bubble-ques", "?"), ("bubble-cup", "cup")):
            cv = r.Canvas(9, 8, sprites.TRANSPARENT)
            r.draw_bubble(cv, 4, 0, kind, 0.0)
            out.append((name, name, cv.rows))
        cv = r.Canvas(4, 2, sprites.TRANSPARENT)  # a coffee cup carried at the side (the steam is drawn by the code)
        cv.rect(0, 0, 3, 2, r.WHITE)
        cv.px(3, 0, r.WHITE)
        out.append(("cup-held", "cup-held", cv.rows))
        cv = r.Canvas(9, 10, sprites.TRANSPARENT)  # the "need your OK" sign
        cv.rect(0, 0, 9, 9, r.YELLOW)
        cv.rect(1, 1, 7, 7, (250, 235, 150))
        cv.text(3, 2, "!", r.INK)
        cv.rect(4, 9, 1, 1, (120, 90, 40))
        out.append(("sign-wait", "sign-wait", cv.rows))
    finally:
        r.ITEM_SPRITES = saved
    return out


def export(force, add):
    for fname, frames, note in (("clawd.txt", clawd_frames(), "Clawd frames (15 x 9, the row above the head is row 0).\nEdit the grids; `python3 sprite_tool.py build` shows them."),
                                ("items.txt", item_frames(), "Items Clawd carries or shows (bubbles, cup, sign). Top-left corner = where the code puts it.")):
        path = os.path.join(sprites.DIR, fname)
        if os.path.exists(path) and not (force or add):
            print("kept", path, "(exists; --add appends new frames, --force rewrites)")
            continue
        if add and os.path.exists(path):
            have = sprites.SpriteFile(path)
            keep = [(n, have.by_name[n].key, [[sprites.TRANSPARENT if c is None else (r.CLAWD if c == "body" else c) for c in row] for row in have.by_name[n].rows]) for n in have.order]
            frames = keep + [f for f in frames if f[1] not in have.by_key and f[0] not in have.by_name]
        sprites.save(path, frames, r.CLAWD, note, KNOWN)
        print("wrote", path, len(frames), "frames")


# ---- pictures ------------------------------------------------------------------------
def png(cv, k, path):
    rows = []
    for y in range(cv.h):
        rows += [b"\0" + b"".join(bytes(c) * k for c in cv.rows[y])] * k

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", cv.w * k, cv.h * k, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


def frame_name(key):
    sp = r.CLAWD_SPRITES.get(key)
    return sp.name if sp else key.split()[0]


def build():
    W, H, K = 26, 18, 8  # a gif frame is W x H room pixels at K screen pixels each
    cols = max(len({k for k, _ in steps(a)}) for a, _, _ in ACTIONS)
    cell_w, cell_h, label_w = 50, 28, 48
    sheet = r.Canvas(label_w + cols * cell_w, len(ACTIONS) * cell_h + 4, BG)
    for i, (action, _, _) in enumerate(ACTIONS):
        y0 = 2 + i * cell_h
        sheet.text(2, y0 + 12, action.upper()[:11], r.WHITE)
        seen = []
        for key, p in steps(action):
            if key not in seen:
                seen.append(key)
                j = len(seen) - 1
                cx = label_w + j * cell_w + cell_w // 2
                r.draw_clawd(sheet, cx, y0 + 8, blank_pose(p))
                sheet.text_c(cx, y0 + 21, frame_name(key).upper(), (170, 170, 190))
        frames = []
        for key, p in steps(action):
            cv = r.Canvas(W, H, BG)
            r.draw_clawd(cv, W // 2, 8, p)
            frames.append((W * K, H * K, [[c for c in row for _ in range(K)] for row in cv.rows for _ in range(K)]))
        sprites.write_gif(os.path.join(ART, action + ".gif"), frames, 100 // FPS)
    png(sheet, 6, os.path.join(ART, "sheet.png"))
    names = [n for n in r.ITEM_SPRITES.order]
    if names:
        icv = r.Canvas(len(names) * 14 + 4, 30, BG)
        for i, n in enumerate(names):
            sp = r.ITEM_SPRITES.by_name[n]
            r.blit_sprite(icv, sp, 4 + i * 14, 4)
            icv.text(4 + i * 14, 20, n.split("-")[-1][:3].upper(), (170, 170, 190))
        png(icv, 8, os.path.join(ART, "items.png"))
    print("built", ART)


def main(argv):
    if not argv or argv[0] not in ("export", "build"):
        print(__doc__)
        return 1
    if argv[0] == "export":
        export("--force" in argv, "--add" in argv)
    else:
        build()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
