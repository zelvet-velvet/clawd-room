# Platforms

WSL2 + Windows Terminal and macOS + iTerm2. Back to the [README](../README.md#platforms).

## macOS + iTerm2

On macOS the same works in **iTerm2** (the room must run inside iTerm2, not inside tmux): instead of PowerShell a small
`osascript` (JavaScript) reports the iTerm2 windows titled `clawd-room ...`, in points, about four times a second, and quits
with the room. **Split panes** count as windows of their own: two clawd-room panes side by side (or stacked) in one window
get a gate between them, like two attached windows. iTerm2 does not say where a pane or the text area is, so they are
worked out from the window size, the title and tab bar and iTerm2's margins (simple splits only: a row of side-by-side
panes or a column of stacked ones). `f` resizes the window there too. Started from a normal iTerm2 window,
`./clawd-room.sh` opens the room in **its own window with a "Clawd Room" profile** (Menlo 6, line spacing 1.1: 4x8 pt
cells, so a room pixel is always a square 4x4 pt) in the quarter of the screen that window was in and closes the window it was typed in (unless that has
other tabs or panes; `--keep` leaves it). After the first run, **Profiles → Clawd Room** (or Cmd+O) starts the room
directly, already a quarter of the screen; its window closes when the room quits. The profile is the file
`~/Library/Application Support/iTerm2/DynamicProfiles/clawd-room.json` (delete it to remove the profile). `--here` runs
the room in the current window instead (iTerm2 ignores the escape code that would switch the open window's profile).
When a room opens in iTerm2 its window becomes a quarter of the screen (`--size screen`, the default there), like macOS's
four-window tiling: half the usable width and height, snapped into the quarter its centre is in (at least 44x20 cells). `--size COLSxROWS` or `--size off`
as on WSL.

## WSL2 + Windows Terminal

### Font and window

Font size follows the window (WSL, Windows Terminal): when you stop resizing, a window with fewer
than 110 columns gets a smaller font and one with more than 170 a bigger one (it sends the
terminal's own Ctrl+minus / Ctrl+plus, and only to a window that has the focus). `--no-auto-font`
turns that off. `f` fits right now: in auto mode it adjusts the font; with a pinned shape it also
resizes the window to exactly that grid. It needs the default font-size shortcuts.

When a room opens on WSL (the start menu included) it sets the terminal font to 5 pt (`--font PT`, 0 = leave it; the
Clawd Room profile already has it, so a room pixel is about the size it is on macOS) and the window to the quarter of the
screen it is in, like iTerm2 (`--size screen`; `COLSxROWS` for a cell count, `off` = leave it). The room is then drawn at a TUI scale (1x, 2x, 3x:
every room pixel is scale x scale cells) that follows the window size and the number of sessions, and a window
resize only changes that scale, never the font. Ctrl +/- sets the terminal font by
hand; after that the automatic part stays off until `f` (font and scale automatic again, camera back to 1x). `--scale N` starts at a
fixed scale, `--no-auto` turns all of it off. The terminal font is put back when the room quits.
A window opened from the profile menu starts at Windows Terminal's own default size and is then moved to its quarter.

### The "Clawd Room" profile and the tab bar

Hiding the tab bar and the scroll bar: a program cannot do that from inside Windows Terminal
(`python3 wt_settings.py` prints the settings that do: `alwaysShowTabs`/`showTabsInTitlebar` off and
`scrollbarState` hidden; it changes nothing).
`python3 setup_profile.py` prints a "Clawd Room" profile (scroll bar hidden, no padding, pixel-Clawd-in-a-room tab icon); `--apply` writes it (and the generated `clawd-room.ico`) as a
Windows Terminal fragment file (`settings.json` is not touched), `--remove` deletes it. Restart Windows Terminal,
then open that profile (a start menu with a picture of each scene asks which to open: office, coffee shop or park, S = solo; passing `--room` or `--theme` skips it) or start a window with the `wt.exe -w new -p "Clawd Room" ...` line it prints.
The tab bar is a global setting, so it needs the two lines from `wt_settings.py`, or
`python3 setup_profile.py --apply-global`, which adds them (and the 5 pt font for the Clawd Room profile; a fragment's own font loses against the `profiles.defaults` font) to Windows Terminal's `settings.json` after copying it to
`settings.json.clawd-room.bak` (copy that back to undo). Run it only if you want that.
