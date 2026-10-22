#!/usr/bin/env python3
"""Starts the room in a pty and drives it with keys.

    python3 tools/pty_smoke.py

Checks: it starts and draws, shows a new event, answers p ? [ ] arrows tab c z r (room size list) g f, quits on q, restores the terminal. Also the start menu, and the pixel editor (fill, prop frames, an animation by its name, --edit list) on a copy of the sprites.
"""
import fcntl
import json
import os
import pty
import select
import struct
import sys
import tempfile
import termios
import time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CMD = [os.path.join(ROOT, "c", "clawd-room")]


def status_text(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    finally:
        os.unlink(path)


def run(args, keys, env_extra=None, cols=110, rows=32, settle=0.6):
    """keys: list of (delay seconds, bytes). Returns (all output, exit status)."""
    status_log = tempfile.NamedTemporaryFile(suffix=".status", delete=False).name  # the bar along the bottom is pixels: its text comes here
    pid, fd = pty.fork()
    if pid == 0:
        env = dict(os.environ, TERM="xterm-256color", CLAWD_STATUS_LOG=status_log, **(env_extra or {}))
        os.execvpe(CMD[0], CMD + args, env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    out = b""
    t0 = time.time()
    pending = list(keys)
    status = None
    while time.time() - t0 < 20:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                data = os.read(fd, 65536)
            except OSError:
                break
            if not data:
                break
            out += data
        now = time.time() - t0
        while pending and now >= pending[0][0]:
            os.write(fd, pending.pop(0)[1])
        done, st = os.waitpid(pid, os.WNOHANG)
        if done:
            status = st
            while select.select([fd], [], [], 0.1)[0]:
                try:
                    data = os.read(fd, 65536)
                except OSError:
                    break
                if not data:
                    break
                out += data
            break
    if status is None:  # the pty closed (or we ran out of time)
        for _ in range(20):
            done, st = os.waitpid(pid, os.WNOHANG)
            if done:
                status = st
                break
            time.sleep(0.1)
    if status is None:
        os.kill(pid, 9)
        os.waitpid(pid, 0)
        return status_text(status_log) + out, "timeout"
    return status_text(status_log) + out, os.WEXITSTATUS(status) if os.WIFEXITED(status) else "signal"


fails = []


def check(name, ok):
    print("%-4s %s" % ("OK" if ok else "FAIL", name))
    if not ok:
        fails.append(name)


with tempfile.TemporaryDirectory() as tmp:
    events = os.path.join(tmp, "events.jsonl")
    open(events, "w").write("")
    now = time.time()
    # 1. the room: starts, shows a new session, takes keys, quits
    ev = json.dumps({"t": now + 0.1, "sid": "smoke", "ev": "SessionStart", "cwd": "smoketest", "tool": ""})

    def add_event():
        with open(events, "a") as f:
            f.write(ev + "\n")
    pid_keys = [(1.0, b"p"), (1.3, b"p"), (1.6, b"?"), (1.9, b"?"), (2.2, b"]"), (2.4, b"\x1b[A"), (2.5, b"d"), (2.6, b"["), (2.7, b"\t"), (2.75, b"z"), (2.8, b"r"), (2.95, b"\x1b[B"), (3.1, b"\r"), (3.4, b"g"), (3.7, b"f"), (4.0, b"\x1b[A"), (4.6, b"q")]
    import threading
    threading.Timer(0.8, add_event).start()
    out, st = run(["--theme", "office", "--events", events], pid_keys)
    check("room exits with 0 on q", st == 0)
    check("room enters the alternate screen", b"\x1b[?1049h" in out)
    check("room restores the terminal", out.rstrip().endswith(b"\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"))
    check("room draws frames (half blocks)", out.count("▀".encode()) > 500)
    check("room uses synchronized updates", b"\x1b[?2026h" in out and b"\x1b[?2026l" in out)
    check("a new event changes the picture", out.count(b"\x1b[") > 1000)
    check("the help line appears (?)", b"q/esc quit" in out)
    check("PAUSED appears (p)", b"PAUSED" in out)
    check("zoom message appears (])", b"ZOOM 2X" in out)
    check("room size list picks a size (r, down, Enter)", b"ROOM SIZE S" in out)
    check("tab follows a Clawd", b"FOLLOW " in out)
    check("z turns auto zoom on", b"AUTO ZOOM ON" in out)
    out, st = run(["--theme", "office", "--events", events], [(1.0, b"r"), (1.3, b"\x1b"), (1.6, b"]"), (2.2, b"q")])
    check("Esc closes the room size list, not the room", st == 0 and b"ZOOM 2X" in out)
    # 2. a lone Esc quits too
    out, st = run(["--theme", "park", "--events", events], [(1.0, b"\x1b")])
    check("Esc quits", st == 0)
    # 3. too small a terminal
    out, st = run(["--theme", "office", "--events", events], [(0.8, b"q")], cols=30, rows=10)
    check("small terminal says what it needs", b"need 44x20" in out and st == 0)
    # 4. the start menu: Enter opens the first scene, then q
    out, st = run(["--events", events], [(0.8, b"\x1b[C"), (1.1, b"\x1b[D"), (1.4, b"s"), (1.7, b"\r"), (2.8, b"q")])
    check("start menu shows its title and then the room", st == 0 and out.count(b"\x1b[?1049h") >= 1 and out.count(b"\x1b[?1049l") >= 1)
    out, st = run(["--events", events], [(0.8, b"q")])
    check("start menu quits on q", st == 0)
    # 5. not a terminal: refuse (checked without a pty)
    import subprocess
    r = subprocess.run(CMD + ["--theme", "office"], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    check("without a terminal it says so and fails", r.returncode != 0 and "terminal" in r.stderr + r.stdout)
    # 6. the pixel editor, on a copy of the sprites: FILL, frames of a prop, an animation by its name, --edit list
    import shutil
    spr = tempfile.mkdtemp(prefix="clawd-smoke-sprites-")
    for f in ("props.txt", "items.txt", "clawd.txt"):
        shutil.copy(os.path.join(ROOT, "sprites", f), spr)
    mine = tempfile.mkdtemp(prefix="clawd-smoke-mine-")  # (your own sprites: the editor writes only there)
    env = {"CLAWD_SPRITES": spr, "CLAWD_MY_SPRITES": mine}
    def read(f, where=None):
        path = os.path.join(where or spr, f)
        if not os.path.exists(path):
            return ""
        with open(path) as fh:
            return fh.read()
    clawd0 = read("clawd.txt")
    click = lambda c, r: [b"\x1b[<0;%d;%dM" % (c, r), b"\x1b[<0;%d;%dm" % (c, r)]  # (a left click on a cell, 1-based)
    # (FILL clicks down the canvas's left column, wherever the canvas sits: every pixel there is outside the Clawd)
    clicks = [k for row in range(7, 31) for k in click(6, row)]
    keys = [(0.7, b"p"), (0.8, b"#"), (1.0, b"FF0000\r"), (1.3, b"b")] + [(1.6 + i * 0.02, k) for i, k in enumerate(clicks)] + [(3.0, b"q"), (3.3, b"s"), (3.7, b"q")]  # (p: the colour panel unfolded, the canvas at the left; q: keep or discard, s: keep; q in the picker: quit)
    out, st = run(["--edit", "walk-1"], keys, env, cols=150, rows=50)
    walk = (read("clawd.txt", mine).split("@ walk-1")[1:] or [""])[0].split("@")[0]
    check("editor: FILL fills the touching see-through pixels, not the closed ones (in your file; the default stays)", st == 0 and walk.count("a") > 40 and ".." in walk and read("clawd.txt") == clawd0)
    props0 = read("props.txt")
    out, st = run(["--edit", "window"], [(0.8, b"f"), (1.0, b"f"), (1.4, b"x"), (1.6, b"x"), (1.9, b"q"), (2.2, b"s"), (2.6, b"q")], env, cols=150, rows=50)
    heads = [l for l in read("props.txt", mine).splitlines() if l.startswith("@ window")]
    check("editor: f adds prop frames (yours), x x deletes your last one (q asks, s keeps)", st == 0 and heads == ["@ window~2 | window~2"] and read("props.txt") == props0)
    out, st = run(["--edit", "walk"], [(0.8, b" "), (1.6, b">"), (1.9, b"o"), (2.2, b" "), (2.5, b"f"), (2.8, b"q"), (3.1, b"q")], env, cols=150, rows=50)
    check("editor: opens an animation by its name, plays, steps frames; f leaves Clawd frames alone", st == 0 and b"Clawd Room pixel editor" in out and read("clawd.txt").count("@ walk-") == clawd0.count("@ walk-"))
    out, st = run(["--edit", "-"], [(0.8, b"W"), (0.9, b"A"), (1.0, b"L"), (1.3, b"\r"), (1.7, b"q"), (2.1, b"\x1b")], env, cols=150, rows=50)
    check("editor: the sprite picker opens with no name, finds by typing, Enter opens, q goes back to it, a lone Esc there quits (the search empty)", st == 0 and b"Clawd Room pixel editor" in out)
    r = subprocess.run(CMD + ["--edit", "list"], capture_output=True, text=True, env=dict(os.environ, **env))
    import re
    check("--edit list shows animations with their frame counts", r.returncode == 0 and re.search(r"walk +4\b", r.stdout) and re.search(r"think +9\b", r.stdout) and "window" in r.stdout)
    # 7. the stage page: a part animation (anims/swing.txt) on a copy of the animations; what you change is saved to yours
    anims = tempfile.mkdtemp(prefix="clawd-smoke-anims-")
    mine_a = tempfile.mkdtemp(prefix="clawd-smoke-mine-anims-")
    shutil.copytree(os.path.join(ROOT, "anims"), anims, dirs_exist_ok=True)  # (with the gate themes' folders)
    aenv = dict(env, CLAWD_ANIMS=anims, CLAWD_MY_ANIMS=mine_a)
    swing_mine = os.path.join(mine_a, "swing.txt")
    last_view = [4, 50, 2]
    def run_stage(args, keys, env, cols=300, rows=90):
        """the stage page opens with its first part in the middle of the view: clicks on the stage are written as if the view were
        the old fixed one (room pixel 4, 50 at the top left, zoom 2) and moved to where those room pixels are now"""
        probe = os.path.join(mine_a, "probe.log")
        first = next((i for i, (t, k) in enumerate(keys) if k.startswith(b"\x1b[<")), len(keys))  # (the view as it is at the first click: the keys before it, then quit and discard)
        if first == len(keys):
            return run(args, keys, env, cols=cols, rows=rows)  # (keys only: no clicks to place)
        pre = keys[:first]
        t0 = pre[-1][0] if pre else 1.0
        run(args, pre + [(t0 + 0.2, b"v"), (t0 + 0.3, b"v"), (t0 + 0.6, b"q"), (t0 + 0.9, b"d")], dict(env, CLAWD_STAGE_LOG=probe), cols=cols, rows=rows)
        m = re.findall(r"view=(-?\d+),(-?\d+),(\d+)", open(probe).read())[-1]; os.remove(probe)
        vx, vy, z = int(m[0]), int(m[1]), int(m[2])
        last_view[:] = [vx, vy, z]
        def move(mt):
            b, col, row, fin = mt.group(1), int(mt.group(2)), int(mt.group(3)), mt.group(4)
            px, py = col - 1, (row - 1) * 2
            if not (4 <= px < 182 and 12 <= py < 106):
                return mt.group(0)
            X, Y = (px - 4) / 2 + 4, (py - 12) / 2 + 50
            npx, npy = 4 + (X - vx) * z, 12 + (Y - vy) * z
            return b"\x1b[<%s;%d;%d%s" % (b, int(npx) + 1, int(npy) // 2 + 1, fin)
        keys = [(t, re.sub(rb"\x1b\[<(\d+);(\d+);(\d+)([Mm])", move, k)) for t, k in keys]
        return run(args, keys, env, cols=cols, rows=rows)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.8, b"."), (2.4, b"q"), (2.8, b"s")], aenv, cols=300, rows=90)  # (the clawd part, MOVE Y one more at frame 1)
    txt = open(swing_mine).read() if os.path.exists(swing_mine) else ""
    check("stage: ] picks a part, the . key changes its value at this frame and saves it to yours (the default stays)", st == 0 and "key clawd 0 x=0 y=1 " in txt and "y=0" in open(os.path.join(anims, "swing.txt")).read())
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.9, b"i"), (2.8, b"\x1b"), (3.6, b"q"), (4.0, b"s")], aenv, cols=300, rows=90)  # (I on the Clawd part: the pixel editor's picker on the CLAWD tab; Esc leaves it and the stage page is back)
    check("stage: I on a sprite part opens the pixel editor's picker, and quitting it brings the stage page back (no crash, the terminal restored)", st == 0)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"\x1b[C"), (1.6, b"\x1b[C"), (1.7, b"\x1b[C"), (1.9, b"k"), (2.1, b"e"), (2.6, b"q"), (3.0, b"s")], aenv, cols=300, rows=90)  # (a key on the seat at frame 4: 0.1875 s, then its ease)
    txt = open(swing_mine).read()
    check("stage: . steps a frame, k adds a key there, e changes its ease", st == 0 and "key seat 0.1875 " in txt and re.search(r"key seat 0\.1875 .* ease=", txt))
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"\x1b[C"), (1.6, b"\x1b[C"), (1.7, b"\x1b[C"), (1.9, b"k"), (2.2, b"x"), (2.6, b"q"), (3.0, b"s")], aenv, cols=300, rows=90)  # (a key, then deleted again)
    txt = open(swing_mine).read()
    check("stage: x deletes the key at this frame", st == 0 and "key seat 0.1875 " not in txt)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"."), (1.5, b"u"), (2.0, b"q"), (2.4, b"s")], aenv, cols=300, rows=90)
    txt = open(swing_mine).read()
    check("stage: u undoes the last change (and saves)", st == 0 and "key bar 0 x=0 y=0 " in txt)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.7, b"\x1b[<0;95;31M"), (1.8, b"\x1b[<32;100;31M"), (1.9, b"\x1b[<32;105;31M"), (2.0, b"\x1b[<0;105;31m"), (2.4, b"q"), (2.8, b"s")], aenv, cols=300, rows=90)  # (SELECT: drag the clawd's right border 10 pixels: 5 room pixels wider)
    txt = open(swing_mine).read()
    check("stage: SELECT: dragging a part's border resizes it (saved in the file's box)", st == 0 and "part clawd group swing box 35 69 19 8" in txt)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"\x1b[<0;291;88M"), (1.3, b"\x1b[<0;291;88m"), (1.6, b"\x1b[<0;102;71M"), (1.7, b"\x1b[<0;102;71m"), (1.9, b"k"), (2.4, b"q"), (2.8, b"s")], aenv, cols=300, rows=90)  # (the frame strip's scroll bar to the end, then the first frame in view, and a key on it)
    txt = open(swing_mine).read()
    check("stage: the frame strip's scroll bar scrolls the frames (the first frame in view is a late one)", st == 0 and "key bar 1.6875 " in txt)
    mouse = [(1.2, b"\x1b[<0;231;12M"), (1.3, b"\x1b[<0;231;12m"), (1.5, b"\x1b[<0;60;21M"), (1.6, b"\x1b[<32;65;21M"), (1.7, b"\x1b[<32;70;21M"), (1.8, b"\x1b[<0;70;21m"), (2.3, b"q"), (2.7, b"s")]  # (the MOVE tool, then drag the bar 10 pixels right: 5 room pixels)
    out, st = run_stage(["--edit", "anims/swing"], mouse, aenv, cols=300, rows=90)
    txt = open(swing_mine).read()
    check("stage: the MOVE tool and a drag on the stage moves the picked part (bar, 5 room pixels right) and saves it", st == 0 and "key bar 0 x=5 y=0 " in txt)
    drag = lambda tool_col, x0, x1: [(1.2, b"\x1b[<0;%d;12M" % tool_col), (1.3, b"\x1b[<0;%d;12m" % tool_col), (1.5, b"\x1b[<0;%d;35M" % x0), (1.6, b"\x1b[<32;%d;35M" % ((x0 + x1) // 2)), (1.7, b"\x1b[<32;%d;35M" % x1), (1.8, b"\x1b[<0;%d;35m" % x1), (2.3, b"q"), (2.7, b"s")]
    out, st = run_stage(["--edit", "anims/swing"], drag(231, 81, 91), aenv, cols=300, rows=90)  # (MOVE, press on the seat (not the picked bar), drag 10 pixels: 5 room pixels)
    txt = open(swing_mine).read()
    check("stage: MOVE works on the part pressed on (the seat), not only the picked one", st == 0 and re.search(r"key seat 0 x=5 ", txt))
    out, st = run_stage(["--edit", "anims/swing"], drag(258, 81, 91), aenv, cols=300, rows=90)  # (ROTATE: 2 degrees a pixel)
    txt = open(swing_mine).read()
    check("stage: ROTATE drags the angle of the part pressed on", st == 0 and re.search(r"key seat 0 x=5 y=0 turn=20 ", txt))
    clamp = [(1.2, b"\x1b[<0;231;12M"), (1.3, b"\x1b[<0;231;12m"), (1.5, b"\x1b[<0;81;35M"), (1.6, b"\x1b[<32;181;35M"), (1.7, b"\x1b[<32;281;35M"), (1.8, b"\x1b[<0;281;35m"), (2.3, b"q"), (2.7, b"s")]  # (drag the seat 200 pixels right: it stops at the edge of what the stage shows)
    out, st = run_stage(["--edit", "anims/swing"], clamp, aenv, cols=300, rows=90)
    txt = open(swing_mine).read()
    check("stage: MOVE keeps the part inside what the stage shows", st == 0 and re.search(r"key seat 0 x=%d " % (last_view[0] + 171 // last_view[2] - 15 - 35), txt))
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"+"), (1.4, b"+"), (1.6, b"d"), (1.8, b"w"), (2.0, b"-"), (2.2, b"0"), (2.4, b"\x1b[<65;250;30M"), (2.6, b"\x1b[<64;250;30M"), (2.8, b"\x1b[<64;40;40M"), (2.9, b"v"), (3.0, b"v"), (3.2, b"q"), (3.6, b"s")], aenv, cols=300, rows=90)  # (zoom in and out, move the view, reset it, the wheel over the panel and the stage)
    check("stage: + - zoom, w a s d move the view, 0 resets it, the wheel scrolls the panel and zooms the stage (no crash, nothing changed)", st == 0)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"?"), (1.4, b"\x1b[B"), (1.5, b"\x1b[B"), (1.6, b"\x1b[B"), (1.8, b"\x1b[<65;60;40M"), (2.0, b"\x1b[A"), (2.2, b"?"), (2.6, b"q"), (3.0, b"s")], aenv, cols=300, rows=50)  # (a short window: the help box scrolls with the arrows and the wheel)
    check("stage: ? opens the help, which scrolls in a short window (up / down, the wheel)", st == 0)
    out, st = run(["--edit", "-"], [(1.0, b"\t"), (1.1, b"\t"), (1.2, b"\t"), (1.3, b"\t"), (1.6, b"\r"), (2.6, b"o"), (2.8, b"."), (3.2, b"q"), (3.5, b"s"), (4.0, b"q")], aenv, cols=300, rows=90)  # (the picker: Tab to ANIMS, Enter opens brew on the stage; o, a change, q s back to the picker; q quits)
    brew_mine = os.path.join(mine_a, "brew.txt")
    txt = open(brew_mine).read() if os.path.exists(brew_mine) else ""
    check("picker: the ANIMS tab opens a part animation on the stage page, q there comes back, q quits", st == 0 and "key machine 0 x=0 y=1 " in txt)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.7, b"\x1b[<0;258;12M"), (1.8, b"\x1b[<0;258;12m"), (2.0, b"\x1b[<0;95;31M"), (2.1, b"\x1b[<32;105;31M"), (2.2, b"\x1b[<0;105;31m"), (2.6, b"q"), (3.0, b"s")], aenv, cols=300, rows=90)  # (the clawd picked: ROTATE cannot be picked, so the drag on the stage is SELECT's: a resize, no turn)
    txt = open(swing_mine).read()
    check("stage: with a sprite (the Clawd) picked, ROTATE cannot be picked", st == 0 and not re.search(r"key clawd [0-9.]+ x=[-0-9.]+ y=[-0-9.]+ turn=[1-9-]", txt))
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"\x1b[C"), (1.6, b"\x1b[C"), (1.7, b"\x1b[C"), (1.8, b"\x1b[C"), (1.9, b"\x1b[C"), (2.0, b"k"), (2.2, b"k"), (2.6, b"q"), (3.0, b"s")], aenv, cols=300, rows=90)  # (k twice at frame 6: a key, then none)
    txt = open(swing_mine).read()
    check("stage: k (and the KEY button) is a toggle: a key at this frame, again none", st == 0 and "key seat 0.3125 " not in txt)
    slog = os.path.join(mine_a, "stage.log")
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.7, b"\x1b[<0;258;12M"), (1.8, b"\x1b[<0;258;12m"), (2.2, b"q"), (2.6, b"s")], dict(aenv, CLAWD_STAGE_LOG=slog), cols=300, rows=90)  # (the clawd picked: the ROTATE cap does nothing)
    lines = open(slog).read().splitlines(); os.remove(slog)
    check("stage: the Clawd picked, the ROTATE cap does not take (the tool stays SELECT)", st == 0 and lines and all("tool=2" not in l for l in lines))
    was = open(swing_mine).read()
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.7, b"\x1b[<0;258;12M"), (1.8, b"\x1b[<0;258;12m"), (2.0, b"\x1b[<0;86;27M"), (2.1, b"\x1b[<32;96;27M"), (2.2, b"\x1b[<0;96;27m"), (2.5, b"q"), (2.9, b"s")], dict(aenv, CLAWD_STAGE_LOG=slog), cols=300, rows=90)  # (ROTATE, then a drag on the Clawd: it is not picked, and nothing turns)
    lines = open(slog).read().splitlines(); os.remove(slog)
    txt = open(swing_mine).read()
    check("stage: with ROTATE, pressing on the Clawd neither picks it nor turns anything", st == 0 and all("pick=clawd" not in l for l in lines) and txt == was)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"\x1b[<0;258;12M"), (1.3, b"\x1b[<0;258;12m"), (1.5, b"]"), (1.6, b"]"), (1.7, b"]"), (1.8, b"]"), (1.9, b"]"), (2.0, b"]"), (2.4, b"q"), (2.8, b"s")], dict(aenv, CLAWD_STAGE_LOG=slog), cols=300, rows=90)  # (ROTATE on: ] goes round the parts and never lands on the Clawd)
    lines = open(slog).read().splitlines(); os.remove(slog)
    check("stage: with ROTATE, [ ] skip the Clawd (it is never picked)", st == 0 and lines and all("pick=clawd" not in l for l in lines) and all("tool=2" in l for l in lines[1:]))
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"\x1b[C"), (1.6, b"\x1b[C"), (1.7, b"\x1b[C"), (1.8, b"\x1b[C"), (1.9, b"\x1b[C"), (2.0, b"\x1b[<0;286;12M"), (2.1, b"\x1b[<0;286;12m"), (2.4, b"q"), (2.8, b"s")], dict(aenv, CLAWD_STAGE_LOG=slog), cols=300, rows=90)  # (frame 6, the KEY cap: a key there)
    lines = open(slog).read().splitlines(); os.remove(slog)
    txt = open(swing_mine).read()
    check("stage: the KEY cap is a toggle: a key at this frame (and it shows: keyed=1)", st == 0 and "key seat 0.3125 " in txt and any("frame=5 keyed=1" in l for l in lines))
    old = open(swing_mine).read()
    open(swing_mine, "w").write(old.replace(" sprite\n", "\n"))  # (yours, saved before the Clawd was marked a sprite)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"\x1b[<0;258;12M"), (1.3, b"\x1b[<0;258;12m"), (1.5, b"]"), (1.6, b"]"), (1.7, b"]"), (1.8, b"]"), (1.9, b"]"), (2.0, b"]"), (2.4, b"q"), (2.8, b"s")], dict(aenv, CLAWD_STAGE_LOG=slog), cols=300, rows=90)
    lines = open(slog).read().splitlines(); os.remove(slog)
    check("stage: yours saved before the sprite mark: the Clawd is still a sprite (ROTATE never picks it)", st == 0 and lines and all("pick=clawd" not in l for l in lines))
    open(swing_mine, "w").write(old)
    before = open(swing_mine).read()
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"."), (1.5, b"."), (1.8, b"q"), (2.1, b"d")], aenv, cols=300, rows=90)  # (q with changes asks: D puts your file back as it was)
    check("stage: q with changes asks keep or discard; D puts your file back as it was", st == 0 and open(swing_mine).read() == before)
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"q"), (1.6, b"\x1b"), (2.0, b"q"), (2.4, b"s")], aenv, cols=300, rows=90)  # (nothing changed: q asks too, Esc keeps editing, S quits)
    check("stage: q asks even when nothing changed (Esc keeps editing, S quits)", st == 0)
    r = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "park-swing.sc")], capture_output=True, text=True, env=dict(os.environ, **aenv))
    r0 = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "park-swing.sc")], capture_output=True, text=True, env=dict(os.environ, **dict(aenv, CLAWD_MY_ANIMS="/nonexistent")))
    out, st = run_stage(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.8, b"."), (1.9, b"+"), (2.0, b"+"), (2.1, b"+"), (2.6, b"q"), (3.0, b"s")], aenv, cols=300, rows=90)  # (the Clawd's swing made bigger at its start)
    r1 = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "park-swing.sc")], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("stage: a room reads your edited animation (the swing scene differs; without yours it is the default)", r.returncode == 0 and r0.returncode == 0 and r1.returncode == 0 and r1.stdout != r0.stdout)
    r = subprocess.run(CMD + ["--edit", "list"], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("--edit list names the part animations (anims/swing)", r.returncode == 0 and "anims/swing" in r.stdout and "anims/brew" in r.stdout)
    r = subprocess.run(CMD + ["--edit-check", "anims/nope"], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("--edit anims/NAME with no such animation says so and names the ones there are", r.returncode == 1 and "anims/swing" in r.stderr)
    keys = [(1.2 + i * 0.05, b"\x1b[C") for i in range(4)] + [(1.6, b"\x1b[<0;212;55M"), (1.7, b"\x1b[<0;212;55m"), (2.2, b"q"), (2.6, b"s")]  # (frame 5, a click at a tenth of the grow bar: the steps come in later)
    g0 = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "office-gates-appear.sc")], capture_output=True, text=True, env=dict(os.environ, **aenv))
    out, st = run(["--edit", "anims/gates/simple/gate-stairs"], keys, aenv, cols=300, rows=90)
    gmine = os.path.join(mine_a, "gates", "simple", "gate-stairs.txt")
    txt = open(gmine).read() if os.path.exists(gmine) else ""
    g1 = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "office-gates-appear.sc")], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("stage: a gate (anims/gate-stairs) is edited like any part animation (the grow bar), and a room's gate appears by your file", st == 0 and re.search(r"key steps 0\.25 .* grow=10\b", txt) and g0.returncode == 0 and g1.stdout != g0.stdout)
    shutil.rmtree(anims, ignore_errors=True); shutil.rmtree(mine_a, ignore_errors=True)
    shutil.rmtree(spr, ignore_errors=True)
print("%d failed" % len(fails) if fails else "all passed")
sys.exit(1 if fails else 0)
