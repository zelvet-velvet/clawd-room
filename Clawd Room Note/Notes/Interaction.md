---
title: Interaction
tags:
  - clawd-room
  - interaction
---
# 🎮 Interaction

What the keys do and how Clawds, the camera and linked windows behave. Looks are in [[Rooms and Art]], window handling in [[Platforms and Windows]].

---
## Keys

| Key | Does |
| --- | --- |
| `q`, Esc | quit (Esc closes the room size list first) |
| `p` | pause |
| `?` | help line |
| `r` | **room size list**: AUTO, S, M, L, XL (up/down or W/S, Enter picks, Esc or `r` closes) |
| `[` `]` | **camera zoom** 1x–4x (the room keeps its layout; only the view gets closer) |
| `z` | **auto zoom** on / off (frames the busy Clawds) |
| ← → or A D, Tab | while following: pick the previous / next Clawd (the camera stays on it) |
| `c` | switch **auto follow ↔ free camera** (in a room with nobody to follow it stays on auto follow, waiting) |
| arrows / W A S D | free camera only: move the view a quarter at a time |
| `g` | send an idle Clawd to the next window |
| `b` | **call back**: our Clawds next door walk home; with none away, visitors here are sent home |
| `f` | auto font and scale again (WSL and iTerm2), camera back to 1x |

## Room size and resizing

- The room **always fills the window** and takes its shape; dragging the window re-lays it out at once.
- **Grow, then scale:** the room gains floor up to the chosen size, past that every room pixel gets bigger.
- **AUTO** (default) is just big enough to seat every session with floor to walk on, and changes when sessions come and go. **S / M / L / XL** are maximum sizes: 120x60, 160x90, 220x120, 300x160 room pixels (an 80x25 window is 80x50). Flag: `--room-size`.

## The camera

- When zoomed, the camera **follows a Clawd** and only moves when that Clawd nears the edge of the view (dead zone a quarter of the view, smooth).
- **Auto follow** picks, in order: the Clawd that **needs you**, then the one that **just finished**, then the most recently **busy** one; when nothing happens it moves to the next Clawd every 10 s. Visitors count too.
- A Clawd **walking out through a gate** takes the camera to the door / stairs / well; once it has gone through, the camera **zooms out to the whole room**.
- While zoomed the status line says what the camera does: `AUTO FOLLOW (WAITING)`, `AUTO: NAME`, `FOLLOW NAME`, `WITH NAME TO THE GATE`, `FREE CAMERA`.
- Words (names, `+N MORE`) stay normal size when zoomed and never cover a Clawd.

## Linked windows

- Rooms started with `--room NAME` find each other through files in `~/.clawd-room/link/` (no network). Linked rooms **share the sessions**: each shows its own part.
- With the window helper (WSL or iTerm2), a gate appears when two windows are **next to each other** (gap up to 30 px; an attached pair stays attached until 45 px) and have been **still for 1 s**. Stacked windows get up / down gates, side by side ones get doors. iTerm2 **split panes** count as windows.
- Both windows **start the gate's appear animation together**, from the moment the windows stopped moving.
- Windows are **never moved** to attach them.

## Crossing

- Each gate has an **in** point (where a Clawd steps on, on the floor) and an **out** point (the door, the top of the stairs, the treehouse door, the bottom of the well).
- **Going out:** the Clawd walks to "in", then to "out"; when it gets there it has crossed and appears in the other window at that window's matching gate.
- **Coming in:** it appears at "out" and walks to "in", then into the room.
- A side door **opens while a Clawd is near it** and closes after.
- Idle Clawds visit the next window by themselves sometimes (20% of their plans when a gate is there). A visitor stays 25–45 s, then walks home; `b` ends visits early.

## Sessions and states

- Each Claude Code session is a Clawd. States: working / thinking (at its workspace), needs you (yellow), just finished (cheer, 2.5 s), idle (walks, visits, plays), asleep after 90 s idle, gone after 30 min without events.
- **NO SESSIONS / START CLAUDE** shows only when **no session runs anywhere**; a linked room whose share happens to be empty just shows an empty room.
- Idle play: wave, dance, stretch, jump; in the park also the swing and the slide; in the coffee shop customers order from the barista.
