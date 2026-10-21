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
    for f in os.listdir(os.path.join(ROOT, "anims")):
        shutil.copy(os.path.join(ROOT, "anims", f), anims)
    aenv = dict(env, CLAWD_ANIMS=anims, CLAWD_MY_ANIMS=mine_a)
    swing_mine = os.path.join(mine_a, "swing.txt")
    out, st = run(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.8, b"+"), (2.4, b"q")], aenv, cols=300, rows=90)  # (the clawd part, MOVE Y one more at frame 1)
    txt = open(swing_mine).read() if os.path.exists(swing_mine) else ""
    check("stage: ] picks a part, + changes its value at this frame and saves it to yours (the default stays)", st == 0 and "key clawd 0 x=0 y=1 " in txt and "y=0" in open(os.path.join(anims, "swing.txt")).read())
    out, st = run(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"."), (1.6, b"."), (1.7, b"."), (1.9, b"k"), (2.1, b"e"), (2.6, b"q")], aenv, cols=300, rows=90)  # (a key on the seat at frame 4: 0.1875 s, then its ease)
    txt = open(swing_mine).read()
    check("stage: . steps a frame, k adds a key there, e changes its ease", st == 0 and "key seat 0.1875 " in txt and re.search(r"key seat 0\.1875 .* ease=", txt))
    out, st = run(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"."), (1.6, b"."), (1.7, b"."), (1.9, b"k"), (2.2, b"x"), (2.6, b"q")], aenv, cols=300, rows=90)  # (a key, then deleted again)
    txt = open(swing_mine).read()
    check("stage: x deletes the key at this frame", st == 0 and "key seat 0.1875 " not in txt)
    out, st = run(["--edit", "anims/swing"], [(1.2, b"+"), (1.5, b"u"), (2.0, b"q")], aenv, cols=300, rows=90)
    txt = open(swing_mine).read()
    check("stage: u undoes the last change (and saves)", st == 0 and "key bar 0 x=0 y=0 " in txt)
    mouse = [(1.2, b"\x1b[<0;235;12M"), (1.3, b"\x1b[<0;235;12m"), (1.5, b"\x1b[<0;41;31M"), (1.6, b"\x1b[<32;46;31M"), (1.7, b"\x1b[<32;51;31M"), (1.8, b"\x1b[<0;51;31m"), (2.3, b"q")]  # (the MOVE tool, then drag on the stage 10 pixels right: 5 room pixels)
    out, st = run(["--edit", "anims/swing"], mouse, aenv, cols=300, rows=90)
    txt = open(swing_mine).read()
    check("stage: the MOVE tool and a drag on the stage moves the picked part (bar, 5 room pixels right) and saves it", st == 0 and "key bar 0 x=5 y=0 " in txt)
    r = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "park-swing.sc")], capture_output=True, text=True, env=dict(os.environ, **aenv))
    r0 = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "park-swing.sc")], capture_output=True, text=True, env=dict(os.environ, **dict(aenv, CLAWD_MY_ANIMS="/nonexistent")))
    out, st = run(["--edit", "anims/swing"], [(1.2, b"]"), (1.3, b"]"), (1.4, b"]"), (1.5, b"]"), (1.8, b"+"), (1.9, b"+"), (2.0, b"+"), (2.1, b"+"), (2.6, b"q")], aenv, cols=300, rows=90)  # (the Clawd's swing made bigger at its start)
    r1 = subprocess.run(CMD + ["--script", os.path.join(ROOT, "tools", "scenarios", "park-swing.sc")], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("stage: a room reads your edited animation (the swing scene differs; without yours it is the default)", r.returncode == 0 and r0.returncode == 0 and r1.returncode == 0 and r1.stdout != r0.stdout)
    r = subprocess.run(CMD + ["--edit", "list"], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("--edit list names the part animations (anims/swing)", r.returncode == 0 and "anims/swing" in r.stdout and "anims/brew" in r.stdout)
    r = subprocess.run(CMD + ["--edit-check", "anims/nope"], capture_output=True, text=True, env=dict(os.environ, **aenv))
    check("--edit anims/NAME with no such animation says so and names the ones there are", r.returncode == 1 and "anims/swing" in r.stderr)
    shutil.rmtree(anims, ignore_errors=True); shutil.rmtree(mine_a, ignore_errors=True)
    shutil.rmtree(spr, ignore_errors=True)
print("%d failed" % len(fails) if fails else "all passed")
sys.exit(1 if fails else 0)
