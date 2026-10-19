#!/usr/bin/env python3
"""Real Windows Terminal windows (your own throwaway ones, titled alpha / beta): open two linked rooms, check each is sized to
110x32 once, put beta next to alpha, and read where the gates were placed from the size log.

    python3 tools/win_smoke.py [gap_px]

Needs Windows Terminal with the "Clawd Room" profile (setup_profile.py --apply and --apply-global). Never touches other windows.
"""
import json
import os
import subprocess
import sys
import tempfile
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

gap = int(sys.argv[1]) if len(sys.argv) > 1 else 10
HOME_ROOT = ROOT  # this repo
PROG = [HOME_ROOT + "/c/clawd-room"]


def windows():
    """{title: (l, t, r, b)} of the clawd-room windows, from the program's own listing."""
    out = subprocess.run(PROG + ["--list-windows"], capture_output=True, text=True).stdout
    found = {}
    for line in out.splitlines():
        if ": " in line and line.startswith("clawd-room "):
            title, rest = line.split(": ", 1)
            found[title] = tuple(int(v) for v in rest.split("(")[0].split())
    return found


with tempfile.TemporaryDirectory(dir="/tmp") as tmp:
    events = os.path.join(tmp, "events.jsonl")
    with open(events, "w") as f:
        now = time.time()
        for i in range(6):
            f.write(json.dumps({"t": now - 5, "sid": "w%d" % i, "ev": "SessionStart", "cwd": "proj%d" % i, "tool": ""}) + "\n")
    logs = {}
    for name in ("alpha", "beta"):
        logs[name] = os.path.join(tmp, name + ".log")
        open(logs[name], "w").close()
        cmd = ["wt.exe", "-w", "new", "-p", "Clawd Room", "wsl.exe", "-d", "Ubuntu", "-e", "env", "CLAWD_SIZE_LOG=" + logs[name], "CLAWD_LINK_DIR=" + tmp + "/link"] + PROG + [
            "--room", name, "--profile-font", "8", "--events", events]
        subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.0)
    time.sleep(6)
    ws = windows()
    mine = {k: v for k, v in ws.items() if k.startswith("clawd-room alpha-") or k.startswith("clawd-room beta-")}
    print("windows:", mine)
    ta = next((t for t in mine if "alpha" in t), None)
    tb = next((t for t in mine if "beta" in t), None)
    try:
        if not (ta and tb):
            raise SystemExit("both windows did not open")
        a, b = mine[ta], mine[tb]
        w, h = a[2] - a[0], a[3] - a[1]
        ax, ay = 60, 80
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "place_windows.py"), ta, str(ax), str(ay), str(w), str(h),
                        tb, str(ax + w + gap), str(ay + 20), str(w), str(h)], capture_output=True)
        time.sleep(7)
        for name in ("alpha", "beta"):
            print("---", name)
            print(open(logs[name]).read().strip())
    finally:
        subprocess.run(["pkill", "-f", tmp], capture_output=True)
        time.sleep(1.5)
