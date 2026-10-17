# clawd-room

A pixel room with one little character per running Claude Code session.

- `hook.py` — Claude Code hook. Appends `{time, session id, event, folder name, tool name}` to
  `~/.clawd-room/events.jsonl` (mode 600). Never stores prompts, tool input/output or full paths.
- `room.py` — the window (pygame-ce, via WSLg). Replays the last 10 min, then follows the file.
- `c/` — the terminal version, written in C (standard C11, no libraries, no window needed). `clawd-room.sh` builds and starts it. Same states and timings as the window version.
- `install_hooks.py` — prints / applies / removes the hook entries in `~/.claude/settings.json`.
- `simulate.py` — fake sessions to watch the animations. `run.sh` — start the window.

States: thinking (dots), working (typing, monitor shows the tool), needs OK (sign), done (cheer),
idle 90 s (asleep), session end (walks out). A session with no events for 30 min is dropped.

    ./run.sh                                  # open the window
    .venv/bin/python simulate.py              # in another terminal: fake activity
    python3 install_hooks.py                  # show the settings change
    python3 install_hooks.py --apply          # install (backs up settings.json first)
    python3 install_hooks.py --remove         # uninstall

## Terminal version

    ./clawd-room.sh                           # draws in the terminal (works in a 60x20 pane or fullscreen); builds c/ on the first start
    ./clawd-room.sh --dump 100x30 --events /tmp/e.jsonl --at 1800000010   # one plain-text frame, for tests

Keys: `q` or Esc quit, `p` pause, `?` help line, `r` room size list, `[` `]` camera zoom, `z` auto zoom, Tab follow the next Clawd, `c` auto follow, arrows or W A S D move the camera freely, `g` send a Clawd next door, `f` fit. Needs at least 44x20 cells. The hook must be
installed once (`python3 install_hooks.py` shows what it would change) or the room stays empty.
Needs a truecolor terminal (Windows Terminal is fine). It only reads the events file.

In the terminal version the Clawds move around: busy ones sit at their desk (typing, thinking,
waving for OK, cheering, sleeping), idle ones walk around the room and sometimes visit another idle
Clawd to chat, wave (hearts), hop, high-five, dance or play tag; they also watch a busy friend work,
applaud when a friend finishes, and do little solo acts (stretch, dance, jump, wave). New sessions walk in through the left side, ended ones walk out.

## Linking windows and room themes

    ./clawd-room.sh --room park               # window 1
    ./clawd-room.sh --room coffee             # window 2

Rooms on the same machine find each other and exchange small messages through files in
`~/.clawd-room/link/` (private folder, no network, no daemon). The banner shows `LINK COFFEE`,
with an arrow (`>` or `<`) and a glowing door on that side of the room when the other window is
close enough on the Windows desktop. `clawd-room --link-demo NAME [SECONDS]` is a tiny demo that pings other rooms.
Messages carry only a type and small values, never prompts or tool data.

How "close enough" works: each room sets its window title to `clawd-room <id>`, and one small
`powershell.exe` (started by the room, stopped when it quits) reports the screen rectangles of
windows whose title starts with `clawd-room ` and nothing else. Windows side by side with a gap of
at most 30 px (`--near PX`; an attached pair stays attached until the gap passes 45 px) count as close. It needs the room's tab to be the active tab of its
window; `--no-detect` turns it off. `clawd-room --list-windows` lists the clawd-room windows it can see.

