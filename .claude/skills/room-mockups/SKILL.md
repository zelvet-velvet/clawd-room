---
name: room-mockups
description: How visual changes start in clawd-room: write a mock program that draws with the room's own code, show the options as one picture, wait for Zaviel's pick, record it. Use before any change to how the room looks (pixel art, rooms, props, colours, layout, UI) and when writing or running a program in gallery/mock/.
---
# Mockups (clawd-room)

**Visual changes start with rendered mockups** that Zaviel picks; then build. The look is style A (one-point perspective, `Design/Rooms and Art.md`). The Clawd looks official: flat `#CD6E58`, 14x8 sprite (8-wide torso, 2x2 arms, four 1-pixel legs), 1x1 eyes, no mouth. Pixel text is upper case only (3x5 font).

## Local scratch (Zaviel, 2026-10-27)
- Every person keeps mockups **local** (`gallery/` is not in git): the program in `gallery/mock/NAME_mock.c`, binaries in `gallery/mock/bin/`, pictures in `gallery/mock/out/NN-slug/` (one folder per question, numbered on from *your* last one), and a row in `gallery/mock/out/README.md` (folder, question and options, program, status: waiting / **X picked** / replaced by NN). Never only in a temp folder. Numbers are per person: the vault never uses them.

## Writing a mock program (Zaviel, 2026-10-30)
The room draws the mockup; the mock only changes what the question is about.
- **Build line** in the top comment, with the Makefile's flags and only the objects of this branch: `make -C ../../c && cc -O2 -std=c11 -D_DEFAULT_SOURCE -mfma -ffp-contract=fast -I../../c NAME_mock.c $(ls ../../c/*.c | sed 's/\.c$/.o/' | grep -v main.o) -lm -o bin/NAME_mock && bin/NAME_mock`.
- **The room's own drawing**: a scene from `fixture_load_text` drawn with `render()`, sprites loaded as `main.c` does (`sprites_load`, `theme_set_props`, `draw_set_sprites`). To reach a `static` helper, `#include "draw.c"` (or `theme.c`) and leave its `.o` out of the link (two such files: two mock files). Change behaviour in the mock, never in `c/`: override a constant before the include (`#undef WALK_MIN` / `#define WALK_MIN 999` stops the placer, so the mock places things itself).
- **Fixtures stand still**: Clawds stay at `px` / `py` and the placer's places are not applied: seat them yourself (`cx - 5`, body top `by + SEAT_DY`); `room.s[]` is filled only once the room is drawn.
- **The real size**: draw at the size the question is about (the user's window or pane: `--list-windows`) and enlarge for the sheet (`canvas_scaled`); props of a few pixels also get an enlarged crop. The sheet: themes along the top, options down the left with their letter in yellow, today's look as the first row.
- **Before showing**: look at every picture yourself (touching, on the wall, cut off, clipped at any window size); a crash: build once with `-g -fsanitize=address`. One program may answer several questions (one `out/` folder and README row each).

## Shown, picked, recorded
- Show the options as **one picture** (frames side by side, `.gif` for an animation) and wait for the pick. Update the README row.
- **Into git, only the decision**: the picked option and the one side-by-side picture go to `Clawd Room Note/Assets/Snapshots/<subject>/` through `/cmd-notes-sync` (picked one `(picked)`, others `not picked` in `_Meta/Snapshot status.md`). Never `gallery/`, binaries, or every variant.
- **Before it is built**: a `> [!todo]- Picked, not built yet` callout in the design note, with the date and a link to the day note. The pull request for the visual change carries the picked picture.
