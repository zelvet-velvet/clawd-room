#!/usr/bin/env python3
"""Redraws the README's editor GIFs: the program runs in a pty with a window of a fixed size, keys are sent at given times and the editor
records its own pages (CLAWD_EDIT_GIF). Standard library only.

    python3 tools/readme/make_editor_gifs.py [themes stage walk]
"""
import fcntl
import os
import pty
import select
import shutil
import struct
import sys
import tempfile
import termios
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "docs", "images")
RIGHT, LEFT, TAB = b"\x1b[C", b"\x1b[D", b"\t"

SHOTS = {
    # name: (arguments, window columns x rows, GIF scale, [(seconds, keys)], keys that end it)
    "editor-themes": (["--edit", "-"], (374, 72), 3,
                      [(1.0, TAB), (1.5, TAB), (2.0, TAB), (2.5, TAB), (3.0, TAB), (3.8, RIGHT), (5.0, RIGHT), (6.2, LEFT), (7.4, LEFT)], [b"q"]),
    "editor-stage": (["--edit", "anims/gates/construct/gate-stairs"], (300, 90), 3,
                     [(1.5 + i * 0.18, RIGHT) for i in range(24)], [b"q", b"s"]),
    "editor-walk": (["--edit", "walk"], (230, 92), 3, [(1.2, b" "), (4.8, b" ")], [b"q", b"q"]),
}


def record(name, args, size, scale, keys, quit_keys):
    work = tempfile.mkdtemp(prefix="clawd-readme-")
    settings = os.path.join(work, "settings.conf")
    open(settings, "w").write("")
    env = dict(os.environ, CLAWD_HERE="1", CLAWD_SPRITES=os.path.join(ROOT, "sprites"), CLAWD_MY_SPRITES=os.path.join(work, "sprites"),
               CLAWD_MY_ANIMS=os.path.join(work, "anims"), CLAWD_SETTINGS=settings, CLAWD_GATE_THEME="simple",
               CLAWD_EDIT_GIF=os.path.join(OUT, name + ".gif"), CLAWD_EDIT_GIF_SCALE=str(scale))
    pid, fd = pty.fork()
    if pid == 0:
        os.execvpe(os.path.join(ROOT, "c", "clawd-room"), ["clawd-room"] + args, env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", size[1], size[0], 0, 0))
    t0, i, end = time.time(), 0, keys[-1][0] + 1.0
    while time.time() - t0 < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                os.read(fd, 65536)
            except OSError:
                break
        while i < len(keys) and time.time() - t0 >= keys[i][0]:
            os.write(fd, keys[i][1])
            i += 1
    for k in quit_keys:
        os.write(fd, k)
        time.sleep(0.5)
    t1 = time.time()
    while time.time() - t1 < 60:  # (it writes the GIF as it ends)
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                os.read(fd, 65536)
            except OSError:
                pass
        done, _ = os.waitpid(pid, os.WNOHANG)
        if done:
            break
    else:
        os.kill(pid, 9)
    shutil.rmtree(work, ignore_errors=True)
    path = os.path.join(OUT, name + ".gif")
    print(name, os.path.getsize(path) if os.path.exists(path) else "NOT WRITTEN")


if __name__ == "__main__":
    want = sys.argv[1:] or [k.replace("editor-", "") for k in SHOTS]
    for key, (args, size, scale, keys, quit_keys) in SHOTS.items():
        if key.replace("editor-", "") in want:
            record(key, args, size, scale, keys, quit_keys)
