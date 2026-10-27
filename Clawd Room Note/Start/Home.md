---
title: Home
tags:
  - clawd-room
sticker: lucide//house
---
**clawd-room** is a pixel room where every running Claude Code session is a little Clawd, in the terminal (WSL2 + Windows Terminal, macOS + iTerm2). This vault is its notebook: what was built and why, how it works, what still needs checking.

> [!tip] New here? Read in this order
> 1. `CONTRIBUTING.md` in the repo: setup and how we work (branches, pull requests, who merges).
> 2. [[LLM Guide]]: the map of the code and the rules (written for AI assistants, short and useful for people).
> 3. [[Task Queue]]: work you can pick up; [[Dev Status]]: where things stand (today, ongoing, ideas); [[Dev Log]]: what changed, day by day.
> 4. The design notes for the part you work on (below).

## 🗂️ What is where

| Folder | What | Written by |
| --- | --- | --- |
| `Start/` | this page, [[LLM Guide]], [[Collaboration]] | people |
| `Review/` | [[Task Queue]] (work anyone can claim), [[Dev Status]] (today, ongoing, ideas, done), [[Dev Log]] (the index of days), `Log/<year>/<month>/<week>/` (one note per day; *Open today's daily note* makes today's), [[Test Checklist]] (the test queue T1, T2 …) | people |
| `Design/` | how things work **now**: [[Rooms and Art]], [[Interaction]], [[Platforms and Windows]], [[Engine and Tests]], [[Engine Design]], [[Animations]] | people |
| `Gallery/` | pictures and animations to browse: [[Index\|Gallery index]] | `tools/vault_gallery.py` (never by hand) |
| `Assets/` | the files only: `Snapshots/<subject>/` pictures, `Animations/` GIFs and sheets | people (pictures), `--build-art` (animations) |
| `_Meta/` | vault helpers: [[Checkbox Style]], [[Snapshot status]] | people |

> [!warning]- Claude Code says `UserPromptSubmit operation blocked by hook … hook.py: No such file`
> The repo was moved, renamed or re-cloned, and `~/.claude/settings.json` still points at the old `hook.py`. Fix it in a plain terminal: remove the hook entries whose command is `python3 <old path>/hook.py` (back the file up first), then run `python3 install_hooks.py --apply` from the current folder. Quick way back in: `ln -s <current folder> <old path>`. More in [[LLM Guide]] (rules). Before moving the folder next time, run `python3 install_hooks.py --remove` first.

## 🔍 Reviewing a change

- **What changed:** the day's note in `Review/Log/` (linked from the [[Dev Log]]): short entries, a picture for anything visual, the branch.
- **Does it work:** the [[Test Checklist]]: tests by ID (`T14`), ticked with a name and date.
- **How does it look:** the [[Index|Gallery index]]: every picture marked ✅ current, ♻️ replaced or ❌ not picked.
- **The details:** the pull request on GitHub and `git log`.

## ✍️ Writing here

- No `# Title` inside a note: the file name is the title. A lucide `sticker:` in the frontmatter.
- Design notes describe how things work now (rewrite, do not append history); history goes in the day notes.
- Never write a note named like its folder (make.md's folder notes), and never edit `Gallery/` by hand.
