---
title: clawd-room - LLM Guide
date: 2026-10-16
tags:
  - llm
  - guide
  - clawd-room
sticker: lucide//bot
---
# 🤖 LLM Guide

Read this first if you are an AI assistant (Claude Code or similar) working on this repo. It is a short map; the details live in the notes it links to.

---
## 🧭 What this is

**clawd-room** is a pixel room where every running **Claude Code** session is a little Clawd (the Claude Code mascot). `hook.py` appends one JSON line per Claude Code event to `~/.clawd-room/events.jsonl`; `room_tui.py` follows that file and animates one character per session in the terminal (half-block truecolor). Several windows can be linked (`--room NAME`): Clawds walk through doors, ladders and hatches into the window next to them. Python 3, **standard library only**, used on **WSL2 + Windows Terminal**; the window helpers call `powershell.exe`.

**The events file is about the user's real work.** It holds only `t, sid, ev, cwd (folder basename), tool`. Never widen that; never print or commit its contents.

---
## 📚 Where to read

| You need | Read |
| --- | --- |
| Commands, keys | `README.md` |
| What was built, current phase, pending | [[Dev Log]] |
| Every animation frame | [[Animations]] (edit `sprites/*.txt`, `python3 sprite_tool.py build` redraws the pictures) |
| Behaviour details | the code (docstrings and constants at the top of each file) and `git log` |

---
## 🗂️ Map

```
hook.py             Claude Code hook: one JSON line per event -> ~/.clawd-room/events.jsonl (mode 600)
install_hooks.py    prints / applies / removes the hook in ~/.claude/settings.json (prints by default)
clawd-room.sh       the launcher the profile calls: C program if built, else room_tui.py (CLAWD_ENGINE or ~/.clawd-room/engine override)
c/                  the C port of the terminal room (`make` in c/, `make test` compares it with Python): see the C port section of [[Dev Log]]
sprites.py          loads / saves the editable sprite files, writes GIFs (sprites/clawd.txt, sprites/items.txt)
sprite_tool.py      `export` code drawing -> sprites/*.txt, `build` sheet + GIFs into Clawd Room Note/Animations/
simulate.py         fake sessions: live, or --fixture TS --out FILE for a static case
room.py + run.sh    the original pygame-ce window (needs .venv). Reference for the state machine; leave alone
room_tui.py         the terminal room: state machine, sprites, layout, gates, main loop, keys
themes.py           office / coffee shop / park: background, furniture, roles, playground spots
roomlink.py         presence + inbox files between windows (~/.clawd-room/link/)
windetect.py        powershell.exe helpers: window rectangles, resize/zoom, Fitter
wt_settings.py      print-only: Windows Terminal settings that hide the tab bar / scroll bar
setup_profile.py    prints / writes (--apply) / removes (--remove) a "Clawd Room" Windows Terminal profile fragment (scroll bar hidden, generated pixel Clawd-in-a-room `.ico`; the 512 px picture is `clawd-room-512px.png` as tab icon); `--apply-global` edits settings.json (tab bar off, with backup)
Clawd Room Note/    this Obsidian vault (open this folder in Obsidian; plugin code is not in git)
CLAUDE.md           points here
```

Test hooks (environment): `CLAWD_EVENTS` (events file), `CLAWD_LINK_DIR` (link folder), `CLAWD_SIZE_LOG` (logs terminal size and gate positions to a file).

---
## ⚠️ Rules that prevent breakage

- **Privacy of the events file.** Keep `{"t","sid","ev","cwd","tool"}`. No prompts, tool input/output or full paths, anywhere (hook, link messages, logs). Link messages carry a type and small values only.
- **Standard library only.** No pip installs, no venv needed for `room_tui.py`. Only `room.py` uses pygame (in `.venv`).
- **No network, no background daemons.** Writes go only to the project folder and `~/.clawd-room/`; the one exception is `setup_profile.py --apply`, which the user runs by hand and which writes a single fragment file under Windows Terminal's `Fragments\clawd-room\` (`settings.json` only via the explicit `--apply-global`, with a `.clawd-room.bak` backup). The `powershell.exe` children belong to the room and must be stopped when it exits.
- **Do not edit `~/.claude/settings.json` yourself.** Tell the user to run `python3 install_hooks.py` (prints the change); `--apply` only if they say so. The installed hook calls `python3 ~/clawd-room/hook.py`, so the **symlink `~/clawd-room` -> the Desktop folder must stay**.
- **Two engines, one behaviour.** `room_tui.py` is the reference; the C version in `c/` must match it (`make -C c test`). A behaviour change goes into both, with the same random calls in the same order, and the golden tools (`tools/golden_*.py`, fixtures, scenarios) extended if needed. Launchers call `clawd-room.sh`, never a program directly.
- **Leave `room.py` alone** (reference only). Keep the state machine and its timings unless asked: 90 s sleep, 30 min stale, 2.5 s cheer, 0.8 s hold-working.
- **Terminal hygiene.** Alternate screen, hidden cursor, one string written per frame, diff updates; restore the terminal on exit, Ctrl-C and exceptions.
- **Windows are never moved to attach them** (the user decided). Gates (doors, ladder hatches) line up on screen instead. Do not add window snapping or following again.
- **Windows helpers (WSL only).** Only look at windows whose title starts with `clawd-room `. Be DPI aware (real pixels). Send the font-size keys only to a window that already has the focus, and never type into another window. Windows Terminal's focus mode (command palette) dropped the window in tests: do not use it.
- **The Clawd looks official**: flat `#CD6E58`, 14x8 sprite (8-wide torso, 2x2 arms, four 1-pixel legs), 1x1 eyes, no mouth. Draw gates at the size they need in the window with the bigger cells, so both ends match on screen.
- **Constants with a reason** (change only on request): attach gap 30 px (stays until 45), gates wait 1 s for still windows, a door needs 14 room pixels of shared floor, a hatch 21 room pixels of overlap, slots 30-62 px wide and 40 tall, minimum terminal 44x20, 16 FPS.
- **Do not hurt the user's running windows.** When testing with real Windows Terminal windows, create your own (names like `alpha` / `beta`), find them by exact title or pid, and never resize or kill the user's `park` / `coffee` windows. Do not use `pkill -f` with a pattern that also matches your own command line.
- **Testing needs the user's eyes** for the real windows. Say what could not be tested and what the user must look at.

