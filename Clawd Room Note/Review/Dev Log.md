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
> Since 2026-10-19 (William joined) every change is a branch and a pull request into `main`, merged by Zaviel ([[Collaboration]]). Before that `main` was kept equal to `feat/style-a`. The graph is made by `python3 tools/git_graph.py --write "Clawd Room Note/Review/Dev Log.md"` (a summary of the first-parent line of `main`; the numbers are pull requests): run it again after a merge.

<!-- git-graph:start -->
_The last 24 of 30 entries on the line of `main`._

```mermaid
gitGraph
    commit id: "earlier history"
    branch wsl-queue
    commit id: "wsl up to date --try snap group, 5 pt profi…"
    checkout main
    merge wsl-queue id: "#6 (10-20)"
    branch vault-layout
    commit id: "reorganise the vault by purpose, one dev lo…"
    checkout main
    merge vault-layout id: "#7 (10-20)"
    branch mockups-2026-10-20
    commit id: "mockups of 2026-10-20 kept in the vault, ga…"
    checkout main
    merge mockups-2026-10-20 id: "#9 (10-20)"
    branch editor-animation
    commit id: "the editor edits animations timeline, play…"
    checkout main
    merge editor-animation id: "#10 (10-20)"
    branch fixture-names
    commit id: "made-up names in the test data nothing priv…"
    checkout main
    merge fixture-names id: "#11 (10-20)"
    branch pr-icon-eyes
    commit id: "the pr signature clawd's eyes filled dark"
    checkout main
    merge pr-icon-eyes id: "#12 (10-20)"
    branch stage-page
    commit id: "part animations the file, the swing driven…"
    checkout main
    merge stage-page id: "#19 (10-21)"
    branch vault-readme-plan
    commit id: "the vault sync, the README and docs, the pa…"
    checkout main
    merge vault-readme-plan id: "#20 (10-21)"
    branch stage-unselect
    commit id: "Esc picks nothing on the stage page"
    checkout main
    merge stage-unselect id: "#21 (10-22)"
    branch vault-sync-2026-10-21b
    commit id: "sync"
    checkout main
    merge vault-sync-2026-10-21b id: "#23 (10-22)"
    branch gate-parts
    commit id: "gates appearing are part animations, edited…"
    checkout main
    merge gate-parts id: "#22 (10-22)"
    branch gate-themes
    commit id: "gate themes (simple, construct) in anims/ga…"
    checkout main
    merge gate-themes id: "#24 (10-22)"
    branch readme-screenshots
    commit id: "redesigned with the latest progress and 11…"
    checkout main
    merge readme-screenshots id: "#25 (10-23)"
    branch vault-sync-2026-10-23
    commit id: "sync"
    checkout main
    merge vault-sync-2026-10-23 id: "#26 (10-23)"
    branch prepare-public
    commit id: "prepare for going public"
    checkout main
    merge prepare-public id: "#27 (10-23)"
    branch generic-contributing
    commit id: "contributing notes for anyone Claude may me…"
    checkout main
    merge generic-contributing id: "#28 (10-23)"
    branch status-before-rewrite
    commit id: "status before the history rewrite"
    checkout main
    merge status-before-rewrite id: "#29 (10-23)"
    branch new-repo-notes
    commit id: "the history rewrite and the new repo"
    checkout main
    merge new-repo-notes id: "#1 (10-24)"
    branch codeowners
    commit id: "CODEOWNERS and the public status"
    checkout main
    merge codeowners id: "#2 (10-24)"
    branch new-look-docs
    commit id: "linked rooms and usage describe the current…"
    checkout main
    merge new-look-docs id: "#3 (10-24)"
    branch task-queue
    commit id: "a task queue anyone can claim"
    checkout main
    merge task-queue id: "#4 (10-24)"
    branch task-queue-callouts
    commit id: "the task queue as callouts with checkboxes"
    checkout main
    merge task-queue-callouts id: "#5 (10-24)"
    branch picked-clawd
    commit id: "picked Clawd (yellow corners and a spotligh…"
    checkout main
    merge picked-clawd id: "#6 (10-24)"
    branch mock-workflow
    commit id: "mockups stay local, the picked option goes…"
    checkout main
    merge mock-workflow id: "#8 (10-27)"
```
<!-- git-graph:end -->

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
