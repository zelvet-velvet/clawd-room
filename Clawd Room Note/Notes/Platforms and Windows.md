---
title: Platforms and Windows
tags:
  - clawd-room
  - platforms
---
# 🪟 Platforms and Windows

How the room runs on WSL + Windows Terminal and on macOS + iTerm2, and what the window helpers do.

---
## Both platforms

- The room is C11 with no libraries; `clawd-room.sh` builds it when needed and starts it.
- The hook (`hook.py`) writes one line per Claude Code event to `~/.clawd-room/events.jsonl`: only time, session id, event, folder name, tool. Install with `python3 install_hooks.py` (shows the change) and `--apply`.
- **Terminal hygiene:** alternate screen, hidden cursor, one write per frame, only changed cells. The room **never sends clear-screen** (`ESC[2J`) and ends every frame with the cursor at the top-left, because iTerm2 copies a cleared screen into scrollback and pushes lines off the top when the window gets shorter (it left the view scrolled up after a resize).
- Minimum terminal size 44x20; below that it says "need 44x20".

## WSL + Windows Terminal

- A small `powershell.exe` started by the room reports the windows titled `clawd-room …` (rectangles, DPI); it stops with the room.
- On open the room sets the window to 110x32 cells and the font to 8 pt (`--size`, `--font`, `off` leaves them); `setup_profile.py` makes the "Clawd Room" profile (`--apply-global` edits `settings.json` with a backup).
- Font keys are sent only to a window that already has the focus. Focus mode is not used (it dropped the window in tests).
- Windows Terminal panes are not seen separately yet (idea: UI Automation exposes each pane's rectangle).

## macOS + iTerm2

- Used when `TERM_PROGRAM=iTerm.app`, not inside tmux. The helper is `osascript` (JavaScript, `c/mac_scripts.h`): it reports the iTerm2 windows **and split panes** titled `clawd-room …` four times a second, in points, and quits when the room is gone.
- iTerm2 does not say where a pane or the text area is, so they are worked out (title bar, tab bar, margins); gates may sit a few points off.
- **New windows open as a quarter of the screen** (like macOS's four-window tiling), snapped into the quarter their centre is in, so two rooms land next to each other.
- The font is left alone unless `--font` is given (that sends Cmd −/+ to the room's own focused window and needs Accessibility).
- **Cmd +/− resizes the window** because of iTerm2's own setting `AdjustWindowForFontSizeChange` (on by default). With it off (quit iTerm2, `defaults write com.googlecode.iterm2 AdjustWindowForFontSizeChange -bool false`), Cmd +/− changes the room's pixel size inside the same window.
- iTerm2 does not answer the pixel-size questions (`CSI 16t`), so the "font changed" guess never fires on a Mac.

![[2026-10-17 iTerm2 resize stripes (Zaviel).png|360]]

- **Resize stripes:** iTerm2 re-wraps the screen when a window gets narrower, so during a drag the last frame shows as stripes until the next frame. The room repaints at once after a resize; the stripes cannot be switched off.
- **Native full screen:** with an iTerm2 window in native full screen, new windows open full-screen size and placing them by AppleScript hangs; a room in such a window can block writing (only SIGKILL stopped it). Leave full screen to test linked windows. (Open: make the room's writes unable to block.)