---
## 🧪 How to test

There is no test runner in the repo. What works:

1. `python3 room_tui.py --dump 100x30 --events FILE --at TS` prints one plain-text frame. `python3 simulate.py --fixture 1800000000 --out /tmp/e.jsonl` writes a static case for it.
2. Render frames to PNG with a small script that imports `room_tui` (`render(cols, rows, room, now, t)` returns a canvas of RGB tuples) and look at them.
3. Run `room_tui.py` inside a pty (`pty.fork`) for a few seconds: it must start, draw, answer `p`, `?`, `q`, and restore the terminal. Feed a real event through `hook.py` with `CLAWD_EVENTS` set.
4. Two linked rooms: start two pty processes with `--room a` and `--room b` and `CLAWD_LINK_DIR` set to a temp folder; check presence/inbox files and the messages. For crossing without real windows, give the two `Room` objects each other's `outbox` messages in a loop.
5. Real windows: `python3 windetect.py` lists the `clawd-room` windows; the geometry tests used a throwaway helper to place windows. New Windows Terminal windows may open minimized.

---
## 🔁 Workflow

1. Read [[Dev Log]] and the code you are about to change before editing.
2. Make the smallest change that does the job; match the surrounding style (small functions, a short comment per non-obvious step, no new dependencies).
3. Test as above, then say plainly what works, what could not be tested and what you were unsure about.
4. Record it: add a dated entry to [[Dev Log]]; update `README.md` when commands or keys change.
5. **Commit and push every change** on a feature branch, following [[#🌿 Regulation: repo management]].

---
## 🌿 Regulation: repo management

The repo is private on GitHub (`zelvet-velvet/clawd-room`). History starts clean (2026-10-16): the first commit is `chore: start clean history`. The old repo, with the original pygame prototype and the branches `feature/terminal-ui` / `feature/room-link`, is archived (read-only, private) as `zelvet-velvet/clawd-room-archive`; look there only for old history. New work goes on branches off `main`.

**Branches**
- `main` is the stable line. Do not commit to it directly; it only receives merges.
- Work on a branch named `type/short-slug`, for example `feat/seesaw`, `fix/gate-offset`. 

**Commit messages:** `type(scope): summary`, lowercase, imperative, no trailing period.
- `type` is one of `feat`, `fix`, `refactor`, `chore`, `docs`.
- `scope` is the area touched: `room`, `link`, `gates`, `themes`, `detect`, `hook`, `vault`, `docs`...
- Example: `fix(gates): wait one second for still windows before showing a gate`.
- End each commit message with the co-author line from the session's attribution rules.
- Git identity: name `Zaviel`, email `you@example.com` (set in the repo, not globally).

**Every change = commit + push** on the current feature branch, without asking; never force-push (the history was restarted once, on purpose, in a new repo).

**Merging to `main`**
- When a feature seems done, **ask the user** whether to open a PR or merge. Never merge without a yes.
- Merge with `--no-ff` so each feature shows as one branch: `git merge --no-ff <branch> -m "type(scope): summary"`, then push `main`. Do not chain a merge and a branch delete in one command.
- After a merge, delete the merged branch (local and remote).
- Unrelated things go on a new branch.

---
## 📜 Regulation: `CMD_SYNC`

When the user sends **`CMD_SYNC`**, update the notes for every feature or change added since the last sync:

1. Find what changed: `git status`, `git log` since the previous `docs(vault): sync` commit, and the conversation so far.
2. Add or update the entry for each change in [[Dev Log]]: what it does, decisions, and a `[!TODO] Test` checklist of what the user still has to check by eye. Bug fixes go into the related entry.
3. Keep constants in this guide in step with the code.
4. Update `README.md` if commands, flags or keys changed, and this guide if a rule changed.
5. Do not mark anything as confirmed; only the user's yes does that.
6. Commit and push as `docs(vault): sync`, then finish with a short list of what was added.

---
## 🗣️ Conventions

- Call the user **Zaviel**. Reply in the language they write in; the notes stay English.
- Pixel text is upper case only (the 3x5 font has no lower case); keep UI strings short.
- Times in the events file are epoch seconds; `--at TS` pretends "now" is TS.
- Terminal sizes are `COLSxROWS`; a cell is 1 room pixel wide and 2 high, so room pixels are square on screen.
- Prefer a named constant at the top of `room_tui.py` over a magic number.
