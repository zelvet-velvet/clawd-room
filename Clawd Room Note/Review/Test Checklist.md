---
title: Test Checklist
tags:
  - clawd-room
  - tests
sticker: lucide//list-checks
---
> [!info] How to use this
> Things only a person can check by looking. **Ticked = a person confirmed it**, with their name and the date: `(William, 2026-10-21)`; looks and art are ticked by Zaviel. Assistants never tick. Automatic tests are in [[Engine and Tests]]. Checks for things that were replaced are removed.

---
## 🧪 Test queue

What to test next, one `[!example] Group X · …` callout per group. Each test is a box with an ID (**T1**, **T2** … counting on across groups, so an ID always means the same test; open questions are **Q1**, **Q2** …): what to do → what should happen. Tick as you check, then a `✅ date: … (who)` line says what was seen. The steps are in the folded **how to run** under each group. A group that is merged and all ticked moves to [[Test Checklist Archive]] (keeping its IDs). Rules: skill `room-test-queue`.

> [!info]- Box legend ([[Checkbox Style]])
> - [ ] to test
> - [/] partly checked
> - [x] works (name, date)
> - [!] fails: say what you saw, open an issue
> - [?] unsure: say why
> - [-] dropped (no longer applies)
> - [>] moved to later
> - [i] a note, not a check

> [!tip] Setting up a test
> `./clawd-room.sh --try` opens **four linked rooms**, one in each quarter of the screen (office, coffee shop / park, office), with **made-up Clawds** in them, so every gate shows at once. Quit any one to close all four, or `./clawd-room.sh --try close`. `--try real` shows only your real sessions. On WSL `--try --desktop N` opens them on virtual desktop N without switching to it (switch there yourself to look).

> [!done]- No open groups on `main`
> Groups on open branches live on their branch (e.g. Group N, `feat/split-panes`). Done groups: [[Test Checklist Archive]].

## Rooms and art ([[Rooms and Art]])
- [x] Each room (office, coffee shop, park) looks like the mockups at your window size
- [x] Seated Clawds show behind their desk / table / bench; the barista behind the bar
- [x] Coffee shop: tables, square cushioned chairs, laptop / tablet / phone, coffee / matcha look right
- [x] Park: bench, swing and picnic table workspaces; at a swing workspace the Clawd sits on the seat between the chains
- [x] Park playground: the small swing set (a Clawd on it swings towards you and back) and the slide at 45°
- [x] Park: a city plaza (skyline, promenade, lamps, benches); a side with a gate has a brick building with a door (it opens as a Clawd passes) and no bushes
- [x] The room's sign reads well; with stairs it hangs in front of them
- [x] Dots over heads; the name shows only for a Clawd that needs you
- [x] A crowded small room (several gates and Clawds) still reads; nothing collides badly
- [x] Happy Clawds keep 1x1 dark eyes with the peach top and blush

