---
title: Dev Status
tags:
  - clawd-room
  - status
sticker: lucide//activity
---
> [!important] For AI assistants
> Read [[LLM Guide]] before any `CMD_` or status update. This note is kept by `CMD_SYNC` / `CMD_ROLLOVER` / `CMD_NOTES_SYNC`; the day notes in `Review/Log/` by `CMD_JOURNAL`.

## 🚀 Today's Progress (2026-10-30)
- **Callout States**: A callout's type shows its block's state (`[!todo]`, `[!done]`, `[!warning]`, `[!question]`, `[!failure]`), callouts before tables; rule in [[LLM Guide]] (#14).
- **Split Panes**: A room in a Windows Terminal split pane finds its own pane, gets gates to neighbouring panes and never resizes the shared window or sends font keys (#15, open; to test: T67 – T72 in [[Test Checklist]]).
- **Crowded Rooms Picked**: Scattered workspaces mixed with loose clusters, smaller mixed furniture, and activities for Clawds without a seat instead of "+N MORE" (mockups; not built) ([[Rooms and Art]]).
- **Mock Programs**: [[LLM Guide]] says how to write a mockup program (the room's own drawing, the real size, one build line that survives other branches' leftovers).

## 📅 Yesterday's Progress (2026-10-27)
- **Dev Log Graph**: `tools/git_graph.py` draws the last merged code branches of `main` as a woven gitGraph and writes it into the [[Dev Log]]; the graph's colours and branch chips follow the lane (#9).
- **Mockups Stay Local**: A mockup is local scratch; only the picked option and one side-by-side picture go into the vault (#8).
- **Stale Hook Path**: [[LLM Guide]] and [[Home]] say what to do when a moved clone leaves an old `hook.py` path in `settings.json`; no absolute paths or symlinks in the repo or notes.
- **Vault Layout**: Long titles in the Days list wrap instead of scrolling sideways; general-layout section 1 stays empty (#9).

---

## ⏳ Ongoing

### 🎨 Rooms and art
- [ ] **Railed Stair Well**: Build design A (a bigger opening, steps right to left, a guard rail); no big tree in the park when it has a way up ([[2026-10-20]]).
- [>] **Gates Appear**: Built and merged (#22, #24): themes `simple` and `construct`, the `settings.conf` line, the picker's THEMES tab. Not yet: the park's own gates, gates leaving, the construct's pieces as authored sprites.
- [ ] **Art as Sprite Pieces**: Geometry stays in code (perspective, how many steps), the look moves to small sprites the code places and repeats (a tread, a baluster, a length of rail, a door leaf), so a gate, desk or well can be edited in the pixel editor; the default sprites must reproduce today's pictures exactly (snapshots). Props already work this way.
- [ ] **Crowded Rooms**: Build the picked layout: scattered spots mixed with clusters of 2–3, the smaller mixed furniture, activities for Clawds without a seat (demolish, fire, clean up, play), so "+N MORE" never shows ([[Rooms and Art]], [[2026-10-30]]).
- [?] **Crowded Rooms, Open**: Hot desking (waiting / idle Clawds give up their seat)? Activity by state or at random?
- [x] **Docs Look**: `docs/Linked-Rooms.md` describes the current gates (doors, stairs, wells on the walls and floor), the bottom bar's `G` / `M` hint and how rooms find each other; `docs/Platforms.md` was checked and fits. Still to look at: the code names the park's way up `TREEHOUSE` in the bar's gate names, while the park has a corner building with a stoop.

### 🎮 Interaction
- [>] **Picked Clawd**: Built on `feat/picked-clawd` (#6): Tab puts yellow corners and a soft spotlight (60 in 100) on the picked Clawd, they glide on the next pick and stay until Esc; Esc lets go of it and no longer quits the room (only `q` does); to check by eye: T58 – T61 (Group L of [[Test Checklist]]).
- [ ] **Bar Below the Room, Gate Tags**: The room above a one-line bar; while `g` / `m` is held a tag centred over each gate (arrow style still to pick).
- [>] **Settings**: `~/.clawd-room/settings.conf` exists with one line, `gate_theme` (a missing theme falls back to simple quietly). Not yet: more settings and the panel beside the room on `.`, which reads and writes the same file.
- [ ] **Room Setup Page**: An editor page to choose, per room, which Clawd look / role and which desks, props and options appear. Needs the data first: `rooms/*.txt` declares a room's desks, roles and allowed props (today the code decides), then the page edits those files with the room as the preview.
- [?] **Every Size**: A strip for panes under 44x20 (S1 band, S2 names, S3 chips; mini Clawds under 8 rows): which one ([[Rooms and Art]])?
- [?] **Clawds That React**: Which first: a waiting Clawd waves (taskbar flash), eyes follow the mouse, click for a card, click to bring its terminal forward (needs `WT_SESSION` in the hook), drag to a gate, a coffee treat.
- [ ] **Moving, Step 3**: Drag a Clawd with the mouse onto a door, the stairs or the well. (Settled: `--try real` stays split; uneven shares are fine.)

### 🖥️ Platforms
- [ ] **Mac Check**: Run `make -C c test` on the Mac after the fused multiply-add change (it should still match).
- [ ] **Blocked Writes**: The room can block writing when the terminal stops reading (seen with iTerm2 in full screen).
- [>] **Windows Terminal Panes**: Built on `feat/split-panes` (#15): panes seen through UI Automation, gates between panes, no resize or font keys in a pane. To check by eye: T67 – T72 (Group N of [[Test Checklist]]); panes on other virtual desktops are not seen (Windows hides them).

### ⚙️ Engine and tools
- [x] **Stage Page Polish**: Esc picks nothing (#21), folding the panel and timeline, the picker's search focus, the eye + KEYS button (#22); tests T39 – T50 are in [[Test Checklist]].
- [ ] **Editor Add Buttons**: NEW in the picker for what the room already looks up by name: a prop (placed through a room's wall plan), an animation (a copy of a template), a gate theme (a copy of `simple`). A UI icon waits until the code asks for a name. Aimed at Zaviel, William and later outside people, so friendly (names checked, templates, short help).
- [ ] **Less CPU**: Skip frames when nothing moves; stop drawing (FPS 0) when the window is covered, minimised or on another desktop; later dirty rects. (The terminal output is already diffed and written once per frame.)

### 🗂️ Repo and vault
- [x] **Pull Requests**: The public repo counts from #1: #1 – #6 and #8 – #9 merged, #7 closed, #10 (day note 2026-10-27) open. Only merged branches are deleted.
- [x] **Go Public** (2026-10-24): `zelvet-velvet/clawd-room` is public with an MIT `LICENSE` and a clean history (a new repo; the old one is `clawd-room-pre-public`, private, which can be archived). `main` is protected: changes only through a pull request with one approval from a code owner (`.github/CODEOWNERS`: Zaviel), no force-push, no deletion, conversations resolved; admins are not enforced, so Zaviel merges their own PRs. Merge commits only, branches deleted on merge.
- [>] **Contributors Setup**: Invitations to the new repo are sent; each contributor accepts, clones the new repo (or `git fetch && git reset --hard origin/main` in an old clone, after saving unmerged work), sets Templater ([[Collaboration]]).

## 💡 Ideas
- Animations gallery: `--build-art` GIFs sorted into category folders (Session states, Getting around, Playground, Together, On their own) with Gallery notes; the GIF colour rounding is already in. Dropped from Ongoing 2026-10-23 with the `wip/animations-stash` branch (a small `action_folder()` function, easy to write again).
- A seesaw in the park; an empty-desk look for Clawds that are away.
- Camera polish: smooth zoom, a slight lead in the walking direction, a small bounce when a Clawd finishes, parallax in the park.
- Full screen: a "hub" room that others visit, or several rooms side by side in one window.

---

## 🏁 Last Progress

### 🎨 Rooms and art
- **Style A Rooms**: Office, coffee shop and park in one-point perspective; workspaces in rows; Clawds behind desks, the barista behind the bar.
- **Stairs and Wells**: Real steps to a half floor with a railing Clawds walk behind; a stair well at the front left they walk down into.
- **Park Plaza**: Skyline, promenade, lamps, benches; side buildings with doors; one corner building with a stoop as the way up.

### 🎮 Interaction
- **Camera and Room Size**: Zoom, follow, free camera, room sizes; the camera goes through gates with a Clawd.
- **Linked Rooms**: Gates on walls, ceiling and floor; `g` sends, `b` calls back; `m` moves a Clawd with packing and unpacking; desks stay in their room.
- **Bottom Bar**: Status and the `g` / `m` hint in the room's pixel font.

### 🖥️ Platforms
- **WSL Tests**: Every check in the WSL test queue passes (T1 – T30): profile, rooms, bar, sending and moving, desks, the pixel editor.
- **C Room**: The terminal room in C11 (a tenth of Python's CPU and memory).
- **macOS + iTerm2**: Builds and runs; window helper with osascript; fixed pixel size profile; `--try`.

### ⚙️ Engine and tools
- **Part Animations in the Editor**: A file per animation (parts with keys), the swing driven by it, the editor's stage page with an ANIMS tab in the picker, onion skin, zoom and pan; T31 – T38 pass on WSL ([[Interaction]]).
- **Pixel Editor Animates**: A timeline of an animation's frames, play at the room's speed, onion skin, a fill tool, a sprite picker, your own sprites over the defaults ([[Interaction]]); T24 – T30 pass on WSL.
- **Engine Plan Built**: Scene and layers, placer, sprite and room files, pixel editor, animator; `--check` at 0 overlaps.
