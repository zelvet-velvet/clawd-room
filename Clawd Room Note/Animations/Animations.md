---
title: clawd-room - Animations
date: 2026-10-16
tags:
  - animation
  - clawd-room
sticker: lucide//film
---
# 🎞️ Animations

Every frame a Clawd can show, and the items it carries. All pictures are drawn by the real sprite code, not screenshots.

> [!TODO] Edit a frame
> 1. Open `sprites/clawd.txt` (or `sprites/items.txt`). Each frame is a small grid, one character per pixel: `.` empty, `B` body colour, `K` eye, `P` blush, `G` eye glint; the legend is at the top and you can add colours.
> 2. Change the characters. The line `@ name | key` says which pose uses the grid (leave the key alone).
> 3. `python3 sprite_tool.py build` redraws the pictures below. The room reads the same files when it starts, so a restart shows your edit.
> 4. A frame missing from the file is drawn by the code, so you can delete a frame to go back to the original. `sprite_tool.py export --force` rewrites the files from the code (**throws away your edits**); `export --add` only appends frames the file lacks.

![[sheet.png]]

Frame names in the sheet are the names in `sprites/clawd.txt`. A frame shared by two actions has the name of the first one. The body bob (up and down) is not part of a frame: it moves the whole sprite.

## Actions (looping, 3.4 s)

| Action | GIF |
| --- | --- |
| idle | ![[idle.gif]] |
| work | ![[work.gif]] |
| think | ![[think.gif]] |
| wait | ![[wait.gif]] |
| cheer | ![[cheer.gif]] |
| sleep | ![[sleep.gif]] |
| walk | ![[walk.gif]] |
| climb | ![[climb.gif]] |
| chase | ![[chase.gif]] |
| flee | ![[flee.gif]] |
| chat-nod | ![[chat-nod.gif]] |
| chat-wave | ![[chat-wave.gif]] |
| chat-hop | ![[chat-hop.gif]] |
| chat-hi5 | ![[chat-hi5.gif]] |
| chat-dance | ![[chat-dance.gif]] |
| barista | ![[barista.gif]] |
| visitor | ![[visitor.gif]] |
| stretch | ![[stretch.gif]] |
| solo-dance | ![[solo-dance.gif]] |
| swing | ![[swing.gif]] |
| slide | ![[slide.gif]] |
| jump | ![[jump.gif]] |
| wave | ![[wave.gif]] |
| applaud | ![[applaud.gif]] |
| peek | ![[peek.gif]] |

The poses are sampled from `pose_for` with a stand-in Session, so the timing in the GIFs is close, not exact.

## Items

![[items.png]]

Files: `sprites/items.txt`. Speech bubbles (`bubble-dots-0..3`, `bubble-excl`, `bubble-ques`, `bubble-cup`), the carried cup (`cup-held`; its steam is drawn by the code) and the "need your OK" sign (`sign-wait`).

> [!NOTE] Not editable yet
> Furniture per theme (desks, laptop, coffee machine, shelf, trees) and the playground (swing, slide, rope, ladder, doors) are still drawn in `themes.py` / `room_tui.py` because their size changes with the room. Next step: move the fixed-size pieces into `sprites/items.txt` the same way.
