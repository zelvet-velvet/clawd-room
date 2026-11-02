---
name: room-journal
description: clawd-room command CMD_JOURNAL [YYYY-MM-DD]. Use when the user types CMD_JOURNAL in the clawd-room repo, or a pull request needs its Dev Log entry: writes the day note in "Clawd Room Note/Review/Log/" from Dev Status.
---
# CMD_JOURNAL [YYYY-MM-DD] (clawd-room)

Default day: today. Source: `Today's Progress` in `Clawd Room Note/Review/Dev Status.md` (or `Yesterday's` when its date matches). Read only the top of Dev Status (up to the first `---`).

- **Day note**: `Clawd Room Note/Review/Log/YYYY/MM/Www/YYYY-MM-DD.md` (the day's own year and month, its ISO week, e.g. `2026/11/W45/2026-11-02.md`). Missing: Obsidian's *Open today's daily note* makes it from `_Meta/Templates/Day note.md`; outside Obsidian copy the frontmatter and the intro line of the last day note (title, date, tags, `up: "[[Dev Log]]"`, sticker), make the folders, and change the dates.
- **Entries**: one `## ` entry per change, newest first, right under the intro's `---`: an emoji title with the author in brackets (`## 🛝 A seesaw in the park (Zaviel)`), then two to four plain bullets of what changed **for someone using the room**, the branch if not merged, a link to the design note. Superseded attempts get one line.
- **Pictures** (visual changes): embed from `Assets/Snapshots/<subject>/`; one on a line `|774`, two on a line `|380` each, never wider than the picture itself. Filing new pictures is `CMD_NOTES_SYNC`.
- **Never rewrite** the user's own lines or another author's entry. Two branches adding to the same day: keep both entries.
- **Notify**: `**[Journal]** 2 entries in 2026-11-02`.
