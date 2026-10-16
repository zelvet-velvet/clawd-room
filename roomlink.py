#!/usr/bin/env python3
"""Link between clawd-room windows on the same machine (standard library only).

Every window ("room") that starts with --room NAME announces itself with a small presence file in
~/.clawd-room/link/ and listens on its own inbox file. Other rooms see the presence file, and
send messages by appending one JSON line to that inbox. No network, no background process: it is
just files in a private folder (mode 700, files mode 600).

Messages are tiny and carry no prompts or tool data: {"t", "from", "to", "type", "data"}.

Try it in two terminals:   python3 roomlink.py park     and     python3 roomlink.py coffee
"""
import argparse
import json
import os
import re
import time

LINK_DIR = os.environ.get("CLAWD_LINK_DIR", os.path.expanduser("~/.clawd-room/link"))
BEAT_EVERY = 1.0  # a room rewrites its presence file this often
SCAN_EVERY = 0.5  # and looks for other rooms this often
ALIVE_FOR = 4.0  # a room whose presence file is older than this is gone
TYPES = {"hello", "bye", "ping", "pong", "walk", "recall"}  # walk: a Clawd crosses over; recall: come home
ID_RE = re.compile(r"^[a-z0-9_-]{1,24}-[0-9]{1,10}$")  # room id = name-pid, safe to use in a path
MAX_LINE = 2000


def clean_name(name):
    """Lower case letters, digits, - and _ only, so the name can be part of a file name."""
    return re.sub(r"[^a-z0-9_-]", "", str(name).lower())[:24] or "room"


