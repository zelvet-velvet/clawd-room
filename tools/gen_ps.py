#!/usr/bin/env python3
"""Writes c/ps_scripts.h from the PowerShell scripts in windetect.py, so the C program runs the same ones.

    python3 tools/gen_ps.py          rewrite the header
    python3 tools/gen_ps.py --check  exit 1 if the header is out of date (make test does this)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import windetect  # noqa: E402

OUT = os.path.join(HERE, "..", "c", "ps_scripts.h")


def literal(name, text):
    assert all(ord(c) < 128 for c in text), "the scripts must be ASCII"
    lines = ["/* generated from windetect.py by tools/gen_ps.py: do not edit */", "static const char *%s =" % name]
    for line in text.split("\n"):
        esc = line.replace("\\", "\\\\").replace('"', '\\"')
        lines.append('    "%s\\n"' % esc)
    lines[-1] += ";"
    return "\n".join(lines) + "\n"


def build():
    return "#ifndef PS_SCRIPTS_H\n#define PS_SCRIPTS_H\n" + literal("PS_WATCH", windetect._PS) + "\n" + literal("PS_TOOL", windetect._TOOL_PS) + "#endif\n"


if __name__ == "__main__":
    text = build()
    if "--check" in sys.argv:
        sys.exit(0 if os.path.exists(OUT) and open(OUT).read() == text else 1)
    open(OUT, "w").write(text)
    print("wrote", OUT)
