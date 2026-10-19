#!/bin/sh
# Starts the room: builds the C program (make -C c) if it is missing or older than its sources, then runs it with the arguments.
# The Clawd Room profile and the shortcuts call this, so the program behind them can change without touching Windows Terminal's settings.
here=$(cd "$(dirname "$0")" && pwd -P)
bin="$here/c/clawd-room"
stale=0
[ -x "$bin" ] || stale=1
if [ "$stale" = 0 ]; then
  for f in "$here"/c/*.c "$here"/c/*.h "$here"/c/Makefile; do
    [ "$f" -nt "$bin" ] && { stale=1; break; }
  done
fi
if [ "$stale" = 1 ]; then
  if command -v make >/dev/null 2>&1 && command -v cc >/dev/null 2>&1; then
    echo "building clawd-room..." >&2
    make -C "$here/c" -s >/dev/null 2>&1 || echo "(the build failed; run 'make -C c' to see why)" >&2
  else
    case "$(uname -s)" in
      Darwin) echo "clawd-room: needs make and a C compiler to build (xcode-select --install)" >&2 ;;
      *) echo "clawd-room: needs make and a C compiler to build (sudo apt install build-essential)" >&2 ;;
    esac
  fi
fi
# iTerm2: the room runs in a window with the "Clawd Room" profile (Menlo 6 at 1.1 line spacing: 4x8 pt cells, so a room
# pixel is always a square 4x4 pt; Cmd = + - 0, iTerm2's font size keys, are ignored). The profile is a dynamic profile file
# written here; opening it (Profiles menu) starts the room, and the window closes when the room quits. iTerm2 ignores the
# escape code that would switch an open window's profile, so run from another window this opens the room's window in the
# quarter of the screen that window is in and closes the window it was typed in (when that has no other tabs or panes; --keep leaves it).
# --here (or CLAWD_HERE=1) runs the room in the current window; so does a run that only prints.
here_only=0 keep=0 editing=0
prev=""
for a in "$@"; do
  case "$a" in --here) here_only=1 ;; --keep) keep=1 ;; --dump*|--build-art*|-h|--help|--version) here_only=1 ;; --edit) keep=1 editing=1 ;; esac # (the editor: your window stays)
  [ "$prev" = --edit ] && [ "$a" = list ] && here_only=1 # (--edit list just prints)
  prev=$a
done
for a in "$@"; do shift; case "$a" in --here|--keep) ;; *) set -- "$@" "$a" ;; esac; done
# --try: four Clawd Room windows, one in each quarter of the screen (office, coffee shop / park, office), so left/right
# and up/down gates all show at once; made-up sessions in them (--try real: only your real sessions); quitting one
# closes all (--try close closes them from anywhere). They use their own link folder, apart from rooms you run.
# try_setup ends a --try still running and starts the made-up sessions; it sets $try and $ev (exits for --try close).
try_setup() {
  try="$HOME/.clawd-room/try"
  mkdir -p "$try"
  if [ "${2:-}" = close ]; then touch "$try/stop"; exit 0; fi
  # end a --try still running: wait until its rooms are gone (a room takes a moment to quit; one quitting after the new
  # rooms started would write the stop file again and close them)
  touch "$try/stop"; i=0
  while pgrep -f "try_room.sh .* $try/stop" >/dev/null 2>&1 && [ $i -lt 50 ]; do sleep 0.2; i=$((i+1)); done
  rm -f "$try/stop" "$try/events.jsonl"; rm -rf "$try/link"
  if [ "${2:-}" = real ]; then ev="$HOME/.clawd-room/events.jsonl"
  else
    ev="$try/events.jsonl"
    : > "$ev"
    # the made-up sessions start once all four rooms are up (or after 20 s): a session a room shows stays there, so
    # sessions started before the others open would all stay in the first room
    nohup sh -c 'i=0; while [ "$(ls "$1"/*.peer 2>/dev/null | wc -l)" -lt 4 ] && [ $i -lt 80 ]; do sleep 0.25; i=$((i+1)); done
                 exec python3 "$2" "$3" "$4" 7' sh "$try/link" "$here/tools/demo_events.py" "$ev" "$try/stop" >/dev/null 2>&1 &
  fi
}
# WSL + Windows Terminal: --try opens the four windows with wt.exe (tools/try_wsl.py; --try --desktop N: on virtual desktop N,
# without switching to it)
if [ "${1:-}" = --try ] && [ -n "${WSL_DISTRO_NAME:-}" ] && command -v wt.exe >/dev/null 2>&1; then
  desk=""
  for a in "$@"; do [ "$prev" = --desktop ] && desk="--desktop $a"; prev=$a; done
  [ -n "$desk" ] && { python3 "$here/tools/try_wsl.py" --check $desk || exit 1; } # (before a running --try is ended)
  try_setup "$@"
  [ -z "$desk" ] && echo "clawd-room: opening 4 rooms and snapping them into a group: hands off the keyboard for a few seconds" >&2
  python3 "$here/tools/try_wsl.py" "$bin" "$try" "$ev" $desk || { touch "$try/stop"; exit 1; }
  echo "clawd-room: --try opened 4 rooms (quit one to close all, or ./clawd-room.sh --try close)" >&2
  exit 0
fi
# WSL + Windows Terminal: the pixel editor opens in its own Clawd Room window in the middle of the screen (like iTerm2);
# the window you typed in stays. --here (or CLAWD_HERE=1) runs it in the current window.
if [ "$editing" = 1 ] && [ "$here_only" = 0 ] && [ "${CLAWD_HERE:-0}" = 0 ] && [ -n "${WSL_DISTRO_NAME:-}" ] && [ -x "$bin" ] && command -v wt.exe >/dev/null 2>&1; then
  python3 "$here/tools/wsl_open.py" "Clawd Room pixel editor" 70 78 "$bin" "$@" && { echo "clawd-room: the editor opened in its own window" >&2; exit 0; }
  echo "clawd-room: could not open a window for the editor, running here" >&2
fi
if [ "$(uname -s)" = Darwin ] && [ "$TERM_PROGRAM" = iTerm.app ] && [ -z "$TMUX" ]; then
  dyn="$HOME/Library/Application Support/iTerm2/DynamicProfiles"
  prof="$dyn/clawd-room.json"
  ign='{"Action": 13, "Text": ""}'
  # a window opened with the profile is already a quarter of the screen (4x8 pt cells, iTerm2's title bar and margins)
  grid=$(osascript -l JavaScript -e 'ObjC.import("AppKit"); const v = $.NSScreen.mainScreen.visibleFrame.size;
    Math.floor((v.width / 2 - 10) / 4) + " " + Math.floor((v.height / 2 - 38) / 8)' 2>/dev/null) || grid="181 53"
  cols=${grid% *} rows=${grid#* }
  want='{"Profiles": [{"Name": "Clawd Room", "Guid": "clawd-room-profile-1", "Normal Font": "Menlo 6", "Horizontal Spacing": 1.0, "Vertical Spacing": 1.1, "Use Non-ASCII Font": false, "Custom Command": "Yes", "Command": "/bin/sh -lc '"'$here/clawd-room.sh'"'", "Close Sessions On End": true, "Columns": '"$cols"', "Rows": '"$rows"', "Keyboard Map": {"0x3d-0x100000": '"$ign"', "0x2b-0x120000": '"$ign"', "0x3d-0x120000": '"$ign"', "0x2d-0x100000": '"$ign"', "0x30-0x100000": '"$ign"'}}]}'
  if [ "$(cat "$prof" 2>/dev/null)" != "$want" ]; then
    mkdir -p "$dyn" && printf '%s\n' "$want" > "$prof" && sleep 1.5 # (iTerm2 picks the file up by itself)
  fi
  if [ "${1:-}" = --try ]; then
    try_setup "$@"
    CR_HERE="$here" CR_BIN="$bin" CR_TRY="$try" CR_EV="$ev" osascript -l JavaScript -e '
      ObjC.import("AppKit");
      const env = $.NSProcessInfo.processInfo.environment, get = k => env.objectForKey(k).js;
      const here = get("CR_HERE"), bin = get("CR_BIN"), tr = get("CR_TRY"), ev = get("CR_EV");
      const scr = $.NSScreen.mainScreen, f = scr.frame, v = scr.visibleFrame;
      const vx = v.origin.x, vy = f.size.height - v.origin.y - v.size.height, w = Math.round(v.size.width / 2), h = Math.round(v.size.height / 2);
      const rooms = [["office", "office", 0, 0], ["coffee", "coffee", 1, 0], ["park", "park", 0, 1], ["office2", "office", 1, 1]];
      const it = Application("iTerm2");
      for (const [name, theme, qx, qy] of rooms) {
        const q = String.fromCharCode(39), sq = x => q + x + q;
        const run = "cd " + sq(here) + " && exec env CLAWD_HERE=1 CLAWD_LINK_DIR=" + sq(tr + "/link") + " CLAWD_SIZE_LOG=" + sq(tr + "/log-" + name) + " sh " + sq(here + "/tools/try_room.sh") + " " + sq(bin) + " " + sq(tr + "/stop") + " --room " + name + " --theme " + theme + " --scale 1 --events " + sq(ev); /* (a fixed scale: AUTO would change it as Clawds visit, and the gates wait for still windows) */
        const win = it.createWindowWithProfile("Clawd Room", {command: "/bin/sh -lc " + JSON.stringify(run)});
        win.bounds = {x: vx + qx * w, y: vy + qy * h, width: w, height: h};
      }
      ""' >/dev/null 2>&1 || { echo "clawd-room: could not open the --try windows" >&2; touch "$try/stop"; exit 1; }
    echo "clawd-room: --try opened 4 rooms (quit one to close all, or ./clawd-room.sh --try close)" >&2
    exit 0
  fi
  if [ "$here_only" = 0 ] && [ "${CLAWD_HERE:-0}" = 0 ] && [ "$ITERM_PROFILE" != "Clawd Room" ] && [ -x "$bin" ] && command -v osascript >/dev/null 2>&1; then
    cmd="'$bin'"
    for a in "$@"; do cmd="$cmd '$(printf %s "$a" | sed "s/'/'\\\\''/g")'"; done
    run="cd '$PWD' && exec env CLAWD_HERE=1 $cmd" # (iTerm2 starts the command with its own environment: pass on the folder)
    # open the room's window over this one; say whether this window holds only this session
    alone=$(CR_RUN="$run" CR_SID="${ITERM_SESSION_ID#*:}" CR_EDIT="$editing" osascript -l JavaScript -e '
      const env = $.NSProcessInfo.processInfo.environment, get = k => env.objectForKey(k).js;
      const it = Application("iTerm2"), sid = get("CR_SID");
      let mine = null, alone = false;
      it.windows().forEach(w => w.tabs().forEach(t => t.sessions().forEach(s => { if (s.id() === sid) { mine = w; alone = w.tabs().length === 1 && t.sessions().length === 1; } })));
      const nw = it.createWindowWithProfile("Clawd Room", {command: "/bin/sh -lc " + JSON.stringify(get("CR_RUN"))});
      if (get("CR_EDIT") === "1") { /* the editor: big, in the middle of the screen */
        ObjC.import("AppKit");
        const scr = $.NSScreen.mainScreen, f = scr.frame, v = scr.visibleFrame;
        const vx = v.origin.x, vy = f.size.height - v.origin.y - v.size.height, vw = v.size.width, vh = v.size.height, ww = Math.round(vw * 0.7), wh = Math.round(vh * 0.78);
        nw.bounds = {x: Math.round(vx + (vw - ww) / 2), y: Math.round(vy + (vh - wh) / 2), width: ww, height: wh};
      } else if (mine) { /* the quarter of the screen this window is in, like the room would pick */
        ObjC.import("AppKit");
        const scr = $.NSScreen.mainScreen, f = scr.frame, v = scr.visibleFrame, b = mine.bounds();
        const vx = v.origin.x, vy = f.size.height - v.origin.y - v.size.height, vw = v.size.width, vh = v.size.height;
        nw.bounds = {x: Math.round(b.x + b.width / 2 < vx + vw / 2 ? vx : vx + vw / 2), y: Math.round(b.y + b.height / 2 < vy + vh / 2 ? vy : vy + vh / 2),
                     width: Math.round(vw / 2), height: Math.round(vh / 2)};
      }
      alone ? "1" : "0"' 2>/dev/null) || alone=x
    if [ "$alone" != x ]; then
      if [ "$alone" = 1 ] && [ "$keep" = 0 ] && [ -n "$ITERM_SESSION_ID" ]; then
        # close the window this was typed in, once this script has ended and its shell is idle again
        CR_SID="${ITERM_SESSION_ID#*:}" nohup osascript -l JavaScript -e '
          delay(0.6); const sid = $.NSProcessInfo.processInfo.environment.objectForKey("CR_SID").js;
          Application("iTerm2").windows().forEach(w => w.tabs().forEach(t => t.sessions().forEach(s => { if (s.id() === sid) s.close(); })));' >/dev/null 2>&1 &
      else
        echo "clawd-room: opened in its own iTerm2 window (Clawd Room profile, fixed pixel size)" >&2
      fi
      exit 0
    fi
    echo "clawd-room: could not open an iTerm2 window, running here" >&2
  fi
fi
if [ -x "$bin" ]; then exec "$bin" "$@"; fi
echo "clawd-room: c/clawd-room is not built" >&2
exit 1
