#!/usr/bin/env python3
"""Add (or remove) a "Clawd Room" Windows Terminal profile with a hidden scroll bar, no padding and a pixel-Clawd-in-a-room tab icon.

  python3 setup_profile.py            # print what would be written, change nothing
  python3 setup_profile.py --apply    # write the fragment file (settings.json is never touched)
  python3 setup_profile.py --remove   # delete the fragment file again
  python3 setup_profile.py --update   # add the profile entry to settings.json if it is missing (5 pt font, shows up at once, no restart) or point it at clawd-room.sh, and rewrite the fragment; nothing else in settings.json changes (backup first)
  python3 setup_profile.py --apply-global   # also hide the tab bar and give the profile a 5 pt font: edits settings.json (after a backup copy)

Windows Terminal reads extra profiles from  %LOCALAPPDATA%\\Microsoft\\Windows Terminal\\Fragments\\clawd-room\\.
Restart Windows Terminal afterwards. A fragment cannot set global options, so the tab bar still needs
the two lines `python3 wt_settings.py` prints, or `--apply-global` (the only thing here that edits settings.json).
"""
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

NAME = "Clawd Room"
FONT_SIZE = 5  # the profile opens with this font size (the room's default font, c/app.c), so nothing has to zoom
GUID = "{c1a3d0f2-7b64-4e1a-9a5c-2f6d8e4b0c13}"
HERE = os.path.dirname(os.path.abspath(__file__))
LAUNCHER = os.path.join(HERE, "clawd-room.sh")  # builds and starts the C program (see clawd-room.sh)


def win_path(env_var):
    out = subprocess.run(["cmd.exe", "/c", "echo", "%" + env_var + "%"], capture_output=True,
                         text=True, timeout=20, cwd="/mnt/c").stdout.strip()
    if not out or "%" in out:
        raise OSError(env_var + " not found")
    return subprocess.run(["wslpath", "-u", out], capture_output=True, text=True, timeout=10).stdout.strip()


def fragment_dir():
    return os.path.join(win_path("LOCALAPPDATA"), "Microsoft", "Windows Terminal", "Fragments", "clawd-room")


GLOBAL_KEYS = (("alwaysShowTabs", "false"), ("showTabsInTitlebar", "false"))


def settings_file():
    pkgs = os.path.join(win_path("LOCALAPPDATA"), "Packages")
    for d in sorted(os.listdir(pkgs)):
        if d.startswith("Microsoft.WindowsTerminal") and "Preview" not in d:
            return os.path.join(pkgs, d, "LocalState", "settings.json")
    raise OSError("Windows Terminal settings.json not found")


def patch_settings(text):
    """Set the global keys by plain text edit (settings.json allows comments, so it is not parsed)."""
    for key, val in GLOBAL_KEYS:
        pat = re.compile(r'^(\s*"%s"\s*:\s*)[^,\n/]*' % key, re.M)
        if pat.search(text):
            text = pat.sub(lambda m: m.group(1) + val, text, count=1)
        else:
            m = re.search(r"\{[ \t]*\r?\n", text)
            if not m:
                raise OSError("settings.json has no top-level '{' followed by a newline")
            text = text[:m.end()] + '    "%s": %s,\n' % (key, val) + text[m.end():]
    return text


def profile_stub():
    """The user-level entry for our profile. A fragment's own font is overridden by the user's profiles.defaults
    font, so the 5 pt font (and the matching --profile-font) has to be set here, in settings.json."""
    distro = os.environ.get("WSL_DISTRO_NAME")
    if not distro:
        raise OSError("WSL_DISTRO_NAME is not set; run this from inside WSL")
    return {"guid": GUID, "source": "clawd-room", "name": NAME, "hidden": False, "font": {"size": FONT_SIZE},
            "commandline": "wsl.exe -d %s -e %s --profile-font %d"
                           % (distro, LAUNCHER, FONT_SIZE)}


def object_around(text, at):
    """(start, end) of the innermost { ... } holding position `at`, skipping strings and // or /* */ comments
    (settings.json allows them). Windows Terminal saves a profile's keys in alphabetical order, so "commandline"
    comes before "guid": a search must cover the whole profile, never just the text after the guid."""
    opens, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
        elif text.startswith("//", i):
            i = text.find("\n", i)
            i = n if i < 0 else i
        elif text.startswith("/*", i):
            i = text.find("*/", i)
            i = n if i < 0 else i + 1
        elif c == "{":
            opens.append(i)
        elif c == "}" and opens:
            start = opens.pop()
            if start < at < i:
                return start, i + 1
        i += 1
    raise OSError("settings.json: the Clawd Room profile's { ... } was not found")


def patch_profile(text):
    """Add the profile entry at the start of "profiles"."list"; if it is there already, point its commandline at the launcher and set its font size."""
    if GUID in text:
        start, end = object_around(text, text.index(GUID))
        m = re.compile(r'("commandline"\s*:\s*)"(?:[^"\\]|\\.)*"').search(text, start, end)
        if not m:
            return text
        cmd = json.dumps(profile_stub()["commandline"])
        text = text[:m.start()] + m.group(1) + cmd + text[m.end():]
        end += len(cmd) - (m.end() - m.start() - len(m.group(1)))  # (the profile's end moved)
        f = re.compile(r'("font"\s*:\s*\{[^}]*?"size"\s*:\s*)[\d.]+').search(text, start, end)  # and its font size
        return text[:f.start()] + f.group(1) + str(FONT_SIZE) + text[f.end():] if f else text
    m = re.search(r'"list"\s*:\s*\[[ \t]*\r?\n', text)
    if not m:
        raise OSError('no "profiles": {"list": [ ... found in settings.json')
    entry = json.dumps(profile_stub(), indent=4)
    entry = "\n".join("            " + line for line in entry.splitlines())
    return text[:m.end()] + entry + ",\n" + text[m.end():]


