---
title: clawd-room - Dev Log
date: 2026-10-16
updated: 2026-10-16
tags:
  - dev-log
  - clawd-room
sticker: lucide//book-open
---
# 📝 clawd-room Dev Log

> [!INFO] How these notes fit together
> - **Dev Log** (this note): progress so far, the current phase with its checklist, and what is pending.
> - [[LLM_GUIDE]]: map, rules and repo regulation for AI assistants.
> - The detail of every change is in `git log` (commit messages are `type(scope): summary`).

---
## 🕰️ Progress so far (up to 2026-10-16)

- **2026-10-16, new repo:** history restarted as one commit; everything below is the old history, now in `zelvet-velvet/clawd-room-archive` (archived).
- **`main`**: the original pygame prototype (`room.py`), hook, installer, simulator.
- **`feature/terminal-ui`**: `room_tui.py`, a standard-library terminal room (half-block truecolor, 16 FPS, diff output); official-style pixel Clawd; idle life, visits; themes office / coffee shop / park.
- **`feature/room-link`** (current): `roomlink.py` presence + inbox between windows; `windetect.py` (powershell.exe window rectangles, resize / font helper); Clawds crossing through doors, ladders and hatches that line up on screen (windows are never moved); room shape follows the window (`--ratio auto`); start menu (`choose_scene`); `setup_profile.py` (Windows Terminal "Clawd Room" profile, icon, `--apply-global`); auto TUI scale and font on open.

---
## 🧭 Current phase: opening the window at the right size

> [!NOTE] Decisions (2026-10-16)
> 1. The room opens at **110x32 cells at 8 pt** (`--size`, `--font`); `off` leaves the window alone.
> 2. The font comes from the profile (a `settings.json` entry set by `--apply-global`; a fragment font loses to `profiles.defaults`). The room only resizes the window.
> 3. The window is sized once, from **real window pixels** (outer rect, terminal area, cells now). The terminal's own pixel reports are in other units on a scaled screen (125 %: cell 10x20 reported, 8x16 real) and are not mixed in.
> 4. `wt.exe --size` counts cells at the default **14 pt**, so `--size 60,18` opens the 8 pt profile at about 110x33 (110,32 gave 197x58).

> [!TODO] Test (by eye)
> - [x] Profile from the dropdown: the window shrinks once to about 110x32, font stays 8 pt (confirmed 2026-10-16)
> - [x] `wt.exe -w new --size 60,18 -p "Clawd Room"` opens at the final size (confirmed 2026-10-16)
> - [ ] Dragging the window changes only the TUI scale; `[` `]` change the scale, Ctrl +/- the font and stop the auto part; `f` brings it back
> - [ ] Quitting puts the terminal font back to normal
> - [ ] Window dragged very small: no crash, nothing zooms back and forth
> - [ ] Menu: arrows / 1-3 / S / Enter / q work; the room opens without resizing again
> - [ ] `--size off` leaves the window alone; `--room park` skips the menu
> - [ ] `ccc` many times (also quickly): no crash of all windows over a day or two (`sleep 0.7` before `exit` in `~/.zshrc` is a guess)

> [!TODO] Test: happy eyes
> - [ ] Happy Clawds (cheer, dance, visits) keep 1x1 dark eyes; each eye is 1 wide and 2 high: a pale peach (242,190,168) top over the dark pupil, plus blush, no white

> [!NOTE] 2026-10-16: [[Animations]] shows every frame and item (sheet + one GIF per action). Frames and items are editable text grids in `sprites/*.txt`, loaded by `room_tui.py` (a frame missing from the file falls back to the code); `sprite_tool.py` exports / builds.
> - [ ] Edit a grid in `sprites/clawd.txt` (e.g. change an eye), run the room: the change shows; delete the frame: the original is back
> - [ ] Bubbles, the carried cup and the OK sign look as before
> - [ ] Open the GIFs in Obsidian: they loop
> - Next: furniture and playground pieces into the sprite files

---
## 🔧 The room is in C (2026-10-16)

