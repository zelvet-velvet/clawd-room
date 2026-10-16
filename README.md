# clawd-room

A pixel room with one little character per running Claude Code session.

- `hook.py` — Claude Code hook. Appends `{time, session id, event, folder name, tool name}` to
  `~/.clawd-room/events.jsonl` (mode 600). Never stores prompts, tool input/output or full paths.
- `room.py` — the window (pygame-ce, via WSLg). Replays the last 10 min, then follows the file.
- `room_tui.py` — the terminal version (standard library only, no window needed). Same states and timings.
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

    python3 room_tui.py                       # draws in the terminal (works in a 60x20 pane or fullscreen)
    python3 room_tui.py --dump 100x30 --events /tmp/e.jsonl --at 1800000010   # one plain-text frame, for tests

Keys: `q` or Esc quit, `p` pause, `?` help line. Needs at least 44x20 cells. The hook must be
installed once (`python3 install_hooks.py` shows what it would change) or the room stays empty.
Needs a truecolor terminal (Windows Terminal is fine). It only reads the events file.

In the terminal version the Clawds move around: busy ones sit at their desk (typing, thinking,
waving for OK, cheering, sleeping), idle ones walk around the room and sometimes visit another idle
Clawd to chat, wave (hearts), hop, high-five, dance or play tag; they also watch a busy friend work,
applaud when a friend finishes, and do little solo acts (stretch, dance, jump, wave). New sessions walk in through the left side, ended ones walk out.

## Linking windows and room themes

    python3 room_tui.py --room park           # window 1
    python3 room_tui.py --room coffee         # window 2

Rooms on the same machine find each other and exchange small messages through files in
`~/.clawd-room/link/` (private folder, no network, no daemon). The banner shows `LINK COFFEE`,
with an arrow (`>` or `<`) and a glowing door on that side of the room when the other window is
close enough on the Windows desktop. `python3 roomlink.py NAME` is a tiny demo that pings other rooms.
Messages carry only a type and small values, never prompts or tool data.

How "close enough" works: each room sets its window title to `clawd-room <id>`, and one small
`powershell.exe` (started by the room, stopped when it quits) reports the screen rectangles of
windows whose title starts with `clawd-room ` and nothing else. Windows side by side with a gap of
at most 30 px (`--near PX`; an attached pair stays attached until the gap passes 45 px) count as close. It needs the room's tab to be the active tab of its
window; `--no-detect` turns it off. `python3 windetect.py` lists the clawd-room windows it can see.

Themes (`--theme office|coffee|park`, by default guessed from the room name):
- office: desks and monitors.
- coffee: the oldest session is the barista (apron, espresso machine behind a counter), the others
  are customers at cafe tables; idle customers go and order a coffee and carry it back.
- park: picnic tables with laptops; idle Clawds play on the swing and the slide.

Room shape: by default (`--ratio auto`) the room is as big and as shaped as the window, so it
follows you when you resize the window (1:1, 4:3, 16:9, 3:1 ...). `--ratio 3:1|2:1|16:9|4:3|1:1|3:4` (or the `r` key) pins a standard room of
that shape instead (3:1 = 144x48 pixels = 144x24 cells, ...), drawn bigger when the window is bigger.

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
resize only changes that scale, never the font. `[` and `]` set the scale by hand, Ctrl +/- sets the terminal font by
hand; after either the automatic part stays off until `f` (font and scale automatic again). `--scale N` starts at a
fixed scale, `--no-auto` turns all of it off. The terminal font is put back when the room quits.
A window opened from the profile menu starts at Windows Terminal's own default size and is resized once to the
size above; to open it already at that size use `wt.exe -w new --size 60,18 -p "Clawd Room"` (wt counts cells at the default 14 pt font, so 60,18 gives about 110x33 at 8 pt).

## The C version

`c/` holds the same room written in C (standard C11, no libraries; 10x less CPU, 0.5 % vs 5 % of a core). `clawd-room.sh` starts it (`make -C c` builds it, the script does that by itself on first start) and falls back to `room_tui.py` if it cannot. Choose the program with `CLAWD_ENGINE=python` / `c`, or put `python` or `c` on the first line of `~/.clawd-room/engine`; delete that file for automatic. The Clawd Room profile and the `wt.exe` line from `setup_profile.py` call `clawd-room.sh`; after pulling a new version run `python3 setup_profile.py --update`. `room_tui.py` stays the reference: `make -C c test` checks the C version draws and behaves exactly like it, `make smoke`, `linksmoke` and `winsmoke` drive the real programs.

## Editing the Clawd frames and items

Every Clawd frame and the items it carries (speech bubbles, cup, sign) are text grids in `sprites/clawd.txt` and `sprites/items.txt`, one character per pixel. Edit them, then `python3 sprite_tool.py build` redraws the sheet and one GIF per action in `Clawd Room Note/Animations/`; the room uses the same files when it starts. A frame missing from the files is drawn by the code. `python3 sprite_tool.py export --force` rewrites the files from the code.

## Notes vault

`Clawd Room Note/` is the Obsidian vault for the project notes (open that folder as a vault). It reuses
the CSS snippets, the Minimal theme and the community plugins of another vault; the plugin code, the
workspace state and Make.md's cache are not in git (see `.gitignore`), copy the plugins from that vault
or install them from Obsidian's community plugins list (`.obsidian/community-plugins.json` names them).
Start with `Docs/LLM_GUIDE.md`.
