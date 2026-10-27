# Contributing to clawd-room

Welcome! This is a small project by **Zaviel** (owner, `zelvet-velvet`), open to anyone who wants to help. This page is the agreed way of working. If something here does not fit, say so in an issue and we change the page.

---
## 1. Who decides what

| Role | Who | Does |
| --- | --- | --- |
| Owner | Zaviel (`zelvet-velvet`) | decides the look and the features, reviews and merges pull requests into `main`, keeps the Dev Log and notes in shape |
| Contributor | you | picks or proposes issues, works on branches, opens pull requests, reviews others' PRs when asked |

- **The look is the owner's call.** Visual changes (pixel art, rooms, colours, layout) start as a picture: post a mockup or a screenshot in the issue, Zaviel picks, then it is built. Keep your mockups local (`gallery/` is not in git); the picked one and the side-by-side picture of all the options go into the vault (`Clawd Room Note/Assets/Snapshots/`).
- **`main` is the stable line.** Nobody pushes to `main` directly; everything arrives through a pull request (PR) that Zaviel merges.
- **Ask early.** A short question in the issue saves a long PR that goes the wrong way.

---
## 2. First day (setup)

1. Clone (collaborators accept the GitHub invitation first; everyone else forks the repo and opens pull requests from the fork, replacing the URL below with their fork's):
   ```
   git clone git@github.com:zelvet-velvet/clawd-room.git
   cd clawd-room
   git config user.name  "Your Name"         # your own identity, set in this repo only
   git config user.email "you@example.com"
   ```
2. Build and test (needs `make` and a C compiler: `sudo apt install build-essential` on WSL/Linux, `xcode-select --install` on macOS):
   ```
   make -C c test        # about a second; must say "all snapshots match"
   ```
3. Look at it: `./clawd-room.sh --try` opens four linked rooms with made-up Clawds (iTerm2 on macOS, Windows Terminal on WSL). On WSL first run `python3 setup_profile.py --update` once (adds the "Clawd Room" profile, 5 pt font).
4. Optional, to see your own Claude Code sessions as Clawds: `python3 install_hooks.py` shows the change to `~/.claude/settings.json`; `--apply` installs it. The hook remembers where the repo is, so **do not move the repo folder** afterwards (or run `--apply` again).
5. Read, in this order: this page, `Clawd Room Note/Start/LLM Guide.md` (map and rules, written for AI assistants but short and useful for people), `Clawd Room Note/Review/Dev Log.md` (the days, what is pending). The `Clawd Room Note/` folder is an Obsidian vault (plain Markdown works too); how to share it is in `Clawd Room Note/Start/Collaboration.md`.

---
## 3. The everyday loop (SOP)

```
issue  ->  branch  ->  commits  ->  pull request  ->  review  ->  merge  ->  delete branch
```

1. **Issue first.** Every piece of work has a GitHub issue (Issues tab): what and why, a picture if it is visual. Assign yourself (or comment that you are on it), so we never work on the same thing. Small fixes can skip this and go straight to a PR.
2. **Branch from a fresh `main`:**
   ```
   git switch main && git pull
   git switch -c feat/seesaw          # type/short-slug: feat, fix, refactor, chore, docs
   ```
3. **Commit small and often**, message `type(scope): summary` (lowercase, imperative, no period), e.g. `feat(park): add a seesaw` or `fix(gates): wait one second for still windows`. Push your branch whenever you like: `git push -u origin feat/seesaw`.
4. **Before opening the PR:** `make -C c test` passes (see 5 for snapshots), the change is described in a short Dev Log entry (see 6), and your branch is up to date with `main`:
   ```
   git fetch && git merge origin/main     # fix conflicts if any, test again, push
   ```
5. **Open the PR** (`gh pr create` or the GitHub page) into `main`. Fill in the template: what changed, pictures for anything visual, what you tested, what still needs someone's eyes. Link the issue (`Closes #12`).
6. **Review.** Zaviel comments or approves. Answer comments with new commits on the same branch (no force-push once a PR is open). A PR is merged when tests pass and Zaviel says yes.
7. **Merge** (Zaviel): "Create a merge commit" (each feature shows as one branch in the history). Then the branch is deleted on GitHub; locally: `git switch main && git pull && git branch -d feat/seesaw`.

Keep PRs to **one topic** and ideally under a few hundred lines; two small PRs review much faster than one big one.

---
## 4. What you can work on

| Kind | Where | Notes |
| --- | --- | --- |
| **Pixel art** (Clawd frames, props, items) | `sprites/*.txt`, edit with `./clawd-room.sh --edit NAME` (`--edit list` shows what exists) | Rooms redraw as you save. Post a before / after picture in the PR. Sprite changes alter the snapshots (see 5). |
| **Room layouts** (what hangs on the walls) | `rooms/*.txt` | Run `./c/clawd-room --check` afterwards: it must stay at **0 overlaps**. |
| **Rooms and drawing** (new props, gates, a new room) | `c/theme.c`, `c/draw.c`, `c/persp.c`, `c/scene.c`, `c/place.c` | Mockup first (a picture Zaviel picks), then code. |
| **Behaviour** (Clawd states, walking, visits, moves) | `c/room.c`, `c/clawd.c`, `c/anim.c` | Keep the timings unless agreed (90 s sleep, 30 min stale, 2.5 s cheer). Add a test scene in `tools/scenarios/`. |
| **Linked windows, platforms** | `c/link.c`, `c/win.c`, `c/ps_scripts.h`, `c/mac_scripts.h`, `clawd-room.sh` | Say which platform you tested on; the other one is checked by its owner. |
| **Tools, docs** | `tools/`, `README.md`, `docs/`, `Clawd Room Note/` | |

Work waiting for someone is in the **Task Queue** (`Clawd Room Note/Review/Task Queue.md`): pick a free row, claim it with a draft pull request titled with its ID, and work on your own branch (how it works is at the top of that note). Ideas that are not decided yet are at the bottom of it; ask in an issue before starting a big one.

---
## 5. Tests and snapshots

- `make -C c test` compares the program with saved results in `c/tests/expected/`. If it fails and you **did not mean** to change what is drawn, it is a bug: fix it.
- If you **did** mean to (new art, a new look), run `tools/test.sh --update`, and say in the PR which snapshot files changed and why. Never update snapshots just to make a failing test pass.
- Also: `make -C c smoke` (keys in a real terminal), `linksmoke` / `movesmoke` / `splitsmoke` (linked rooms), `./c/clawd-room --check` (0 overlaps).
- New behaviour gets a test case: a fixture in `tools/fixtures/*.fx` or a scene in `tools/scenarios/*.sc`.

---
## 6. Notes and the Dev Log

The notes are an Obsidian vault in git; setup, what is shared and how to write in it: `Clawd Room Note/Start/Collaboration.md`. In short:

- Every PR adds a short entry to today's note in `Clawd Room Note/Review/Log/<year>/<month>/<week>/` (`YYYY-MM-DD.md`, newest first; in Obsidian *Open today's daily note* makes it in the right folder): a title, two to four plain bullets about what changed **for someone using the room**, and your name in brackets after the title, e.g. `(Zaviel)`. Pictures go in the subject folder under `Clawd Room Note/Assets/Snapshots/` (Office, Park, Gates ...) as `YYYY-MM-DD subject.png`.
- Several people adding entries at the top of the same file conflict easily: when `git merge origin/main` reports a conflict in the Dev Log, keep **both** entries.
- Do not commit your own Obsidian layout or settings (`git restore "Clawd Room Note/.obsidian"` if you changed one by accident).
- Where things stand is in `Clawd Room Note/Review/Dev Status.md`. Your Claude keeps it and the notes with `CMD_SYNC`, `CMD_JOURNAL`, `CMD_NOTES_SYNC`, `CMD_ROLLOVER` (see the LLM guide); bigger rewrites of the design notes (`Clawd Room Note/Design/`) happen in `CMD_NOTES_SYNC`.