On macOS the same works in **iTerm2** (the room must run inside iTerm2, not inside tmux): instead of PowerShell a small
`osascript` (JavaScript) reports the iTerm2 windows titled `clawd-room ...`, in points, about four times a second, and quits
with the room. **Split panes** count as windows of their own: two clawd-room panes side by side (or stacked) in one window
get a gate between them, like two attached windows. iTerm2 does not say where a pane or the text area is, so they are
worked out from the window size, the title and tab bar and iTerm2's margins (simple splits only: a row of side-by-side
panes or a column of stacked ones). `f` resizes the window there too. The room leaves the iTerm2 font alone unless you
pass `--font PT` (that sends Cmd -/+ to the room's own window when it has the focus, and needs Accessibility for iTerm2).

Themes (`--theme office|coffee|park`, by default guessed from the room name):
- office: desks and monitors.
- coffee: the oldest session is the barista (apron, espresso machine behind a counter), the others
  are customers at cafe tables; idle customers go and order a coffee and carry it back.
- park: picnic tables with laptops; idle Clawds play on the swing and the slide.

Room size: the room always fills the window and takes its shape; dragging the window resizes it live. It grows (more
floor, more desks) up to the chosen room size, then the pixels get bigger instead (grow, then scale). `r` opens the ROOM SIZE
list (up/down or W/S, Enter picks, Esc or `r` closes; `--room-size auto|s|m|l|xl`):
- AUTO (default): just big enough to seat every session with floor to walk on; it changes when sessions come and go.
- S 120x60, M 160x90, L 220x120, XL 300x160 room pixels: the biggest the room may be (an 80x25 window is 80x50 room pixels).
(`--ratio 3:1|2:1|16:9|4:3|1:1|3:4` still pins an old fixed shape; it is kept for the tests.)

Camera: `[` and `]` zoom a camera into the room (1x to 4x; `--zoom N` starts zoomed); the room keeps its layout, only the
view gets closer. When zoomed the camera follows a Clawd and only moves when that Clawd nears the edge of the view. Auto
follow (default) picks the Clawd that needs you, else the one that just finished, else the most recently active one, and
takes turns every 10 s when nothing happens. Tab locks on the next Clawd, the arrows or W A S D move the camera freely, `c`
goes back to auto follow. `z` turns auto zoom on or off: the zoom then frames the busy Clawds (close on one, wider for several).

Font size follows the window (WSL, Windows Terminal): when you stop resizing, a window with fewer
than 110 columns gets a smaller font and one with more than 170 a bigger one (it sends the
terminal's own Ctrl+minus / Ctrl+plus, and only to a window that has the focus). `--no-auto-font`
turns that off. `f` fits right now: in auto mode it adjusts the font; with a pinned shape it also
resizes the window to exactly that grid. It needs the default font-size shortcuts.

Crossing: when two windows are close enough (or you pass `--left/--right/--up/--down NAME` to say
who is next to whom), an idle Clawd sometimes walks out through the door and into the next window
(a `walk` message with its folder name and height only). There it visits for 25-45 s (chats,
plays, orders a coffee, uses the swing), shown with a blue name tag, then walks back. Its desk
shows AWAY meanwhile; if its session gets busy or ends, the other window is asked to send it home.

Windows above or below: a window stacked over another gets a knotted rope hanging from the top
(Clawds climb it to the room above) and the one below gets stairs in the floor. A door on the
side shows the floor and wall of the room next door. Press `g` to send an idle Clawd next door now.

Sessions are shared between linked rooms (each session lives in one room, picked by a hash of its
id), so two windows do not show the same Clawds. `--no-split` shows every session in every room.
The rooms read the same `~/.clawd-room/events.jsonl`: real Claude Code sessions (hook installed)
and `simulate.py` fake ones both show up.

Windows are never moved to attach them: you put them where you like. The gates line up on
screen instead:
- Windows above/below get a ladder hatch (in the floor of the upper window, with a rope ladder
  hanging from a ceiling hatch in the lower one) at the same screen x.
- Windows side by side get a doorframe at the same screen height.
The two rooms swap where their cells are on screen (cell size, offset, scale, walking strip) in the
small presence file and search for the position that both of their grids can hit most closely, so
the two ends match to within a few pixels even with different fonts.
Different font sizes: a room pixel is a different size on screen in each window, so a gate drawn
the same number of pixels would be small in one and big in the other. Both windows therefore draw
the gate at the size it needs in the window with the bigger cells (the window with the smaller
cells draws it 1.5x, 2x ... as many pixels), so both ends are the same size on screen to within
a few percent. Only the Clawds themselves keep their window's scale.
Gates wait for the windows to be still: while a window is being moved or resized there is no gate,
and it appears (at its new, aligned place) once nothing has moved for a second.
A gate only opens when it fits: a door needs the two floors to share enough height for a Clawd
(14 room pixels), a hatch needs the two windows to overlap sideways by at least a hatch width
(21 room pixels). Otherwise there is no gate, no arrow in the banner, and nobody crosses.
The terminal's own cell size (asked with CSI 14t/16t) is used to turn screen pixels into cells.
Names: a Clawd is labelled with the folder name the Claude Code session was started in (the
`cwd` basename the hook records, nothing else), cut to fit its slot. The room for this repo shows
as CLAWD-R... for `clawd-room`, a session started in a folder called `LLM` shows as LLM.

Hiding the tab bar and the scroll bar: a program cannot do that from inside Windows Terminal
(`python3 wt_settings.py` prints the settings that do: `alwaysShowTabs`/`showTabsInTitlebar` off and
`scrollbarState` hidden; it changes nothing).
`python3 setup_profile.py` prints a "Clawd Room" profile (scroll bar hidden, no padding, pixel-Clawd-in-a-room tab icon); `--apply` writes it (and the generated `clawd-room.ico`) as a
Windows Terminal fragment file (`settings.json` is not touched), `--remove` deletes it. Restart Windows Terminal,
then open that profile (a start menu with a picture of each scene asks which to open: office, coffee shop or park, S = solo; passing `--room` or `--theme` skips it) or start a window with the `wt.exe -w new -p "Clawd Room" ...` line it prints.
The tab bar is a global setting, so it needs the two lines from `wt_settings.py`, or
`python3 setup_profile.py --apply-global`, which adds them (and the 8 pt font for the Clawd Room profile; a fragment's own font loses against the `profiles.defaults` font) to Windows Terminal's `settings.json` after copying it to
`settings.json.clawd-room.bak` (copy that back to undo). Run it only if you want that.

When a room opens on WSL (the start menu included) it sets the terminal font to 8 pt (`--font PT`, 0 = leave it) and
the window to 110x32 cells (`--size COLSxROWS`, `off` = leave it; sized once in pixels together with the font change). The room is then drawn at a TUI scale (1x, 2x, 3x:
every room pixel is scale x scale cells) that follows the window size and the number of sessions, and a window
resize only changes that scale, never the font. Ctrl +/- sets the terminal font by
hand; after that the automatic part stays off until `f` (font and scale automatic again, camera back to 1x). `--scale N` starts at a
fixed scale, `--no-auto` turns all of it off. The terminal font is put back when the room quits.
A window opened from the profile menu starts at Windows Terminal's own default size and is resized once to the
size above; to open it already at that size use `wt.exe -w new --size 60,18 -p "Clawd Room"` (wt counts cells at the default 14 pt font, so 60,18 gives about 110x33 at 8 pt).

## Building and testing

`c/` is plain C11 with no libraries. `clawd-room.sh` builds it by itself (`make -C c`) when the program is missing or older than its sources, so the profile and shortcuts only ever call the script (after pulling a new version run `python3 setup_profile.py --update`). Needs `make` and a C compiler (`sudo apt install build-essential`).

- `make -C c test` compares the program with the snapshots in `c/tests/expected` (frames, terminal output, random numbers, scenes, geometry) — about a second, no terminal needed. `tools/test.sh --update` rewrites the snapshots when a change of behaviour is meant.
- `make -C c smoke` / `linksmoke` / `winsmoke` drive the real program in ptys and in real Windows Terminal windows (your own throwaway `alpha` / `beta` ones).
- The Python version of the room (`room_tui.py` and friends) was removed on 2026-10-16 after the two had been compared frame by frame; it is in git history and `clawd-room-archive`. Python is still used for the hook, the installer, the simulator and the profile tools.

## Editing the Clawd frames and items

Every Clawd frame and the items it carries (speech bubbles, cup, sign) are text grids in `sprites/clawd.txt` and `sprites/items.txt`, one character per pixel. Edit them, then `clawd-room --build-art` redraws the sheet and one GIF per action in `Clawd Room Note/Animations/`; the room uses the same files when it starts. A frame missing from the files is drawn by the code. `clawd-room --export-sprites --force` rewrites the files from the code (`--add` only appends frames a file lacks).

## Notes vault

`Clawd Room Note/` is the Obsidian vault for the project notes (open that folder as a vault). It reuses
the CSS snippets, the Minimal theme and the community plugins of another vault; the plugin code, the
workspace state and Make.md's cache are not in git (see `.gitignore`), copy the plugins from that vault
or install them from Obsidian's community plugins list (`.obsidian/community-plugins.json` names them).
Start with `Docs/LLM_GUIDE.md`.
