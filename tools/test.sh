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
export CLAWD_MY_SPRITES=/nonexistent-clawd-my-sprites # (the defaults only: your own sprites never change the snapshots)
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
snap parts.txt $bin --golden-parts anims/swing.txt anims/brew.txt anims/gates/simple/gate-door.txt anims/gates/simple/gate-stairs.txt anims/gates/simple/gate-well.txt tools/fixtures/anims/bad-order.txt tools/fixtures/anims/bad-field.txt tools/fixtures/anims/bad-ease.txt tools/fixtures/anims/bad-time.txt tools/fixtures/anims/loop-wrap.txt
snap swing-file.txt env CLAWD_ANIMS=tools/fixtures/anims-big $bin --script tools/scenarios/park-swing.sc   # the swing's motion comes from anims/swing.txt (here a bigger one)
snap gate-file.txt env CLAWD_ANIMS=tools/fixtures/anims-big $bin --script tools/scenarios/office-gates-appear.sc   # gates appear by anims/gate-*.txt (here slower steps)
snap gate-theme.txt env CLAWD_ANIMS=tools/fixtures/anims-big CLAWD_GATE_THEME=slow $bin --script tools/scenarios/office-gates-appear.sc   # a theme is a folder anims/gates/NAME (here the stairs slower; what it lacks comes from simple)
snap gate-lift.txt env CLAWD_ANIMS=tools/fixtures/anims-big CLAWD_GATE_THEME=lift $bin --script tools/scenarios/office-gates-appear.sc   # a part's x / y / glow move and tint it as it comes in (here the side door drops in, glowing)
snap gate-construct.txt env CLAWD_GATE_THEME=construct $bin --script tools/scenarios/office-gates-appear.sc   # the construct theme (anims/gates/construct): the gates are built from their own pieces
if [ $mode = compare ]; then # ... and the file makes the same motion as the code it replaced (2 * sin(3.2 t)): without a file nothing differs
  if env CLAWD_ANIMS=/nonexistent $bin --script tools/scenarios/park-swing.sc | diff -q - <($bin --script tools/scenarios/park-swing.sc) > /dev/null; then echo "OK   park-swing.sc (the file and the code swing the same)"; else echo "FAIL park-swing.sc (the file and the code swing differently)"; fail=1; fi
  if env CLAWD_ANIMS=/nonexistent $bin --script tools/scenarios/office-gates-appear.sc | diff -q - <($bin --script tools/scenarios/office-gates-appear.sc) > /dev/null; then echo "OK   office-gates-appear.sc (the gate files and the code appear the same)"; else echo "FAIL office-gates-appear.sc (the gate files and the code appear differently)"; fail=1; fi
fi
snap dump.txt dumps
snap overlap.txt $bin --check   # items painting over each other (Engine Design: the placer brings this to 0)
[ $mode = update ] && exit 0
if [ $fail = 0 ]; then echo "all snapshots match"; else echo "SOME SNAPSHOTS DIFFER"; fi
exit $fail
