#!/usr/bin/env python3
"""Add (or remove) the clawd-room hook in ~/.claude/settings.json.

  python3 install_hooks.py            # print the settings change only
  python3 install_hooks.py --apply    # back up settings.json, then merge the hook in
  python3 install_hooks.py --remove   # take our hook out again (other hooks stay)
"""
import json
import os
import shutil
import sys
import time

SETTINGS = os.path.expanduser("~/.claude/settings.json")
HOOK = "python3 " + os.path.join(os.path.dirname(os.path.abspath(__file__)), "hook.py")
EVENTS = ["SessionStart", "SessionEnd", "UserPromptSubmit", "PreToolUse",
          "PostToolUse", "Notification", "Stop", "SubagentStop"]
WITH_MATCHER = {"PreToolUse", "PostToolUse"}


def ours(entry):
    return any(h.get("command") == HOOK for h in entry.get("hooks", []))


def merged(cfg, remove=False):
    hooks = cfg.setdefault("hooks", {})
    for ev in EVENTS:
        lst = [e for e in hooks.get(ev, []) if not ours(e)]
        if not remove:
            entry = {"hooks": [{"type": "command", "command": HOOK}]}
            if ev in WITH_MATCHER:
                entry["matcher"] = "*"
            lst.append(entry)
        if lst:
            hooks[ev] = lst
        else:
            hooks.pop(ev, None)
    if not hooks:
        cfg.pop("hooks")
    return cfg


def main():
    with open(SETTINGS, encoding="utf-8") as f:
        cfg = json.load(f)
    new = merged(json.loads(json.dumps(cfg)), remove="--remove" in sys.argv)
    text = json.dumps(new, indent=2, ensure_ascii=False) + "\n"
    if "--apply" in sys.argv or "--remove" in sys.argv:
        backup = SETTINGS + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
        shutil.copy2(SETTINGS, backup)
        with open(SETTINGS, "w", encoding="utf-8") as f:
            f.write(text)
        print("settings.json updated; backup:", backup)
    else:
        print(json.dumps({"hooks": new.get("hooks", {})}, indent=2))
        print("\n(not written; run with --apply to merge it into", SETTINGS + ")")


if __name__ == "__main__":
    main()
