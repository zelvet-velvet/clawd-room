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
## 🔧 C port (branch `feat/c-port`, started 2026-10-16)

> [!NOTE] Plan
> `room_tui.py` stays the reference and keeps working. The C version lives in `c/` (standard C11 + libm, `make`), is built stage by stage, and each stage must match Python exactly (`make test` compares frame hashes and terminal strings from `tools/golden_clawd.py`).
> 1. [x] canvas, 3x5 font, sprite files, `pose_for`, `draw_clawd` (1025 frames identical, from the sprite files and from the code)
> 2. [x] canvas -> half-block cells -> diff ANSI string (identical)
> 3. [x] themes (backgrounds, desks, accessories, playground), gates, labels, props, `draw_room` and `render` (17 scenes in `tools/fixtures/*.fx`, made by `tools/make_fixtures.py`, identical pixel for pixel)
> 4. [x] `Room` state machine in `c/room.c` (events, desks, idle plans, visits, tag, solo acts, playground, particles, crossing to other windows and messages), using Python's random generator (`c/rng.c`, see [[#🎲 How the random numbers work]]). 8 scripted scenarios in `tools/scenarios/*.sc` run frame by frame in both versions: canvas hash and everybody's state agree at every checkpoint (they cover all visit kinds incl. tag, coffee orders, peeks, swing / slide, rope / stairs, visitors, recall, the g key). `CLAWD_DUMP=FRAME` prints every session at that frame in both, for finding a difference
> 5. [ ] events tail + hook file, main loop, keys, start menu
> 6. [ ] window link and powershell helpers (`windetect`, `roomlink`), auto font and scale
> 7. [ ] decide: switch the installed command to the binary, keep Python for the hook and tools

---
## 🎲 How the random numbers work

> [!INFO] Why it matters
> The state machine (`Room.plan`, `step`) chooses what an idle Clawd does with random numbers. To check that the C version behaves exactly like the Python one, both must draw the **same sequence**, so the C version reproduces Python's own generator.

- **One generator per Clawd.** `Session.rng = random.Random(zlib.crc32(sid))`: seeded from the session id, so the same id always gives the same stream (a Clawd's habits are repeatable, and different sessions differ). There is no room-wide generator; `plan()` just uses `s.rng`.
- **What it decides:** when to plan next (`uniform(1, 4)` s and similar), what to do (`random()` roll, `choice` of friend / visit kind / solo act, 20 % chance to visit the next window), where to walk (`uniform` over the floor), how long to chat, the angle and distance when running away in tag, and the little particles (hearts, sparks, colours).
- **Algorithm (what the C port copies):** Mersenne Twister MT19937. The seed (a 32-bit crc) goes through `init_by_array` with a one-word key, exactly like `random.seed(int)`.
  - `random()` takes two 32-bit outputs `a`, `b` and returns `(a>>5 * 2^26 + b>>6) / 2^53` (53 bits).
  - `uniform(a, b)` = `a + (b - a) * random()`.
  - `choice(seq)` = `seq[k]` where `k` is drawn by `getrandbits(bit_length(n))` (the top bits of one 32-bit output), repeated until `k < n`. So it can use more than one output; the C port loops the same way.
- **Test:** `c/clawd-room --golden-rng` and `tools/golden_rng.py` print the numbers for four seeds (random, uniform, choice, and one value after 2000 draws to cross the 624-word refill); `make test` compares them. Matches Python 3.14; the algorithm has not changed since Python 3.2.
- **Not random:** the grass texture of the park (`noise(x, y)`, a hash of the position) and the walk / sprite timing (functions of time).
- **To keep both versions equal:** call the generator in the same order and the same number of times as `room_tui.py` (including calls whose result is unused), and never add a draw in only one of them.

---
## ⏳ Pending (last step)

- Profile from the dropdown still opens at Windows Terminal's default size and then jumps once (WT has no per-profile size). Planned: `setup_profile.py --shortcut` making a shortcut that runs `wt.exe -w new --size 60,18 -p "Clawd Room"`. Not a global `initialCols/Rows` (it would shrink every new window).

---
## 💡 Open ideas

- Seesaw in the park; an empty-desk look for Clawds that are away.
- Opt-in focus-mode flag, if it can be made reliable.
