---
name: room-git
description: Git and pull-request rules of the clawd-room repo (zelvet-velvet/clawd-room): branch names, commit messages, the PR description with the Clawd logo, who merges. Use before creating a branch, committing, pushing, opening a pull request or merging in clawd-room.
---
# Git and pull requests (clawd-room)

**Zaviel** (owner, `zelvet-velvet`) merges; everyone else is a contributor. Human version: `CONTRIBUTING.md`. Old history (pygame prototype): `zelvet-velvet/clawd-room-archive`, read-only.

- **Branches**: `main` is protected and only receives pull requests. Work on a branch off a fresh `main` (`git switch main && git pull --ff-only && git switch -c type/short-slug`), one topic per branch: `feat/seesaw`, `fix/gate-offset`, `docs/…` for vault-only changes.
- **Commits**: `type(scope): summary`, lowercase, imperative, no period. Types `feat`, `fix`, `refactor`, `chore`, `docs`; scope the area (`room`, `link`, `gates`, `themes`, `hook`, `vault`, `docs`…). End with the co-author line from the session's attribution rules. The user's own identity, set in the repo (check `git config user.name`).
- **Stage by name**: `git add -- "<file>"` for each file; never `git add <folder>`, `-A` or `.` (untracked notes were swept into commits that way). Never commit the user's `.obsidian` changes, `gallery/`, mock binaries or `fix_*` scratch files unless asked.
- **Every change = commit + push** on the current feature branch, without asking. Never force-push a shared branch or `main`; never rewrite published history.
- **Pull request** when a feature is done (tests pass, Dev Log entry written): first `git fetch && git merge origin/main`, test again, then `gh pr create`. The template asks for what changed, pictures (visual changes), what was tested and what still needs someone's eyes. The description ends with the attribution line with a **Clawd instead of the 🤖**: `<img src="https://github.com/zelvet-velvet/clawd-room/blob/main/.github/clawd.png?raw=true" height="22" align="absmiddle"> Generated with [Claude Code](https://claude.com/claude-code)`.
- **Merging**: only Zaviel, on GitHub with "Create a merge commit". Exception (Zaviel, 2026-10-23): when Zaviel says in the conversation that they tested a PR and it is ready, merge it for them: check it is mergeable (resolve conflicts on its own branch first), `gh pr merge N --merge`, delete the branch. Never on your own judgement; never someone else's PR unless Zaviel says so.
- **After a merge**: `git switch main && git pull --ff-only && git branch -d <branch>`; for a code branch run `python3 tools/git_graph.py --write "Clawd Room Note/Review/Dev Log.md"` (on a docs branch).
