#!/bin/bash
# Checks the C program against the snapshots in c/tests/expected. The snapshots were made with the Python version of the room
# (room_tui.py, removed 2026-10-16; it is in git history) and the two agreed on every line; the empty-room-visitor scene and the
# office-empty-guest fixture were added later from the C program (they check that visitors are drawn in a room with no sessions).
#   tools/test.sh            compare
#   tools/test.sh --update   rewrite the snapshots from the C program (only when a change of behaviour is meant)
cd "$(dirname "$0")/.." || exit 1
bin=c/clawd-room
exp=c/tests/expected
export CLAWD_SPRITES=sprites
mode=compare
[ "$1" = "--update" ] && mode=update
fail=0

snap() { # name, then the command
  local name=$1; shift
  if [ $mode = update ]; then "$@" > "$exp/$name"; echo "wrote $name"; return; fi
  if "$@" | diff -q - "$exp/$name" > /dev/null; then echo "OK   $name"; else echo "FAIL $name"; "$@" | diff - "$exp/$name" | head -6; fail=1; fi
}

dumps() { # the --dump frames, in the format of dump.txt
  local events at c
  for pair in "tools/fixtures/events-odd.jsonl 1010" "tools/fixtures/events-sim.jsonl 1800000000"; do
    set -- $pair; events=$1; at=$2
    for c in "--dump 100x30" "--dump 120x40 --theme coffee" "--dump 140x50 --theme park" "--dump 80x24 --ratio 16:9" "--dump 160x60 --theme office --room alpha" "--dump 200x70 --scale 2" "--dump 100x30 --zoom 2" "--dump 120x40 --theme park --zoom 3" "--dump 100x30 --room-size s" "--dump 200x70 --room-size s" "--dump 200x70 --room-size m --zoom 2"; do
      echo "### $(basename "$events") $c"
      $bin $c --events "$events" --at "$at"
    done
  done
}

[ -x $bin ] || make -C c -s || exit 1
snap golden-sprites.txt $bin --golden
snap golden-code.txt $bin --golden --code
snap golden-term.txt $bin --golden-term
snap fixtures.txt $bin --fixture tools/fixtures/*.fx
snap rng.txt $bin --golden-rng
snap scenarios.txt $bin --script tools/scenarios/*.sc
snap ui.txt $bin --golden-ui tools/fixtures/events-odd.jsonl
snap win.txt $bin --golden-win
snap dump.txt dumps
[ $mode = update ] && exit 0
if [ $fail = 0 ]; then echo "all snapshots match"; else echo "SOME SNAPSHOTS DIFFER"; fi
exit $fail
