---
title: Pixel Editor
tags:
  - clawd-room
  - editor
sticker: lucide//brush
---
> [!abstract] In short
> `./clawd-room.sh --edit` draws the sprites, props and part animations the rooms use; running rooms show each stroke. How the files are read and written: [[Art Data and Editor]]. The rest of the keys: [[Interaction]].

---
## 🎨 Sprites and the picker

`./clawd-room.sh --edit` (no name) opens it on the **sprite picker**: every sprite and animation as a **card** with its picture (the picked one plays; a tab per file: PROPS, ITEMS, CLAWD, UI) or, with the view buttons at the top right or `\`, as a **list** by file with the picked one big, its file, size and frames. Type to find (the cards move to the first tab with a match); arrows pick, Enter or a click on the picked one opens it; `q` (or `l`) while editing goes back to the picker, the sprite you were on picked (Enter reopens it); if the sprite changed it first asks **S** keep or **D** discard (back to how it was when you opened it; Esc stays). In the picker the **search box** has the focus as it opens (a yellow frame): letters go into it and **`q` quits** while it is empty. A click elsewhere leaves it (letters do nothing, `q` quits); a click on it focuses it, and then `q` is typed like any letter. Esc clears the search, then leaves a clicked box, then quits. `./clawd-room.sh --edit NAME` opens it in the middle of the screen; the window you typed in stays, and you are back there when you quit. NAME is a sprite (`walk-2`, `window`) or an animation (`walk` opens its first frame). A name that is not there is said in the terminal you typed in, with the closest names (`did you mean: walk`), and no window opens. `./clawd-room.sh --edit list` prints what it can open, by file: each animation once with how many frames it has, in columns that fit the terminal. It saves after each stroke, so running rooms show the change; on quitting you choose to keep or discard.

**Your own sprites over the defaults.** The repo's `sprites/*.txt` are the defaults. What you change is written only to **`~/.clawd-room/sprites/`** (the same file names, holding just the colours you added or changed and the sprites that differ, or that only you have); the room and the editor show yours over the defaults (a sprite painted back to the default stops being yours; a colour you added that nothing uses any more is left out when saving, so a colour picked by mistake and erased or undone goes away; the defaults' colours always stay). In the picker a **blue dot** marks yours; the bar above the hints shows **YOURS: N** (left) and **RESET** (the picked one back to the default), **RESET ALL** (asks first) and **MAKE DEFAULT** (yours in the picked one's file written into the repo's file, then yours goes: new official art, through a commit). Deleting a frame works only on your last added one (the defaults stay).

**Animations.** An animation is the sprites named `NAME-1`, `NAME-2` … (Clawd frames, items) or `NAME`, `NAME~2`, `NAME~3` … (props). The **timeline** along the bottom shows its frames (twice their size when they fit; a long one scrolls to keep the open frame in view): the open one framed in yellow, the one playing marked under it; click one, or ← →, to open it. On the left: `? SHORTCUTS  Q QUIT` in a short window, and under it the sprite **as in the room**, as big as the column lets it be (the frame playing while PLAY is on). On the right (a line between the two sides): the animation's name and frame, ONION / the speed / PLAY, and the frames, at one size for the window (2x or 1x as its height allows, however many there are; the timeline takes at most a quarter of the window); more than fit **scroll sideways** (the mouse wheel over them; a scroll bar under them; opening a frame brings it into view). **Drag the divider** above the timeline (its handle, a short bar in the middle, lights up under the pointer) to make the timeline taller or shorter: the frames and the sprite on the left grow with it; the canvas and the panel keep at least their tools, colour field and hex line (the panel leaves out the shades first, then rows of the file's colours). Dragged down past its smallest size it **folds** to its top line (drag it up again for the frames). Double-click the divider: back to the automatic height.

**Folding the colour panel** (like Obsidian's sidebar): the button at the top right (a window with its right part filled while the panel is open) or `p` folds the panel to a **ribbon** at the right edge (the editor opens with it folded): the four tools one over another and the current colour; the canvas takes the room, in the middle of it. Again to unfold.

**The editor's own icons** (the tools and the fold button) are sprites in **`sprites/ui.txt`** (`tool-paint`, `tool-erase`, `tool-pick`, `tool-fill`, `tool-fill-all`, `fold-open`, `fold-closed`, and the picker's `view-cards`, `view-list`: each a small picture of its page): edit them in the editor like any sprite (`--edit fold-open`, or Tab to the file); the editor reads the file again when it changes. One left out: the built-in drawing. The fold button is drawn dim and brightened under the pointer.
- **PLAY** (Space) runs the animation **at the room's speed** in the grid and the real-size view (a click on the grid stops it, to draw on the open frame): Clawd frames through the room's own poses (`pose_for` with the stand-in sessions of `actions.c`, the same as the sprite sheet), so the order, the timing, the bounce and the blinks are the room's; a pose the file has no frame for is drawn by the code, as in the room. Props change every 0.3 s, the thinking dots three times a second. **1X** switches 1x, 2x, 0.5x (not saved). When the panel has no room for the real-size view, it plays at the right end of the timeline.
- **ONION** (`o`, on at the start): where the open frame is see-through, the frame before shows faintly red and the one after blue (both: purple), to draw the motion in between.
- **New and deleted frames are for props only** (`f` a copy at the end, `+` in the timeline; `x` twice deletes the open one, never the first: that is the prop; the ones after it are numbered on, since the room stops at the first missing `~N`). Clawd and item frames are picked by the room's code, so a new one would never be shown.

| Input | Does |
| --- | --- |
| left click / drag | paint with the chosen colour |
| right click | erase (see-through) |
| Alt + click, or **PICK** then click | take the colour under the pointer |
| **FILL** (`b`) | a click fills the touching pixels of the colour under it (right click: erases them); the area it would fill is outlined in dashes under the pointer. Click FILL again for **FILL ALL**: every pixel of that colour in the frame. (Shift + click is kept by Windows Terminal and iTerm2 for selecting text, so it is a mode, not a modifier.) |
| the panel | the tools as **icons on key caps**, their names under them (PAINT a pencil, ERASE an eraser, PICK an eyedropper, FILL a paint bucket, with a sparkle for FILL ALL; the one in use pressed in, a yellow line under it); a **colour field** (saturation across, brightness down) and a **hue bar** like Figma's; the colour's **hex** and **red / green / blue**; **IN THIS FILE**, the colours to paint with (a ring: the one in hand; a dot: used in this sprite). Its first block is the **+ slot**: changing the colour (field, hue bar, typing) never changes a file colour but makes a new one there (an empty + slot clicked: a new colour from the one in hand); painting with it, or clicking the filled slot, puts it in the file. **Double-click** a file colour to change it in the file instead (every pixel using it, in the whole file; its ring yellow); **SHADES** darker to lighter, as many as a row of the file's colours. A wider window makes the panel wider (beside the real-size view when that goes right of it): more colours in a row, more shades; the sprite **as in the room** |
| typing a colour | click the **hex code** (or press `#`) and type `RRGGBB` (or `RGB`), or click a **red / green / blue number** and type 0–255; Enter applies, Backspace deletes, Esc cancels |
| pointer over the grid | the pixel is outlined; its place and colour show under the grid |
| Ctrl+Z / Ctrl+Y (`u` / `r`) | undo / redo (Ctrl+C asks like `q`) |
| ← → (or `<` `>`, `,` `.`) | the frame before / after in the animation (wrapping) |
| `[` `]` | the sprite before / after (the first frame of the next animation); Tab: the next file (props, items, Clawd) |
| Space / `o` | play / stop; onion skin on / off |
| `p` | fold / unfold the colour panel (the button at the top right does the same) |
| `q`, `l` | back to the sprite picker, asking keep / discard if the sprite changed (`q` there quits; Ctrl+C quits from anywhere, asking about all changes) |
| `f` / `x` `x` | a new frame / delete this one (props) |
| `+` `-` | zoom |
| hold `w` / `n` + an arrow | grow / shrink the sprite on that side (← left, → right, ↑ top, ↓ bottom); on the left or the top the drawing moves over by one. While `w` or `n` is held a hint says so; let go, the arrows step through frames again |
| `?` | the shortcuts in a box (a short window shows only `? SHORTCUTS  Q QUIT`; a very short one has no timeline) |
| Esc in the picker, Ctrl+C | quit: with changes it asks **S** save, **D** discard (the files go back to how they were when you opened them), **Esc** back |

![[2026-10-20 editor timeline, onion skin and fill.png|774]]

![[2026-10-20 editor playing a Clawd at the room's speed.png|774]]

---
## 🎬 The stage page: part animations

A **part animation** is a file in `anims/` (`swing.txt`, `brew.txt`; the format is in the header of `swing.txt`): a thing made of **parts** (the swing's bar, chains, seat and Clawd; the coffee machine, cup, steam and barista), each with **keys**. A key is a saved pose of one part at one frame: where it is (move x / y), rotate, show, glow, and the ease into it. Between keys the part is moved for you (the frames in between are worked out), so a few keys make the whole animation. A part marked `sprite` (a Clawd) moves but does not rotate (a turned pixel drawing breaks up). The room plays the swing's Clawd from `swing.txt`; your edits go to `~/.clawd-room/anims/` and are used over the defaults, read again when they change.

**Gates appearing** are part animations too: `anims/gate-door.txt` (a door on a side wall), `gate-stairs.txt` (the way up: steps, platform, opening, rail) and `gate-well.txt` (the way down). Each part comes in by its **GROW** (0 not there, 100 whole); the files hold what the room always did (every part 0 to 100 in 0.8 s, ease out), so nothing looks different until you change them. Their stage is the office with that gate appearing (`scene still`, `scene appear SIDE`). On the page, GROW is the bar under the panel beside a small running Clawd (no words; faster while you drag the bar or PLAY runs; the sprites `grow-walk-1` … `-4` in `sprites/ui.txt`, the picker's UI tab, so they can be repainted): click or drag it to set how much of the picked part has come in at this frame. The room uses only GROW for a gate part; the other values are for later.

`./clawd-room.sh --edit anims/swing` (or the **ANIMS** tab in the sprite picker, Enter) opens it on the stage page, laid out as the editor (layout B, picked 2026-10-21):

- **The stage** (where the canvas is): the room with the thing, every part outlined (the picked one yellow), the picked part's keys as small marks joined in time order (`v` or the **eye + KEYS** button under the stage hides them: the eye shuts); **ONION** (`o`) shows the frame before faintly red and the one after blue. `+ -` or the wheel zoom it, `W A S D` or a right drag move the view (it may go past the room's edge by half a view, so a part at the edge can be in the middle); the view puts the picked part in the middle when the page opens and when a part is picked from the list or with `[ ]`; `0` zooms back and does the same.
- **The tools** work on the part pressed on (pressing where there is none does nothing): **SELECT** picks a part and shows handles on its border (drag one to resize the part's box); **MOVE** drags the part pressed on (it stays inside the stage); **ROTATE** drags its angle (greyed for a sprite, and a sprite cannot be picked while it is on); **KEY** is not a tool but a toggle beside them: pressed in when this frame is a key of the picked part; a click puts a key here or takes it away.
- **The panel**: the picked part, its values at this frame with `<` `>` (`< >` keys; up / down picks a value), the key's **EASE**, the parts. It scrolls in a short window.
- **The timeline**: the frames as pictures with a diamond under each key of the picked part, the real-size view at its left, PLAY and the speed; its scroll bar and the wheel scroll it. Drag the line above it down to fold it to its one line, up to open it (the editor's grip).
- **Folding**: `p` or the fold button (top right, the editor's) folds the panel to a ribbon of the four tools (no values, no grow bar) and the stage takes the room. Open, the panel has the same gap (9 pixels) to the stage as to the window's edge.
- A change makes a key at this frame if there is none. `u` / `r` undo / redo, `?` the keys (scrolls), `q` asks like the editor (S save, D discard, Esc back); from the picker, `q` comes back to it.

> [!todo]- Picked, not built yet
> - **Rotation drawn in the room**: only the stage's outline turns; how the swing's chains and seat and a Clawd should look turned is parked (a mockup of three ways, 2026-10-21, in `gallery/mock/out/19-rotate/`, not picked).
> - **More cases in the room**: the barista's brewing; gates leaving, the park's own gates, and the construct style (the office's gates appearing are built).
>   ![[2026-10-21 part animations in the editor, a Clawd on the swing (now, A dope sheet, B frames, B picked).png|300]]
>   ![[2026-10-21 part animations in the editor, the barista brewing (now, A dope sheet, B frames, B picked).png|300]]
