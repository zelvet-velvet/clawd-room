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

## 🎥 Camera, room size list, iTerm2 helper (2026-10-17)

> [!WARNING] Superseded the same day
> The `r` shape list (LARGE / HUGE) and the free-only camera below were replaced by the agreed design in the next entry ("Agreed with Zaviel"); kept here for the history.

> [!NOTE] Camera zoom (`[` `]`, arrows / WASD)
> Decided with Zaviel: `[` `]` are a **camera**, no longer a TUI scale that shrinks the room.
> - The room is laid out as before (FIT WINDOW: window / auto scale; a shape: its fixed size), then `render_camera()` (`c/draw.c`) shows part of it at base scale x zoom (1x to `CAM_MAX_ZOOM` 4) around `cam_x, cam_y` (room pixels, clamped so the view stays inside the room). Zooming in starts at the centre; the arrows or W A S D move a quarter of the view. At 1x in FIT WINDOW nothing moves (flash says `]` zooms in).
> - `view_x / view_y / view_k` are set so that screen = view + room pixel x k still holds; the gates use them. While zoomed the gates follow the zoom (not lined up with the next window).
> - The arrows are read now (ESC [ A..D and ESC O A..D); other escape sequences are skipped; a lone Esc closes the size list or quits.
> - `f` (auto) puts the camera back to 1x. `--zoom N` starts zoomed (used by the `--dump` snapshots).

> [!NOTE] Room size list (`r`)
> `r` no longer cycles: it opens a ROOM SIZE list drawn over the room (pixel font, scrolls in small windows): FIT WINDOW, 3:1, 2:1, 16:9, 4:3, 1:1, 3:4, **LARGE 192x108**, **HUGE 256x144**. Up/down or W/S, Enter picks, Esc or `r` closes. LARGE and HUGE can be bigger than the window: shown at 1x (or bigger if the window allows) and moved with the camera. `--ratio large|huge` too.
> - Snapshots: `ui.txt` (help line) changed on purpose; `dump.txt` gained 4 frames (zoom 2, park zoom 3, large, huge zoom 2); every old frame is the same. `pty_smoke` checks the zoom flash, the size list (r, down, Enter) and that Esc closes the list without quitting.

> [!NOTE] macOS helper (iTerm2), replaces PowerShell there
> - `c/mac_scripts.h`: `OSA_WATCH` (JavaScript for Automation, one `osascript` per room) prints the iTerm2 windows **and split panes** whose title (`terminalWindowName`, set by the room with OSC 2) starts with `clawd-room `, in the PowerShell watcher's line format, in points (dpi 96), four times a second. It quits when its parent changes (no PDEATHSIG on macOS); checked: SIGKILL on the room, the watcher was gone 2 s later.
> - The terminal area is worked out (iTerm2 gives no pane or text-area pixels; it answers `CSI 14t` with the whole window and does not answer `CSI 16t`): bounds - title bar 28 pt (none in full screen) - tab bar 25 pt when there are tabs - margins (`TerminalMargin` 5, `TerminalVMargin` 2 from iTerm2's defaults). Error of a few points is possible; gates may sit a little off.
> - **Split panes = windows.** A row of panes with the same rows is split by columns, a column of panes with the same columns by rows (two equal panes: whichever gives cells about 2.05 tall per wide). Each clawd-room pane is reported with its own rectangle, so `neighbor_side` sees two panes as attached windows (gap 0) and puts a gate between them. Other layouts (L shapes) report the whole window for each pane: no gate between them. Only the current tab of each window counts.
> - `OSA_TOOL` (one-shot): `resize` sets the window bounds (the `f` fit and opening at 110x32); `zoomfg` / `resetfg` send Cmd -/+/0 through System Events only when iTerm2 is frontmost and its current session is the room's. Non-focused zoom is dropped (rule: never type into another window).
> - The room uses this only when `TERM_PROGRAM=iTerm.app`, not in tmux, and `/usr/bin/osascript` exists. On iTerm2 the font is left alone unless `--font PT` is given (Accessibility needed).
> - Found while testing: when Zaviel's iTerm2 window is in **native full screen**, new windows open full-screen size and AppleScript `set bounds` hangs; a room in such a window stopped being read by iTerm2 and blocked in `write()` (SIGTERM could not stop it, SIGKILL did). The same happened with `--no-detect` (no helper), so it is iTerm2, not the watcher. Two "Untitled" iTerm2 windows were left over that AppleScript cannot close.

> [!TODO] Test (by eye, iTerm2, not in full screen)
> - [ ] `]` zooms in (2x, 3x, 4x), arrows and W A S D move, the view stops at the room edges, `[` back to 1x shows the whole room
> - [ ] `r` opens ROOM SIZE; up/down, Enter on LARGE: part of a big room, the camera moves over it; Esc closes the list without quitting
> - [ ] Two windows `--room alpha` / `--room beta` side by side (touching): a gate shows on the facing sides; `g` sends a Clawd through
> - [ ] One window split vertically (Cmd+D), `clawd-room.sh --room alpha` in the left pane, `--room beta` in the right: a gate between the panes; same for Cmd+Shift+D (stacked, hatch)
> - [ ] `f` with a pinned shape resizes the iTerm2 window to that grid
> - [ ] Quit: the window title goes back; no `osascript` left (`pgrep -fl osascript`)

> [!WARNING] Open
> - A room blocks in `write()` if the terminal stops reading (seen with iTerm2 in full screen); it could use a non-blocking write with a timeout so `q` / SIGTERM still work.
> - Windows Terminal panes: the PowerShell watcher sees only the whole window. UI Automation exposes each `TermControl` with a bounding rectangle, which could report panes like iTerm2's (to try on WSL).

## 🤝 Agreed with Zaviel: resize, room size, camera (2026-10-17, built)

> [!NOTE] What Zaviel decided
> The `r` list of shapes and the plain `[` `]` camera from the entry above are not what Zaviel wants; this replaces them.
> - **Resize = grow, then scale.** While the window is dragged the room always fills it: it gains room pixels until it reaches the chosen room size, past that every room pixel gets bigger (integer scale) so the picture fills the window. Shrinking does the reverse. The room's shape always follows the window.
> - **Room size = the biggest the room may be**, not a shape. `r` list: **AUTO** (big enough to seat every session with floor to walk on; grows when sessions arrive) and **S / M / L / XL** fixed maximum sizes. The 3:1 / 16:9 / ... shapes go away.
> - **Camera follows Clawds (auto follow is the default when zoomed).** Priority: the Clawd that needs you (waiting for an OK) > the one that just finished (cheering) > the most recently active one > if nothing happens, the next Clawd about every 10 s. The camera moves only when its Clawd nears the edge of the view, smoothly. `Tab` = pick the next Clawd yourself (stays on it), arrows / WASD = free camera, `c` = back to auto follow.
> - **Zoom is manual by default** (`[` `]`); a shortcut turns on **auto zoom** (the camera frames the active Clawds: closer on one busy Clawd, wider when several are active) until pressed again.

> [!NOTE] Also decided
> - Sizes in room pixels: S 120x60, M 160x90, L 220x120, XL 300x160 (an 80x25 window is 80x50 room pixels; a full-screen MacBook Air iTerm2 at 14 pt is about 162x102).
> - Auto zoom key: `z`.

> [!NOTE] How it is built
> - **Room size** (`c/ui.c`: `SIZE_*`, `size_scale()`): AUTO = `auto_scale()` as before (biggest scale up to 3x that seats everybody); S/M/L/XL = the smallest scale k with window / k inside the size (`ceil(cols / W)`, `ceil(rows*2 / H)`). The scale is picked again on every resize and now also when the number of Clawds changes. The room is always the auto shape; `--ratio` shapes stay only for the old tests and are not in the list. `--room-size auto|s|m|l|xl`.
> - **Camera** (`c/camera.c`): `want()` = 3 wait, 2 cheer, 1 work/think, 0 idle/sleep. Auto follow keeps its Clawd unless one wants the camera more; ties go to the most recent event; when the best is 0 it moves to the next Clawd after `CAM_CYCLE` 10 s. Dead zone `CAM_DEAD` 0.25 of the view, `CAM_EASE` 0.18 of the rest each frame. Tab = `CAM_LOCKED` on the next Clawd (back to auto if it leaves), arrows = `CAM_FREE`, `c` = auto. Auto zoom: the box around the busy Clawds + 12 px; the biggest zoom (to 4) whose view holds it, changing at most every 2 s; nobody busy = 1x. `[` `]` turn auto zoom off.
> - Test hook: `CLAWD_SIZE_LOG` also logs `cam MODE SID zoom Z` when the camera target changes. Checked in a pty with the simulated sessions: auto follow took `d` (Notification = needs you), Tab gave `e` then `f`, `c` gave `d` again, `z` gave 1x (busy Clawds spread over the room), an arrow gave free.
> - Snapshots: `ui.txt` help line; `dump.txt` replaced the LARGE / HUGE frames with `--room-size s` (100x30, 200x70) and `--room-size m --zoom 2`; all older frames identical. `pty_smoke` checks `ROOM SIZE S`, `FOLLOW`, `AUTO ZOOM ON`.

> [!TODO] Test (by eye)
> - [ ] Drag the window bigger and smaller: the room re-lays out at once; with S chosen, past 120 columns the pixels get bigger
> - [ ] `r` list shows AUTO, S, M, L, XL; picking one changes the room at once
> - [ ] `]` twice: the camera sits on the Clawd that needs you / just finished / is busy, and follows it when it walks
> - [ ] Tab goes through the Clawds (flash FOLLOW NAME); arrows make it free; `c` back to auto
> - [ ] `z`: the zoom closes in on a single busy Clawd and widens when several are busy

## 🩹 Resize leaves the view scrolled; zoom magnified everything (2026-10-17)

> [!NOTE] What Zaviel saw
> In iTerm2, after the window was too small ("need 44x20") and then made bigger again, the picture was wrong until the view was scrolled down; and `]` magnified the whole picture (banner and words too), not the room.

> [!NOTE] Fix: no clear-screen, cursor parked at the top
> - Cause (two ways lines got into iTerm2's scrollback): every resize sent `ESC[2J`, which iTerm2 copies into scrollback, and the cursor was left where the last changed cell was (often low on the screen), so when the window got shorter iTerm2 pushed the top lines into scrollback to keep the cursor's line. Making the window bigger then showed old lines, scrolled up.
> - Now: no `ESC[2J` anywhere; `wipe()` erases row by row (`ESC[2K`), the "need 44x20" screen does the same, and every frame ends with the cursor at the top-left (`ESC[H`). A full repaint (after a resize) writes every cell anyway.
> - Checked in a pty: 100x30 -> 30x10 -> 120x40, no `ESC[2J`, all 39 frames end with `ESC[H`. Not seen in iTerm2 itself (Zaviel's window was in full screen, test windows could not be captured). `ui.txt` snapshot changed (the too-small strings).

> [!NOTE] Fix: the camera zooms the room only
> - `render_camera()` now keeps the banner at the top at the normal size and shows the room below it zoomed, inside the rectangle the room has at 1x.
> - The words over the room (desk names and states, name tags of walking Clawds, `+N MORE`, `NO SESSIONS`) go through `wtext()`: at 1x they are drawn as before; zoomed they are collected and written at the normal size at the right place over the zoomed room (clipped to the view). Bubbles and the `!` / `Z` signs are part of the pictures and zoom with them.
> - Snapshots: only the zoom frames in `dump.txt` changed; all 1x frames identical.

> [!TODO] Test (by eye, iTerm2)
> - [ ] Make the window smaller than 44x20, then bigger: the room comes back at once, nothing scrolled
> - [ ] Drag-resize quickly up and down: no old frames in the scrollback (scroll up after quitting shows the shell as before)
> - [ ] `]`: the Clawds and furniture get bigger, the names / states and the banner stay small and readable

## 🚪 Doors, call back, window size from the screen, resize stripes (2026-10-17)

> [!NOTE] What Zaviel saw (two linked iTerm2 windows: "works correct")
> After `g`, the Clawd took too long to come in through the door of the other room; there was no way to call it back (`g` again only said NO IDLE CLAWD TO SEND); the window opened too big on a 13" MacBook Air; while resizing, iTerm2 showed striped frames.

> [!NOTE] Fixes
> - **Step in at once.** A visitor arriving by a side door was put just outside the window (x = -10) with no goal and waited for its first plan (1-3 s) out of sight. `enter()` now gives it a goal `DOOR_STEP_IN` 12 px inside the door (the rope and stairs already had one). No random call added or removed; the crossing scenes in `scenarios.txt` changed from the arrival on (expected), all others identical.
> - **`b` = call back.** `room_recall()`: every Clawd of ours that is next door gets the existing recall message (the visit ends, it walks back through its door); with none away, the visitors in this room are sent home. Flash `CALLING N CLAWDS HOME` / `SENDING N VISITORS HOME` / `NOBODY AWAY, NO VISITORS`. `link_smoke` now presses `b` after the crossings and checks the recall reaches the other room.
> - **Window from the screen (iTerm2).** Default `--size screen` there: the `screen` tool command sets the window to `SCREEN_PERCENT` 55% of the screen's usable area (`NSScreen.visibleFrame`), at least 44x20 cells, keeping its position unless it would leave the screen. Checked on a test window: 1100x700 -> 808x508 on a 1470x924 usable screen. WSL keeps 110x32 (`--size screen` there means off).
> - **Resize stripes (not fixed, explained).** iTerm2 reflows the screen when the window gets narrower: every full-width row of the last frame wraps onto two rows (the left part, then the rest, mostly dark margin), so the picture looks stretched with dark rows between. Windows Terminal does not reflow it. The room repaints right after the size changes (SIGWINCH wakes the main loop: no `SA_RESTART`), so the stripes should only show during a drag, between iTerm2's resize steps and the next frame.

> [!TODO] Test (by eye)
> - [ ] `g` from alpha: the Clawd comes out of beta's door at once and walks in
> - [ ] `b` in alpha: the Clawd in beta walks back through the door and home; `b` in a room with visitors and none of its own away sends the visitors home
> - [ ] A new room in iTerm2 opens at about half the screen on the MacBook Air
> - [ ] Drag-resize: stripes only while dragging, a clean picture as soon as you let go

## 🙈 NO SESSIONS only when there are none (2026-10-17)

> [!NOTE] What changed
> Zaviel: with a session running, NO SESSIONS / START CLAUDE must not show in any room. Linked rooms share the sessions out, so a room whose share was empty said NO SESSIONS while its neighbour had Clawds. Now the text shows only when no live session exists at all (every linked room reads the same events file, so `room->pool` holds them all). New fixture `office-split-empty.fx` (and a `split ME ID...` fixture line): a linked room with an empty share draws no text (its frame differs from the old code's); `office-empty-guest.fx` (no sessions anywhere) is unchanged and still shows it.

> [!TODO] Test
> - [ ] Two linked rooms, one session: the room without a Clawd shows an empty room, no NO SESSIONS text; quit all Claude Code sessions: both rooms show NO SESSIONS

## 🎬 Camera at the gates, narrower window (2026-10-17, branch feat/camera-gates)

> [!NOTE] What changed (Zaviel's asks)
> - **Narrower default window (iTerm2):** `--size screen` is now 40% of the usable width (`SCREEN_W_PERCENT`) and 55% of the height (`SCREEN_H_PERCENT`); the tool command is `screen|title|w|h`.
> - **Gates count as not zoomed:** `render_camera()` hands the gate maths (`view_x / view_y / view_k`, the grid sent to the next window, `my_band`) the room's 1x placement, so zooming in one window never moves or closes the gates; the words over the zoomed room use the zoomed transform themselves.
> - **The camera follows a Clawd through a gate:** while zoomed (auto or locked, not free), a Clawd heading out (`has_travel`: sent with `g` or visiting on its own; or a visitor going home) takes the camera to the door / rope / stairs; when it has crossed (gone from the room) the camera goes back to what it was doing. `Camera.cross_sid`; the `CLAWD_SIZE_LOG` camera line shows `cross SID`.
> - Checked in ptys (two linked rooms, alpha at 3x, `g`): `cross sess4` while it walked out, back to `cross -` about 3 s later, auto follow went on. Snapshots unchanged.

> [!NOTE] Second round (Zaviel: a room with one visitor and no desk did not follow; no zoom-out after a crossing; `c` / arrows)
> - Visitors are candidates everywhere (auto follow, Tab, left/right, auto zoom, crossings): a room with only a visitor follows it.
> - When the Clawd followed to a gate has crossed, the camera **zooms out to the whole room** (zoom 1, auto zoom off).
> - **`c` = follow <-> free camera.** Auto follow -> `c` -> free (only now the arrows / WASD move the view) -> `c` -> auto follow; a picked Clawd -> `c` -> auto follow. While following, left / right (A / D) or Tab pick the previous / next Clawd (`camera_step`, locked); up / down only say how to steer.
> - Checked in ptys (two linked rooms, one session, both 3x): beta followed its Clawd to the door and went to zoom 1 when it had crossed; alpha (no desk) followed the visitor; `d` locked, `c` back to auto, left arrow picked again. `ui.txt` help line changed.

> [!NOTE] Third round (Zaviel: an empty zoomed room, `c`, then a visitor comes in: the camera must follow it)
> - Cause: the room was already on auto follow, so `c` switched it to the free camera (`cam 2` in the log), which follows nobody; nothing on screen said which mode was on.
> - Now: while zoomed the status line shows the camera mode (`camera_label`: AUTO FOLLOW (WAITING) / AUTO: NAME / FOLLOW NAME / WITH NAME TO THE GATE / FREE CAMERA); `c` in a room with nobody to follow stays on auto follow ("WAITING FOR A CLAWD"). Also: our Clawds that are next door (`has_away`, parked just outside the edge) are never followed.
> - Checked in ptys: beta with no sessions, 3x, `c` -> still auto, label WAITING; alpha sends its Clawd -> beta's camera follows `~alpha~s1`.

> [!TODO] Test (by eye)
> - [ ] Empty room zoomed in: the status line says AUTO FOLLOW (WAITING); `c` keeps it; a visitor comes in: the camera follows it
> - [ ] A new iTerm2 room opens narrower (about 40% of the screen wide)
> - [ ] Zoom in (`]`) in alpha: the gate next to beta stays where it was (beta's gate does not move or close)
> - [ ] Zoomed, `g`: the camera goes with the Clawd to the door (or rope / stairs when stacked), then zooms out to the whole room
> - [ ] The room receiving the visitor (no desk of its own), zoomed: the camera follows the visitor
> - [ ] Following: left / right pick another Clawd, the view does not move by hand; `c`: free camera, the arrows move it; `c` again: auto follow
> - [ ] A visitor going home in a zoomed room: the camera takes it to its door too

## 🪜 The bottom gate is the top of the ladder (2026-10-17)

> [!NOTE] What changed
> Zaviel: the stairs at the bottom of the upper room (a wood frame, a dark shaft and short rungs) did not read right; it should just be the ladder carrying on, no frame. `draw_stairs()` now draws only the top of the ladder from the room below: two rails rising `LADDER_TOP_H` 9 room pixels from the bottom edge, rungs every 5 px, the same rail spacing (7) and colours as the ladder hanging from the hatch in the room below, so the two read as one ladder across the windows. Snapshots: only fixtures / scenes with a bottom gate changed (`fixtures.txt`, `scenarios.txt`).

> [!TODO] Test
> - [ ] Two stacked windows: the bottom of the upper room shows the ladder top (no frame), lining up with the ladder in the room below

---
## ⏳ Pending (last step)

- Profile from the dropdown still opens at Windows Terminal's default size and then jumps once (WT has no per-profile size). Planned: `setup_profile.py --shortcut` making a shortcut that runs `wt.exe -w new --size 60,18 -p "Clawd Room"`. Not a global `initialCols/Rows` (it would shrink every new window).

---
## 💡 Open ideas

- Seesaw in the park; an empty-desk look for Clawds that are away.
- Opt-in focus-mode flag, if it can be made reliable.
