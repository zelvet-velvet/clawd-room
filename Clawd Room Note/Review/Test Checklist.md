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

What to test next, one `[!todo] Group X · …` callout per group (`[!done]` once merged and all ticked: **Callout type** in [[LLM Guide]]). Each test is a box with an ID (**T1**, **T2** … counting on across groups, so an ID always means the same test; open questions are **Q1**, **Q2** …): what to do → what should happen. Tick as you check, then a `✅ date: … (who)` line says what was seen. The steps are in the folded **how to run** under each group. A group that is merged and all ticked moves to [[Test Checklist Archive]] (keeping its IDs). Rules: skill `room-test-queue`.

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

> [!todo] Group N · WSL: rooms in split panes (T67–T75)
> - [ ] **T67** In your work window, Alt + click **Clawd Room** in the ⌄ menu → the room opens in a split pane; the **window keeps its size**, and the font of the other panes does not change.
> - [ ] **T68** Open a second Clawd Room pane beside the first → after about 1 s still, a **gate** between the two panes; `g` + arrow sends a Clawd through.
> - [ ] **T69** A room pane next to a room in its own window (edges close) → a gate between the pane and the window.
> - [ ] **T70** Click into your work pane and back, drag the divider between the panes → the gates follow the panes and do not jump to the wrong one.
> - [ ] **T71** A room in a whole window (Clawd Room from the ⌄ menu without Alt) → still sizes its window as before.
> - [ ] **T72** With a split open, `./clawd-room.sh --list-windows` in another shell → a `(split window)` line with its panes under it, the room panes marked `(Clawd Room)`.
> - [ ] **T73** Two room panes side by side; drag the window, let go → the gate rises in **both** rooms at the same moment (about 2 s after you let go), not one after the other.
> - [ ] **T74** Three Clawd Room panes side by side in a full-screen window → each room is shorter than its pane, in the middle with dark above and below; the floor looks like a normal room, not a long runway; the bar is at the bottom.
> - [ ] **T75** Two room panes, one with 1 Clawd and one with 4 → both rooms have the same pixel size (no room twice as big as the other).
>
> > [!tip]- T67–T75 · how to run them
> > 1. Branch `feat/split-panes`: `git switch feat/split-panes && make -C c`.
> > 2. Windows Terminal: hold **Alt** while picking a profile in the ⌄ menu to open it as a split pane of the current tab (Alt + Shift + D duplicates the focused pane).
> > 3. A room learns its pane when the pane has the focus (it has, just after it opens), or as the only Clawd Room pane no other room claims. Two rooms that both never had their pane focused wait for a click into one of them. A room that cannot tell within 15 s whether it is in a pane leaves the window and font alone.
> > 4. If something looks wrong: `CLAWD_SIZE_LOG=/tmp/size.log ./clawd-room.sh` logs why a gate is shown or not.

