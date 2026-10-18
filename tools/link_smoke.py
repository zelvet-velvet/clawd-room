#!/usr/bin/env python3
"""Two linked rooms in ptys: send a Clawd with the g key from each, call alpha's back with b, and check the walk and recall messages arrive and everything is cleaned up.

    python3 tools/link_smoke.py
"""
import fcntl
import glob
import re
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
PROGS = {"c": [os.path.join(ROOT, "c", "clawd-room")]}


class Proc:
    def __init__(self, kind, args, env):
        self.status_log = tempfile.NamedTemporaryFile(suffix=".status", delete=False).name  # the bar is pixels: its text comes here
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execvpe(PROGS[kind][0], PROGS[kind] + args, dict(os.environ, TERM="xterm-256color", CLAWD_STATUS_LOG=self.status_log, **env))
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 140, 0, 0))
        self.out = b""

    def pump(self, secs):
        end = time.time() + secs
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.out += os.read(self.fd, 65536)
                except OSError:
                    return

    def key(self, k):
        os.write(self.fd, k)

    def read_status(self):
        try:
            with open(self.status_log, "rb") as f:
                self.out += f.read()
            os.unlink(self.status_log)
        except OSError:
            pass

    def stop(self):
        try:
            os.write(self.fd, b"q")
        except OSError:
            pass
        for _ in range(60):
            self.pump(0.05)  # keep reading, or the program blocks writing its last frame
            time.sleep(0.05)
            done, st = os.waitpid(self.pid, os.WNOHANG)
            if done:
                self.read_status()
                return os.WEXITSTATUS(st) if os.WIFEXITED(st) else -1
        os.kill(self.pid, 9)
        os.waitpid(self.pid, 0)
        self.read_status()
        return -9


def pair(kind_a, kind_b):
    with tempfile.TemporaryDirectory() as tmp:
        link = os.path.join(tmp, "link")
        events = os.path.join(tmp, "events.jsonl")
        now = time.time()
        with open(events, "w") as f:
            for i in range(8):
                f.write(json.dumps({"t": now - 5, "sid": "sess%d" % i, "ev": "SessionStart", "cwd": "proj%d" % i, "tool": ""}) + "\n")
        env = {"CLAWD_LINK_DIR": link}
        a = Proc(kind_a, ["--room", "alpha", "--theme", "office", "--right", "beta", "--events", events], env)
        b = Proc(kind_b, ["--room", "beta", "--theme", "coffee", "--left", "alpha", "--events", events], env)
        for _ in range(40):  # let both start, see each other and show their gates
            a.pump(0.1); b.pump(0.1)
        results = {}
        a.key(b"g"); a.pump(0.6)
        b.key(b"g"); b.pump(0.6)
        a.pump(1.0); b.pump(1.0)
        for _ in range(40):  # the Clawds walk to the door and through it
            a.pump(0.1); b.pump(0.1)
        a.key(b"b"); a.pump(0.6); b.pump(0.6)
        inboxes = {}
        for path in glob.glob(os.path.join(link, "*.inbox")):
            lines = [json.loads(x) for x in open(path).read().splitlines() if x.strip()]
            inboxes[os.path.basename(path).split("-")[0]] = lines
        walks = {room: [m for m in lines if m["type"] == "walk"] for room, lines in inboxes.items()}
        hellos = {room: [m for m in lines if m["type"] == "hello"] for room, lines in inboxes.items()}
        recalls = {room: [m for m in lines if m["type"] == "recall"] for room, lines in inboxes.items()}
        sa, sb = a.stop(), b.stop()
        leftovers = glob.glob(os.path.join(link, "*"))
        return walks, hellos, recalls, sa, sb, leftovers, a.out, b.out


if __name__ == "__main__":
    bad = 0
    for ka, kb in (("c", "c"),):
        walks, hellos, recalls, sa, sb, left, oa, ob = pair(ka, kb)
        ok = (sa == 0 and sb == 0 and not left
              and len(hellos.get("alpha", [])) >= 1 and len(hellos.get("beta", [])) >= 1
              and len(walks.get("beta", [])) >= 1 and len(walks.get("alpha", [])) >= 1
              and all(m["data"]["side"] == "left" and m["data"]["sid"].startswith("sess") for m in walks.get("beta", []))
              and all(m["data"]["side"] == "right" and m["data"]["sid"].startswith("sess") for m in walks.get("alpha", []))
              and b"SENDING" in oa and b"SENDING" in ob and b"BETA JOINED" in oa and b"ALPHA JOINED" in ob
              and b"CALLING " in oa and len(recalls.get("beta", [])) >= 1)
        bad += not ok
        print("%-4s alpha=%s beta=%s: hello %s/%s, walk messages %s/%s, recall to beta %s, exit %s/%s, files left %d" % (
            "OK" if ok else "FAIL", ka, kb, len(hellos.get("alpha", [])), len(hellos.get("beta", [])), len(walks.get("alpha", [])), len(walks.get("beta", [])), len(recalls.get("beta", [])), sa, sb, len(left)))
        if not ok:
            print("   alpha out has SENDING:", b"SENDING" in oa, "JOINED:", b"BETA JOINED" in oa, "| beta:", b"SENDING" in ob, b"ALPHA JOINED" in ob)
            print("   alpha b flash:", re.findall(rb"(CALLING[ A-Z0-9]*|SENDING \d VISITOR[ A-Z]*|NOBODY AWAY[ ,A-Z]*)", oa)[:3])
    sys.exit(1 if bad else 0)
