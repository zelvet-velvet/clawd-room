/* Mockup (tools/mock, build.sh): the real bottom bar over a coffee shop with the well, short and long (g held) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "art.h"
#include "fixture.h"
#include "ui.h"
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
int main(void) {
    int sizes[2][2] = {{110, 32}, {150, 44}};
    Canvas sheet = canvas_new(3 * 154 + 8, 2 * 92 + 8, RGB(24, 24, 30));
    for (int si = 0; si < 2; si++) for (int held = 0; held < 3; held++) {
        FILE *f = fopen("/tmp/clawd-mock-bd.fx", "w");
        fprintf(f, "theme coffee\ntitle coffee\nterminal %d %d\ntime 1000.0 3.4\nneighbor down park\nneighbor right office\nneighbor up office\n"
                   "session s00 name=clawd-room created=900 state=work tool=Bash\nsession s01 name=api created=910 state=idle tool=Read\n", sizes[si][0], sizes[si][1]);
        fclose(f);
        static Fixture fx; fixture_load(&fx, "/tmp/clawd-mock-bd.fx");
        for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; }
        fx.room.freeze = 0;
        char note[64]; Canvas cv = {0};
        int fix = held == 2, rows = fx.rows - (fix ? 4 : 0); /* the fix: the room is laid out above a one-line bar (4 rows = 8 px) */
        for (int k = 0; k < 40; k++) { if (cv.p) canvas_free(&cv); cv = render(fx.cols, rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note); }
        if (fix) {
            Canvas full = canvas_new(cv.w, fx.rows * 2, RGB(20, 20, 28));
            canvas_blit(&full, &cv, 0, 0);
            /* g held: each gate gets a tag with its arrow and where it goes, right where it is */
            const Geo *g = &fx.room.geo;
            static const char *TAG[SIDES] = {"< ", "> OFFICE", "^ OFFICE", "V PARK"};
            for (int sd = 0; sd < SIDES; sd++) {
                int on = 0; for (int q = 0; q < fx.room.nnb; q++) if (fx.room.nb[q].side == sd) on = 1;
                if (!on) continue;
                int w = text_width(TAG[sd]) + 4, x = (int)g->gate_out[sd][0] - w / 2, y = (int)g->gate_out[sd][1] - (sd == SIDE_DOWN ? 10 : 22);
                if (sd == SIDE_UP) { x = (int)g->gate_out[sd][0] - w - 6; y = (int)g->gate_out[sd][1] - 12; }
                if (x + w > cv.w - 1) x = cv.w - 1 - w; if (x < 1) x = 1; if (y < 1) y = 1;
                canvas_rect(&full, x, y, w, 7, RGB(20, 20, 28));
                canvas_text(&full, x + 2, y + 1, TAG[sd], C_YELLOW);
            }
            canvas_free(&cv); cv = full;
        }
        BarPiece bar[6]; int n = 0;
        bar[n++] = (BarPiece){"AUTO FOLLOW", C_WHITE, 0};
        if (held == 2) { bar[0] = (BarPiece){"G SEND: PRESS AN ARROW", C_YELLOW, 0}; }
        if (held == 1) {
            bar[n++] = (BarPiece){"G SEND", C_YELLOW, 1}; bar[n++] = (BarPiece){"/", RGB(140, 128, 84), 0};
            bar[n++] = (BarPiece){"M MOVE API", RGB(140, 128, 84), 0}; bar[n++] = (BarPiece){"+ ARROW:", C_YELLOW, 0};
            bar[n++] = (BarPiece){"> OFFICE DOOR   ^ OFFICE STAIRS UP   V PARK STAIRS DOWN", C_YELLOW, 0};
        }
        draw_bar(&cv, bar, n);
        canvas_blit(&sheet, &cv, 4 + held * 154, 4 + si * 92);
        canvas_free(&cv);
    }
    png_write("out/04-bar-and-gate-tags/bar-over-well.png", &sheet, 4);
    return 0;
}
