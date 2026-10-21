---
title: Dev Status
tags:
  - clawd-room
  - status
sticker: lucide//activity
---
> [!important] For AI assistants
> Read [[LLM Guide]] before any `CMD_` or status update. This note is kept by `CMD_SYNC` / `CMD_ROLLOVER` / `CMD_NOTES_SYNC`; the day notes in `Review/Log/` by `CMD_JOURNAL`.

## 🚀 Today's Progress (2026-10-21)
- **Editor Animation Tests Pass**: T24 – T30 pass on WSL (timeline, play, onion skin, fill, frames of props), so the whole queue T1 – T30 passes.

## 📅 Yesterday's Progress (2026-10-20)
- **WSL Test Queue Done**: T16 – T23 pass, so all of [[Test Checklist]]'s queue (T1 – T23) passes on WSL.
- **Pull Requests**: #6 (WSL work), #7 (vault layout), #9 (mockups), #10 (editor), #11 (test names) and #12 (PR icon) merged.
- **Pixel Editor Animates**: A timeline of an animation's frames, play at the room's speed, onion skin, a fill tool, `--edit list` by animation (#10).
- **Made-Up Test Names**: The simulator and its test data used real work folder names; now made up (`web-shop`, `api-server` …). Rule added: nothing private in the repo, notes or pictures.
- **Designs Picked** (mockups, [[2026-10-20]]): the picked Clawd's corners and spotlight; the bar below the room with tags on the gates; a settings file and panel; gates appearing as a construct of real parts (or simple); a railed stair well; no tree behind the park's building.

---

## ⏳ Ongoing

### 🎨 Rooms and art
- [ ] **Railed Stair Well**: Build design A (a bigger opening, steps right to left, a guard rail); no big tree in the park when it has a way up ([[2026-10-20]]).
- [ ] **Gates Appear**: A setting: simple (the parts in turn, nothing popping in at the end) or construct (real parts fly into a blue outline, 1.4 s).
- [ ] **README Look**: `README.md` still describes the old flat look in places (banner, gates at the window edge).

### 🎮 Interaction
- [ ] **Picked Clawd**: Yellow corners and a spotlight (60 in 100) that fade in and out on Tab.
- [ ] **Bar Below the Room, Gate Tags**: The room above a one-line bar; while `g` / `m` is held a tag centred over each gate (arrow style still to pick).
- [ ] **Settings**: `~/.clawd-room/settings.conf` and a panel beside the room on `.`.
- [ ] **Moving, Step 3**: Drag a Clawd with the mouse onto a door, the stairs or the well. (Settled: `--try real` stays split; uneven shares are fine.)

### 🖥️ Platforms
- [ ] **Mac Check**: Run `make -C c test` on the Mac after the fused multiply-add change (it should still match).
- [ ] **Blocked Writes**: The room can block writing when the terminal stops reading (seen with iTerm2 in full screen).
- [ ] **Windows Terminal Panes**: Split panes are not seen separately yet.

### ⚙️ Engine and tools
- [>] **Animations**: `--build-art` to record room animations (packing, carrying, unpacking, doors, stairs, the well, each room) as GIFs, in category folders, with Gallery notes. Started (GIF colour rounding, category folders), parked in a stash.
- [ ] **Gate Styles**: Gates as parts with keyframes that developers can add, edited on a gate page of the editor; picked, not built (#13).
- [ ] **Less CPU**: Skip frames when nothing moves; stop drawing (FPS 0) when the window is covered, minimised or on another desktop; later dirty rects. (The terminal output is already diffed and written once per frame.)

### 🗂️ Repo and vault
- [>] **Pull Requests**: #13 (gate styles and gate keyframes: the plan and a mockup) waiting for Zaviel; #1 – #12 merged.
- [ ] **Go Public** (decided 2026-10-20: code and notes public together): at a quiet moment with no open PRs, look at the six `(Zaviel)` screenshots; rewrite the old fixture names out of the history (`git filter-repo`; everyone re-clones once); add a `LICENSE` (MIT) and an unofficial-fan-project line to the README (Clawd is Anthropic's character); make the repo public; then **restrict merging**: protect `main` (no direct pushes, changes only through a pull request, Zaviel's approval to merge).
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
- **WSL Tests**: Every check in the WSL test queue passes (T1 – T30): profile, rooms, bar, sending and moving, desks, the pixel editor.
- **C Room**: The terminal room in C11 (a tenth of Python's CPU and memory).
- **macOS + iTerm2**: Builds and runs; window helper with osascript; fixed pixel size profile; `--try`.

### ⚙️ Engine and tools
- **Pixel Editor Animates**: A timeline of an animation's frames, play at the room's speed, onion skin, a fill tool, a sprite picker, your own sprites over the defaults ([[Interaction]]); T24 – T30 pass on WSL.
- **Engine Plan Built**: Scene and layers, placer, sprite and room files, pixel editor, animator; `--check` at 0 overlaps.