> [!todo] Group O · WSL: a pane too small for the room shows a strip (T76–T97)
> - [ ] **T76** Split a Clawd Room pane until it is under 20 rows (about 5 rows high) → a band of Clawds at laptops instead of a black pane; each laptop's screen is green (working), yellow (waiting) or purple (thinking); an idle Clawd has its eyes shut and no laptop.
> - [ ] **T77** Press Tab in it → yellow corners around one Clawd, the rest dims, its name shows beside it; Tab again moves on; Esc lets go.
> - [ ] **T78** Make the pane narrower than all the Clawds need → only some show; Tab past the last one in view scrolls the band to the next.
> - [ ] **T79** With a Clawd out of view, make its session wait for you (a permission prompt) → the band scrolls to it by itself and shows its name.
> - [ ] ~~**T80** Make the pane 2 or 3 rows high → faces on coloured tiles; Tab outlines one in yellow.~~ (replaced by T90)
> - [ ] **T81** In a strip taller than 5 rows press Tab → the yellow corners hug the Clawd (not the whole band's height), the light is round around it, the name sits beside the corners.
> - [ ] **T82** A room pane beside a strip pane → no gate to the strip on either side; make the strip pane big again → after about 1 s still, the gate appears.
> - [ ] ~~**T83**~~ (replaced by T89) One session; make a room pane about 22 rows high (office) or 35 (park) → the strip shows, not an empty floor saying "+1 MORE"; make it taller → the room comes back with the Clawd at its desk.
> - [ ] **T84** A strip 10 or more rows high, Tab → the name sits above the yellow corners, not over the Clawd.
> - [ ] **T85** A sleeping session in the strip → eyes shut and Zs rising from it; an idle one keeps its eyes open, no laptop.
> - [ ] **T86** One Clawd in a full-screen window, then a quarter window, then a pane → the Clawd is the same size in all three (only the room gets bigger or smaller).
> - [ ] **T87** A whole window taller than it is wide (drag it narrow and tall) → the room is shorter than the window, in its middle with dark above and below, like a tall pane.
> - [ ] **T88** `r` does nothing now; `?` shows the help line without "r room size"; `[` `]` still zoom.
> - [ ] **T89** An 80x24 window with one session (office) → the room, not the strip: the Clawd on the floor near the back wall with a laptop on its right, the screen green while it works; with 6 sessions → 2 on the floor and "+4 MORE".
> - [ ] **T90** Make the pane 2 or 3 rows high → a face per Clawd on the room's wall colour (no black, no tiles), dark eyes as in the room, a small dot in the state colour on each face's left; Tab puts yellow brackets around one.
> - [ ] **T91** In the 80x24 room press Tab, and open a pane beside it → Tab picks a Clawd on the floor; a gate appears, and Clawds can walk through it and come back to their floor place.
> - [ ] **T92** Make a strip 2 or 3 rows high → each face has tall eyes: straight when working, raised when waiting, up to the side when thinking, half shut (dots) when idle, a line when sleeping.
> - [ ] **T93** In a normal room press `v` three times → the Clawds at laptops, then the faces filling the pane's height (the same shape, bigger; at least one whole face across), then the room again; Tab picks in the first two.
> - [ ] **T94** In a pane too small for the room press `v` → band and faces swap; under 4 rows `v` keeps the faces.
> - [ ] **T95** Make the font big (Ctrl+= several times) so the window's edge has a gap less than a cell → the gap is the colour of the picture next to it, not black; quit → the terminal's background is back to normal.
> - [ ] **T96** A strip band 5 rows and then 10 or more rows high in each theme → its wall has its room's things far apart (office: window, clock, pin board; coffee: shelves, lamps, menu; park: skyline, lamp posts, tree, cloud), behind the Clawds, never repeating within one Clawd or two.
> - [ ] **T97** A band 14 or more rows high (a wide, short window, or `v`) → the wall's things spread over the whole wall (lamps from the ceiling, the skyline taller, clouds high up), not only just above the Clawds; at 13 rows or fewer they stay just above them.
>
> > [!tip]- T76–T97 · how to run them
> > 1. Branch `feat/split-panes`: `git switch feat/split-panes && make -C c`.
> > 2. Windows Terminal: Alt + Shift + − splits the focused pane in two (below); drag the divider to make the room's pane small. Any terminal window made small works too.

> [!todo]- Group P · macOS + iTerm2: the room in iTerm2 (T98–T105)
> - [ ] **T98** iTerm2 split panes (Cmd+D, Cmd+Shift+D) get gates between them
> - [ ] **T99** iTerm2: Profiles → Clawd Room starts the room directly, already a quarter of the screen (no resize); `q` closes the window
> - [ ] **T100** iTerm2: `./clawd-room.sh` typed in a normal window: the room's window opens in its place (4x4 pt square pixels) and the typed-in window closes; `--keep` keeps it, `--here` runs in it
> - [ ] **T101** iTerm2: Cmd + / − / 0 do nothing in a room window
> - [ ] **T102** iTerm2: a new room opens as a quarter of the screen, in the quarter it was in
> - [ ] **T103** iTerm2: smaller than 44x20 then bigger: the room comes back, nothing scrolled; no old frames in scrollback after quitting
> - [ ] **T104** iTerm2: stripes only while dragging, a clean picture after
> - [ ] **T105** iTerm2: quitting gives the window title back and leaves no `osascript`

> [!todo]- Group Q · WSL: older checks never ticked (T106–T109)
> - [ ] **T106** In the editor, `F` on a prop adds `NAME~2`; draw it: the prop animates in a running room
> - [ ] **T107** Going down the well: the Clawd walks down its steps and disappears into the hole; coming up it rises out of it
> - [ ] **T108** Gates rise in eased (fast, then settling); doors swing open and shut smoothly, turning back if a Clawd comes back
> - [ ] **T109** Change a line in `rooms/office.txt` (e.g. the window's prefer): the window moves in a running room

Groups on open branches live on their branch (M). Done groups and the older checks by subject: [[Test Checklist Archive]].
