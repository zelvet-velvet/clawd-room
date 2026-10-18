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
# iTerm2: the room runs in its own window with the "Clawd Room" profile (Menlo 6 at 1.1 line spacing: 4x8 pt cells, so a
# room pixel is always a square 4x4 pt). iTerm2 ignores the escape code that switches an open window's profile, so a new
# window is opened instead. The profile ignores Cmd = + - 0 (iTerm2's font size keys), so the pixel size stays. --here (or CLAWD_HERE=1) keeps the current window; so does a run that only prints.
here_only=0
for a in "$@"; do case "$a" in --here) here_only=1 ;; --dump*|--build-art*|-h|--help|--version) here_only=1 ;; esac; done
for a in "$@"; do shift; [ "$a" = --here ] || set -- "$@" "$a"; done
if [ "$(uname -s)" = Darwin ] && [ "$TERM_PROGRAM" = iTerm.app ] && [ -z "$TMUX" ] && [ "$here_only" = 0 ] \
   && [ "${CLAWD_HERE:-0}" = 0 ] && [ "$ITERM_PROFILE" != "Clawd Room" ] && [ -x "$bin" ] && command -v osascript >/dev/null 2>&1; then
  dyn="$HOME/Library/Application Support/iTerm2/DynamicProfiles"
  prof="$dyn/clawd-room.json"
  want='{"Profiles": [{"Name": "Clawd Room", "Guid": "clawd-room-profile-1", "Normal Font": "Menlo 6", "Horizontal Spacing": 1.0, "Vertical Spacing": 1.1, "Use Non-ASCII Font": false, "Keyboard Map": {"0x3d-0x100000": {"Action": 13, "Text": ""}, "0x2b-0x120000": {"Action": 13, "Text": ""}, "0x3d-0x120000": {"Action": 13, "Text": ""}, "0x2d-0x100000": {"Action": 13, "Text": ""}, "0x30-0x100000": {"Action": 13, "Text": ""}}}]}'
  if [ "$(cat "$prof" 2>/dev/null)" != "$want" ]; then
    mkdir -p "$dyn" && printf '%s\n' "$want" > "$prof" && sleep 1.5 # (iTerm2 picks the file up by itself)
  fi
  cmd="'$bin'"
  for a in "$@"; do cmd="$cmd '$(printf %s "$a" | sed "s/'/'\\\\''/g")'"; done
  # (iTerm2 starts the command with its own environment: pass ours on, and the folder)
  run="cd '$PWD' && exec env CLAWD_HERE=1 $cmd"
  if CR_RUN="$run" osascript -l JavaScript -e '
      const env = $.NSProcessInfo.processInfo.environment.objectForKey("CR_RUN").js;
      Application("iTerm2").createWindowWithProfile("Clawd Room", {command: "/bin/sh -lc " + JSON.stringify(env)});
      ""' >/dev/null 2>&1; then
    echo "clawd-room: opened in its own iTerm2 window (Clawd Room profile, fixed pixel size); --here runs it in this one" >&2
    exit 0
  fi
  echo "clawd-room: could not open an iTerm2 window, running here" >&2
fi
if [ -x "$bin" ]; then exec "$bin" "$@"; fi
echo "clawd-room: c/clawd-room is not built" >&2
exit 1
