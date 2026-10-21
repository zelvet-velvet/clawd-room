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
A Clawd walking out through a gate takes the camera to the door, rope or stairs; once it has crossed the camera zooms out to
the whole room (not in free mode). Zooming never moves the gates: they line up with the next window as at 1x.

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
| Space, `u` / `r`, `?`, `q` | play; undo / redo; the keys; quit (save or discard) |
