---
title: Engine and Tests
tags:
  - clawd-room
  - engine
  - tests
---
# ⚙️ Engine and Tests

> [!abstract] In short
> The room is a small C11 program (`c/`, no libraries). Snapshot tests pin its output; pty tests drive the real program; one seeded random generator per Clawd keeps behaviour repeatable.

---
## 🗺️ The code (`c/`)

```mermaid
%%{init: {"flowchart": {"defaultRenderer": "elk"}}}%%
flowchart LR
    App["app.c<br/>main loop, keys, menu"]
    Tail["tail.c<br/>events file"]
    Room["room.c<br/>state machine"]
    Draw["draw.c<br/>layout, frame, camera view"]
    Theme["theme.c<br/>rooms, workspaces, gates, signs"]
    Persp["persp.c<br/>perspective"]
    Clawd["clawd.c + sprites.c<br/>poses, sprite files"]
    Cam["camera.c<br/>who to follow"]
    Link["link.c + json.c<br/>linked windows"]
    Win["win.c<br/>window helpers"]
    UI["ui.c<br/>status line, sizes"]
    Term["term.c<br/>cells, diff output"]
    App --> Tail --> Room
    App --> Cam
    App --> Link
    App --> Win
    App --> UI
    App --> Draw
    Draw --> Room
    Draw --> Theme --> Persp
    Draw --> Clawd
    Draw --> Term
```

| File | What it does |
| --- | --- |
| `app.c` | arguments, the main loop, keys, the start menu |
| `room.c` | who goes where, visits, crossing |
| `draw.c` | layout (`geo_init`), drawing a frame, the camera view |
| `theme.c` | the three rooms: backgrounds, workspaces, gates, signs |
| `persp.c` | vanishing point, floor mapping, quads, boxes |
| `camera.c` | what the zoomed camera follows |
| `clawd.c`, `sprites.c` | the Clawd's poses and the editable sprite files |
| `link.c`, `json.c` | linked windows talking through files |
| `win.c`, `ps_scripts.h`, `mac_scripts.h` | the window helpers |
| `ui.c` | status line, menus, auto scale, room sizes |
| `tail.c` | reading the events file |
| `art.c` | sprite files, PNG and GIF output (`--build-art`) |

---
## 🧪 Tests

```mermaid
%%{init: {"flowchart": {"defaultRenderer": "elk"}}}%%
flowchart LR
    Change["✏️ code change"] --> T["make -C c test<br/>snapshots (1 s)"]
    T -->|differs on purpose| U["tools/test.sh --update<br/>+ say so in the Dev Log"]
    T -->|differs by accident| Fix["🐛 fix it"]
    T -->|matches| S["make -C c smoke<br/>pty: keys, menus, quit"]
    S --> L["make -C c linksmoke<br/>two rooms: g, b, messages"]
    L --> Eye["👀 Test Checklist<br/>(Zaviel)"]
```

- `make -C c test` compares with `c/tests/expected`: sprite frames, terminal strings, random numbers, fixture frames (`tools/fixtures/*.fx`), scripted scenes over time (`tools/scenarios/*.sc`), gate planning, `--dump` text frames.
- `make -C c winsmoke` uses real Windows Terminal windows.

> [!info] Where the snapshots come from
> First made by the old Python version (the C port matched it line by line); since style A (2026-10-18) they come from the C program.

> [!info] Next: the engine
> The agreed plan to replace this with items, layers and a placer is in [[Engine Design]].

> [!tip] Test hooks
> `CLAWD_EVENTS` (events file) · `CLAWD_LINK_DIR` (link folder) · `CLAWD_SIZE_LOG` (sizes, gate positions, who the camera follows) · `--dump COLSxROWS` with `--zoom`, `--room-size`.

---
## 🎲 Random numbers

- **One generator per Clawd**, seeded from its session id (Python's Mersenne Twister, MT19937, reproduced exactly): a Clawd's habits repeat, different Clawds differ.
- It decides when to plan next, what to do (visit, play, walk), where to walk, how long to chat, and the particles.
- Not random: the park's grass pattern and the walk timing.

> [!warning] Careful with random calls
> Adding or removing a random call changes every scene after it. Fine when meant (update the snapshots and say so), never by accident. Clamping a value after it is drawn does not change the draws.
