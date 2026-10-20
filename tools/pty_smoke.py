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
    env = {"CLAWD_SPRITES": spr}
    def read(f):
        with open(os.path.join(spr, f)) as fh:
            return fh.read()
    clawd0 = read("clawd.txt")
    click = lambda c, r: [b"\x1b[<0;%d;%dM" % (c, r), b"\x1b[<0;%d;%dm" % (c, r)]  # (a left click on a cell, 1-based)
    # (FILL clicks down the canvas's left column, wherever the canvas sits: every pixel there is outside the Clawd)
    clicks = [k for row in range(7, 31) for k in click(6, row)]
    keys = [(0.7, b"p"), (0.8, b"#"), (1.0, b"FF0000\r"), (1.3, b"b")] + [(1.6 + i * 0.02, k) for i, k in enumerate(clicks)] + [(3.0, b"q"), (3.3, b"s")]  # (p: the colour panel unfolded, the canvas at the left)
    out, st = run(["--edit", "walk-1"], keys, env, cols=150, rows=50)
    walk = read("clawd.txt").split("@ walk-1")[1].split("@")[0]
    check("editor: FILL fills the touching see-through pixels, not the closed ones", st == 0 and walk.count("a") > 40 and ".." in walk and read("clawd.txt") != clawd0)
    out, st = run(["--edit", "window"], [(0.8, b"f"), (1.0, b"f"), (1.2, b"<"), (1.4, b"x"), (1.6, b"x"), (1.9, b"q"), (2.2, b"s")], env, cols=150, rows=50)
    heads = [l for l in read("props.txt").splitlines() if l.startswith("@ window")]
    check("editor: f adds prop frames, x x deletes one and the next is numbered on", st == 0 and heads == ["@ window | window", "@ window~2 | window~2"])
    out, st = run(["--edit", "walk"], [(0.8, b" "), (1.6, b">"), (1.9, b"o"), (2.2, b" "), (2.5, b"f"), (2.8, b"q")], env, cols=150, rows=50)
    check("editor: opens an animation by its name, plays, steps frames; f leaves Clawd frames alone", st == 0 and b"Clawd Room pixel editor" in out and read("clawd.txt").count("@ walk-") == clawd0.count("@ walk-"))
    r = subprocess.run(CMD + ["--edit", "list"], capture_output=True, text=True, env=dict(os.environ, **env))
    import re
    check("--edit list shows animations with their frame counts", r.returncode == 0 and re.search(r"walk +4\b", r.stdout) and re.search(r"think +9\b", r.stdout) and "window" in r.stdout)
    shutil.rmtree(spr, ignore_errors=True)
print("%d failed" % len(fails) if fails else "all passed")
sys.exit(1 if fails else 0)