def pid_alive(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except OSError:  # exists but belongs to someone else
        return True
    return True


class Link:
    def __init__(self, name, directory=None):
        self.name = clean_name(name)
        self.dir = directory or LINK_DIR
        self.id = "%s-%d" % (self.name, os.getpid())
        self.peer_map = {}  # id -> presence dict of the other rooms that are alive
        self.pos = 0  # how much of our inbox we have read
        self.buf = b""
        self.last_beat = self.last_scan = 0.0
        self.theme = ""  # shown to the other rooms so a door can look like the room behind it
        self.band = None  # [top, bottom] screen pixels where our Clawds walk, so doors can line up
        self.grid = None  # [ox, oy, cw, ch, k, x0, y0]: where our cells are on screen, so gates can line up

    # ---- files ---------------------------------------------------------------------------
    def _path(self, room_id, kind):
        return os.path.join(self.dir, "%s.%s" % (room_id, kind))

    def start(self):
        os.makedirs(self.dir, mode=0o700, exist_ok=True)
        os.close(os.open(self._path(self.id, "inbox"), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600))
        self.pos, self.buf = 0, b""
        self.beat(time.time(), 0, 0, force=True)

    def beat(self, now, cols, rows, force=False):
        """Rewrite our presence file (atomically, so nobody reads half of it)."""
        if not force and now - self.last_beat < BEAT_EVERY:
            return
        self.last_beat = now
        rec = {"id": self.id, "name": self.name, "pid": os.getpid(), "t": now, "cols": cols, "rows": rows,
               "theme": self.theme, "band": self.band, "grid": self.grid}
        tmp = self._path(self.id, "tmp")
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        try:
            os.write(fd, json.dumps(rec).encode())
        finally:
            os.close(fd)
        os.replace(tmp, self._path(self.id, "peer"))

    def close(self):
        for pid_id in list(self.peer_map):
            self.send(pid_id, "bye")
        for kind in ("peer", "inbox", "tmp"):
            try:
                os.remove(self._path(self.id, kind))
            except OSError:
                pass

    # ---- other rooms -----------------------------------------------------------------------
    def scan(self, now):
        """Look at the presence files; returns (joined, left) lists of presence dicts."""
        found = {}
        try:
            names = os.listdir(self.dir)
        except OSError:
            names = []
        for fname in names:
            if not fname.endswith(".peer"):
                continue
            rid = fname[:-5]
            if rid == self.id or not ID_RE.match(rid):
                continue
            try:
                with open(os.path.join(self.dir, fname), "rb") as f:
                    rec = json.loads(f.read(4096))
                alive = now - float(rec["t"]) < ALIVE_FOR and pid_alive(int(rec["pid"]))
                if rec["id"] != rid:
                    continue
            except (OSError, ValueError, KeyError, TypeError):
                continue
            if alive:
                found[rid] = {"id": rid, "name": clean_name(rec.get("name", "")),
                              "cols": int(rec.get("cols") or 0), "rows": int(rec.get("rows") or 0),
                              "theme": clean_name(rec.get("theme", "")), "band": self._band(rec.get("band")),
                              "grid": self._grid(rec.get("grid"))}
            elif not pid_alive(int(rec.get("pid") or 0)):  # tidy up after a room that crashed
                for kind in ("peer", "inbox"):
                    try:
                        os.remove(self._path(rid, kind))
                    except OSError:
                        pass
        joined = [p for rid, p in found.items() if rid not in self.peer_map]
        left = [p for rid, p in self.peer_map.items() if rid not in found]
        self.peer_map = found
        return joined, left

    @staticmethod
    def _grid(v):
        try:
            return tuple(float(x) for x in v) if len(v) == 7 else None
        except (TypeError, ValueError):
            return None

    @staticmethod
    def _band(v):
        try:
            return (int(v[0]), int(v[1])) if len(v) == 2 else None
        except (TypeError, ValueError):
            return None

    @property
    def peers(self):
        return sorted(self.peer_map.values(), key=lambda p: p["id"])

    # ---- messages ----------------------------------------------------------------------------
    def send(self, to, mtype, **data):
        """Append one message to another room's inbox. False if that room is gone."""
        if not ID_RE.match(to) or mtype not in TYPES:
            raise ValueError("bad room id or message type")
        line = json.dumps({"t": time.time(), "from": self.id, "to": to, "type": mtype, "data": data}) + "\n"
        if len(line) > MAX_LINE:
            raise ValueError("message too long")
        try:
            fd = os.open(self._path(to, "inbox"), os.O_WRONLY | os.O_APPEND)  # not created: peer must exist
        except OSError:
            return False
        try:
            os.write(fd, line.encode())  # one small write = one whole line
        finally:
            os.close(fd)
        return True

    def receive(self):
        """New messages in our inbox; bad lines and messages for someone else are dropped."""
        path = self._path(self.id, "inbox")
        try:
            size = os.path.getsize(path)
            if size < self.pos:
                self.pos, self.buf = 0, b""
            if size == self.pos:
                return []
            with open(path, "rb") as f:
                f.seek(self.pos)
                data = f.read()
        except OSError:
            return []
        self.pos += len(data)
        *lines, self.buf = (self.buf + data).split(b"\n")
        return [m for m in map(self._parse, lines) if m]

    def _parse(self, line):
        try:
            m = json.loads(line)
            ok = (m["type"] in TYPES and ID_RE.match(m["from"]) and m["to"] == self.id
                  and isinstance(m["data"], dict))
            if not ok:
                return None
            data = {str(k)[:16]: v for k, v in m["data"].items() if isinstance(v, (str, int, float, bool))}
            return {"t": float(m["t"]), "from": m["from"], "type": m["type"], "data": data}
        except (ValueError, KeyError, TypeError, AttributeError):
            return None

    # ---- one call per frame --------------------------------------------------------------
    def update(self, now, cols=0, rows=0):
        """Keep our presence fresh, notice rooms coming and going, say hello to new ones.

        Returns a list of events: ("joined", peer), ("left", peer) and ("msg", message).
        """
        self.beat(now, cols, rows)
        events = []
        if now - self.last_scan >= SCAN_EVERY:
            self.last_scan = now
            joined, left = self.scan(now)
            for p in joined:
                self.send(p["id"], "hello", room=self.name)
                events.append(("joined", p))
            events += [("left", p) for p in left]
        events += [("msg", m) for m in self.receive()]
        return events


def demo(name, seconds):
    """Tiny command line demo: ping every new room, answer pings, print what happens."""
    link = Link(name)
    link.start()
    print("room %s is up (id %s); looking for other rooms..." % (link.name, link.id), flush=True)
    end = time.time() + seconds if seconds else None
    try:
        while end is None or time.time() < end:
            now = time.time()
            for kind, x in link.update(now):
                if kind == "joined":
                    print("joined:", x["name"], flush=True)
                    link.send(x["id"], "ping", sent=now)
                elif kind == "left":
                    print("left:", x["name"], flush=True)
                elif x["type"] == "ping":
                    link.send(x["from"], "pong", sent=x["data"].get("sent", 0))
                elif x["type"] == "pong":
                    print("pong from %s after %.1f ms" % (x["from"], (time.time() - x["data"].get("sent", now)) * 1000), flush=True)
                else:
                    print("got", x["type"], "from", x["from"], flush=True)
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="clawd-room link demo")
    ap.add_argument("name", help="name of this room, for example park")
    ap.add_argument("--seconds", type=float, default=0, help="stop after this long (default: until Ctrl-C)")
    a = ap.parse_args()
    demo(a.name, a.seconds)
