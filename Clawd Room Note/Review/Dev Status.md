---
title: Dev Status
tags:
  - clawd-room
  - status
sticker: lucide//activity
---
> [!important] For AI assistants
> Read [[LLM Guide]] before any `CMD_` or status update. This note is kept by `CMD_SYNC` / `CMD_ROLLOVER` / `CMD_NOTES_SYNC`; the day notes in `Review/Log/` by `CMD_JOURNAL`.

## 🚀 Today's Progress (2026-10-23)

## 📅 Yesterday's Progress (2026-10-22)
- **Stage Page, Gates, Esc**: Esc picks nothing on the stage page (#21); gates appearing are part animations with folding panels and an eye + KEYS button (#22); the vault sync (#23).
- **Gate Themes**: A theme is a folder `anims/gates/NAME/` (`simple`, `construct`), chosen by `--gate-theme`, `CLAWD_GATE_THEME` or `gate_theme` in `~/.clawd-room/settings.conf` (read again while the room runs) (#24).
- **Construct Gate**: A gate built from its own pieces that fly in under a blue outline in 1.4 s; a part's x / y / glow / show are drawn in the room (#24).
- **Picker THEMES Tab**: A card per theme (picked one plays), Space makes it active, Enter opens its files, NEW THEME copies the last picked; Esc in the picker never quits (#24).
- **Stage Page `I`**: Opens the pixel art of a sprite part; a part as wide as the stage fits whole; the editor window opens wider (#24).
- **Art Map**: New design note [[Art Data and Editor]] (the repo defaults and your copy, the two kinds of animation).

---

## ⏳ Ongoing

### 🎨 Rooms and art
- [ ] **Railed Stair Well**: Build design A (a bigger opening, steps right to left, a guard rail); no big tree in the park when it has a way up ([[2026-10-20]]).
- [>] **Gates Appear**: Built and merged (#22, #24): themes `simple` and `construct`, the `settings.conf` line, the picker's THEMES tab. Not yet: the park's own gates, gates leaving, the construct's pieces as authored sprites.
- [ ] **Art as Sprite Pieces**: Geometry stays in code (perspective, how many steps), the look moves to small sprites the code places and repeats (a tread, a baluster, a length of rail, a door leaf), so a gate, desk or well can be edited in the pixel editor; the default sprites must reproduce today's pictures exactly (snapshots). Props already work this way.
- [ ] **Docs Look**: `README.md` is a short front page now (`docs/` has the details, #20); `docs/Linked-Rooms.md` and `docs/Platforms.md` still describe the old flat look in places (banner, gates at the window edge).

### 🎮 Interaction
- [ ] **Picked Clawd**: Yellow corners and a spotlight (60 in 100) that fade in and out on Tab.
- [ ] **Bar Below the Room, Gate Tags**: The room above a one-line bar; while `g` / `m` is held a tag centred over each gate (arrow style still to pick).
- [>] **Settings**: `~/.clawd-room/settings.conf` exists with one line, `gate_theme` (a missing theme falls back to simple quietly). Not yet: more settings and the panel beside the room on `.`, which reads and writes the same file.
- [ ] **Room Setup Page**: An editor page to choose, per room, which Clawd look / role and which desks, props and options appear. Needs the data first: `rooms/*.txt` declares a room's desks, roles and allowed props (today the code decides), then the page edits those files with the room as the preview.
- [ ] **Moving, Step 3**: Drag a Clawd with the mouse onto a door, the stairs or the well. (Settled: `--try real` stays split; uneven shares are fine.)

### 🖥️ Platforms
- [ ] **Mac Check**: Run `make -C c test` on the Mac after the fused multiply-add change (it should still match).
- [ ] **Blocked Writes**: The room can block writing when the terminal stops reading (seen with iTerm2 in full screen).
- [ ] **Windows Terminal Panes**: Split panes are not seen separately yet.

### ⚙️ Engine and tools
- [>] **Animations**: `--build-art` to record room animations (packing, carrying, unpacking, doors, stairs, the well, each room) as GIFs, in category folders, with Gallery notes. Started (GIF colour rounding, category folders), parked in a stash.
- [x] **Stage Page Polish**: Esc picks nothing (#21), folding the panel and timeline, the picker's search focus, the eye + KEYS button (#22); tests T39 – T50 are in [[Test Checklist]].
- [ ] **Editor Add Buttons**: NEW in the picker for what the room already looks up by name: a prop (placed through a room's wall plan), an animation (a copy of a template), a gate theme (a copy of `simple`). A UI icon waits until the code asks for a name. Aimed at Zaviel, William and later outside people, so friendly (names checked, templates, short help).
- [ ] **Less CPU**: Skip frames when nothing moves; stop drawing (FPS 0) when the window is covered, minimised or on another desktop; later dirty rects. (The terminal output is already diffed and written once per frame.)

### 🗂️ Repo and vault
- [>] **Pull Requests**: #27 (prepare for going public) is the last open one; #1 – #12 and #19 – #26 merged; #13 – #18 closed into #19 and #20.
- [>] **Go Public** (decided 2026-10-20: code and notes public together): done on `docs/prepare-public`: the MIT `LICENSE` and the README's License section, your home path and email out of the LLM Guide, the iTerm2 screenshot cropped to the room window (it showed a Mac username and hostname). Still to do, at a quiet moment with no open PRs: rewrite the old fixture names and that screenshot out of the history (`git filter-repo`; everyone re-clones once; keep a backup first); decide `wip/animations-stash`; make the repo public; then **restrict merging**: protect `main` (no direct pushes, changes only through a pull request, Zaviel's approval to merge; free once public).
- [ ] **William's Setup**: Accept the invite, clone, Templater settings ([[Collaboration]]).

## 💡 Ideas
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
