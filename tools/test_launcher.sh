#!/bin/sh
# clawd-room.sh: the engines give the same frame, the engine file and CLAWD_ENGINE choose, a missing C build is rebuilt
here=$(cd "$(dirname "$0")/.." && pwd -P)
ev="$here/tools/fixtures/events-odd.jsonl"
args="--dump 100x30 --events $ev --at 1010"
fail=0
check() { if [ "$2" = "$3" ]; then echo "OK   $1"; else echo "FAIL $1"; fail=1; fi; }
py=$(CLAWD_ENGINE=python "$here/clawd-room.sh" $args)
c=$(CLAWD_ENGINE=c "$here/clawd-room.sh" $args)
auto=$(env -u CLAWD_ENGINE "$here/clawd-room.sh" $args)
check "python and c print the same frame" "$py" "$c"
check "automatic gives the same frame" "$py" "$auto"
check "the frame is not empty" "$([ -n "$py" ] && echo yes)" "yes"
tmp=$(mktemp -d)
mkdir -p "$tmp/.clawd-room"
echo python > "$tmp/.clawd-room/engine"
# the engine file picks python: prove it by hiding the C build from automatic mode
bin="$here/c/clawd-room"
mv "$bin" "$bin.hidden"
via_file=$(HOME="$tmp" env -u CLAWD_ENGINE "$here/clawd-room.sh" $args)
check "the engine file (python) works without the C build" "$py" "$via_file"
rm -f "$tmp/.clawd-room/engine"
rebuilt=$(HOME="$tmp" env -u CLAWD_ENGINE "$here/clawd-room.sh" $args 2>/dev/null)
check "a missing C build is rebuilt and used" "$py" "$rebuilt"
check "... and the binary is back" "$([ -x "$bin" ] && echo yes)" "yes"
rm -f "$bin.hidden"
rm -rf "$tmp"
exit $fail
