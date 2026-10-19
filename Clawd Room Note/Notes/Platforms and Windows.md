---
title: Platforms and Windows
tags:
  - clawd-room
  - platforms
---
# 🪟 Platforms and Windows

> [!abstract] In short
> The room runs in **Windows Terminal (WSL)** and in **iTerm2 (macOS)**. A small helper the room starts (PowerShell or osascript) tells it where the clawd-room windows are; linked rooms talk through files. Nothing goes over the network.

---
## 🧩 How linked windows find each other

```mermaid
sequenceDiagram
    autonumber
    participant A as 🏢 room alpha
    participant H as 🔭 window helper<br/>(PowerShell / osascript)
    participant T as 🖥️ terminal<br/>(Windows Terminal / iTerm2)
    participant L as 📁 ~/.clawd-room/link
    participant B as ☕ room beta
    A->>T: window title "clawd-room alpha-…"
    B->>T: window title "clawd-room beta-…"
    loop 4 times a second
        H->>T: where are the clawd-room windows?
        T-->>H: rectangles (and iTerm2 split panes)
        H-->>A: list of windows
    end
    A->>L: heartbeat (name, theme, grid)
    B->>L: heartbeat
    Note over A,B: next to each other and still for 1 s → a gate in each room
    A->>L: walk message (a Clawd goes through)
    L-->>B: the Clawd comes out of beta's gate
```

> [!bug] Fixed: gates came and went (2026-10-18)
> Each room's helper walks every iTerm2 window; with several rooms open a list could take over 3 s, the room then "lost" its own window, dropped its gates and waited again for still windows. The last list is now kept for 10 s, and a neighbour's heartbeat without its grid no longer counts as a move. `CLAWD_SIZE_LOG` notes why a gate is shown or not (`gates seen= still= closed= shown=`).

> [!warning] Hands off the user's windows
> The helpers only look at windows titled `clawd-room …`, never move windows to attach them, and never type into a window that does not have the focus.

---
## 🧱 Both platforms

- The room is C11 with no libraries; `clawd-room.sh` builds it when needed and starts it.
- The hook (`hook.py`) writes one line per Claude Code event to `~/.clawd-room/events.jsonl`: only time, session id, event, folder name, tool. Install with `python3 install_hooks.py` (shows the change), then `--apply`.
- **Terminal hygiene:** alternate screen, hidden cursor, one write per frame, only changed cells.

> [!bug] Fixed: the view stayed scrolled up after a resize (2026-10-17)
> iTerm2 copies a cleared screen into its scrollback and pushes lines off the top when a window gets shorter. The room now **never sends clear-screen** (`ESC[2J`) and ends every frame with the cursor at the top-left.

- Minimum terminal size 44x20; below that it says "need 44x20".

---
## 🪟 WSL + Windows Terminal

> [!warning] Behind macOS (2026-10-19)
> Since 2026-10-17 everything was built on macOS: style A rooms, the engine and pixel editor, the park plaza, moving Clawds (`m`), the bottom bar, the stable split. None of it has been run on WSL yet; that is the next session (the checklist is in the [[Dev Log]], ⏳ Pending). `--try` is iTerm2 only. The `g` / `m` hint learns the key repeat rate from the repeats it gets (macOS settings are read with `defaults`, which WSL does not have).

- A small `powershell.exe` started by the room reports the windows (rectangles, DPI); it stops with the room.
- On open the room sets the window to **110x32 cells** and the font to **8 pt** (`--size`, `--font`; `off` leaves them). `setup_profile.py` makes the "Clawd Room" profile; `--apply-global` edits `settings.json` with a backup.
- `--try` (`tools/try_wsl.py`): four Clawd Room windows open minimized, are placed on the quarters (visible frames edge to edge, as a snap layout would) and shown together once each room has started; `--desktop N` puts them on virtual desktop N without switching (Explorer's undocumented virtual desktop interfaces). On your own desktop they then become a **snap group** (one taskbar entry; Windows has no API for groups): the first room is snapped into the top-left quarter with Win + ← + ↑ (keys only while it has the focus), the other three are picked in **Snap Assist** by their titles (UI Automation, Explorer's list only); a room it does not show in 3 s is snapped with its own keys. `CLAWD_TRY_SNAP=0` skips it.
- Font keys are sent only to a window that already has the focus. Focus mode is not used (it dropped the window in tests).

