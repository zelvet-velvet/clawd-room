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

**clawd-room** is a pixel room where every running **Claude Code** session is a little Clawd (the Claude Code mascot). `hook.py` appends one JSON line per Claude Code event to `~/.clawd-room/events.jsonl`; the C program in `c/` follows that file and animates one character per session in the terminal (half-block truecolor). Several windows can be linked (`--room NAME`): Clawds walk through doors, ladders and hatches into the window next to them. The room is **C11 with no libraries**; Python 3 (standard library only) is used for the hook, the installer, the simulator and the profile tools. Used on **WSL2 + Windows Terminal** (window helpers call `powershell.exe`) and **macOS + iTerm2** (osascript; a "Clawd Room" dynamic profile with a fixed 4x8 pt cell).

**The events file is about the user's real work.** It holds only `t, sid, ev, cwd (folder basename), tool`. Never widen that; never print or commit its contents.

---
## 📚 Where to read

| You need | Read |
| --- | --- |
| Commands, flags | `README.md` |
| How two people work together (branches, PRs, the vault) | `CONTRIBUTING.md`, [[Collaboration]] |
| What changed and when (short, with pictures), pending, ideas | [[Dev Log]] |
| The look: rooms, workspaces, gates, signs, art rules, layout numbers | [[Rooms and Art]] |
| Keys, camera, room size, linked windows, crossing, sessions | [[Interaction]] |
| WSL / Windows Terminal, macOS / iTerm2, window helpers | [[Platforms and Windows]] |
| The code's layout, tests, random numbers | [[Engine and Tests]] |
| What is still to check by eye | [[Test Checklist]] |
| Every animation frame | [[Animations]] (edit `sprites/*.txt`, `clawd-room --build-art` redraws the pictures) |
| Exact behaviour | the code (comments and constants at the top of each file) and `git log` |

---
## 📍 Where things stand

The current state and the **next steps** are at the bottom of the [[Dev Log]] (⏳ Pending: "Next session"). In short (2026-10-19): style A rooms, the engine (scene, placer, sprite and room files, pixel editor, animator), the macOS iTerm2 setup with `--try`, moving Clawds between rooms (`m`, steps 1 and 2 of 3) and a stable split of the sessions are done on macOS; the next session is on **WSL2**, to bring it up to date there.

---
## 🗂️ Map

```
hook.py             Claude Code hook: one JSON line per event -> ~/.clawd-room/events.jsonl (mode 600)
install_hooks.py    prints / applies / removes the hook in ~/.claude/settings.json (prints by default)
clawd-room.sh       the launcher the profile calls: builds c/ when needed, then runs c/clawd-room
c/                  the room, in C: canvas (+ font_data.h, the 3x5 pixel font), sprites, clawd (poses), persp (the perspective), theme (the rooms, workspaces, gates, signs), draw (layout, frame, camera view, debug overlay), scene (items on layers, overlap measuring), place (the placer), anim (eased values), editor (the pixel editor), gallery (--check / --gallery), room (state machine, visits, moves), camera, rng, tail (events), link (windows), win (window helpers: PowerShell on WSL, osascript on macOS iTerm2; scripts in ps_scripts.h / mac_scripts.h), ui (bottom bar, menus, sizes), app (main loop), fixture (test scenes), art (sprite files, PNG, GIF)
c/tests/expected/   snapshots the tests compare with (from the C program since style A, 2026-10-18)
sprites/            editable Clawd frames, items and props (text grids; reloaded while the room runs; edit with --edit NAME)
rooms/              wall plans per room (what the placer puts on the walls; reloaded while the room runs)
gallery/            not in git: contact sheets (make -C c gallery) and gallery/mock/ (scratch mockup programs, a test worktree wt/)
tools/              test.sh (snapshots), fixtures/ + scenarios/ (test scenes), pty_smoke.py, link_smoke.py, move_smoke.py, split_smoke.py, win_smoke.py, place_windows.py (test helper for your own windows), demo_events.py + try_room.sh (--try)
simulate.py         fake sessions: live, or --fixture TS --out FILE for a static case
room.py + run.sh    the original pygame-ce window (needs .venv). Leave alone
wt_settings.py      print-only: Windows Terminal settings that hide the tab bar / scroll bar
setup_profile.py    prints / writes (--apply) / removes (--remove) a "Clawd Room" Windows Terminal profile fragment (scroll bar hidden, generated pixel Clawd-in-a-room `.ico`; the 512 px picture is `clawd-room-512px.png` as tab icon); `--apply-global` edits settings.json (tab bar off, with backup)
Clawd Room Note/    this Obsidian vault (open this folder in Obsidian; plugin code is not in git):
                      Docs/Dev Log.md (short dated entries) and this guide, Notes/ (one note per subject),
                      Screenshots/ (pictures linked from the log), Animations/ (frames)
CLAUDE.md           points here
```

