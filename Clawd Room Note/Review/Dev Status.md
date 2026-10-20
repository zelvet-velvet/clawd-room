---
title: Dev Status
tags:
  - clawd-room
  - status
sticker: lucide//activity
---
> [!important] For AI assistants
> Read [[LLM Guide]] before any `CMD_` or status update. This note is kept by `CMD_SYNC` / `CMD_ROLLOVER` / `CMD_NOTES_SYNC`; the day notes in `Review/Log/` by `CMD_JOURNAL`.

## 🚀 Today's Progress (2026-10-19)
- **WSL Up to Date**: All tests pass on WSL; the build uses fused multiply-add on both platforms, so the Mac snapshots match.
- **`--try` on WSL**: Four linked rooms open at once and become a Windows snap group (first room snapped with Win+←+↑, the others picked in Snap Assist, about 2.3 s); `--try --desktop N` for another virtual desktop; a `--try` that closed itself (an old one quitting late) fixed.
- **5 pt Profile**: The Clawd Room profile is 5 pt and a room opens as its screen quarter, like iTerm2 (`setup_profile.py --update`).
- **Pixel Editor**: Opens in its own window on WSL; a Figma-like colour picker (picked from three mockups); the shortcuts no longer run over the colours (`?` box in short windows); **Ctrl+Z / Ctrl+Y** undo and redo.
- **Collaboration**: William Pan invited; `CONTRIBUTING.md`, [[Collaboration]], a PR template; every change through a branch and a PR, Zaviel merges.
- **Test Queue**: [[Test Checklist]] grouped with test IDs (T1 … T23, Q1); WSL T1 – T15 ticked by Zaviel (profile, rooms, bar, sending and moving, desks, the editor window and painting).
- **Pull Requests**: #1 – #4 merged (collaboration docs, `--try` stop reason, test queue, screenshot folders); `feat/style-a` and `feat/camera-gates` deleted.
- **Vault**: Reorganised (Start, Review, Design, Gallery, Assets); one Dev Log note per day in year / month / week folders with a Day note template; snapshots marked current / replaced / not picked; this status note and the `CMD_` set.

## 📅 Yesterday's Progress (2026-10-18)
- **Style A**: The three rooms in one-point perspective, with stairs to a half floor, a stair well, doors, and the park as a city plaza with buildings.
- **Engine**: Scene and layers, the placer (overlaps 107 → 0), props and rooms as text files with a pixel editor, an animator for gates and doors.
- **`--try` on iTerm2**: Four linked rooms in one command with made-up Clawds.
- **Gates Stay Put**: Gates no longer come and go between linked rooms; `g` + an arrow picks where to send a Clawd.
- **iTerm2 Profile**: A fixed 4x4 pt room pixel; the profile starts the room in a screen quarter.

---

## ⏳ Ongoing

### 🎨 Rooms and art
- [ ] **README Look**: `README.md` still describes the old flat look in places (banner, gates at the window edge).

### 🎮 Interaction
- [ ] **Moving, Step 3**: Drag a Clawd with the mouse onto a door, the stairs or the well. (Settled: `--try real` stays split; uneven shares are fine.)

### 🖥️ Platforms
- [>] **WSL Test Queue**: Group E, the pixel editor: T16 – T23 left (T14, T15 pass) in [[Test Checklist]].
- [ ] **Mac Check**: Run `make -C c test` on the Mac after the fused multiply-add change (it should still match).
- [ ] **Blocked Writes**: The room can block writing when the terminal stops reading (seen with iTerm2 in full screen).
- [ ] **Windows Terminal Panes**: Split panes are not seen separately yet.

### ⚙️ Engine and tools
- [>] **Animations**: `--build-art` to record room animations (packing, carrying, unpacking, doors, stairs, the well, each room) as GIFs, in category folders, with Gallery notes. Started (GIF colour rounding, category folders), parked in a stash.
- [ ] **Less CPU**: Skip frames when nothing moves; stop drawing (FPS 0) when the window is covered, minimised or on another desktop; later dirty rects. (The terminal output is already diffed and written once per frame.)

### 🗂️ Repo and vault
- [>] **Pull Requests**: #5 (PR icon), #6 (WSL work), #7 (vault layout, after #6) waiting for Zaviel.
- [ ] **William's Setup**: Accept the invite, clone, Templater settings ([[Collaboration]]).

## 💡 Ideas
- A seesaw in the park; an empty-desk look for Clawds that are away.
- Camera polish: smooth zoom, a slight lead in the walking direction, a small bounce when a Clawd finishes, parallax in the park.
- Full screen: a "hub" room that others visit, or several rooms side by side in one window.

---

## 🏁 Last Progress

### 🎨 Rooms and art
- **Style A Rooms**: Office, coffee shop and park in one-point perspective; workspaces in rows; Clawds behind desks, the barista behind the bar.
- **Stairs and Wells**: Real steps to a half floor with a railing Clawds walk behind; a stair well at the front left they walk down into.
- **Park Plaza**: Skyline, promenade, lamps, benches; side buildings with doors; one corner building with a stoop as the way up.

### 🎮 Interaction
- **Camera and Room Size**: Zoom, follow, free camera, room sizes; the camera goes through gates with a Clawd.
- **Linked Rooms**: Gates on walls, ceiling and floor; `g` sends, `b` calls back; `m` moves a Clawd with packing and unpacking; desks stay in their room.
- **Bottom Bar**: Status and the `g` / `m` hint in the room's pixel font.

### 🖥️ Platforms
- **C Room**: The terminal room in C11 (a tenth of Python's CPU and memory).
- **macOS + iTerm2**: Builds and runs; window helper with osascript; fixed pixel size profile; `--try`.

### ⚙️ Engine and tools
- **Engine Plan Built**: Scene and layers, placer, sprite and room files, pixel editor, animator; `--check` at 0 overlaps.
