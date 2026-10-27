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
> Since 2026-10-19 (William joined) every change is a branch and a pull request into `main`, merged by Zaviel ([[Collaboration]]). Before that `main` was kept equal to `feat/style-a`. The graph is made by `python3 tools/git_graph.py --write "Clawd Room Note/Review/Dev Log.md"`: the last 12 code branches (docs, chore and test ones left out; `--all` adds them), the number in a branch name is its pull request. Run it again after a merge.

```mermaid
%%{init: {"gitGraph": {"showCommitLabel": false}}}%%
gitGraph
    commit
    checkout main
    commit
    checkout main
    branch pr2-try-stop-reason
    checkout pr2-try-stop-reason
    commit
    checkout main
    merge pr2-try-stop-reason
    checkout main
    branch pr10-editor-animation
    checkout main
    branch pr11-fixture-names
    checkout pr10-editor-animation
    commit
    checkout pr11-fixture-names
    commit
    checkout pr11-fixture-names
    commit
    checkout pr10-editor-animation
    commit
    checkout pr10-editor-animation
    commit
    checkout main
    merge pr10-editor-animation
    checkout main
    merge pr11-fixture-names
    checkout main
    branch pr19-stage-page
    checkout pr19-stage-page
    commit
    checkout pr19-stage-page
    commit
    checkout pr19-stage-page
    commit
    checkout main
    merge pr19-stage-page
    checkout main
    branch pr21-stage-unselect
    checkout main
    branch pr22-gate-parts
    checkout main
    branch pr24-gate-themes
    checkout pr21-stage-unselect
    commit
    checkout pr22-gate-parts
    commit
    checkout pr22-gate-parts
    commit
    checkout pr22-gate-parts
    commit
    checkout main
    merge pr21-stage-unselect
    checkout pr24-gate-themes
    commit
    checkout main
    merge pr22-gate-parts
    checkout pr24-gate-themes
    commit
    checkout pr24-gate-themes
    commit
    checkout main
    merge pr24-gate-themes
    checkout main
    branch pr6-picked-clawd
    checkout pr6-picked-clawd
    commit
    checkout pr6-picked-clawd
    commit
    checkout pr6-picked-clawd
    commit
    checkout main
    merge pr6-picked-clawd
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
