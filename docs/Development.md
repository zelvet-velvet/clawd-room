# Development

For people working on the code. How we work together (branches, pull requests) is in [CONTRIBUTING.md](../CONTRIBUTING.md); the map and rules for AI assistants are in `Clawd Room Note/Start/LLM Guide.md`.

## What is where

- `hook.py`: Claude Code hook. Appends `{time, session id, event, folder name, tool name}` to `~/.clawd-room/events.jsonl` (mode 600). Never stores prompts, tool input/output or full paths.
- `c/`: the terminal room, written in C (standard C11, no libraries). `clawd-room.sh` builds and starts it.
- `install_hooks.py`: prints / applies / removes the hook entries in `~/.claude/settings.json`.
- `setup_profile.py`, `wt_settings.py`: the Windows Terminal profile and settings helpers.
- `simulate.py`: fake sessions to watch the animations.
- `room.py` + `run.sh`: the original pygame-ce window (needs `.venv`), kept for reference.
- `sprites/`, `rooms/`: the art and the wall plans, as text files.
- `Clawd Room Note/`: the Obsidian vault with the project notes.

## Building and testing

`c/` is plain C11 with no libraries. `clawd-room.sh` builds it by itself (`make -C c`) when the program is missing or older than its sources, so the profile and shortcuts only ever call the script (after pulling a new version run `python3 setup_profile.py --update`). Needs `make` and a C compiler (`sudo apt install build-essential`).

- `make -C c test` compares the program with the snapshots in `c/tests/expected` (frames, terminal output, random numbers, scenes, geometry) — about a second, no terminal needed. `tools/test.sh --update` rewrites the snapshots when a change of behaviour is meant.
- `make -C c smoke` / `linksmoke` / `winsmoke` drive the real program in ptys and in real Windows Terminal windows (your own throwaway `alpha` / `beta` ones).
- The Python version of the room (`room_tui.py` and friends) was removed on 2026-10-16 after the two had been compared frame by frame; it is in git history and `clawd-room-archive`. Python is still used for the hook, the installer, the simulator and the profile tools.

## Editing the Clawd frames and items

Every Clawd frame and the items it carries (speech bubbles, cup, sign) are text grids in `sprites/clawd.txt` and `sprites/items.txt`, one character per pixel. Edit them, then `clawd-room --build-art` redraws the sheet and one GIF per action in `Clawd Room Note/Assets/Animations/`; the room uses the same files when it starts. A frame missing from the files is drawn by the code. `clawd-room --export-sprites --force` rewrites the files from the code (`--add` only appends frames a file lacks).

## Notes vault

`Clawd Room Note/` is the Obsidian vault for the project notes (open that folder as a vault). It reuses
the CSS snippets, the Minimal theme and the community plugins of another vault; the plugin code, the
workspace state and Make.md's cache are not in git (see `.gitignore`), copy the plugins from that vault
or install them from Obsidian's community plugins list (`.obsidian/community-plugins.json` names them).
Start with `Start/Home.md`.
