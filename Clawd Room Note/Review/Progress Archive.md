---
title: Progress Archive
tags:
  - clawd-room
  - status
up: "[[Dev Status]]"
sticker: lucide//archive
---
Finished work moved out of [[Dev Status]] (Ongoing), newest on top, one line per item by area, result only. `CMD_NOTES_SYNC` adds to it; read it only when asked or for history.

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
- **Part Animations in the Editor**: A file per animation (parts with keys), the swing driven by it, the editor's stage page with an ANIMS tab in the picker, onion skin, zoom and pan; T31 – T38 pass on WSL ([[Pixel Editor]]).
- **Pixel Editor Animates**: A timeline of an animation's frames, play at the room's speed, onion skin, a fill tool, a sprite picker, your own sprites over the defaults ([[Pixel Editor]]); T24 – T30 pass on WSL.
- **Engine Plan Built**: Scene and layers, placer, sprite and room files, pixel editor, animator; `--check` at 0 overlaps.
