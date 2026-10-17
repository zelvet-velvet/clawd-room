---
title: Rooms and Art
tags:
  - clawd-room
  - ui
  - art
---
# 🎨 Rooms and Art

How the rooms look and the rules the drawing follows. History is in [[Dev Log]]; keys and behaviour in [[Interaction]].

---
## The style: one-point perspective ("style A")

Picked by Zaviel on 2026-10-18 from rendered options (perspective, 3/4 top-down, isometric).

![[2026-10-18 style options A perspective.png|300]]

- The **back wall** sits in the middle, the **side walls** slant in towards it, the **ceiling** is a stylised band.
- The **floor and everything on it** come from one vanishing point (where the floor's side edges meet, above the picture). Code: `c/persp.c`.
- The floor **leans forward a little**: the back wall spans 17%–83% of the width and 14%–47% of the height (`VIEW_BL / BR / BT / BB`).
- Furniture keeps the **same size on screen at any depth**, like the sprites; only its shape follows the perspective.

## The three rooms

| Room | Look | Workspaces (one per session) |
| --- | --- | --- |
| **Office** | light walls, a window, a plant, wooden floor boards | a desk with a monitor (the screen shows the tool's colour) |
| **Coffee shop** | warm walls, a checker floor, hanging lamps, a shelf of cups | the first session is the **barista** behind a **low bar** with the espresso machine on it; the others sit at **round cafe tables** with a square cushioned chair (and an empty one), a **laptop, tablet or phone**, and a **coffee or matcha** |
| **Park** | sky, clouds, hills, mowed grass, bushes, a big tree, a swing and a slide | a **bench** (phone), a **swing** (tablet) or a **picnic table** (tablet) |

The device, drink or park thing for a session is picked from a hash of its id (`variant`), so it stays the same.

![[2026-10-18 style A built office.png|260]] ![[2026-10-18 style A built coffee.png|260]] ![[2026-10-18 style A built park.png|260]]

## Layout

- Workspaces stand in **rows on the floor**, filled from the back; the **walking strip** is in front of the last row, down to the front edge.
- A Clawd **sits behind its workspace**: its body top is `SEAT_DY` (−12) above the furniture's front edge, so the furniture hides its lower half.
- Furniture and Clawds are **painted back to front** (by where they stand), so nearer things cover farther ones.
- When there are more sessions than places, the last place says **+N MORE**.

| Constant | Value | Meaning |
| --- | --- | --- |
| `ROW_PITCH` | 15 | room pixels between rows |
| `SLOT_MIN_W` / `SLOT_MAX_W` | 24 / 40 | a workspace's width |
| `ROW_MARGIN` | 3 | kept free at the floor's sides of each row |
| `WALK_MIN` | 16 | floor kept in front of the last row |
| `SEAT_DY` | −12 | a seated Clawd's body top above its furniture's front edge |

## Gates (the ways to the next window)

Gates sit **on the room's walls, ceiling and floor**, not at the window edge, so the two ends do not have to line up pixel for pixel.

| Way | Office / coffee shop | Park |
| --- | --- | --- |
| **Side** | a **door** on the side wall facing the neighbour: frame, leaf, knob; it **opens while a Clawd passes** | a **fence** along that side with a **gate** |
| **Up** | **stairs on the back wall** (see below) | a **treehouse**: a little house on a platform in the big tree, a ladder up to it |
| **Down** | a **stair well** at the front: a trapezoid opening, side walls going down, steps dropping away and getting darker, a railing on three sides | **stone steps** going into the ground, low stone walls round them |

**The stairs (as Zaviel drew them):** they start a fifth of the way along the back wall (`STAIRS_FOOT` 0.2), climb steeply to just under the ceiling (`STAIRS_TOP` 0.45), then a **landing** runs along under the ceiling to the right-hand wall. Their **side wall** fills the shape down to the floor (the coffee shop's **menu** hangs on it). A **banister** (hand rail and balusters) runs up the stairs and along the landing. The way up is an **opening in the ceiling** at the top of the stairs.

![[2026-10-18 stairs as drawn (Zaviel).png|360]]

**Appearing:** a gate that appears while the room runs animates in over `GATE_ANIM` (0.8 s): doors and fences rise from the floor, stairs appear step by step, the well opens, the treehouse ladder grows. Gates there from the first frame are simply there.

## Text on screen

- **No top banner.** The room's name is a **sign in the room**: a plaque on the office wall, a shop sign in the coffee shop, a signpost in the park. With stairs in the room the sign **hangs from the ceiling in front of them**.
- Over each Clawd's head a **dot**: green busy, yellow needs you, grey idle, blue a visitor. The **name shows only when it needs you**.
- **NO SESSIONS / START CLAUDE** only when no Claude Code session runs anywhere (see [[Interaction]]).
- When zoomed, words are written at the normal size and never over a Clawd.

## Rules for the art

- **The Clawd looks official:** flat `#CD6E58`, a 14x8 sprite (8-wide torso, 2x2 arms, four 1-pixel legs), 1x1 dark eyes, no mouth. Happy eyes are 1 wide and 2 high (a pale peach top over the dark pupil) plus blush. Frames are editable in `sprites/*.txt`; [[Animations]] shows them all.
- **Pixel text is upper case only** (the 3x5 font has no lower case); keep words short.
- Room pixels are square on screen: a terminal cell is 1 room pixel wide and 2 high.
- **Visual changes start with rendered mockups** that Zaviel picks from, then the code.