Test hooks (environment): `CLAWD_EVENTS` (events file), `CLAWD_LINK_DIR` (link folder), `CLAWD_SIZE_LOG` (logs terminal size, gate positions and who the camera follows), `CLAWD_STATUS_LOG` (the bottom bar's text: it is pixels, so the pty tests read it here), `CLAWD_SHOWN_LOG` (which sessions a linked room shows).

---
## ⚠️ Rules that prevent breakage

- **Privacy of the events file.** Keep `{"t","sid","ev","cwd","tool"}`. No prompts, tool input/output or full paths, anywhere (hook, link messages, logs). Link messages carry a type and small values only.
- **No dependencies.** The room is C11 and libc/libm only; the Python tools use the standard library only. No pip installs. Only `room.py` uses pygame (in `.venv`).
- **No network, no background daemons.** Writes go only to the project folder and `~/.clawd-room/`; the one exception is `setup_profile.py --apply`, which the user runs by hand and which writes a single fragment file under Windows Terminal's `Fragments\clawd-room\` (`settings.json` only via the explicit `--apply-global`, with a `.clawd-room.bak` backup). The `powershell.exe` children belong to the room and must be stopped when it exits.
- **Do not edit `~/.claude/settings.json` yourself.** Tell the user to run `python3 install_hooks.py` (prints the change); `--apply` only if they say so. The installed hook calls `hook.py` by its full path, so the repo must not move: on Zaviel's WSL machine it calls `python3 ~/clawd-room/hook.py`, so the **symlink `~/clawd-room` -> the Desktop folder must stay**.
- **Snapshots are the contract.** `make -C c test` must pass; a change that alters behaviour on purpose updates the snapshots (`tools/test.sh --update`) and says so in the Dev Log. New behaviour gets a fixture (`tools/fixtures/*.fx`) or scene (`tools/scenarios/*.sc`). The random generator is Python's Mersenne Twister, kept so the old comparisons stay valid: do not add or remove random calls casually. Launchers call `clawd-room.sh`, never the binary directly.
- **Leave `room.py` alone** (reference only). Keep the state machine and its timings unless asked: 90 s sleep, 30 min stale, 2.5 s cheer, 0.8 s hold-working.
- **Terminal hygiene.** Alternate screen, hidden cursor, one string written per frame, diff updates; restore the terminal on exit, Ctrl-C and exceptions.
- **Windows are never moved to attach them** (the user decided). Gates sit on the room's walls, ceiling and floor (style A), so the two ends need not line up on screen. Do not add window snapping or following again.
- **Window helpers (WSL: PowerShell; macOS: osascript + iTerm2, panes count as windows).** Only look at windows / panes whose title starts with `clawd-room `. Be DPI aware (real pixels). Send the font-size keys only to a window that already has the focus, and never type into another window. Windows Terminal's focus mode (command palette) dropped the window in tests: do not use it.
- **The Clawd looks official**: flat `#CD6E58`, 14x8 sprite (8-wide torso, 2x2 arms, four 1-pixel legs), 1x1 eyes, no mouth.
- **The look is style A** (one-point perspective, see [[Rooms and Art]]). **Visual changes start with rendered mockups** (a scratch program drawing with the room's own canvas code) that Zaviel picks from; send them as pictures, then build.
- **Constants with a reason** (change only on request): attach gap 30 px (stays until 45), gates wait 1 s for still windows, a door needs 14 room pixels of shared floor, a hatch 21 room pixels of overlap, workspaces 24-40 px wide in rows 15 px apart, minimum terminal 44x20, 16 FPS; the rest are listed in [[Rooms and Art]] and [[Interaction]].
- **Do not hurt the user's running windows.** When testing with real Windows Terminal windows, create your own (names like `alpha` / `beta`), find them by exact title or pid, and never resize or kill the user's `park` / `coffee` windows. Do not use `pkill -f` with a pattern that also matches your own command line.
- **Testing needs the user's eyes** for the real windows. Say what could not be tested and what the user must look at.

---
## 🧪 How to test

1. `make -C c test` (about a second): snapshots of frames, terminal output, random numbers, scripted scenes over time, geometry and `--dump` frames.
2. `clawd-room --dump 100x30 --events FILE --at TS` prints one plain-text frame; `python3 simulate.py --fixture 1800000000 --out /tmp/e.jsonl` writes a static case for it.
3. `make -C c smoke` runs the program in a pty (keys, new events, menu, quitting, terminal restored); feed a real event through `hook.py` with `CLAWD_EVENTS` set.
4. `make -C c linksmoke`: two linked rooms in ptys, `g` sends a Clawd, the messages arrive. `make -C c movesmoke`: `m` moves a Clawd, the other room claims it. `make -C c splitsmoke`: rooms opening, freezing and closing do not move other rooms' Clawds.
5. `clawd-room --check` (part of `make test`): items painting over each other in every room x gates x crowd x size (keep it at 0); `make -C c gallery` draws the same scenes as contact sheets to look at.
6. Glitches that last a frame (blinks) or involve linked rooms: reproduce with the real program in ptys and a temporary per-frame log of what `draw_room` draws, not with state alone. On iTerm2, `./clawd-room.sh --try` opens four linked rooms with made-up Clawds.
7. `make -C c winsmoke`: real Windows Terminal windows (your own `alpha` / `beta`): sizing and gate positions. `clawd-room --list-windows` lists the `clawd-room` windows; `tools/place_windows.py` moves only the titles you give it. New Windows Terminal windows may open minimized, and the PowerShell helper needs a few seconds to start.
8. `clawd-room --build-art` redraws the sprite sheet and GIFs; look at them.

Run the tests that rewrite snapshots in a separate worktree (`git worktree add gallery/mock/wt`) when the working tree has the user's own uncommitted edits (e.g. a sprite being drawn in `sprites/props.txt`): never commit or alter those.

---
## 🔁 Workflow

1. Read [[Dev Log]] and the code you are about to change before editing.
2. Make the smallest change that does the job; match the surrounding style (small functions, a short comment per non-obvious step, no new dependencies).
3. Test as above, then say plainly what works, what could not be tested and what you were unsure about.
4. Record it (see the notes regulation below); update `README.md` when commands or keys change.
5. **Commit and push every change** on a feature branch, following [[#🌿 Regulation: repo management]].

---
## 🌿 Regulation: repo management

The repo is private on GitHub (`zelvet-velvet/clawd-room`). Two people work on it: **Zaviel** (owner, `zelvet-velvet`) and **William Pan** (collaborator, `WilliamPanTW`, since 2026-10-19). The human version of these rules, with the setup and the everyday loop, is `CONTRIBUTING.md`; the vault side is [[Collaboration]]. History starts clean (2026-10-16): the first commit is `chore: start clean history`. The old repo, with the original pygame prototype and the branches `feature/terminal-ui` / `feature/room-link`, is archived (read-only, private) as `zelvet-velvet/clawd-room-archive`; look there only for old history.

**Branches**
- `main` is the stable line. Nobody commits or pushes to it directly; it only receives pull requests.
- Work on a branch off a fresh `main`, named `type/short-slug`, for example `feat/seesaw`, `fix/gate-offset`. One topic per branch; unrelated things go on a new branch.
- (Until 2026-10-19 Zaviel worked alone with `feat/style-a` kept equal to `main`; that practice ended when William joined.)

**Commit messages:** `type(scope): summary`, lowercase, imperative, no trailing period.
- `type` is one of `feat`, `fix`, `refactor`, `chore`, `docs`.
- `scope` is the area touched: `room`, `link`, `gates`, `themes`, `detect`, `hook`, `vault`, `docs`...
- Example: `fix(gates): wait one second for still windows before showing a gate`.
- End each commit message with the co-author line from the session's attribution rules.
- Git identity: the user's own, set in the repo (not globally). Zaviel: `Zaviel`, `you@example.com`. Check `git config user.name` to know whom you work with.

**Every change = commit + push** on the current feature branch, without asking; never force-push a shared branch or `main`.

**Pull requests and merging**
- When a feature is done (tests pass, Dev Log entry written), **open a pull request** into `main` with `gh pr create` (the template asks for what changed, pictures, what was tested). Before that, `git fetch && git merge origin/main` and test again.
- **Only Zaviel merges** (on GitHub: "Create a merge commit", so each feature shows as one branch). Never merge a PR yourself, and for Zaviel's own PRs ask before merging.
- After a merge, delete the branch (GitHub's button, then `git branch -d` locally).

## 📜 Regulation: notes and `CMD_SYNC`

When the user sends **`CMD_SYNC`**, update the notes for every feature or change added since the last sync:

1. Find what changed: `git status`, `git log` since the previous `docs(vault): sync` commit, and the conversation so far.
2. **[[Dev Log]]** (`Docs/Dev Log.md`): one short entry per change under its date (newest first): a title with the author's name in brackets, two to four plain bullets of what changed for the user, the branch if not merged, a link to the topic note. No code details, no rounds of back and forth; superseded attempts get one line.
3. **Pictures:** put screenshots (the user's, renders from the real program, chosen mockups) in `Screenshots/` named `YYYY-MM-DD subject.png` (a person's own: `... (Zaviel).png` / `... (William).png`, scaled to 1400 px) and embed them in the Dev Log entry (`![[name.png|250]]`).
4. **Topic notes** (`Notes/`): update the note(s) the change belongs to so they describe how things work *now* (rewrite, do not append history; use mermaid diagrams (flowcharts with `%%{init: {"flowchart": {"defaultRenderer": "elk"}}}%%`, state and sequence diagrams) and callouts where they explain better than text): [[Rooms and Art]], [[Interaction]], [[Platforms and Windows]], [[Engine and Tests]]. Constants go in the tables there.
5. **[[Test Checklist]]:** add what still has to be checked by eye to the **test queue** at the top: one `> [!todo]+ N. Title` callout per test with tick boxes, and inside it a foldable `> > [!example]- How to test` with numbered steps; drop checks for things that were replaced. Do not tick anything; only a person does that. Fully ticked tests move down under their subject.
6. Update `README.md` if commands, flags or keys changed, and this guide if a rule changed.
7. Commit and push as `docs(vault): sync` on its own branch, open a pull request, then finish with a short list of what was added.

---
## 🗣️ Conventions

- Call the user by name (**Zaviel** or **William**, from `git config user.name`). Reply in the language they write in; the notes stay English.
- Pixel text is upper case only (the 3x5 font has no lower case); keep UI strings short.
- Times in the events file are epoch seconds; `--at TS` pretends "now" is TS.
- Terminal sizes are `COLSxROWS`; a cell is 1 room pixel wide and 2 high, so room pixels are square on screen.
- Prefer a named constant at the top of a C file over a magic number.
