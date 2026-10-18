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
## Rooms and art ([[Rooms and Art]])
- [ ] Each room (office, coffee shop, park) looks like the mockups at your window size
- [ ] Seated Clawds show behind their desk / table / bench; the barista behind the bar
- [ ] Coffee shop: tables, square cushioned chairs, laptop / tablet / phone, coffee / matcha look right
- [ ] Park: bench, swing and picnic table workspaces; the swing and slide stand at 45° and Clawds play on them where they are drawn
- [ ] Park: with a fence on one side, no bushes on that side
- [ ] The room's sign reads well; with stairs it hangs in front of them
- [ ] Dots over heads; the name shows only for a Clawd that needs you
- [ ] A crowded small room (several gates and Clawds) still reads; nothing collides badly
- [ ] Happy Clawds keep 1x1 dark eyes with the peach top and blush

## Gates and crossing ([[Interaction]])
- [x] Two linked windows side by side: gates appear, `g` sends a Clawd across (2026-10-17, older gates)
- [ ] Side by side: a door on each room's facing wall; `g`: the Clawd walks to the door, it opens, the Clawd goes through and comes out of the other room's door
- [ ] Stacked: the upper room shows the small stair well at the back right (office / coffee) or a small stone well between the swing and the slide (park); the lower room shows the stairs on the back wall (office / coffee) or the treehouse (park)
- [ ] Office stairs are steel with slim steel balusters (no glass), coffee shop stairs wooden; the way up is a tall opening at the right end of the platform with steps going up and a light
- [ ] Side doors look tall enough for a Clawd; the stair well sits on open floor with a wooden rim and reads as a hole with steps going down
- [ ] Going up: the Clawd climbs the stairs to the half floor, walks along it and goes through the opening (or up the treehouse ladder); coming down the other way round; going down: it walks into the well / steps
- [ ] Gates appearing animate in, in both windows at the same moment; gates there at start are simply there
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
- [ ] iTerm2: a new room opens as a quarter of the screen, in the quarter it was in
- [ ] iTerm2: smaller than 44x20 then bigger: the room comes back, nothing scrolled; no old frames in scrollback after quitting
- [ ] iTerm2: stripes only while dragging, a clean picture after
- [ ] iTerm2: quitting gives the window title back and leaves no `osascript`
- [ ] Sprite files: change a grid in `sprites/clawd.txt`, the change shows; delete the frame, the original is back
