# clawd-room: agent instructions

A pixel room where every running Claude Code session is a little Clawd (terminal UI, C11 in `c/`; WSL2 + Windows Terminal and macOS + iTerm2). `hook.py` appends one JSON line per event to `~/.clawd-room/events.jsonl`; the room follows that file. Any coding agent reads this file; Claude Code reads it through `CLAUDE.md`.

## People and git
- **Zaviel** (GitHub `zelvet-velvet`) is the owner; everyone else is a contributor. Find out whom you work with from `git config user.name` (ask if unsure); commit with that identity (set in the repo, not globally).
- Work on a `type/slug` branch off a fresh `main`, open a pull request. Never push to `main`; never merge on your own judgement: Zaviel merges. Details: skill `room-git`.
- Each PR: `make -C c test` passes, a Dev Log entry with the author's name, pictures for visual changes.
- Visual changes start with a rendered mockup picture that Zaviel picks: skill `room-mockups`.
- Helping a newcomer: explain the code and the reasons behind the rules when asked (`CONTRIBUTING.md` is the human version).

## Rules that prevent breakage
- **Privacy**: the events file holds only `t, sid, ev, cwd (folder basename), tool`. Never widen it, print or commit its contents. Nothing private in the repo, notes or pictures (it is public): no real project names (test data: `web-shop`, `api-server`), no real desktops in screenshots, no personal paths.
- **No dependencies, no network, no daemons**: C11 + libc/libm; Python standard library only. Writes only to the repo and `~/.clawd-room/`. Leave `room.py` alone.
- **Never edit `~/.claude/settings.json`**: tell the user to run `python3 install_hooks.py` (prints; `--apply` only if they say so). Hook paths are per machine: never write one into the repo, notes or memory. Blocked by an old hook path: `Clawd Room Note/Start/LLM Guide.md`, section Hooks.
- **Snapshots are the contract**: `make -C c test` must pass; an intended change updates them (`tools/test.sh --update`) and says so. Do not add or remove random calls casually. Launchers call `clawd-room.sh`.
- **Never touch the user's windows**: helpers only look at titles starting `clawd-room `; test with your own (`alpha`, `beta`). Testing: skill `room-testing`.

## Where to read (only what the task needs)
- Map of the code and the vault, conventions for notes: `Clawd Room Note/Start/LLM Guide.md`.
- Where things stand: `Clawd Room Note/Review/Dev Status.md` (Today, Yesterday, Ongoing). History: `Review/Progress Archive.md`, day notes in `Review/Log/`, `git log`. Read archives only when asked.
- How a part works now: `Clawd Room Note/Design/<note>.md`. Exact behaviour: the code.
- Large notes (`Design/Pixel Editor.md`, `Design/Rooms and Art.md`, `_Meta/Snapshot status.md`, the archives, big day notes): grep first, read the matching section. Never read `Gallery/` (generated), `Assets/` or `_Meta/Theme Color Preview.md` (swatches for people).
- Read a note right before editing it: the user edits in Obsidian at the same time; never restore what they rolled back.

## Commands
The user types a command. Each one is a project skill in `.claude/skills/<skill>/SKILL.md`; an agent without skill support opens that file and follows it.

| Command | Skill | What it does |
| :--- | :--- | :--- |
| `CMD_SYNC` | `room-sync` | Today / Yesterday / Ongoing in Dev Status from the chat |
| `CMD_JOURNAL [date]` | `room-journal` | The day note's entries (`Review/Log/…`) |
| `CMD_ROLLOVER` | `room-rollover` | Today → Yesterday (script `tools/dev_status.py`) |
| `CMD_NOTES_SYNC [date]` | `room-notes-sync` | Design notes, pictures, test queue against the code; `[x]` Ongoing → Progress Archive |
| (branch, commit, PR, merge) | `room-git` | Branches, messages, pull requests, who merges |
| (a visual change, a mockup) | `room-mockups` | Mock programs, showing options, recording the pick |
| (running or adding tests) | `room-testing` | Test commands, real windows, worktrees |
| (test groups, results) | `room-test-queue` | `Review/Test Checklist.md` rules |

Vault changes go on their own branch as `docs(vault): …` with a pull request. Stage files by name, never a folder.