def apply_global(only_profile=False):
    path = settings_file()
    with open(path, encoding="utf-8-sig") as f:
        old = f.read()
    new = patch_profile(old if only_profile else patch_settings(old))
    if new == old:
        print("settings.json is already set up:", path)
        return
    backup = path + ".clawd-room.bak"
    shutil.copy2(path, backup)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(new)
    print("updated", path)
    print("backup ", backup, "(copy it back to undo)")


# The icon: pixel Clawd (the Clawd sprite, 14 x 8, each pixel 2 x 2 cells) standing in a little room,
# on a 32 x 32 grid. Drawn 16 px per cell it is the 512 px picture; smaller sizes are whole multiples.
GRID = 32
BODY = (205, 110, 88, 255)
INK = (20, 20, 28, 255)
WALL = (46, 49, 64, 255)
WALL2 = (38, 41, 52, 255)
FLOOR = (150, 104, 70, 255)
FLOOR2 = (128, 88, 58, 255)
TRIM = (230, 200, 120, 255)
SKY = (120, 170, 220, 255)


def sprite():
    grid = [[WALL if (x + y) % 2 else WALL2 for x in range(GRID)] for y in range(GRID)]

    def rect(x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                grid[yy][xx] = c
    for y in range(22, GRID):  # floor boards
        for x in range(GRID):
            grid[y][x] = FLOOR if (x // 4 + (y - 22) // 3) % 2 else FLOOR2
    rect(0, 21, GRID, 1, TRIM)  # skirting board
    rect(12, 1, 8, 4, TRIM)  # window frame, sky and cross bars
    rect(13, 2, 6, 2, SKY)
    rect(15, 2, 2, 2, TRIM)
    ox, oy = 2, 6  # Clawd's top-left, in cells
    for lx in (3, 5, 8, 10):
        rect(ox + lx * 2, oy + 12, 2, 4, BODY)
    rect(ox + 6, oy, 16, 12, BODY)
    rect(ox + 2, oy + 4, 4, 4, BODY)
    rect(ox + 22, oy + 4, 4, 4, BODY)
    rect(ox + 8, oy + 2, 2, 2, INK)
    rect(ox + 18, oy + 2, 2, 2, INK)
    return grid


def png(grid, k):
    n = GRID * k
    raw = b"".join(b"\0" + b"".join(bytes(c) * k for c in row) for row in grid for _ in range(k))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def icon_bytes():
    """A .ico holding 32, 64, 128 and 256 px PNGs of the picture (nearest-neighbour, so it stays crisp)."""
    grid = sprite()
    images = [(GRID * k, png(grid, k)) for k in (1, 2, 4, 8)]
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for size, data in images:
        out += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    return out + b"".join(d for _, d in images)


def fragment(icon):
    distro = os.environ.get("WSL_DISTRO_NAME")
    if not distro:
        raise OSError("WSL_DISTRO_NAME is not set; run this from inside WSL")
    return {"profiles": [{
        "guid": GUID,
        "name": NAME,
        "commandline": "wsl.exe -d %s -e %s" % (distro, LAUNCHER),
        "scrollbarState": "hidden",
        "padding": "0",
        "icon": icon,
        "hidden": False,
    }]}


def main():
    arg = sys.argv[1] if len(sys.argv) > 1 else ""
    if arg not in ("", "--apply", "--remove", "--apply-global", "--update"):
        sys.exit(__doc__)
    if arg == "--update":  # the profile entry in settings.json (added if missing, repointed if not), then the fragment like --apply
        try:
            apply_global(only_profile=True)
        except (OSError, subprocess.SubprocessError) as e:
            sys.exit("cannot edit settings.json: %s" % e)
        arg = "--apply"
    if arg == "--apply-global":
        try:
            apply_global()
        except (OSError, subprocess.SubprocessError) as e:
            sys.exit("cannot edit settings.json: %s" % e)
        return
    try:
        path = os.path.join(fragment_dir(), "clawd-room.json")
        if arg == "--remove":
            if os.path.exists(path):
                os.remove(path)
                if os.path.exists(os.path.join(os.path.dirname(path), "clawd-room.ico")):
                    os.remove(os.path.join(os.path.dirname(path), "clawd-room.ico"))
                try:
                    os.rmdir(os.path.dirname(path))
                except OSError:
                    pass
                print("removed", path)
            else:
                print("nothing to remove at", path)
            return
        icon_path = os.path.join(os.path.dirname(path), "clawd-room.ico")
        icon_win = subprocess.run(["wslpath", "-w", icon_path], capture_output=True, text=True,
                                  timeout=10).stdout.strip()
        text = json.dumps(fragment(icon_win), indent=2) + "\n"
    except (OSError, subprocess.SubprocessError) as e:
        sys.exit("cannot set up the profile: %s" % e)
    print("file:", path)
    print(text)
    if arg == "--apply":
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)
        with open(icon_path, "wb") as f:
            f.write(icon_bytes())
        print("written. Restart Windows Terminal, then open the '%s' profile (it asks which scene), or from a shell:" % NAME)
    else:
        print("(not written; add --apply). After applying, start a window with:")
    print('  wt.exe -w new --size 60,18 -p "%s" --title "clawd-room park" wsl.exe -e %s --room park --profile-font %d'
          % (NAME, LAUNCHER, FONT_SIZE))


if __name__ == "__main__":
    main()
