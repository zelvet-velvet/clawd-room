---
title: clawd-room - LLM Guide
date: 2026-10-16
tags:
  - llm
  - guide
  - clawd-room
sticker: lucide//bot
---
The map of the code and the vault, and how notes are written. The always-loaded rules and the command table are in `AGENTS.md`; each command, git, mockups and testing are skills in `.claude/skills/` (loaded only when used). Read only the section you need.

---
## 🧭 What this is

**clawd-room** is a pixel room where every running **Claude Code** session is a little Clawd (the Claude Code mascot). `hook.py` appends one JSON line per Claude Code event to `~/.clawd-room/events.jsonl`; the C program in `c/` follows that file and animates one character per session in the terminal (half-block truecolor). Several windows can be linked (`--room NAME`): Clawds walk through doors, ladders and hatches into the window next to them. The room is **C11 with no libraries**; Python 3 (standard library only) is used for the hook, the installer, the simulator and the profile tools. Used on **WSL2 + Windows Terminal** (window helpers call `powershell.exe`) and **macOS + iTerm2** (osascript; a "Clawd Room" dynamic profile with a fixed 4x8 pt cell).

**The events file is about the user's real work.** It holds only `t, sid, ev, cwd (folder basename), tool`. Never widen that; never print or commit its contents.

---
## 📚 Where to read

| You need | Read |
| --- | --- |
| What it is, how to start | `README.md` |
| Commands, flags, keys, platforms, build and tests | `docs/` ([[Usage]] is `docs/Usage.md`; also `Linked-Rooms.md`, `Platforms.md`, `Development.md`) |
| How contributors work together (branches, PRs, the vault) | `CONTRIBUTING.md`, [[Collaboration]] |
| Where to start, what is where in the vault | [[Home]] |
| Where things stand: today, ongoing | [[Dev Status]]; work to pick up and ideas: [[Task Queue]] (finished: [[Progress Archive]]) |
| What changed and when (one note per day, with pictures) | [[Dev Log]] (index) → `Review/Log/YYYY/MM/Www/YYYY-MM-DD` |
| The look: rooms, workspaces, gates, signs, art rules, layout numbers | [[Rooms and Art]] |
| Keys, camera, room size, linked windows, crossing, sessions | [[Interaction]] |
| The pixel editor, the sprite picker, the stage page | [[Pixel Editor]] |
| WSL / Windows Terminal, macOS / iTerm2, window helpers | [[Platforms and Windows]] |
| The code's layout, tests, random numbers | [[Engine and Tests]] |
| What is still to check by eye | [[Test Checklist]] (done groups: [[Test Checklist Archive]]) |
| Every animation frame | [[Animations]] (edit `sprites/*.txt`, `clawd-room --build-art` redraws the pictures) |
| Pictures and animations to browse | [[Index\|Gallery index]] |
| Commands (`/cmd-*`), git, mockups, testing | `AGENTS.md` (always loaded) → the skills in `.claude/skills/` (`cmd-*` commands, `room-*` task rules) |
| Exact behaviour | the code (comments and constants at the top of each file) and `git log` |

---
## 🗂️ Map

