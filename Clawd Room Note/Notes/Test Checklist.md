---
title: Test Checklist
tags:
  - clawd-room
  - tests
---
# ✅ Test Checklist

> [!info] How to use this
> Things only a person can check by looking. **Ticked = Zaviel confirmed it** (nobody else ticks). Automatic tests are in [[Engine and Tests]]. Checks for things that were replaced are removed.

---
> [!tip] Setting up a test (iTerm2)
> `./clawd-room.sh --try` opens **four linked rooms**, one in each quarter of the screen (office, coffee shop / park, office), with **made-up Clawds** in them, so every gate shows at once. Quit any one to close all four, or `./clawd-room.sh --try close`. `--try real` shows only your real sessions.

## Rooms and art ([[Rooms and Art]])
- [ ] Each room (office, coffee shop, park) looks like the mockups at your window size
- [ ] Seated Clawds show behind their desk / table / bench; the barista behind the bar
- [ ] Coffee shop: tables, square cushioned chairs, laptop / tablet / phone, coffee / matcha look right
- [ ] Park: bench, swing and picnic table workspaces; at a swing workspace the Clawd sits on the seat between the chains
- [ ] Park playground: the small swing set (a Clawd on it swings towards you and back) and the slide at 45°
- [ ] Park: a city plaza (skyline, promenade, lamps, benches); a side with a gate has a hedge with a stone archway and no bushes
- [ ] The room's sign reads well; with stairs it hangs in front of them
- [ ] Dots over heads; the name shows only for a Clawd that needs you
- [ ] A crowded small room (several gates and Clawds) still reads; nothing collides badly
- [ ] Happy Clawds keep 1x1 dark eyes with the peach top and blush

## Gates and crossing ([[Interaction]])
- [x] Two linked windows side by side: gates appear, `g` sends a Clawd across (2026-10-17, older gates)
- [ ] Side by side: a door on each room's facing wall; `g`: the Clawd walks to the door, it opens, the Clawd goes through and comes out of the other room's door
- [ ] Stacked: the upper room shows the small stair well at the front left (office / coffee) or a small stone well (park), at the front left; the lower room shows the stairs on the back wall (office / coffee) or the treehouse (park)
- [ ] Office stairs are steel with slim steel balusters (no glass), coffee shop stairs wooden; the way up is a tall opening at the right end of the platform with steps going up and a light
- [ ] The stair well's steps go down from its right end to the left (matching the stairs below); a Clawd going down walks in at the right
- [ ] Coffee shop: the steps and the platform stand out from the wall and the floor (darker wood, a dark skirting)
- [ ] Nothing overlaps in any room: sign, window, plant, menu, lamps, shelf; desks clear of the stairs, doors, well and treehouse (`o` shows the boxes)
- [ ] Crowded park: the swing and slide give way to workspaces; a quiet park keeps them
- [ ] With many sessions no workspace sits on the stair well
- [ ] Stairs read as real steps (lit treads, a baluster on each step, the rail parallel to them); the platform reaches the corner; office window and coffee lamps clear of the sign and the opening
- [ ] Side doors look tall enough for a Clawd; the stair well sits on open floor with a wooden rim and reads as a hole with steps going down
- [ ] On the stairs and the half floor the Clawd walks (not the climbing pose) and is behind the railing; coming back it walks along the half floor, then down the stairs
- [ ] Going up: the Clawd climbs the stairs to the half floor, walks along it and goes through the opening (or up the treehouse ladder); coming down the other way round; going down: it walks into the well / steps
- [ ] Gates appearing animate in, in both windows at the same moment; gates there at start are simply there
- [ ] Hold `g`: a line at the bottom of the room shows which arrow goes where; `g` + arrow sends a Clawd that way; letting go hides the line; with one linked room `g` sends straight away
- [ ] Gates stay put in `--try` (no coming and going)
- [ ] `b` calls our Clawds home; with none away it sends visitors home
- [ ] One session, two linked rooms: the room without a Clawd shows an empty room (no NO SESSIONS); with no sessions at all both say NO SESSIONS
- [ ] iTerm2 split panes (Cmd+D, Cmd+Shift+D) get gates between them

## Camera and room size ([[Interaction]])
- [ ] `]` zooms in; the camera follows the Clawd that needs you / just finished / is busy, and moves when it walks
- [ ] ← → / Tab pick a Clawd; `c` switches to the free camera (arrows move it) and back
- [ ] Empty zoomed room: `AUTO FOLLOW (WAITING)`; a visitor comes in and is followed
- [ ] Zoomed, a Clawd goes through a gate: the camera goes with it, then zooms out
- [ ] `z` frames the busy Clawds (close on one, wider for several)
- [ ] Drag the window: the room re-lays out at once; with S chosen, past 120 columns the pixels get bigger
- [ ] Words stay small when zoomed and never cover a Clawd

## Platforms ([[Platforms and Windows]])
- [x] WSL: profile from the dropdown, 110x32 at 8 pt, drag / scale / font keys (2026-10-16)
- [x] WSL: `wt.exe -w new --size 60,18 -p "Clawd Room"` opens at the final size (2026-10-16)
- [ ] WSL: quitting puts the terminal font back
- [ ] iTerm2: Profiles → Clawd Room starts the room directly, already a quarter of the screen (no resize); `q` closes the window
- [ ] iTerm2: `./clawd-room.sh` typed in a normal window: the room's window opens in its place (4x4 pt square pixels) and the typed-in window closes; `--keep` keeps it, `--here` runs in it
- [ ] iTerm2: Cmd + / − / 0 do nothing in a room window
- [ ] iTerm2: a new room opens as a quarter of the screen, in the quarter it was in
- [ ] iTerm2: smaller than 44x20 then bigger: the room comes back, nothing scrolled; no old frames in scrollback after quitting
- [ ] iTerm2: stripes only while dragging, a clean picture after
- [ ] iTerm2: quitting gives the window title back and leaves no `osascript`
- [ ] Sprite files: change a grid in `sprites/clawd.txt` or `props.txt` while a room runs: it shows within a second; delete the frame, the original is back
- [ ] `./clawd-room.sh --edit window`: paint, erase, pick, add a colour, undo; the rooms show it as you draw; your typed-in window stays
- [ ] `q` after changes asks save / discard; D puts the file back (the rooms change back too); `--edit list` prints the names
- [ ] In the editor, `F` on a prop adds `NAME~2`; draw it: the prop animates in a running room
- [ ] Going down the well: the Clawd walks down its steps and disappears into the hole; coming up it rises out of it
- [ ] Gates rise in eased (fast, then settling); doors swing open and shut smoothly, turning back if a Clawd comes back
- [ ] Change a line in `rooms/office.txt` (e.g. the window's prefer): the window moves in a running room
