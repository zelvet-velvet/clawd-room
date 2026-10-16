"""Editable pixel sprites for Clawd and the items it carries, kept in plain text files (sprites/*.txt).

A file is a legend plus named grids, one character per pixel:

    : B = body            (the Clawd body colour; follows dimming)
    : K = 20,20,30        (any other character = an RGB colour)
    . is always empty
    @ idle-1 | arms=0,0 legs=0 eyes=open look=0,0 lean=0
    ......BBBBBBBB......
    ...

The text after `|` is the key the room matches a frame by. A frame that is not in the file is still drawn by
the code, so the files may hold only some frames. Standard library only (also writes GIFs, see write_gif).
"""
import os
import struct

TRANSPARENT = (1, 2, 3)  # background of the canvas a frame is exported from
HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, "sprites")
_POOL = "KPGWYEcstmzdyhvxqrnajkuo"  # characters handed out for new colours


class Sprite:
    def __init__(self, name, key, rows):
        self.name, self.key, self.rows = name, key, rows  # rows: lists of None, "body" or an RGB tuple
        self.h, self.w = len(rows), max((len(r) for r in rows), default=0)


class SpriteFile:
    def __init__(self, path):
        self.path, self.by_name, self.by_key, self.order = path, {}, {}, []
        if os.path.exists(path):
            self._parse(open(path, encoding="utf-8").read())

    def get(self, key):
        return self.by_key.get(key)

    def _parse(self, text):
        legend, cur = {".": None}, None
        for line in text.splitlines():
            if line.startswith(":"):
                ch, val = [p.strip() for p in line[1:].split("=", 1)]
                legend[ch] = "body" if val.split()[0] == "body" else tuple(int(v) for v in val.split()[0].split(","))
            elif line.startswith("@"):
                head = line[1:].split("|", 1)
                cur = Sprite(head[0].strip(), head[1].strip() if len(head) > 1 else head[0].strip(), [])
                self.by_name[cur.name] = cur
                self.by_key[cur.key] = cur
                self.order.append(cur.name)
            elif line.startswith("#") or not line.strip():
                cur = cur if not line.strip() == "" else None
            elif cur is not None:
                cur.rows.append([legend.get(c) for c in line.rstrip()])
        for s in self.by_name.values():
            s.h, s.w = len(s.rows), max((len(r) for r in s.rows), default=0)


def save(path, frames, body_rgb, note="", known=None):
    """frames: [(name, key, rows)], rows of RGB tuples / TRANSPARENT. Writes the legend and grids."""
    chars, pool = {body_rgb: "B"}, [c for c in _POOL if c not in (known or {}).values()]
    chars.update(known or {})
    for _, _, rows in frames:
        for row in rows:
            for c in row:
                if c != TRANSPARENT and c not in chars:
                    chars[c] = pool.pop(0)
    used = {c for _, _, rows in frames for row in rows for c in row}
    chars = {c: ch for c, ch in chars.items() if c in used or c == body_rgb}
    out = ["# " + l for l in note.splitlines()] + [": B = body"]
    out += [": %s = %d,%d,%d" % (ch, *c) for c, ch in chars.items() if c != body_rgb]
    for name, key, rows in frames:
        out += ["", "@ %s | %s" % (name, key)]
        out += ["".join("." if c == TRANSPARENT else chars[c] for c in row) for row in rows]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")


# ---- GIF ---------------------------------------------------------------------------------
def _lzw(indices):
    clear, eoi = 256, 257
    out, buf, nbits = bytearray(), 0, 0
    state = {"buf": 0, "n": 0}

    def emit(code, size):
        state["buf"] |= code << state["n"]
        state["n"] += size
        while state["n"] >= 8:
            out.append(state["buf"] & 255)
            state["buf"] >>= 8
            state["n"] -= 8
    table, nxt, size = {}, 258, 9
    emit(clear, size)
    prefix = indices[0]
    for k in indices[1:]:
        if (prefix, k) in table:
            prefix = table[(prefix, k)]
            continue
        emit(prefix, size)
        table[(prefix, k)] = nxt
        nxt += 1
        if nxt - 1 == (1 << size) and size < 12:
            size += 1
        if nxt >= 4095:
            emit(clear, size)
            table, nxt, size = {}, 258, 9
        prefix = k
    emit(prefix, size)
    emit(eoi, size)
    if state["n"]:
        out.append(state["buf"] & 255)
    return bytes(out)


def write_gif(path, frames, delay_cs=8):
    """frames: list of (w, h, rows of RGB tuples), all the same size. Looping GIF."""
    w, h = frames[0][0], frames[0][1]
    colors = {}
    for _, _, rows in frames:
        for row in rows:
            for c in row:
                colors.setdefault(c, len(colors))
    if len(colors) > 256:
        raise ValueError("more than 256 colours")
    bits = max(1, (len(colors) - 1).bit_length())
    pal = [c for c, _ in sorted(colors.items(), key=lambda kv: kv[1])]
    pal += [(0, 0, 0)] * ((1 << bits) - len(pal))
    out = bytearray(b"GIF89a" + struct.pack("<HHBBB", w, h, 0x80 | (bits - 1), 0, 0))
    out += b"".join(bytes(c) for c in pal)
    out += b"\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00"  # loop forever
    for _, _, rows in frames:
        out += b"\x21\xf9\x04\x00" + struct.pack("<H", delay_cs) + b"\x00\x00"
        out += b"\x2c" + struct.pack("<HHHHB", 0, 0, w, h, 0)
        out += b"\x08"
        data = _lzw([colors[c] for row in rows for c in row])
        for i in range(0, len(data), 255):
            chunk = data[i:i + 255]
            out += bytes([len(chunk)]) + chunk
        out += b"\x00"
    out += b";"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)
