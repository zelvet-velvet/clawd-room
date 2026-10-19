# clawd-room

A pixel room where every running Claude Code session is a little Clawd (terminal UI; WSL2 + Windows Terminal and macOS + iTerm2). Read `Clawd Room Note/Start/LLM Guide.md` first (map, rules, repo regulation, `CMD_SYNC`), then `Clawd Room Note/Review/Dev Log.md` (the index of daily notes in `Review/Log/`) for what was built and what is pending.

Two people work on this repo: **Zaviel** (owner, GitHub `zelvet-velvet`) and **William Pan** (collaborator, GitHub `WilliamPanTW`). Find out whom you are working with from `git config user.name` (ask if unsure) and follow `CONTRIBUTING.md`:
- Work on a `type/slug` branch off `main` and open a pull request; never push to `main`, never merge a PR yourself (Zaviel merges).
- Visual changes start with a rendered mockup picture that Zaviel picks.
- Each PR: `make -C c test` passes, a Dev Log entry with the author's name, pictures for visual changes.
- Commit with the user's own git identity (set in the repo, not globally).
- Helping William: explain the code and the reasons behind the rules when asked; they are new to the project.
