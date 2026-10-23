# Linked rooms

Several rooms, side by side or stacked, can be linked: Clawds walk through doors, stairs and wells into the room next door. Back to the [README](../README.md#linked-rooms) or [Usage](Usage.md).

## How rooms find each other

Rooms on the same machine find each other and exchange small messages through files in
`~/.clawd-room/link/` (private folder, no network, no daemon). Start each with `--room NAME`. Each room writes a small
presence file: its name, size and theme, where it is on screen, which sessions it shows and which Clawds moved in.
Messages carry only a type and small values, never prompts or tool data.
`clawd-room --link-demo NAME [SECONDS]` is a tiny demo that pings other rooms.

When another window is close enough, a gate appears in the room. While you hold `g` or `m`, the bar at the bottom shows
`G SEND` and `M MOVE` (with the name of the Clawd the camera follows, or `TAB PICKS`) and one arrow per neighbour, for
example `< COFFEE DOOR` or `^ PARK STAIRS UP`. How "close enough" works: each room sets its window title to
`clawd-room <id>`, and one small helper started by the room and stopped when it quits (`powershell.exe` on WSL,
`osascript` on macOS with iTerm2) reports the screen rectangles of windows whose title starts with `clawd-room ` and
nothing else. Windows side by side with a gap of at most 30 px (`--near PX`; an attached pair stays attached until the gap
passes 45 px) count as close. On WSL it needs the room's tab to be the active tab of its window; `--no-detect` turns it
off. `clawd-room --list-windows` lists the clawd-room windows it can see. If you would rather say who is next to whom,
pass `--left`, `--right`, `--up` or `--down` with the other room's name.

## Crossing and moving

**Crossing.** An idle Clawd sometimes walks out through the gate into the next window (a `walk` message with its folder
name and height only). There it visits for 25-45 s (chats, plays, orders a coffee, uses the swing), shown with a blue
name tag, then walks back. Its desk shows AWAY meanwhile; if its session gets busy or ends, the other window is asked to
send it home. `g` sends an idle Clawd next door now, and `b` calls the visitors back.

**Moving.** `m` plus an arrow moves the Clawd you follow to the room in that direction for good. It packs its desk into a
box, carries it out, and the room it walks into claims it and unpacks the box at a new desk. Claims last only while that
room runs. Desks stay in their room.

## Gates

Windows are never moved to attach them: you put them where you like. Gates sit on the room's walls, ceiling and floor,
not at the window's edge, so the two ends do not have to line up on screen.

| Neighbour | Office and coffee shop | Park |
| --- | --- | --- |
| **Beside** (left or right) | a door in the side wall, which opens while a Clawd passes | the side wall is a brick building with a door |
| **Above** | stairs up to a half floor, with a railing | the corner building and its stoop up to a double door |
| **Below** | a small stair well in the floor at the front left | the same well, with a stone rim |

A Clawd walks (behind the railing on the stairs) through each gate; in a well it walks down the steps and disappears into
the hole.

- **Appearing.** A gate that appears while the room runs builds itself in (its parts come in one after another, or as real
  pieces that fly into place, depending on the gate theme: [Usage](Usage.md#gate-themes-and-settings)).
- **Still windows.** While a window is being moved or resized there is no gate; it appears once nothing has moved for a
  second.
- **A gate only opens when it fits.** A door needs the two floors to share enough height for a Clawd (14 room pixels);
  windows stacked above each other need to overlap sideways by at least 21 room pixels. Otherwise there is no gate and
  nobody crosses.
- The terminal's own cell size (asked with CSI 14t/16t) is used to turn screen pixels into cells.

## Sessions

Sessions are shared between linked rooms. Each session lives in one room and stays there while that room runs: a new
session goes to the room a hash of its id and the room's id scores highest for, and one moved with `m` lives in the room
it moved to. So two windows do not show the same Clawds. `--no-split` shows every session in every room.
The rooms read the same `~/.clawd-room/events.jsonl`: real Claude Code sessions (hook installed) and `simulate.py` fake
ones both show up.

Names: a Clawd is labelled with the folder name the Claude Code session was started in (the `cwd` basename the hook
records, nothing else), cut to fit its slot. A session started in a folder called `web-shop` shows as WEB-SHOP.
