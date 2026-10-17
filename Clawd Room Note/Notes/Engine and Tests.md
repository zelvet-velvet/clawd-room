---
title: Engine and Tests
tags:
  - clawd-room
  - engine
  - tests
---
# ⚙️ Engine and Tests

The code's layout, the tests, and how the random numbers work.

---
## The code (`c/`)

| File | What it does |
| --- | --- |
| `app.c` | arguments, the main loop, keys, the start menu |
| `room.c` | the state machine: who goes where, visits, crossing |
| `draw.c` | layout (`geo_init`), drawing a frame, the camera view, render |
| `theme.c` | the three rooms: backgrounds, workspaces, gates, signs |
| `persp.c` | the perspective: vanishing point, floor mapping, quads, boxes |
| `camera.c` | what the zoomed camera follows |
| `clawd.c`, `sprites.c` | the Clawd's poses and the editable sprite files |
| `link.c`, `json.c` | linked windows talking through files |
| `win.c`, `ps_scripts.h`, `mac_scripts.h` | the window helpers (PowerShell, osascript) |
| `ui.c` | status line, menus, auto scale, room sizes |
| `tail.c` | reading the events file |
| `art.c` | sprite files, PNG and GIF output (`--build-art`) |

## Tests

- `make -C c test` (about a second) compares the program's output with `c/tests/expected`: sprite frames, terminal strings, random numbers, fixture frames (`tools/fixtures/*.fx`), scripted scenes over time (`tools/scenarios/*.sc`), gate planning, and `--dump` text frames.
- A change that alters behaviour **on purpose** rewrites the snapshots (`tools/test.sh --update`) and says so in [[Dev Log]]. The snapshots were first made by the old Python version; since style A (2026-10-18) they come from the C program.
- `make -C c smoke` drives the program in a pty (keys, menus, quitting, terminal restored); `make -C c linksmoke` runs two linked rooms (send with `g`, call back with `b`, messages arrive); `make -C c winsmoke` uses real Windows Terminal windows.
- Test hooks: `CLAWD_EVENTS` (events file), `CLAWD_LINK_DIR` (link folder), `CLAWD_SIZE_LOG` (logs sizes, gate positions and who the camera follows). `--dump COLSxROWS`, `--zoom`, `--room-size` work with `--dump`.

## Random numbers

- **One generator per Clawd**, seeded from its session id (Python's Mersenne Twister, MT19937, reproduced exactly), so a Clawd's habits repeat and different Clawds differ.
- It decides when to plan next, what to do (visit, play, walk), where to walk, how long to chat, and the particles.
- **Adding or removing a random call changes every scene after it.** Fine when meant (update the snapshots and say so), never by accident. Clamping a value after it is drawn does not change the draws.
- Not random: the park's grass pattern and the walk timing.
