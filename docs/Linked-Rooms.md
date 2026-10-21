# Linked rooms

Several rooms, side by side or stacked, can be linked: Clawds walk through doors, stairs and wells into the room next door. Back to the [README](../README.md#linked-rooms) or [Usage](Usage.md).

## How rooms find each other

Rooms on the same machine find each other and exchange small messages through files in
`~/.clawd-room/link/` (private folder, no network, no daemon). The banner shows `LINK COFFEE`,
with an arrow (`>` or `<`) and a glowing door on that side of the room when the other window is
close enough on the Windows desktop. `clawd-room --link-demo NAME [SECONDS]` is a tiny demo that pings other rooms.
Messages carry only a type and small values, never prompts or tool data.

How "close enough" works: each room sets its window title to `clawd-room <id>`, and one small
`powershell.exe` (started by the room, stopped when it quits) reports the screen rectangles of
windows whose title starts with `clawd-room ` and nothing else. Windows side by side with a gap of
at most 30 px (`--near PX`; an attached pair stays attached until the gap passes 45 px) count as close. It needs the room's tab to be the active tab of its
window; `--no-detect` turns it off. `clawd-room --list-windows` lists the clawd-room windows it can see.

## Crossing and moving

Crossing: when two windows are close enough (or you pass `--left/--right/--up/--down NAME` to say
who is next to whom), an idle Clawd sometimes walks out through the door and into the next window
(a `walk` message with its folder name and height only). There it visits for 25-45 s (chats,
plays, orders a coffee, uses the swing), shown with a blue name tag, then walks back. Its desk
shows AWAY meanwhile; if its session gets busy or ends, the other window is asked to send it home.

Windows above or below: a window stacked over another gets a knotted rope hanging from the top
(Clawds climb it to the room above) and the one below gets stairs in the floor. A door on the
side shows the floor and wall of the room next door. Press `g` to send an idle Clawd next door now.

## Sessions

Sessions are shared between linked rooms (each session lives in one room and stays there while that room runs:
a new session goes to the room a hash of its id and the room's id scores highest; one moved with `m`
lives in the room it moved to), so two windows do not show the same Clawds. `--no-split` shows every session in every room.
The rooms read the same `~/.clawd-room/events.jsonl`: real Claude Code sessions (hook installed)
and `simulate.py` fake ones both show up.

## Gates

Windows are never moved to attach them: you put them where you like. The gates line up on
screen instead:
- Windows above/below get a ladder hatch (in the floor of the upper window, with a rope ladder
  hanging from a ceiling hatch in the lower one) at the same screen x.
- Windows side by side get a doorframe at the same screen height.
The two rooms swap where their cells are on screen (cell size, offset, scale, walking strip) in the
small presence file and search for the position that both of their grids can hit most closely, so
the two ends match to within a few pixels even with different fonts.
Different font sizes: a room pixel is a different size on screen in each window, so a gate drawn
the same number of pixels would be small in one and big in the other. Both windows therefore draw
the gate at the size it needs in the window with the bigger cells (the window with the smaller
cells draws it 1.5x, 2x ... as many pixels), so both ends are the same size on screen to within
a few percent. Only the Clawds themselves keep their window's scale.
Gates wait for the windows to be still: while a window is being moved or resized there is no gate,
and it appears (at its new, aligned place) once nothing has moved for a second.
A gate only opens when it fits: a door needs the two floors to share enough height for a Clawd
(14 room pixels), a hatch needs the two windows to overlap sideways by at least a hatch width
(21 room pixels). Otherwise there is no gate, no arrow in the banner, and nobody crosses.
The terminal's own cell size (asked with CSI 14t/16t) is used to turn screen pixels into cells.
Names: a Clawd is labelled with the folder name the Claude Code session was started in (the
`cwd` basename the hook records, nothing else), cut to fit its slot. The room for this repo shows
as CLAWD-R... for `clawd-room`, a session started in a folder called `LLM` shows as LLM.
