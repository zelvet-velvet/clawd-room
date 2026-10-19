---
title: Test Checklist
tags:
  - clawd-room
  - tests
---
# ✅ Test Checklist

> [!info] How to use this
> Things only a person can check by looking. **Ticked = a person confirmed it**, with their name and the date: `(William, 2026-10-21)`; looks and art are ticked by Zaviel. Assistants never tick. Automatic tests are in [[Engine and Tests]]. Checks for things that were replaced are removed.

---
## 🧪 Test queue

What to test next, one big callout per group: a bold title per test (no box, no dash) with its checks as boxes under it; tick them as you check (with your name and the date), open **How to test** for the steps. A test whose boxes are all ticked moves down into its subject below.

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

> [!todo]+ WSL2 + Windows Terminal (since 2026-10-19)
> **1. The Clawd Room profile at 5 pt**
> - [i] Needs `python3 setup_profile.py --update` once (Zaviel ran it on 2026-10-19)
> - [x] From Windows Terminal's dropdown the room starts and its window moves to the quarter of the screen it is in
> - [x] Room pixels are square and about as many as on the Mac (the whole room fits, the park shows the swing set and the building's plaque)
> - [x] `q` closes the room and the window; nothing is left behind
> - [x] `./clawd-room.sh --try real`: all four rooms appear together, snapped into the quarters, and show as one **Group** in the taskbar
>
> **2. The rooms and the bottom bar**
> - [i] All four `--try` rooms once closed together after about 15 s: an old `--try` quitting late (fixed 2026-10-19). `cat ~/.clawd-room/try/stop` says which room ended a `--try`
> - [x] Office, coffee shop and park look as on the Mac (style A, the park plaza and its buildings)
> - [x] The bottom bar's text is readable at 5 pt (`?` shows the help)
> - [x] Hold `g` or `m`: the yellow hint shows only while the key is held and goes right after letting go
>
> **3. Sending and moving Clawds between rooms**
> - [x] `g` + an arrow: a Clawd walks to that gate (door, stairs, well), goes through and comes out in the room next to it
> - [x] `m` + an arrow: the picked Clawd packs its desk into a box (poof), carries it out and unpacks at a new desk in the other room
> - [x] The room it left does not blink; `m` + arrow while it carries the box sends it straight to the new gate
> - [x] `b` calls your Clawds back
>
> **4. Desks stay in their room**
> - [x] Opening the rooms one by one, a desk never jumps to another room
> - [x] Closing one room: only its Clawds show up elsewhere
>
> **5. The pixel editor with the mouse**
> - [ ] `./clawd-room.sh --edit window` opens the editor in its own Clawd Room window in the middle of the screen; the window you typed in stays
> - [ ] Left click paints, right click erases, the palette picks a colour
> - [ ] A running room shows the change as you draw
> - [ ] `q` asks save (S) or discard (D); D puts the picture back, in the room too
>
> > [!example]- How to test
> > **1.** Windows Terminal → the ⌄ next to the tabs → **Clawd Room** → pick a scene → the window jumps once into its quarter; compare with ![[2026-10-19 wsl try, four rooms at 5 pt.png|300]]; press `q`.
> >
> > **2.** In a WSL shell in the repo: `./clawd-room.sh --try` → look at the four rooms for a minute → click into one, `?` to read the help, `?` again → hold `g` two seconds and let go, then the same with `m` → `./clawd-room.sh --try close`.
> >
> > **3.** `./clawd-room.sh --try`, wait for the gates (about 10 s) → in the top-left room hold `g` and press → (coffee shop), then ↓ (park) → `Tab` until a Clawd at a desk is picked, `m` + →, watch both rooms → pick a Clawd carrying a box, `m` + ↓ → `b`.
> >
> > **4.** With at least one real Claude Code session running: open **Clawd Room** (office), note the desks → open a second and a third (coffee shop, park) next to it → the first room's desks stay → quit the coffee shop: only its Clawds move.
> >
> > **5.** Start a room (`--try` or the profile; the office has the window) → in a second window `./clawd-room.sh --edit window` → paint a few pixels and watch the room's window → `q`, then `D`: the window is back as before.

## Rooms and art ([[Rooms and Art]])
- [ ] Each room (office, coffee shop, park) looks like the mockups at your window size
- [ ] Seated Clawds show behind their desk / table / bench; the barista behind the bar
- [ ] Coffee shop: tables, square cushioned chairs, laptop / tablet / phone, coffee / matcha look right
- [ ] Park: bench, swing and picnic table workspaces; at a swing workspace the Clawd sits on the seat between the chains
- [ ] Park playground: the small swing set (a Clawd on it swings towards you and back) and the slide at 45°
- [ ] Park: a city plaza (skyline, promenade, lamps, benches); a side with a gate has a brick building with a door (it opens as a Clawd passes) and no bushes
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
- [ ] Going up: the Clawd climbs the stairs to the half floor, walks along it and goes through the opening (the park: up the stoop to the building's double door); coming down the other way round; going down: it walks into the well / steps
- [ ] Gates appearing animate in, in both windows at the same moment; gates there at start are simply there
- [ ] Pick a Clawd (Tab), `m` + arrow: it walks out (also while working) and comes in at the other room, stays there (not back after a visit); close that room: it shows up in its usual room again
- [ ] Moving: the desk shrinks into a box with a poof, the Clawd carries it out, no blink in the room it left; it unpacks at the new desk; `m` + arrow while it carries the box: straight to the new gate
- [ ] `--try` / `--try real`: desks stay in their room while the rooms open one by one, run and one closes (no hopping); the made-up Clawds spread over the four rooms
- [ ] Hold `g` / `m`: the hint shows only while held, gone right after letting go
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
- [ ] WSL: started from a normal shell (14 pt), the room sets 5 pt and quitting puts the terminal font back
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
