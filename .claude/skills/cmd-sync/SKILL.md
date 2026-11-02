---
name: cmd-sync
description: clawd-room progress command. Use when the user runs /cmd-sync in the clawd-room repo (or asks to update Dev Status from this chat): updates Today, Yesterday and Ongoing in "Clawd Room Note/Review/Dev Status.md".
---
# /cmd-sync (clawd-room)

Target: `Clawd Room Note/Review/Dev Status.md`. Run commands from the repo root.

1. **Read first**: read the top of Dev Status (up to `## 🏁 Last Progress`) right before editing; the user may have changed or rolled back content in Obsidian: never restore it. Use the chat and `git log` / `git status` as sources.
2. **Date**: keep `## 🚀 Today's Progress (YYYY-MM-DD)` right. An earlier date with items → run `/cmd-rollover` first (or ask); empty → just set today's date.
3. **Update** `Today` (results of today), `Yesterday` only if it is wrong, and `Ongoing` by area: status only, with the task's W-ID from `Review/Task Queue.md`, where what it is and what it needs is written (add a new task there first). Boxes: `[x]` done, `[>]` in progress, `[ ]` not started, `[?]` waiting for a decision, `[!]` blocked. Finished history is in `Review/Progress Archive.md`; only `/cmd-notes-sync` writes it.
4. **Format**: each section body is one callout under its heading (headings stay: links and the script use them). Every body line starts with `> `; nested items use 4 spaces after it. Today items have no box: `> - **Title**: result`. Empty Today = heading only, no callout.
5. **Results, not steps**: one line per item, bold title, the result; no file lists or function chains. Details belong in the notes: link them (`[[Rooms and Art]]`), PR numbers as `(#15)`.
6. **Counts**: never by hand. After editing run `python3 tools/dev_status.py counts`; it rewrites every callout's type and `N items` / `N open`.
7. **Stale-note hint** (read-only, last step): from the items just written, name the notes that probably need an update (grep the vault for the feature, key, file or function) as `**[Notes to review]** <note>, <note>`. Do not edit them: that is `/cmd-notes-sync`.
8. **Notify** at the very end, only what changed, one line each with a blank line before: `**[Today's Progress]** Updated in Dev Status`.
