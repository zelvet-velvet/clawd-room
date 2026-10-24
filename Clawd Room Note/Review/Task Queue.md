---
title: Task Queue
tags:
  - clawd-room
  - tasks
sticker: lucide//list-checks
---
Work anyone can pick up. Each task has an ID (`W1`, `W2` …) that never changes, and sits on **one line** so two people claiming different tasks rarely conflict. Where things stand overall is in [[Dev Status]]; how to work (branches, pull requests, tests) is in `CONTRIBUTING.md`. Boxes follow [[Checkbox Style]].

> [!tip] How to claim a task
> 1. Pick a task whose box is empty `[ ]`. Check nobody else has it in an open pull request: `gh pr list --search "W7"` (or the Pull requests tab).
> 2. Branch off a fresh `main`: `git switch main && git pull && git switch -c feat/short-slug`.
> 3. In **your first commit** change the box to `[/]` and add `🙋 @your-github-name · feat/short-slug` at the end of the line. Push and open a **draft pull request** titled with the ID (`W7: railed stair well`). The draft is the claim: it is how others see the task is taken.
> 4. Work on the branch. Finish it as `CONTRIBUTING.md` says (tests, a Dev Log entry, pictures for anything visual) and take the PR out of draft.
> 5. In the same PR move the line to **Taken and done** at the bottom, tick it `[x]` and add the PR number. The owner merges.
>
> **Let go:** close the draft PR and put the line back as `[ ]` without the name. A claim with no commits for two weeks is free for anyone to take (ask on the PR first).

> [!warning] Before you start
> - **Visual tasks start with a picture.** Where *needs* says *mockup*, post a rendered mockup in the draft PR and wait for the owner to pick before building (see [[LLM Guide]]).
> - **Snapshots are the contract.** A change that alters what is drawn updates the snapshots on purpose and says so in the PR.
> - **Big or unclear?** Say so in the draft PR before you build; a short question saves a long detour.

> [!example] 🎨 Rooms and art
> - [ ] **W1** **Railed stair well**: a bigger opening, steps right to left, a guard rail; no big tree in the park when it has a way up (design A is picked, [[2026-10-20]]) · *M · needs: picked design*
> - [ ] **W2** **Gates in the park and gates leaving**: the park's own fence building and stoop as part animations, and gates that go away as well as appear ([[Art Data and Editor]]) · *M · needs: mockup*
> - [ ] **W3** **Art as sprite pieces**: geometry stays in code, the look moves to small sprites the code places and repeats (a tread, a baluster, a length of rail), so gates, desks and wells can be edited in the pixel editor; the default sprites must reproduce today's pictures exactly · *L · needs: a design chat first*

> [!example] 🎮 Interaction
> - [ ] **W4** **Picked Clawd**: yellow corners and a spotlight (60 in 100) that fade in and out on Tab (design picked) · *M · needs: picked design*
> - [ ] **W5** **Bar below the room, gate tags**: the room above a one-line bar; while `g` / `m` is held a tag centred over each gate · *M · needs: mockup (arrow style to pick)*
> - [ ] **W6** **Settings panel**: more settings in `~/.clawd-room/settings.conf` and a panel beside the room on `.` that reads and writes the same file · *L · needs: mockup*
> - [ ] **W7** **Room setup page**: an editor page to choose, per room, which Clawd look / role and which desks, props and options appear; `rooms/*.txt` must declare them first · *L · needs: a design chat first*
> - [ ] **W8** **Moving, step 3**: drag a Clawd with the mouse onto a door, the stairs or the well · *L*

> [!example] ⚙️ Engine and tools
> - [ ] **W9** **Editor add buttons**: NEW in the picker for a prop, an animation (a copy of a template) and a gate theme (a copy of `simple`); friendly (names checked, templates, short help) · *M · needs: mockup*
> - [ ] **W10** **Less CPU**: skip frames when nothing moves; stop drawing when the window is covered, minimised or on another desktop · *M*

> [!example] 🖥️ Platforms
> - [ ] **W11** **Mac check**: run `make -C c test` on a Mac and report whether it matches · *S · needs: a Mac with iTerm2*
> - [ ] **W12** **Blocked writes**: the room can block writing when the terminal stops reading (seen with iTerm2 in full screen) · *M · needs: a Mac with iTerm2*
> - [ ] **W13** **Windows Terminal panes**: split panes are not seen as separate windows yet · *M · needs: WSL and Windows Terminal*
> - [ ] **W14** **The park's way up in the bar**: the bar names the park's way up `TREEHOUSE` (in `c/app.c`) while the park has a corner building with a stoop; rename it and the stale "treehouse" wording in [[Interaction]] (a snapshot changes) · *S*

> [!question]- 💡 Ideas (not decided yet)
> Talk about one in an issue before building it.
> - A seesaw in the park.
> - An empty-desk look for Clawds that are away.
> - Camera polish: smooth zoom, a slight lead in the walking direction, a small bounce when a Clawd finishes, parallax in the park.
> - A full-screen "hub" room that others visit.

> [!success]- ✅ Taken and done
> Move a finished line here, ticked `[x]`, with the PR number, for example `- [x] **W7** Room setup page · #31`. The history of what changed is in [[Dev Log]].
