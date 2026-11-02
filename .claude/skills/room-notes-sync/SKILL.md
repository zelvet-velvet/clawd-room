---
name: room-notes-sync
description: clawd-room command CMD_NOTES_SYNC [YYYY-MM-DD]. Use when the user types CMD_NOTES_SYNC in the clawd-room repo: brings the design notes, pictures, test queue and docs up to date with the code, then moves finished Ongoing items of Dev Status to the Progress Archive.
---
# CMD_NOTES_SYNC [YYYY-MM-DD] (clawd-room)

Vault: `Clawd Room Note/`. No date: use `Today's Progress` of `Review/Dev Status.md`; with a date: `Yesterday's` when its date matches. Grep first; read only the matching sections of large notes. Note format (callouts, pictures, stickers): `Start/LLM Guide.md`, section Conventions.

1. **Design notes** (`Design/`: Rooms and Art, Interaction, Pixel Editor, Platforms and Windows, Engine and Tests, Engine Design, Animations, Art Data and Editor): describe how things work **now**; rewrite, do not append history. Check against the code (`git log` / `git diff` of the day); fix only what is provably stale: status words, constants in their tables, keys, file and function names. Diagrams: mermaid (`%%{init: {"flowchart": {"defaultRenderer": "elk"}}}%%` for flowcharts). A design **picked but not built yet** gets a folded `> [!todo]- Picked, not built yet` callout at the end of its section (what, the date, the picked mockup, a link to the day note); once built, rewrite the section and drop it from the callout.
2. **Pictures**: screenshots (the user's, renders, and every mockup shown to Zaviel, the picked one marked `(picked)`) go to `Assets/Snapshots/<subject>/YYYY-MM-DD subject.png` (folders: `Office/`, `Coffee shop/`, `Park/`, `Gates (stairs, doors, wells)/`, `Interaction (keys, moving)/`, `Engine and tools/`, `Platforms/`, `Style and mockups/`, `Old look/`; a new one only if none fits; a person's own: `... (Zaviel).png`, scaled to 1400 px; names unique). Embed them in the day note entry, give each a row in `_Meta/Snapshot status.md` (current / replaced by / not picked), then `python3 tools/vault_gallery.py` (it rewrites `Gallery/`; never edit `Gallery/` by hand). From a mockup only the picked one and the side-by-side picture go in (skill `room-mockups`).
3. **Test queue**: what still needs someone's eyes: skill `room-test-queue`.
4. `README.md` / `docs/` if commands, flags or keys changed; `Start/LLM Guide.md` or a skill if a rule changed.
5. **Finish Ongoing**: read Dev Status again. Move `[x]` items from `Ongoing` to the top of their area in `Review/Progress Archive.md` (one line, result only, no `> ` or box). Leave `[>]`, `[ ]`, `[?]`. An `[x]` whose note was not updated stays and is named in the report. Then `python3 tools/dev_status.py counts`.
6. **Do not touch**: the user's own lines, day notes of other days, boxes a person ticked. A decision the user has not made: ask.
7. **Report**, one line per changed note: `**[Notes]** Rooms and Art: the park's way up`; then `**[Last Progress]** Moved N items to Progress Archive`; the notes checked and found current; anything left open.

**Trust order** when sources disagree: the code, then today's chat and Dev Status, then the design notes, then other notes. A note contradicting the code: say so, never repeat the stale fact.
