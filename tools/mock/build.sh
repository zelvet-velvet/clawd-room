#!/usr/bin/env bash
# The mockup programs: scratch pictures drawn with the room's own code (canvas, renderer, themes), for Zaviel to pick from.
# The pictures shown are kept in the vault (Assets/Snapshots, marked picked / not picked in _Meta/Snapshot status); these
# programs are here so an idea can be drawn again or taken further.
#
#   tools/mock/build.sh            build them all and draw every mockup into gallery/mock/out/ (not in git)
#   tools/mock/build.sh NAME ...   only these (pick_gate_mock, tag_mock, well_mock, settings_mock, bar_demo, palette_mock, editor_mock, gate_editor_mock, part_anim_mock)
#
# Each links the room's object files (all but main.o). theme_mock.c is made from c/theme.c with one switch added (the
# park without its big tree), for the construct mockup of the park's building.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT="$ROOT/gallery/mock"
make -s -C "$ROOT/c" clawd-room
mkdir -p "$OUT/bin" "$OUT/out"/{01-pick-highlight,02-gate-appear-first-round,03-gate-construct,04-bar-and-gate-tags,05-pick-d-spotlight,06-settings,07-construct-real-parts,08-well-redesign,09-editor-animation,10-gate-editor,11-part-animations,palette}
python3 - "$ROOT/c/theme.c" "$OUT/bin/theme_mock.c" <<'EOF'
import sys
s = open(sys.argv[1]).read()
s = "int g_mock_no_tree; /* (mockup: the park without its big tree) */\n" + s
s = s.replace("typedef struct { const Theme *th; int w, h; Canvas bg; } Cache;", "typedef struct { const Theme *th; int w, h, nt; Canvas bg; } Cache;")
s = s.replace("cache[i].h == h) return", "cache[i].h == h && cache[i].nt == g_mock_no_tree) return")
s = s.replace("cache[i].h == cv->h) hit", "cache[i].h == cv->h && cache[i].nt == g_mock_no_tree) hit")
s = s.replace("(Cache){th, cv->w, cv->h, canvas_new(", "(Cache){th, cv->w, cv->h, g_mock_no_tree, canvas_new(")
tree = "    canvas_rect(c, tx - 2, ty - 26, 5, 26, RGB(120, 86, 56));"
end = "    blob(c, tx - 8, ty - 25, 4, RGB(66, 132, 66));\n"
if tree not in s or end not in s: sys.exit("theme_mock: c/theme.c changed, update tools/mock/build.sh")
s = s.replace(tree, "    if (!g_mock_no_tree) {\n" + tree).replace(end, end + "    }\n", 1)
open(sys.argv[2], "w").write(s)
EOF
CC=${CC:-cc}
FLAGS="-O2 -std=c11 -D_DEFAULT_SOURCE -I$ROOT/c"
OBJS=$(ls "$ROOT"/c/*.o | grep -v -e '/main.o$' -e '/theme.o$')
$CC $FLAGS -c "$OUT/bin/theme_mock.c" -o "$OUT/bin/theme_mock.o"
NAMES=${*:-pick_gate_mock tag_mock well_mock settings_mock bar_demo palette_mock editor_mock gate_editor_mock part_anim_mock}
cd "$OUT"
for n in $NAMES; do
    if [ "$n" = editor_mock ]; then # (it includes c/editor.c, to draw with the editor's own code)
        $CC $FLAGS -Wno-unused-function "$HERE/$n.c" "$OUT/bin/theme_mock.o" $(echo $OBJS | tr ' ' '\n' | grep -v '/editor.o$') -lm -o "bin/$n"
    else
        $CC $FLAGS "$HERE/$n.c" "$OUT/bin/theme_mock.o" $OBJS -lm -o "bin/$n"
    fi
    case $n in
        pick_gate_mock) bin/$n; bin/$n construct; bin/$n construct2; bin/$n pickd ;;
        tag_mock) bin/$n; bin/$n 1; bin/$n 2 ;;
        palette_mock) bin/$n out/palette ;;
        editor_mock) bin/$n out/09-editor-animation "$ROOT/sprites" ;;
        gate_editor_mock) bin/$n out/10-gate-editor ;;
        part_anim_mock) bin/$n out/11-part-animations ;;
        *) bin/$n ;;
    esac
done
echo "mockups in $OUT/out"
