#!/bin/sh
# Starts the room. The windows (the Clawd Room profile, shortcuts) call this, so the program behind them can change
# without touching Windows Terminal's settings.
#
# Which program runs (first match wins):
#   1. the environment:  CLAWD_ENGINE=python  or  CLAWD_ENGINE=c
#   2. the first line of ~/.clawd-room/engine  (python or c)
#   3. automatic: the C program (c/clawd-room) if it is there or can be built, else room_tui.py
# The C program is built with `make -C c` when it is missing or older than its sources (first start takes a few seconds).
# If that fails, automatic mode falls back to Python; `c` does not (it says why).
here=$(cd "$(dirname "$0")" && pwd -P)
bin="$here/c/clawd-room"
engine=${CLAWD_ENGINE:-}
if [ -z "$engine" ] && [ -r "$HOME/.clawd-room/engine" ]; then
  engine=$(head -n 1 "$HOME/.clawd-room/engine" | tr -d ' \r\t')
fi
if [ "$engine" != python ]; then
  stale=0
  [ -x "$bin" ] || stale=1
  if [ "$stale" = 0 ]; then
    for f in "$here"/c/*.c "$here"/c/*.h "$here"/c/Makefile; do
      [ "$f" -nt "$bin" ] && { stale=1; break; }
    done
  fi
  if [ "$stale" = 1 ] && command -v make >/dev/null 2>&1 && command -v cc >/dev/null 2>&1; then
    echo "building the C version of clawd-room..." >&2
    make -C "$here/c" -s >/dev/null 2>&1 || echo "(the build failed; run 'make -C c' to see why)" >&2
  fi
  if [ -x "$bin" ]; then exec "$bin" "$@"; fi
  if [ "$engine" = c ]; then echo "clawd-room: CLAWD_ENGINE=c but c/clawd-room is not built (run 'make -C c')" >&2; exit 1; fi
fi
exec python3 "$here/room_tui.py" "$@"
