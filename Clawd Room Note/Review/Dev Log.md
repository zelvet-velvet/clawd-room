---
title: clawd-room - Dev Log
date: 2026-10-16
updated: 2026-10-19
tags:
  - dev-log
  - clawd-room
sticker: lucide//book-open
---
What changed and when, in short: **one note per day** in `Log/` (newest first), listed below with their entries. Where things stand now (today, ongoing, ideas, done): [[Dev Status]]. How things work now is in the design notes:

| Note | About |
| --- | --- |
| [[Rooms and Art]] | the look: perspective rooms, workspaces, gates, signs, art rules, layout numbers |
| [[Interaction]] | keys, camera, room size, linked windows, crossing, sessions |
| [[Platforms and Windows]] | WSL + Windows Terminal, macOS + iTerm2, window helpers |
| [[Engine and Tests]] | the code, the tests, random numbers |
| [[Test Checklist]] | what is still to check by eye |
| [[Animations]] | every Clawd frame |

Pictures are in `Assets/Snapshots/`, one folder per subject; browse them (and the animations) from [[Index|Gallery index]]. Where everything is: [[Home]]. Details of every change: `git log`.

> [!info] Where the work is (branches)
> Since 2026-10-19 (William joined) every change is a branch and a pull request into `main`, merged by Zaviel ([[Collaboration]]). Before that `main` was kept equal to `feat/style-a`.

```mermaid
gitGraph
    commit id: "C room"
    commit id: "fixes"
    branch macos-iterm
    commit id: "macOS build"
    commit id: "iTerm2 helper"
    commit id: "camera, room size"
    checkout main
    merge macos-iterm id: "merged 10-17"
    branch style-a
    commit id: "perspective rooms"
    commit id: "engine, editor, --try"
    commit id: "park plaza, moves"
    checkout main
    merge style-a id: "main = style-a (10-18, 10-19)"
```

---

---
## 📅 Days

One note per day in `Review/Log/<year>/<month>/<ISO week>/` (e.g. `2026/10/W43/2026-10-19`). **Open today's daily note** (or the Calendar) makes today's in the right folder from [[Day note]]. In Obsidian the list below is built from those notes (newest first, with each day's entries).

```dataviewjs
const days = dv.pages('"Review/Log"').where(p => /^\d{4}-\d{2}-\d{2}$/.test(p.file.name)).sort(p => p.file.name, "desc");
let year = "", month = "", week = "";
for (const p of days) {
  const d = moment(p.file.name, "YYYY-MM-DD");
  if (d.format("YYYY") !== year) { year = d.format("YYYY"); month = ""; dv.header(3, year); }
  if (d.format("MM") !== month) { month = d.format("MM"); week = ""; dv.header(4, d.format("MMMM")); }
  if (d.format("[W]WW") !== week) { week = d.format("[W]WW"); dv.paragraph("**" + week + "**"); }
  const file = app.vault.getAbstractFileByPath(p.file.path);
  const heads = (app.metadataCache.getFileCache(file)?.headings || []).filter(h => h.level === 2);
  dv.paragraph(p.file.link + " · " + d.format("dddd"));
  dv.list(heads.map(h => dv.sectionLink(p.file.path, h.heading, false, h.heading)));
}
```

---
