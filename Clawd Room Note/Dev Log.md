---
title: clawd-room - Dev Log
date: 2026-10-16
updated: 2026-10-18
tags:
  - dev-log
  - clawd-room
sticker: lucide//book-open
---
# 📝 clawd-room Dev Log

What changed and when, in short. How things work now is in the topic notes:

| Note | About |
| --- | --- |
| [[Rooms and Art]] | the look: perspective rooms, workspaces, gates, signs, art rules, layout numbers |
| [[Interaction]] | keys, camera, room size, linked windows, crossing, sessions |
| [[Platforms and Windows]] | WSL + Windows Terminal, macOS + iTerm2, window helpers |
| [[Engine and Tests]] | the code, the tests, random numbers |
| [[Test Checklist]] | what is still to check by eye |
| [[Animations]] | every Clawd frame |

Pictures are in `Screenshots/`. Details of every change: `git log`.

---
## 2026-10-18

### 🏛️ The rooms in perspective are built
*Branch `feat/style-a` (on top of `feat/camera-gates`), not merged yet.*
- The three rooms are drawn in one-point perspective: workspaces in rows on the floor, Clawds sitting behind them, a sign with the room's name instead of the top banner, a dot over each head.
- Coffee shop and park have their own activities: a barista at a low bar; customers at cafe tables with a laptop, tablet or phone and a coffee or matcha; park Clawds on a bench, a swing or a picnic table.
- Gates live on the walls, ceiling and floor: doors that open while a Clawd passes, stairs on the back wall (drawn the way Zaviel sketched them, with a landing, a banister and an opening in the ceiling), a stair well, and in the park a fence gate, a treehouse and stone steps.
- Every test snapshot was rewritten on purpose.

![[2026-10-18 style A built office.png|250]] ![[2026-10-18 style A built coffee.png|250]] ![[2026-10-18 style A built park.png|250]]

Zaviel's sketch for the stairs and the doors that needed fixing:
![[2026-10-18 stairs as drawn (Zaviel).png|300]] ![[2026-10-18 doors to fix (Zaviel).png|200]]

→ [[Rooms and Art]]

### 🎨 A new look, picked from mockups
- The flat front view made doors and ladders look wrong, so Zaviel chose a new style from three rendered options: **A, one-point perspective** (chosen), B, 3/4 top-down, C, isometric.
- Mockup rounds settled the details: brighter rooms, a low bar with the machine on it, square chairs, stairs on the back wall with the menu on them, a stair well, a treehouse and stone steps in the park, names only for Clawds that need you.

![[2026-10-18 style options A perspective.png|200]] ![[2026-10-18 style options B three-quarter.png|200]] ![[2026-10-18 style options C isometric.png|200]]
![[2026-10-18 style A mockup coffee.png|250]] ![[2026-10-18 style A mockup park.png|250]] ![[2026-10-18 style A mockup stair well.png|250]]

### 🚪 Doors and ladders before the new look (replaced)
- Several rounds of doors and ladders at the window edge (slanted doors, rope ladders, framed hatches, a Terraria-style edge-on door, a slim ladder). The new look replaced them all.

![[2026-10-18 four windows before style A (Zaviel).png|350]]

---
## 2026-10-17

### 🎬 The camera at the gates
*Branch `feat/camera-gates`, not merged yet.*
- A Clawd walking out through a gate takes the camera with it; afterwards the camera zooms out to the whole room.
- Zooming never moves the gates. A room with only a visitor follows the visitor; an empty zoomed room waits for the first Clawd.
- `c` switches auto follow / free camera; ← → or Tab pick a Clawd; the status line shows what the camera does.
- Gates appear with a short animation, in both windows at the same moment.
- New iTerm2 windows open as a quarter of the screen.

![[2026-10-17 door option A open.png|300]] ![[2026-10-17 ladder option A.png|160]]

→ [[Interaction]]

### 🔗 Linked rooms
- Visitors step in through the door at once (they used to wait out of sight).
- `b` calls our Clawds back (or sends visitors home).
- NO SESSIONS shows only when no session runs anywhere.

### 🩹 Resizing in iTerm2
- After a resize the picture no longer stays scrolled up: the room stopped clearing the screen and parks the cursor at the top.
- Zoom magnifies only the room; words and signs stay normal size.
- Stripes while dragging come from iTerm2 re-wrapping the screen; they cannot be switched off.

![[2026-10-17 iTerm2 resize stripes (Zaviel).png|300]]

→ [[Platforms and Windows]]

### 🎥 Camera, room size and the iTerm2 helper
- `[` `]` zoom a camera that follows Clawds; `z` auto zoom.
- `r` picks the room size (AUTO, S, M, L, XL); the room grows with the window, then scales.
- On macOS the window helper is `osascript`: it finds iTerm2 windows and split panes, so gates work there too.

### 🍎 Runs on macOS
*Branch `feat/macos-iterm`, merged into `main`.*
- Builds and passes all tests on macOS; the scripts are executable; the hook works unchanged.

---
## 2026-10-16

### 🔧 The room is written in C
- The terminal room was rewritten from Python to C, checked line by line against the Python version; the Python room was removed (it is in git history). C uses a tenth of the CPU and memory.
- `clawd-room.sh` builds and starts it.

### 🧭 Opening at the right size (WSL)
- The room opens at 110x32 cells at 8 pt; the window is sized once from real window pixels.

### 🌱 New repo
- History restarted as one commit; the old history (pygame prototype, terminal room, linked windows) is archived in `zelvet-velvet/clawd-room-archive`.

---
## ⏳ Pending

- Merge `feat/camera-gates` and `feat/style-a` into `main` (after Zaviel's checks).
- `README.md` still describes the old flat look in places (banner, gates at the window edge).
- WSL: the profile from the dropdown opens at Windows Terminal's default size, then jumps once; planned a shortcut with `wt.exe -w new --size 60,18 -p "Clawd Room"`.
- The room can block writing if the terminal stops reading (seen with iTerm2 in full screen).
- Windows Terminal split panes are not seen separately yet.

## 💡 Ideas

- A seesaw in the park; an empty-desk look for Clawds that are away.
- Camera polish: smooth zoom, a slight lead in the walking direction, a small bounce when a Clawd finishes, parallax in the park.
- Full screen: a "hub" room that others visit, or several rooms side by side in one window.