> [!NOTE] What happened
> The terminal room was rewritten from Python to C on `feat/c-port` in seven stages, each checked against the Python version until the two agreed on every line, then the Python room (`room_tui.py`, `themes.py`, `roomlink.py`, `windetect.py`, `sprites.py`, `sprite_tool.py` and their test tools) was removed. They are in git history. Python stays for `hook.py`, `install_hooks.py`, `simulate.py`, `setup_profile.py`, `wt_settings.py` and `room.py` (the pygame prototype).
> - Layout of `c/`: `canvas` (pixels, font), `sprites` (editable text frames), `clawd` (poses, drawing), `theme`, `draw` (room, gates, props, render), `room` (state machine), `rng` (Python's random generator, see below), `tail` (events file), `json` + `link` (windows talk through files), `win` (PowerShell helpers, gates that line up, font and window size), `ui` (status line, menu, auto scale), `app` (arguments, main loop, menu), `art` (sprite files, PNG, GIF), `actions`.
> - Tests: `make -C c test` compares with `c/tests/expected` (made by the Python version; frames of every pose, terminal strings, random numbers, 19 scenes, 9 scripted scenarios over time incl. visits, tag, playground, crossing, 12 `--dump` frames, geometry). `smoke`, `linksmoke`, `winsmoke` drive the real program. Numbers: Python 5 % of a core and 25 MB, C 0.5 % and 9.5 MB.
> - `clawd-room.sh` is the launcher (builds `c/` when missing or older than its sources); the Clawd Room profile and `wt.exe` line call it; `setup_profile.py --update` repairs an install.
> - Bug found by the user and fixed in C only: a Clawd that crossed into a window with no sessions of its own vanished, because an empty room returned before drawing visitors (`office-empty-guest.fx`, `empty-room-visitor.sc` cover it).

## 🎲 How the random numbers work

> [!INFO] Why it matters
> The state machine (`Room.plan`, `step`) chooses what an idle Clawd does with random numbers. The C version reproduces Python's own generator so that its behaviour could be compared with the Python version line by line, and the snapshots stay valid.

- **One generator per Clawd.** `Session.rng = random.Random(zlib.crc32(sid))`: seeded from the session id, so the same id always gives the same stream (a Clawd's habits are repeatable, and different sessions differ). There is no room-wide generator; `plan()` just uses `s.rng`.
- **What it decides:** when to plan next (`uniform(1, 4)` s and similar), what to do (`random()` roll, `choice` of friend / visit kind / solo act, 20 % chance to visit the next window), where to walk (`uniform` over the floor), how long to chat, the angle and distance when running away in tag, and the little particles (hearts, sparks, colours).
- **Algorithm (what the C port copies):** Mersenne Twister MT19937. The seed (a 32-bit crc) goes through `init_by_array` with a one-word key, exactly like `random.seed(int)`.
  - `random()` takes two 32-bit outputs `a`, `b` and returns `(a>>5 * 2^26 + b>>6) / 2^53` (53 bits).
  - `uniform(a, b)` = `a + (b - a) * random()`.
  - `choice(seq)` = `seq[k]` where `k` is drawn by `getrandbits(bit_length(n))` (the top bits of one 32-bit output), repeated until `k < n`. So it can use more than one output; the C port loops the same way.
- **Test:** `c/clawd-room --golden-rng` prints the numbers for four seeds (random, uniform, choice, and one value after 2000 draws to cross the 624-word refill); `make test` compares them with the snapshot made from Python 3.14; the algorithm has not changed since Python 3.2.
- **Not random:** the grass texture of the park (`noise(x, y)`, a hash of the position) and the walk / sprite timing (functions of time).
- **Changing behaviour:** adding or removing a random call changes every scripted scene afterwards; that is fine when meant (update the snapshots with `tools/test.sh --update` and say so here), but not by accident.

> [!TODO] Test the C version (by eye)
> - [x] Profile from the dropdown, menu, window 110x32 at 8 pt, drag / scale / font keys, two windows with gates, `g` (confirmed 2026-10-16: all pass; the Clawd vanishing in an empty room is fixed, to be checked)
> - [ ] One session only, two linked windows: press `g` in the room that has the session: the Clawd shows up in the other window (NO SESSIONS behind it), stays 25-45 s, walks home
> - [ ] Quit: the font returns to normal

---
## 🍎 macOS + iTerm2 (2026-10-17)

> [!NOTE] What happened
> First step to run the room on a Mac in iTerm2 (branch `feat/macos-iterm`).
> - `c/win.c`: `sys/prctl.h` / `PR_SET_PDEATHSIG` are Linux only, now behind `#ifdef __linux__`. On macOS `powershell.exe` is never found, so no helper is started; the window helpers (font keys, fit, window watcher, gates lining up with real windows) stay off, as with `--no-detect` (`f` says FIT ONLY WORKS ON WSL).
> - Scripts had no exec bit in git (committed from Windows): `tools/test.sh`, `clawd-room.sh`, `run.sh` are now `100755`.
> - `clawd-room.sh` suggests `xcode-select --install` on macOS when there is no compiler.
> - On macOS (clang): `make -C c test` all snapshots match, `smoke` all passed, `linksmoke` OK. `hook.py` / `install_hooks.py` need no change (paths come from `__file__` and `~`).

> [!TODO] Test in iTerm2 (by eye)
> - [ ] `./clawd-room.sh` in iTerm2: colours, half blocks and animation look right, no flicker, `q` restores the terminal
> - [ ] `python3 install_hooks.py --apply`, start a Claude Code session: its Clawd appears and reacts
> - [ ] Two iTerm2 windows with `--room NAME`, `g` sends a Clawd across
> - Next: iTerm2 window helpers (font size / window size / position) instead of PowerShell, e.g. via escape codes or AppleScript

---
## ⏳ Pending (last step)

- Profile from the dropdown still opens at Windows Terminal's default size and then jumps once (WT has no per-profile size). Planned: `setup_profile.py --shortcut` making a shortcut that runs `wt.exe -w new --size 60,18 -p "Clawd Room"`. Not a global `initialCols/Rows` (it would shrink every new window).

---
## 💡 Open ideas

- Seesaw in the park; an empty-desk look for Clawds that are away.
- Opt-in focus-mode flag, if it can be made reliable.
