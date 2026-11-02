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

Groups on open branches live on their branch (M, N, O). Done groups and the older checks by subject: [[Test Checklist Archive]].
