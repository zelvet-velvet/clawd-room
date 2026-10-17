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

> [!NOTE] 2026-10-17: resize and full screen in a real iTerm2 window (my own test window, driven by AppleScript, screenshots)
> - The room follows every resize: 65x22 -> 132x38 -> 76x19 redraws at once; below 44x20 it shows "need 44x20". Full screen (Cmd+Enter) 162x51 and back to 80x25 also redraw correctly.
> - iTerm2 (3.5, default settings) does not answer the pixel queries `CSI 14t` / `CSI 16t`, so `reports.has_cell` stays 0 there: the "font changed" guess (Cmd +/-) never fires on a Mac, and auto scale only looks at cols x rows.
> - In macOS full screen the menu bar and title bar slide over the top terminal rows when the mouse goes to the top: the banner is hidden for that moment (no change in size, nothing to fix in the room).
> - At 162x51 with 7 Clawds auto scale stays 1x (2x would not seat them), so full screen shows small Clawds on a big floor.
> - Open: Zaviel said the TUI "won't auto resize" on the Mac. In these tests it does redraw on resize; what is missing is the WSL part where the room sets the font and window size itself (no PowerShell here). To confirm with Zaviel which one was meant.

> [!INFO] How `[` / `]` scale works now (answer to "camera zoom or smaller room?")
> It is **not** a camera. With the auto room shape the room is rebuilt at `cols / k` x `rows*2 / k` room pixels and every room pixel is drawn as k x k cells (`render()` in `c/draw.c`). So `]` makes everything bigger **and the room smaller**: fewer desks per row, less floor, more `+N MORE`. At 80x25 a 2x room is only 40x25 room pixels. Steps are whole numbers (1x -> 2x halves the room), up to `MAX_SCALE` 8; auto picks at most `SCALE_CAP` 3. With a fixed shape (`r`) the room keeps its size in room pixels and `[` `]` are refused.

## 💡 Ideas: zoom and full screen (2026-10-17, to discuss, nothing built)

**Zoom (`[` `]`)**
- **Only useful steps:** `]` stops at the biggest scale that still seats every Clawd (what `auto_scale` already computes) and flashes why; or allow it but say "+N MORE AT 3X".
- **Camera zoom:** lay the room out once for the window at 1x, then `[` `]` zoom a camera into it: the world keeps its desks, the view shows part of it and follows a Clawd (`Tab` picks which one, e.g. the one that needs you). Pairs with the "focus" idea. Gates and visits keep working because the room size does not change.
- **Fixed world, letterbox:** keep the room at the size it had at 1x and scale it up with margins (like the fixed shapes do now), instead of shrinking the world.
- **Font zoom instead:** on WSL the font keys already exist; on iTerm2 there is no escape code for font size, only AppleScript / the profile (or the user's Cmd +/-). Fractional scales (1.5x) would make uneven pixels with half blocks: no.

**Full screen, both platforms**
- *What happens today:* the room just gets a bigger terminal. Windows Terminal: F11 (and maximised); focus mode is banned (it dropped the window in tests). macOS: native full screen (own Space, Cmd+Enter), or iTerm2's non-native full screen (same Space, setting "Native full screen windows" off), or a maximised window.
- *Linking:* a full-screen window has no neighbours (macOS even puts it in its own Space), so no gates. Ideas: (a) a full-screen room is the **hub**: linked rooms send their Clawds there to visit; (b) in full screen draw **several rooms side by side** (office | coffee | park) with doors between them inside one window, since there is room for it; (c) leave gates off and say "FULL SCREEN: NO GATES".
- *Size:* full screen should pick a bigger scale or font so Clawds are not tiny; maybe a minimum Clawd size on screen (e.g. auto scale prefers 2x and shows two desk rows instead of five tiny desks).
- *Detect it:* WSL: the PowerShell watcher can compare the window rect with the monitor; macOS: AppleScript (`bounds` of the iTerm2 window vs the screen) or simply "cols x rows got very large". Only needed if behaviour should change in full screen.
- *macOS overlay:* the menu bar slides over the top rows when the mouse touches the top; keep important text (banner) out of the top row or accept it.

**macOS window helpers (to replace PowerShell)**
- iTerm2 AppleScript gives every window's `bounds` (points, top-left origin; used in these tests) and can set them: enough for a watcher (neighbours, gates lining up) and for "fit". Window titles: the room already sets `clawd-room <id>` via OSC 2 when it has a watcher; iTerm2 shows it as the window name, so windows can be found by title the same way.
- Points vs pixels: macOS bounds are in points (Retina = 2 px per point); gates must use the same unit on both ends.
- Same rules as WSL: only windows titled `clawd-room `, never move windows to attach them, never type into another window.

---
## ⏳ Pending (last step)

- Profile from the dropdown still opens at Windows Terminal's default size and then jumps once (WT has no per-profile size). Planned: `setup_profile.py --shortcut` making a shortcut that runs `wt.exe -w new --size 60,18 -p "Clawd Room"`. Not a global `initialCols/Rows` (it would shrink every new window).

---
## 💡 Open ideas

- Seesaw in the park; an empty-desk look for Clawds that are away.
- Opt-in focus-mode flag, if it can be made reliable.
