#!/bin/bash
# Redraws the README's GIFs from the program itself (the scenes in tools/readme/):  tools/readme/make_gifs.sh [NAME ...]
cd "$(dirname "$0")/../.." || exit 1
bin=c/clawd-room
out=docs/images
export CLAWD_SPRITES=sprites CLAWD_MY_SPRITES=/nonexistent CLAWD_MY_ANIMS=/nonexistent CLAWD_SETTINGS=/nonexistent
[ -x $bin ] || make -C c >/dev/null || exit 1
names=${*:-office-day coffee park moving simple-stairs construct-stairs construct-door construct-well}
for n in $names; do
  theme=simple; case $n in construct-*) theme=construct ;; esac
  CLAWD_GATE_THEME=$theme CLAWD_GIF=$out/$n.gif $bin --script tools/readme/$n.sc | grep '^gif'
done

# the four-rooms picture
if [ -z "$*" ] || [ "$*" = four-rooms ]; then
  tmp=$(mktemp)
  cc -O2 -std=c11 -D_DEFAULT_SOURCE -Wno-format-truncation -Wno-unused-function -mfma -ffp-contract=fast -Ic tools/readme/four_rooms.c \
    $(ls c/*.o | grep -v 'c/main.o\|c/editor.o') -lm -o "$tmp" && "$tmp" $out/four-rooms.png && echo "png $out/four-rooms.png ok"
  rm -f "$tmp"
fi