---
## 7. Rules that are not negotiable

- **Privacy.** `~/.clawd-room/events.jsonl` describes real work. Never commit it, paste it into issues or widen what the hook records (`t, sid, ev, cwd folder name, tool` only).
- **Nothing private in the repo** (it is to become public, notes included): no real folder or project names from your sessions (test data uses made-up ones), no work details, no screenshots showing your desktop, other apps or real session names.
- **No dependencies.** C11 with libc/libm only; Python tools use the standard library only.
- **No network, no background daemons.**
- **Do not hurt anyone's windows** when testing: the helpers only touch windows titled `clawd-room ...`.
- **No secrets** in the repo (tokens, keys, personal paths in code).
- **Never force-push** a shared branch or `main`; never rewrite published history.

---
## 8. Working with an AI assistant (Claude Code)

Many of us use Claude Code. `CLAUDE.md` points it to the guide, so it follows the same rules. Still:
- You are responsible for what it commits: read the diff before you push.
- Let it open the PR (`gh pr create`) but not merge it; merging is the owner's step. (The owner may tell their own Claude to merge a PR they have tested.)
- Visual changes: ask it for a rendered mockup picture first, and post that in the issue.

---
## 9. Talking

- Questions about a task: in its issue. About a PR: in the PR. Anything bigger or a new idea: open an issue labelled `idea`.
- Found a bug you will not fix now: open an issue with what you did, what you saw, and a screenshot.