> [!todo] Not yet
> Windows Terminal split panes are not seen separately (UI Automation exposes each pane's rectangle: an idea to try).

---
## 🍎 macOS + iTerm2

- Used when `TERM_PROGRAM=iTerm.app`, not inside tmux. The helper is `osascript` (JavaScript, `c/mac_scripts.h`): it reports iTerm2 windows **and split panes** in points, and quits when the room is gone.
- iTerm2 does not say where a pane or the text area is, so they are worked out (title bar, tab bar, margins).
- **New windows open as a quarter of the screen** (like macOS's four-window tiling), snapped into the quarter their centre is in, so two rooms land next to each other.
- **Starting:** **Profiles → Clawd Room** (or Cmd+O) starts the room directly, in a window that is **already a quarter of the screen** (the launcher writes the quarter's grid, e.g. 181x53 on a 13" MacBook Air, into the profile), so nothing resizes; its window closes when the room quits. Typing `./clawd-room.sh` in a normal window **hands over**: the room's window opens in the quarter that window was in and the typed-in window closes (unless it has other tabs or panes; `--keep` leaves it).
- **A fixed pixel size:** the room runs in **its own window with the "Clawd Room" profile**: Menlo 6 at line spacing 1.1, so every cell is **4x8 pt** and a room pixel a square **4x4 pt** (8x8 screen pixels on Retina). The pixel art lines up the same on every run. `--here` keeps the current window.
- The profile is a separate file, `~/Library/Application Support/iTerm2/DynamicProfiles/clawd-room.json`, written by `clawd-room.sh` (delete it to remove the profile); the user's own profiles are not touched.
- **`--try`:** four linked Clawd Room windows in the four quarters (office, coffee shop / park, office) with made-up sessions (`tools/demo_events.py`), their own link folder (`~/.clawd-room/try/link`); quitting one closes all (`tools/try_room.sh` watches a shared stop file); `--try close`, `--try real`.
- The profile **ignores Cmd = + − 0** (iTerm2's font size keys), so the pixel size cannot drift. iTerm2 reloads the file by itself, so open room windows pick up a change at once.

> [!info] Why a new window (tested 2026-10-18, iTerm2 3.5.14)
> iTerm2 **ignores** the escape code that switches an open window's profile (`OSC 1337;SetProfile=…`; other `1337` codes work), and AppleScript cannot change a session's font or profile. Opening a window **with** a profile works.
>
> | Profile font | Line spacing | Cell (pt) |
> | --- | --- | --- |
> | Menlo 6 | 1.1 | **4 x 8** ✅ |
> | Menlo 8 | 1.0 | 5 x 10 |
> | Menlo 6 | 1.0 | 4 x 7 |
> | FiraCode 6 | 1.0 | 4 x 8 |

> [!tip] Bigger or smaller room pixels with Cmd +/−
> iTerm2 resizes the window on Cmd +/− because of its setting `AdjustWindowForFontSizeChange` (on by default). To change the font inside the same window instead: quit iTerm2, then in Terminal.app
> `defaults write com.googlecode.iterm2 AdjustWindowForFontSizeChange -bool false`

> [!warning] Two iTerm2 quirks
> - **Resize stripes:** iTerm2 re-wraps the screen when a window gets narrower, so during a drag the last frame shows as stripes until the next frame. It cannot be switched off; the room repaints at once.
> - **Native full screen:** with an iTerm2 window in native full screen, new windows open full-screen size and placing them hangs; a room in such a window can block writing. Leave full screen when testing linked windows.

![[2026-10-17 iTerm2 resize stripes (Zaviel).png|320]]