## Gates and crossing ([[Interaction]])
- [x] Two linked windows side by side: gates appear, `g` sends a Clawd across (2026-10-17, older gates)
- [x] Side by side: a door on each room's facing wall; `g`: the Clawd walks to the door, it opens, the Clawd goes through and comes out of the other room's door
- [x] Stacked: the upper room shows the small stair well at the front left (office / coffee) or a small stone well (park), at the front left; the lower room shows the stairs on the back wall (office / coffee) or the treehouse (park)
- [x] Office stairs are steel with slim steel balusters (no glass), coffee shop stairs wooden; the way up is a tall opening at the right end of the platform with steps going up and a light
- [x] The stair well's steps go down from its right end to the left (matching the stairs below); a Clawd going down walks in at the right
- [x] Coffee shop: the steps and the platform stand out from the wall and the floor (darker wood, a dark skirting)
- [x] Nothing overlaps in any room: sign, window, plant, menu, lamps, shelf; desks clear of the stairs, doors, well and treehouse (`o` shows the boxes)
- [x] Crowded park: the swing and slide give way to workspaces; a quiet park keeps them
- [x] With many sessions no workspace sits on the stair well
- [x] Stairs read as real steps (lit treads, a baluster on each step, the rail parallel to them); the platform reaches the corner; office window and coffee lamps clear of the sign and the opening
- [x] Side doors look tall enough for a Clawd; the stair well sits on open floor with a wooden rim and reads as a hole with steps going down
- [x] On the stairs and the half floor the Clawd walks (not the climbing pose) and is behind the railing; coming back it walks along the half floor, then down the stairs
- [x] Going up: the Clawd climbs the stairs to the half floor, walks along it and goes through the opening (the park: up the stoop to the building's double door); coming down the other way round; going down: it walks into the well / steps
- [x] Gates appearing animate in, in both windows at the same moment; gates there at start are simply there
- [x] Pick a Clawd (Tab), `m` + arrow: it walks out (also while working) and comes in at the other room, stays there (not back after a visit); close that room: it shows up in its usual room again
- [x] Moving: the desk shrinks into a box with a poof, the Clawd carries it out, no blink in the room it left; it unpacks at the new desk; `m` + arrow while it carries the box: straight to the new gate
- [x] `--try` / `--try real`: desks stay in their room while the rooms open one by one, run and one closes (no hopping); the made-up Clawds spread over the four rooms
- [x] Hold `g` / `m`: the hint shows only while held, gone right after letting go
- [x] Hold `g`: a line at the bottom of the room shows which arrow goes where; `g` + arrow sends a Clawd that way; letting go hides the line; with one linked room `g` sends straight away
- [x] Gates stay put in `--try` (no coming and going)
- [x] `b` calls our Clawds home; with none away it sends visitors home
- [x] One session, two linked rooms: the room without a Clawd shows an empty room (no NO SESSIONS); with no sessions at all both say NO SESSIONS
- [ ] iTerm2 split panes (Cmd+D, Cmd+Shift+D) get gates between them

## Camera and room size ([[Interaction]])
- [x] `]` zooms in; the camera follows the Clawd that needs you / just finished / is busy, and moves when it walks
- [x] ← → / Tab pick a Clawd; `c` switches to the free camera (arrows move it) and back
- [x] Empty zoomed room: `AUTO FOLLOW (WAITING)`; a visitor comes in and is followed
- [x] Zoomed, a Clawd goes through a gate: the camera goes with it, then zooms out
- [x] `z` frames the busy Clawds (close on one, wider for several)
- [x] Drag the window: the room re-lays out at once; with S chosen, past 120 columns the pixels get bigger
- [x] Words stay small when zoomed and never cover a Clawd

## Platforms ([[Platforms and Windows]])
- [x] WSL: started from a normal shell (14 pt), the room sets 5 pt and quitting puts the terminal font back
- [ ] iTerm2: Profiles → Clawd Room starts the room directly, already a quarter of the screen (no resize); `q` closes the window
- [ ] iTerm2: `./clawd-room.sh` typed in a normal window: the room's window opens in its place (4x4 pt square pixels) and the typed-in window closes; `--keep` keeps it, `--here` runs in it
- [ ] iTerm2: Cmd + / − / 0 do nothing in a room window
- [ ] iTerm2: a new room opens as a quarter of the screen, in the quarter it was in
- [ ] iTerm2: smaller than 44x20 then bigger: the room comes back, nothing scrolled; no old frames in scrollback after quitting
- [ ] iTerm2: stripes only while dragging, a clean picture after
- [ ] iTerm2: quitting gives the window title back and leaves no `osascript`
- [x] Sprite files: change a grid in `sprites/clawd.txt` or `props.txt` while a room runs: it shows within a second; delete the frame, the original is back
- [x] `./clawd-room.sh --edit window`: paint, erase, pick, add a colour, undo; the rooms show it as you draw; your typed-in window stays
- [x] `q` after changes asks save / discard; D puts the file back (the rooms change back too); `--edit list` prints the names
- [ ] In the editor, `F` on a prop adds `NAME~2`; draw it: the prop animates in a running room
- [ ] Going down the well: the Clawd walks down its steps and disappears into the hole; coming up it rises out of it
- [ ] Gates rise in eased (fast, then settling); doors swing open and shut smoothly, turning back if a Clawd comes back
- [ ] Change a line in `rooms/office.txt` (e.g. the window's prefer): the window moves in a running room