```
hook.py             Claude Code hook: one JSON line per event -> ~/.clawd-room/events.jsonl (mode 600)
install_hooks.py    prints / applies / removes the hook in ~/.claude/settings.json (prints by default)
clawd-room.sh       the launcher the profile calls: builds c/ when needed, then runs c/clawd-room
c/                  the room, in C: canvas (+ font_data.h, the 3x5 pixel font), sprites, clawd (poses), persp (the perspective), theme (the rooms, workspaces, gates, signs), draw (layout, frame, camera view, debug overlay), scene (items on layers, overlap measuring), place (the placer), anim (eased values), editor (the pixel editor), gallery (--check / --gallery), room (state machine, visits, moves), camera, rng, tail (events), link (windows), win (window helpers: PowerShell on WSL, osascript on macOS iTerm2; scripts in ps_scripts.h / mac_scripts.h), ui (bottom bar, menus, sizes), app (main loop), fixture (test scenes), art (sprite files, PNG, GIF)
c/tests/expected/   snapshots the tests compare with (from the C program since style A, 2026-10-18)
sprites/            the default Clawd frames, items and props (text grids; reloaded while the room runs; edit with --edit); ui.txt: the editor's own icons. A person's own edits go to ~/.clawd-room/sprites/ (only what differs), shown over these; MAKE DEFAULT in the picker writes them here
rooms/              wall plans per room (what the placer puts on the walls; reloaded while the room runs)
gallery/            not in git: contact sheets (make -C c gallery) and gallery/mock/ (mockup programs; their pictures in out/NN-slug/ with out/README.md; a test worktree wt/)
tools/              dev_status.py (Dev Status counts and rollover), git_graph.py (the Dev Log's git graph: `--write` it again after a merge), test.sh (snapshots), fixtures/ + scenarios/ (test scenes), pty_smoke.py, link_smoke.py, move_smoke.py, split_smoke.py, win_smoke.py, place_windows.py (test helper for your own windows), demo_events.py + try_room.sh (--try)
simulate.py         fake sessions: live, or --fixture TS --out FILE for a static case
room.py + run.sh    the original pygame-ce window (needs .venv). Leave alone
wt_settings.py      print-only: Windows Terminal settings that hide the tab bar / scroll bar
setup_profile.py    prints / writes (--apply) / removes (--remove) a "Clawd Room" Windows Terminal profile fragment (scroll bar hidden, generated pixel Clawd-in-a-room `.ico`; the 512 px picture is `clawd-room-512px.png` as tab icon); `--apply-global` edits settings.json (tab bar off, with backup)
Clawd Room Note/    this Obsidian vault (open this folder in Obsidian; plugin code is not in git):
                      Start/   Home (where to begin), this guide, Collaboration
                      Review/  Dev Status (today, ongoing), Progress Archive, Dev Log (index of days), Log/YYYY/MM/Www/YYYY-MM-DD (one note per day), Task Queue, Test Checklist (+ Archive)
                      Design/  how it works now: Rooms and Art, Interaction, Pixel Editor, Art Data and Editor, Platforms and Windows, Engine and Tests, Engine Design, Animations
                      Gallery/ generated by tools/vault_gallery.py, never edited by hand: Index, and a folder per category (Rooms, Gates and moving, Tools and platforms, Style) with a note per subject (not the repo's scratch gallery/)
                      Assets/  files only: Snapshots/<subject>/ (pictures), Animations/ (GIFs and sheets from --build-art)
                      _Meta/   vault helpers: Checkbox Style, Snapshot status, Templates/ (Day note)
AGENTS.md           the agent instructions (always loaded); CLAUDE.md imports it
.claude/skills/     the project skills: cmd-sync, cmd-journal, cmd-rollover, cmd-notes-sync, room-git, room-mockups, room-testing, room-test-queue
```

---
## 🪝 Hooks

- **Do not edit `~/.claude/settings.json` yourself.** Tell the user to run `python3 install_hooks.py` (prints the change); `--apply` only if they say so. The installed hook calls `hook.py` by the folder's full path **on that machine**, so the repo must not move afterwards. The path is per machine and per clone: never write one into the repo, the notes or a memory (they go stale on the next device), and do not make a symlink just to keep an old path alive. Work in the folder you are in; `install_hooks.py` takes its path from where it runs.
  - **If the repo moved, was renamed or re-cloned**, the old hook path stays in `~/.claude/settings.json` and then **blocks every prompt and tool call** (`UserPromptSubmit operation blocked by hook: python3 <old path>/hook.py: can't open file`). `install_hooks.py --remove` only takes out the entry for the *current* path, so it does not clear it. Tell the user to fix it in a plain terminal (not in Claude Code, which is blocked): remove every hook whose command is `python3 <old path>/hook.py` from `~/.claude/settings.json` by hand (back it up first), then run `python3 install_hooks.py` from the new folder and `--apply` if the printout looks right. If you cannot get to a terminal, a symlink from the old path to the new folder is a temporary way back in; remove it once the hook is fixed.
  - **Before moving, renaming or deleting the folder** the hook points to, run `python3 install_hooks.py --remove` first, then `--apply` again from the new place. Re-cloned or moved without that: see the fix above.

