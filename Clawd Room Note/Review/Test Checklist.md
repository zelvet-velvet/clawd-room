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

What to test next, one `[!example] Group X · …` callout per group. Each test is a box with an ID (**T1**, **T2** … counting on across groups, so an ID always means the same test; open questions are **Q1**, **Q2** …): what to do → what should happen. Tick as you check, then a `✅ date: … (who)` line says what was seen. The steps are in the folded **how to run** under each group. A group that is all ticked moves down into its subject below (keeping its IDs).

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

> [!done]- Group A · WSL: the Clawd Room profile at 5 pt (needs `python3 setup_profile.py --update` once)
> - [x] **T1** Clawd Room from Windows Terminal's dropdown → the room starts, its window moves to the quarter of the screen it is in.
> - [x] **T2** Room pixels are square and about as many as on the Mac (the whole room fits; the park shows the swing set and the building's plaque).
> - [x] **T3** `q` → the room and its window close; nothing is left behind.
> - [x] **T4** `./clawd-room.sh --try real` → all four rooms appear together, snapped into the quarters, one **Group** in the taskbar.
>
> ✅ 2026-10-19: T1–T4 (Zaviel). T4 took several rounds: the rooms now open at once and become a snap group through Snap Assist (about 2.3 s).
>
> > [!tip]- T1–T4 · how to run them
> > 1. Windows Terminal → the ⌄ next to the tabs → **Clawd Room** → pick a scene: the window jumps once into its quarter. Compare with ![[2026-10-19 wsl try, four rooms at 5 pt.png|300]]
> > 2. `q`.
> > 3. In a WSL shell in the repo: `./clawd-room.sh --try real`, hands off for about 3 s; hover over Windows Terminal in the taskbar. `~/.clawd-room/try/snap.log` says how each room was snapped.

> [!done]- Group B · WSL: the rooms and the bottom bar
> - [x] **T5** Office, coffee shop and park look as on the Mac (style A, the park plaza and its buildings).
> - [x] **T6** The bottom bar's text is readable at 5 pt (`?` shows the help).
> - [x] **T7** Hold `g` or `m` → the yellow hint shows only while the key is held and goes right after letting go.
> - [x] **Q1** Once all four `--try` rooms closed together after about 15 s. Why?
>
> ✅ 2026-10-19: T5–T7 (Zaviel). Q1: an old `--try` quitting late wrote the stop file and closed the new rooms; fixed (a new `--try` waits until the old rooms are gone). `cat ~/.clawd-room/try/stop` says which room ended a `--try`.
>
> > [!tip]- T5–T7 · how to run them
> > 1. `./clawd-room.sh --try`, look at the four rooms for a minute.
> > 2. Click into one, `?` to read the help, `?` again.
> > 3. Hold `g` for two seconds and let go; the same with `m`.
> > 4. `./clawd-room.sh --try close`.

> [!done]- Group C · WSL: sending and moving Clawds between rooms
> - [x] **T8** `g` + an arrow → a Clawd walks to that gate (door, stairs, well), goes through and comes out in the room next to it.
> - [x] **T9** `m` + an arrow → the picked Clawd packs its desk into a box (poof), carries it out and unpacks at a new desk in the other room.
> - [x] **T10** The room it left does not blink; `m` + arrow while it carries the box → straight to the new gate.
> - [x] **T11** `b` → your Clawds come back.
>
> ✅ 2026-10-19: T8–T11 (Zaviel).
>
> > [!tip]- T8–T11 · how to run them
> > 1. `./clawd-room.sh --try`, wait for the gates (about 5 s).
> > 2. In the top-left room hold `g` and press → (coffee shop), then ↓ (park).
> > 3. `Tab` until a Clawd at a desk is picked, `m` + →, watch both rooms; pick a Clawd carrying a box, `m` + ↓.
> > 4. `b`.

> [!done]- Group D · WSL: desks stay in their room
> - [x] **T12** Opening the rooms one by one → a desk never jumps to another room.
> - [x] **T13** Closing one room → only its Clawds show up elsewhere.
>
> ✅ 2026-10-19: T12, T13 (Zaviel).
>
> > [!tip]- T12–T13 · how to run them
> > 1. With at least one real Claude Code session running: open **Clawd Room** (office), note the desks.
> > 2. Open a second and a third (coffee shop, park) next to it: the first room's desks stay.
> > 3. Quit the coffee shop: only its Clawds move.

> [!done]- Group E · WSL: the pixel editor with the mouse (T18–T22: the new colour panel; T23: Ctrl+Z / Ctrl+Y)
> - [x] **T14** `./clawd-room.sh --edit window` → the editor opens in its own Clawd Room window in the middle of the screen; the window you typed in stays.
> - [x] **T15** Left click paints, right click erases, the palette picks a colour.
> - [x] **T16** A running room shows the change as you draw.
> - [x] **T17** `q` → asks save (S) or discard (D); D puts the picture back, in the room too.
> - [x] **T18** Colour field and hue bar: click or drag → the colour changes; on a file colour every pixel using it changes with it.
> - [x] **T19** From the body colour or ERASE, a click in the field → a new colour; **+ ADD** → it joins IN THIS FILE; painting with it works.
> - [x] **T20** ERASE → left click erases; PICK → the next click on the grid takes that pixel's colour; PAINT → back to the colour before.
> - [x] **T21** Click the hex code or a red / green / blue number → type a value, Enter.
> - [x] **T22** A short editor window → one line `? SHORTCUTS  Q QUIT`, nothing runs over the colours; `?` opens and closes the shortcuts box.
> - [x] **T23** Paint, then **Ctrl+Z** → the pixel goes back (also in a running room); **Ctrl+Y** → it comes back; the editor is never suspended; **Ctrl+C** → asks save / discard like `q`.
>
> ✅ 2026-10-20: T16–T23 (Zaviel).
>
> > [!tip]- T14–T23 · how to run them
> > 1. Start a room (`--try` or the profile; the office has the window).
> > 2. In a WSL shell: `./clawd-room.sh --edit window`.
> > 3. Paint a few pixels, erase some, pick another colour; watch the room's window.
> > 4. `q`, then `D`: the window is back as before.
> > 5. T18–T22: the new colour panel (like Figma). Compare with ![[2026-10-19 pixel editor, colour field palette.png|300]]

> [!done]- Group F · WSL: the pixel editor, animations and fill (T24–T30, PR #10)
> - [x] **T24** `./clawd-room.sh --edit list` → each animation once with its frame count (`walk 4`, `think 9`), in columns, no name cut across two lines.
> - [x] **T25** `./clawd-room.sh --edit walk` → `walk-1` opens; the timeline shows four frames; clicking a frame or `<` `>` opens it; `[` `]` go to the next animation.
> - [x] **T26** Space → the real-size view plays the walk as in the room (legs and arms in turn); **1X** → 2x, 0.5x; Space stops. Try `cheer` (it bounces) and `wait`.
> - [x] **T27** **ONION** on → where the frame is empty, the frame before shows faintly red and the one after blue; `o` turns it off.
> - [x] **T28** **FILL** → the area under the pointer is outlined in dashes; a click fills it (a closed gap between the legs stays); right click erases it; click FILL again → **FILL ALL** fills every pixel of that colour; Ctrl+Z undoes.
> - [x] **T29** `--edit window` → `f` adds `WINDOW~2` (a copy); paint it → a running room animates the window; `x` `x` deletes it; on the first frame `x` `x` says it is the prop and does nothing.
> - [x] **T30** On a Clawd frame `f` or `x` → "PROPS ONLY" and nothing changes; `q` → `D` puts everything back.
>
> > [!tip]- T24–T30 · how to run them
> > 1. In the repo, on `main` (PR #10 is merged).
> > 2. Start a room (`./clawd-room.sh --try`), so you can see edits arrive.
> > 3. In a WSL shell: the `--edit` commands above (the editor opens in its own window).
> > 4. When done, `q` then `D` (discard) unless you mean to keep a drawing.
> > 5. Compare with ![[2026-10-20 editor timeline, onion skin and fill.png|300]] and ![[2026-10-20 editor playing a Clawd at the room's speed.png|300]]

> [!example] Group G · WSL: the stage page for part animations (T31–T38, PR #19)
> - [x] **T31** `./clawd-room.sh --edit anims/swing` → the stage page opens: the park with the Clawd on the swing, the parts outlined, the panel, the frames as pictures; `?` shows the keys (and scrolls in a short window).
> - [x] **T32** Click the seat, then the Clawd, then a name in PARTS → that part is picked (yellow); SELECT shows handles; drag one → the outline resizes.
> ✅ 2026-10-21: T31, T32 pass (Zaviel).
> - [ ] **T33** MOVE: drag the part pressed on → it moves and stops at the edge of the stage; ROTATE: drag the seat → its outline turns; on the Clawd, ROTATE is greyed and cannot be picked; with ROTATE on, pressing on the Clawd picks nothing and turns nothing.
> - [ ] **T34** Go to frame 8 (right arrow or a frame picture): the KEY cap is not pressed in; KEY or `k` → "KEY ADDED", the cap pressed in, a diamond under frame 8 and a mark on the stage; KEY again → gone; `e` changes a key's ease.
> - [ ] **T35** `+ -` and the wheel zoom the stage, `W A S D` and a right drag move the view, `0` puts it back; `o` onion skin, `v` the key marks.
> - [ ] **T36** The panel and the frame strip scroll (the wheel, the strip's scroll bar); the strip follows the frame when it changes.
> - [ ] **T37** A room with a swinging Clawd (the park) follows an edit of the clawd part's MOVE Y while it runs.
> - [ ] **T38** `q` → "SAVE THE CHANGES TO SWING?" (S, D, Esc); D puts your file back. From the picker (`--edit`, Tab to ANIMS, Enter) `q` comes back to the picker.
>
> > [!tip]- T31–T38 · how to run them
> > 1. Until PR #19 is merged: `git switch feat/stage-page` in the repo.
> > 2. Start a room in the park (`./clawd-room.sh --room park`) to see a Clawd swing (or wait for one to play).
> > 3. In a WSL shell: `./clawd-room.sh --edit anims/swing` (the editor opens in its own window).
> > 4. Your edits are in `~/.clawd-room/anims/`; delete `swing.txt` there (or RESET in the picker) to go back to the default.

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
