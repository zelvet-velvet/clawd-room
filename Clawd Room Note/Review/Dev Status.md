---
title: Dev Status
tags:
  - clawd-room
  - status
sticker: lucide//activity
---
> [!important] For AI assistants
> Kept by the `/cmd-*` skills (`.claude/skills/`, table in `AGENTS.md`). Read only the part you need. Never count by hand: `python3 tools/dev_status.py counts` rewrites every callout title. Finished work: [[Progress Archive]].

## 🚀 Today's Progress (2026-10-30)
> [!abstract]+ 4 items
> - **Callout States**: A callout's type shows its block's state (`[!todo]`, `[!done]`, `[!warning]`, `[!question]`, `[!failure]`), callouts before tables; rule in [[LLM Guide]] (#14).
> - **Split Panes**: A room in a Windows Terminal split pane finds its own pane, gets gates to neighbouring panes and never resizes the shared window or sends font keys (#15, open; to test: T67 – T72 in [[Test Checklist]]).
> - **Crowded Rooms Picked**: Scattered workspaces mixed with loose clusters, smaller mixed furniture, and activities for Clawds without a seat instead of "+N MORE" (mockups; not built) ([[Rooms and Art]]).
> - **Mock Programs**: [[LLM Guide]] says how to write a mockup program (the room's own drawing, the real size, one build line that survives other branches' leftovers).

## 📅 Yesterday's Progress (2026-10-27)
> [!abstract]- 4 items
> - **Dev Log Graph**: `tools/git_graph.py` draws the last merged code branches of `main` as a woven gitGraph and writes it into the [[Dev Log]]; the graph's colours and branch chips follow the lane (#9).
> - **Mockups Stay Local**: A mockup is local scratch; only the picked option and one side-by-side picture go into the vault (#8).
> - **Stale Hook Path**: [[LLM Guide]] and [[Home]] say what to do when a moved clone leaves an old `hook.py` path in `settings.json`; no absolute paths or symlinks in the repo or notes.
> - **Vault Layout**: Long titles in the Days list wrap instead of scrolling sideways; general-layout section 1 stays empty (#9).

---

## ⏳ Ongoing
Status only; what each task is and what it needs: [[Task Queue]] (by W-ID). Open questions stay here.

### 🎨 Rooms and art
> [!question]- 5 open
> - [ ] **Railed Stair Well** (W1): design A picked, not built.
> - [>] **Gates Appear** (W2): themes `simple` and `construct` merged (#22, #24). Not yet: the park's own gates, gates leaving, the construct's pieces as authored sprites.
> - [ ] **Art as Sprite Pieces** (W3): needs a design chat first.
> - [ ] **Crowded Rooms** (W15): layout picked 2026-10-30, not built ([[Rooms and Art]]).
> - [?] **Crowded Rooms, Open**: Hot desking (waiting / idle Clawds give up their seat)? Activity by state or at random?
> - [x] **Docs Look**: `docs/Linked-Rooms.md` describes the current gates (doors, stairs, wells on the walls and floor), the bottom bar's `G` / `M` hint and how rooms find each other; `docs/Platforms.md` was checked and fits. Still to look at: the code names the park's way up `TREEHOUSE` in the bar's gate names, while the park has a corner building with a stoop.

### 🎮 Interaction
> [!question]- 6 open
> - [x] **Picked Clawd** (W4): merged (#6); Tab puts yellow corners and a soft spotlight on the picked Clawd, Esc lets go; T58 – T62 pass.
> - [ ] **Bar Below the Room, Gate Tags** (W5): arrow style still to pick.
> - [>] **Settings** (W6): `settings.conf` has one line, `gate_theme`. Not yet: more settings, the panel on `.`.
> - [ ] **Room Setup Page** (W7): `rooms/*.txt` must declare desks, roles and props first.
> - [?] **Every Size**: A strip for panes under 44x20 (S1 band, S2 names, S3 chips; mini Clawds under 8 rows): which one ([[Rooms and Art]])?
> - [?] **Clawds That React**: Which first: a waiting Clawd waves (taskbar flash), eyes follow the mouse, click for a card, click to bring its terminal forward (needs `WT_SESSION` in the hook), drag to a gate, a coffee treat.
> - [ ] **Moving, Step 3** (W8): drag a Clawd onto a gate.

### 🖥️ Platforms
> [!todo]- 3 open
> - [ ] **Mac Check** (W11): after the fused multiply-add change.
> - [ ] **Blocked Writes** (W12): seen with iTerm2 in full screen.
> - [>] **Windows Terminal Panes** (W13): built on `feat/split-panes` (#15); to check: T67 – T72 (Group N, on that branch). Panes on other virtual desktops are not seen.

### ⚙️ Engine and tools
> [!todo]- 2 open
> - [x] **Stage Page Polish**: Esc picks nothing (#21), folding the panel and timeline, the picker's search focus, the eye + KEYS button (#22); tests T39 – T50 are in [[Test Checklist]].
> - [ ] **Editor Add Buttons** (W9): needs a mockup.
> - [ ] **Less CPU** (W10).

### 🗂️ Repo and vault
> [!todo]- 1 open
> - [x] **Pull Requests**: The public repo counts from #1: #1 – #6 and #8 – #9 merged, #7 closed, #10 (day note 2026-10-27) open. Only merged branches are deleted.
> - [x] **Go Public** (2026-10-24): `zelvet-velvet/clawd-room` is public with an MIT `LICENSE` and a clean history (a new repo; the old one is `clawd-room-pre-public`, private, which can be archived). `main` is protected: changes only through a pull request with one approval from a code owner (`.github/CODEOWNERS`: Zaviel), no force-push, no deletion, conversations resolved; admins are not enforced, so Zaviel merges their own PRs. Merge commits only, branches deleted on merge.
> - [>] **Contributors Setup**: Invitations to the new repo are sent; each contributor accepts, clones the new repo (or `git fetch && git reset --hard origin/main` in an old clone, after saving unmerged work), sets Templater ([[Collaboration]]).

---
## 🏁 Last Progress
Finished work moved out of Ongoing is in [[Progress Archive]] (`/cmd-notes-sync` adds to it). Read it only when asked or for history.