---
## 🧱 Code rules

- **Before editing**: read the code you change and the day notes about it. Make the smallest change that does the job; match the surrounding style (small functions, a short comment per non-obvious step, a named constant at the top of the file over a magic number). Update `README.md` / `docs/` when commands or keys change.
- **Terminal hygiene.** Alternate screen, hidden cursor, one string written per frame, diff updates; restore the terminal on exit, Ctrl-C and exceptions.
- **Leave `room.py` alone** (reference only). Keep the state machine and its timings unless asked: 90 s sleep, 30 min stale, 2.5 s cheer, 0.8 s hold-working.
- **Constants with a reason** (change only on request): attach gap 30 px (stays until 45), gates wait 1 s for still windows, a door needs 14 room pixels of shared floor, a hatch 21 room pixels of overlap, workspaces 24-40 px wide in rows 15 px apart, minimum terminal 44x20, 16 FPS; the rest are listed in [[Rooms and Art]] and [[Interaction]].
- **Windows are never moved to attach them** (the user decided). Gates sit on the room's walls, ceiling and floor (style A), so the two ends need not line up on screen. Do not add window snapping or following again.

---
## 🗣️ Conventions

- Call the user by the name in `git config user.name`. Reply in the language they write in; the notes stay English.
- **Notes:** no `# Title` heading inside a note (Obsidian shows the file name as its title); each note in `Design/`, `Review/` and `Gallery/` has a make.md `sticker:` in its frontmatter (a lucide icon, `lucide//palette`). Never write a note named like its folder (make.md's folder notes).
- **Callouts before tables** (Zaviel, 2026-10-30): items that each carry a status or a short detail (tasks, test groups, findings, open points, per-area summaries) go in callouts, one per group. A table is only for a real grid read across columns (constants, key and command tables, colour palettes, [[Snapshot status]]).
- **The callout type is the block's state** (Zaviel, 2026-10-30): a block with a state shows it in its type, changed in the same edit as its boxes or status line. The types follow the boxes of [[Checkbox Style]], so a box and its block say the same thing:

| Boxes | State | Callout type |
|---|---|---|
| `[ ]` `[/]` `[>]` | open, partly done, in progress or later | `[!todo]` |
| `[x]` | done | `[!done]` |
| `[!]` | blocked or waiting for someone (name who or what); a test that fails | `[!warning]` |
| `[?]` | waiting for a decision | `[!question]` |
| `[-]` | failed or dropped (say why) | `[!failure]` |
| `[i]` or no boxes | no state | `[!note]` `[!info]` `[!abstract]` `[!example]` `[!quote]` `[!tip]` |

  - **From the boxes:** every box `[x]` (or `[x]` and `[-]`) → `[!done]`; every box `[-]` → `[!failure]`; otherwise any `[!]` → `[!warning]`, else any `[?]` → `[!question]`, else `[!todo]`. Callouts nested inside (the folded `[!tip]-` steps) have no state.
  - **Keep it in sync:** ticking the last open box turns `[!todo]` into `[!done]`; adding an open item to a `[!done]` block turns it back to `[!todo]`. Keep the fold sign (`-` / `+`) and the title. Only a person ticks tests, but whoever edits a block's boxes also fixes its type. **Test queue groups** in [[Test Checklist]]: `[!todo]` while open, `[!done]` once the branch is merged and every test is ticked.
- **Pictures in notes** (outside a callout; `![[name.png|WIDTH]]`, the width in pixels of a note about 774 wide): **one picture on a line: `|774`**, the full width. **Two pictures on one line: half each, `|380`**, written as `![[a.png|380]] ![[b.png|380]]` on the same line. Never wider than the picture's own width (small pictures stay at their own size; a row of three or more goes one per line). Pictures inside a callout stay small (about 250–300). Check the width of a new picture before embedding it.
- Pixel text is upper case only (the 3x5 font has no lower case); keep UI strings short.
- Times in the events file are epoch seconds; `--at TS` pretends "now" is TS.
- Terminal sizes are `COLSxROWS`; a cell is 1 room pixel wide and 2 high, so room pixels are square on screen.
- Prefer a named constant at the top of a C file over a magic number.
