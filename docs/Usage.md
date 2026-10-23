# Usage

Everything the room can do from the command line, with the keys, the themes, the room size and the camera. For a first start see the [README](../README.md#quick-start); for windows next to each other see [Linked rooms](Linked-Rooms.md); for the terminal and the platforms see [Platforms](Platforms.md).

## Commands

    ./clawd-room.sh --room park               # window 1
    ./clawd-room.sh --room coffee             # window 2
    ./clawd-room.sh --edit                    # the pixel editor on its sprite picker: cards with pictures (or a list), type to find;
                                              # your edits go to ~/.clawd-room/sprites (shown over the defaults; RESET in the picker)
    ./clawd-room.sh --edit list               # what the pixel editor can open: props, items, Clawd animations and their frames
    ./clawd-room.sh --edit window             # the pixel editor (its own window): draw a prop with the mouse, colours in a
                                              # Figma-like picker; rooms show it at once; Ctrl+Z / Ctrl+Y undo / redo;
                                              # ? shortcuts; q: keep or discard
    ./clawd-room.sh --edit walk               # an animation: its frames in a timeline (arrows: frame), Space plays it at the
                                              # room's speed, o onion skin, b fill (again: fill all), f / x x frames (props)
    ./clawd-room.sh --try                     # iTerm2 / WSL: four linked rooms in the four quarters, with made-up Clawds
                                              # (quit one to close all; --try close; --try real for real sessions only;
                                              # WSL: --try --desktop N opens them on virtual desktop N without switching to it)

## The room and its keys

    ./clawd-room.sh                           # draws in the terminal (works in a 60x20 pane or fullscreen); builds c/ on the first start
    ./clawd-room.sh --dump 100x30 --events /tmp/e.jsonl --at 1800000010   # one plain-text frame, for tests

Keys: `q` or Esc quit, `p` pause, `?` help line, `r` room size list, `[` `]` camera zoom, `z` auto zoom, left / right (A / D) or Tab pick the Clawd the camera follows, `c` follow / free camera (then the arrows or W A S D move it), `g` send a Clawd next door (hold `g` + an arrow: which way), `m` + an arrow move the picked Clawd to that room (it packs its desk into a box and lives there while the rooms run), `b` call your Clawds back (or send visitors home), `f` fit. Needs at least 44x20 cells. Messages, the help line and the `g` / `m` hint show in a bar along the bottom of the room. The hook must be
installed once (`python3 install_hooks.py` shows what it would change) or the room stays empty.
Needs a truecolor terminal (Windows Terminal is fine). It only reads the events file.

In the terminal version the Clawds move around: busy ones sit at their desk (typing, thinking,
waving for OK, cheering, sleeping), idle ones walk around the room and sometimes visit another idle
Clawd to chat, wave (hearts), hop, high-five, dance or play tag; they also watch a busy friend work,
applaud when a friend finishes, and do little solo acts (stretch, dance, jump, wave). New sessions walk in through the left side, ended ones walk out.

## Themes

Themes (`--theme office|coffee|park`, by default guessed from the room name):
- office: desks and monitors.
- coffee: the oldest session is the barista (apron, espresso machine behind a counter), the others
  are customers at cafe tables; idle customers go and order a coffee and carry it back.
- park: picnic tables with laptops; idle Clawds play on the swing and the slide.

## Room size

Room size: the room always fills the window and takes its shape; dragging the window resizes it live. It grows (more
floor, more desks) up to the chosen room size, then the pixels get bigger instead (grow, then scale). `r` opens the ROOM SIZE
list (up/down or W/S, Enter picks, Esc or `r` closes; `--room-size auto|s|m|l|xl`):
- AUTO (default): just big enough to seat every session with floor to walk on; it changes when sessions come and go.
- S 120x60, M 160x90, L 220x120, XL 300x160 room pixels: the biggest the room may be (an 80x25 window is 80x50 room pixels).
(`--ratio 3:1|2:1|16:9|4:3|1:1|3:4` still pins an old fixed shape; it is kept for the tests.)

## Camera

Camera: `[` and `]` zoom a camera into the room (1x to 4x; `--zoom N` starts zoomed); the room keeps its layout, only the
view gets closer. When zoomed the camera follows a Clawd and only moves when that Clawd nears the edge of the view. Auto
follow (default) picks the Clawd that needs you, else the one that just finished, else the most recently active one, and
takes turns every 10 s when nothing happens; visitors count too (a room with only a visitor follows it). While following,
left / right (A / D) or Tab pick the previous / next Clawd and stay on it; `c` switches to a free camera (only then do the
arrows or W A S D move the view) and back to auto follow. While zoomed, the status line says what the camera does
(`AUTO FOLLOW (WAITING)`, `AUTO: NAME`, `FOLLOW NAME`, `FREE CAMERA`); in a room with nobody to follow `c` stays on auto
follow, so the first Clawd to come in is followed. `z` turns auto zoom on or off: the zoom then frames the busy Clawds (close on one, wider for several).
A Clawd walking out through a gate takes the camera to the door, stairs or well; once it has crossed the camera zooms out to
the whole room (not in free mode). Zooming never moves the gates: they stay where they are in the room.

## Part animations: the stage page

    ./clawd-room.sh --edit anims/swing         # a part animation of anims/ on the editor's stage page (or the ANIMS tab of the picker)
    ./clawd-room.sh --edit list                # names them too: anims/swing, anims/brew

A part animation is a thing of the room made of parts with keys (a key: a part's pose at one frame; the frames between are worked out). The file format is in the header of `anims/swing.txt`; what you change is saved to `~/.clawd-room/anims/` (yours, used over the default).

| Key / mouse | Does |
| --- | --- |
| click a part (stage or list), `[` `]` | pick a part |
| SELECT, drag a handle | resize the part's box |
| MOVE / ROTATE, drag on the stage | move / turn the part pressed on, at this frame (a sprite does not turn) |
| the KEY cap, `k` / `x` / `e` | a key at this frame or none (pressed in: there is one) / delete it / its ease |
| left / right, click a frame | the frame |
| up / down, `<` `>` | pick a value, change it |
| `+ -`, wheel; `W A S D`, right drag; `0` | zoom; move the view; as it was |
| `o`, `v` | onion skin; the keys' marks on the stage |
| `p` or the fold button; drag the line above the frames | fold the panel to a ribbon of the tools; fold the timeline to its one line |
| Space, `u` / `r`, `?`, `q` | play; undo / redo; the keys; quit (save or discard) |
| `i` | the pixel art of a sprite part (the swing's Clawd): the picker on the CLAWD tab; `q` there comes back (Esc only steps out of the search box). Parts drawn by code (the gates) say so |

## Gate themes and settings

How a gate appears is a **theme**: a folder `anims/gates/NAME/` with `gate-door.txt`, `gate-stairs.txt` and `gate-well.txt`. `simple` is the default (the parts come in turn); `construct` builds the gate from its own pieces that fly in under a blue outline (1.4 s: its one part `build` is the clock, drag the GROW bar on the stage to watch it). A theme that lacks a file uses `simple`'s, so a new theme can start with one file; your edits go to `~/.clawd-room/anims/gates/NAME/`.

    ./clawd-room.sh --gate-theme construct     # for this run
    CLAWD_GATE_THEME=construct ./clawd-room.sh # the same

Or in `~/.clawd-room/settings.conf` (read again while the room runs; the flag and the variable win over it):

    # the gate theme
    gate_theme = construct

In the pixel editor's picker (`./clawd-room.sh --edit`) the **THEMES** tab has a card per theme (its door, stairs and well side by side; the active one says ACTIVE). Arrows pick one; **Space** (or the MAKE ACTIVE button, or the INACTIVE label of an open theme); on the active theme (not simple) the same key, MAKE INACTIVE or a click on its ACTIVE label makes `simple` the active one again makes it the active theme, by writing `gate_theme` into `settings.conf`; **Enter** (or a click on the picked card) shows its three files on the ANIMS tab (Esc comes back); the dashed **NEW THEME** card asks for a name and makes a copy of the theme you last picked under `~/.clawd-room/anims/gates/NAME/`. RESET on a theme card removes your files of it; MAKE DEFAULT copies them into the repo's `anims/gates/`.
