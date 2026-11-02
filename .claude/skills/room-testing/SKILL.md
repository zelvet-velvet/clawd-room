---
name: room-testing
description: How to test the clawd-room program: make -C c test, the pty smoke tests, linked rooms, real Windows Terminal or iTerm2 windows, test environment variables, worktrees for snapshot updates. Use before running, adding or updating tests in clawd-room, or when a change needs checking in a real window.
---
# Testing clawd-room

From the repo root. Say plainly what works, what could not be tested and what the user must look at.

1. `make -C c test` (about a second): snapshots of frames, terminal output, random numbers, scripted scenes, geometry, `--dump` frames, and `--check` (items painting over each other in every room × gates × crowd × size: keep it at 0). An intended change: `tools/test.sh --update`, and say which snapshots changed and why. New behaviour gets a fixture (`tools/fixtures/*.fx`) or scene (`tools/scenarios/*.sc`).
2. One frame as text: `python3 simulate.py --fixture 1800000000 --out /tmp/e.jsonl`, then `c/clawd-room --dump 100x30 --events /tmp/e.jsonl --at 1800000000`.
3. In a pty: `make -C c smoke` (keys, new events, menu, quitting, terminal restored), `linksmoke` (two linked rooms, `g` sends), `movesmoke` (`m` moves), `splitsmoke` (rooms opening, freezing, closing keep the sessions' split). Feed a real event through `hook.py` with `CLAWD_EVENTS` set.
4. Real windows: `make -C c winsmoke` (your own `alpha` / `beta` Windows Terminal windows: sizing, gate positions). `c/clawd-room --list-windows`; `tools/place_windows.py` moves only titles you name. New WT windows may open minimised; the PowerShell helper needs a few seconds. Put test windows on the virtual desktop the user names and never switch their view. Never resize or kill the user's own room windows; no `pkill -f` with a pattern matching your own command line.
5. Glitches that last a frame or involve linked rooms: reproduce in ptys with a temporary per-frame log of what `draw_room` draws, not with state alone. `./clawd-room.sh --try` opens four linked rooms with made-up Clawds (WSL: `--desktop N`).
6. Art: `c/clawd-room --build-art` redraws the sprite sheet and GIFs; `make -C c gallery` draws contact sheets. Look at them.

**Environment hooks**: `CLAWD_EVENTS` (events file), `CLAWD_MY_SPRITES` (your sprites; tests use none), `CLAWD_LINK_DIR`, `CLAWD_SIZE_LOG` (terminal size, gates, camera target), `CLAWD_STATUS_LOG` (the bottom bar's text), `CLAWD_SHOWN_LOG` (sessions a linked room shows), `CLAWD_ANIMS` / `CLAWD_MY_ANIMS`, `CLAWD_STAGE_LOG` (stage page state per input), `CLAWD_SETTINGS`.

**The user's uncommitted edits** (a sprite being drawn in `sprites/props.txt`): run snapshot-rewriting tests in a separate worktree (`git worktree add gallery/mock/wt`); never commit or alter those edits.

**Windows rules**: only windows / panes titled `clawd-room `; DPI aware; font keys only to a window that already has the focus; never type into another window. The one exception (Zaviel, 2026-10-19): WSL `--try` snaps its first window with Win + ← + ↑ and picks the other three in Snap Assist; never send `q` (it quits a room). Do not use Windows Terminal's focus mode. Windows are never moved to attach them (no snapping or following).
