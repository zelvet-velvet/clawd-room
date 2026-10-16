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
    echo "clawd-room: needs make and a C compiler to build (sudo apt install build-essential)" >&2
  fi
fi
if [ -x "$bin" ]; then exec "$bin" "$@"; fi
echo "clawd-room: c/clawd-room is not built" >&2
exit 1
