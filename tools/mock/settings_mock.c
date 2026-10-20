/* Mockup (tools/mock, build.sh): a SETTINGS panel inside the room (key ,): A over the room, B along its right side with the room still live */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "art.h"
#include "fixture.h"
#include "ui.h"
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
static Rgb mix(Rgb a, Rgb b, double t) { return RGB((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t)); }
typedef struct { const char *head, *name, *val; } Row;
static const Row ROWS[] = {
    {"PICKED CLAWD", 0, 0}, {0, "SPOTLIGHT", "28"}, {0, "FADES AFTER", "3 S"},
    {"GATES", 0, 0}, {0, "APPEAR", "CONSTRUCT"}, {0, "LENGTH", "1.4 S"}, {0, "TAGS (G M)", "ON"},
    {"ROOM", 0, 0}, {0, "SIZE", "AUTO"}, {0, "AUTO ZOOM", "OFF"}, {0, "FRAMES", "16 FPS"},
};
#define NR ((int)(sizeof ROWS / sizeof ROWS[0]))
static void panel(Canvas *c, int x, int y, int w, int h, int pick) {
    canvas_rect(c, x - 1, y - 1, w + 2, h + 2, RGB(90, 90, 110));
    canvas_rect(c, x, y, w, h, RGB(20, 20, 28));
    canvas_text(c, x + 4, y + 3, "SETTINGS", C_YELLOW);
    int ly = y + 12;
    for (int i = 0; i < NR; i++) {
        if (ROWS[i].head) { if (i) ly += 2; canvas_text(c, x + 4, ly, ROWS[i].head, RGB(120, 120, 140)); ly += 7; continue; }
        int on = i == pick;
        if (on) canvas_rect(c, x + 1, ly - 1, w - 2, 7, RGB(44, 44, 60));
        canvas_text(c, x + 7, ly, ROWS[i].name, on ? C_WHITE : RGB(190, 190, 205));
        int vw = text_width(ROWS[i].val), vx = x + w - 5 - vw - (on ? 6 : 0);
        if (on) { canvas_text(c, vx - 6, ly, "<", C_YELLOW); canvas_text(c, x + w - 8, ly, ">", C_YELLOW); }
        canvas_text(c, vx, ly, ROWS[i].val, on ? C_YELLOW : RGB(230, 200, 120));
        ly += 7;
    }
}
int main(void) {
    FILE *f = fopen("/tmp/clawd-mock-sm.fx", "w");
    fprintf(f, "theme office\ntitle office\nterminal 220 64\ntime 1000.0 3.4\nscale 1\nneighbor right office\nneighbor up office\n"
               "session s00 name=clawd-room created=900 state=work tool=Bash\nsession s01 name=park created=910 state=think tool=Edit hold=1000.4\n"
               "session s02 name=api created=920 state=wait tool=mcp__x__y\nsession s03 name=web created=930 state=idle tool=Read\n");
    fclose(f);
    static Fixture fx; fixture_load(&fx, "/tmp/clawd-mock-sm.fx");
    for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; }
    fx.room.freeze = 0;
    char note[64]; Canvas room = {0};
    for (int k = 0; k < 40; k++) { if (room.p) canvas_free(&room); room = render(fx.cols, fx.rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note); }
    int W = room.w, H = room.h, pw = 92, ph = 12 + 8 * 7 + 3 * 9 + 6;
    Canvas sheet = canvas_new(2 * (W + 6) + 6, H + 18, RGB(24, 24, 30));
    for (int v = 0; v < 2; v++) {
        Canvas c = canvas_new(W, H, RGB(20, 20, 28));
        if (v == 0) { /* A: over the room, the room dimmed behind */
            canvas_blit(&c, &room, 0, 0);
            for (int i = 0; i < W * H; i++) c.p[i] = mix(c.p[i], RGB(10, 10, 20), 0.55);
            panel(&c, (W - pw) / 2, (H - 8 - ph) / 2, pw, ph, 2);
        } else { /* B: the room shrinks to the left and keeps running; the panel on the right */
            Canvas small = render(fx.cols - pw - 2, fx.rows, &fx.room, fx.now + 2.5, fx.t + 2.5, note, sizeof note);
            canvas_blit(&c, &small, 0, 0); canvas_free(&small);
            panel(&c, W - pw - 1, 1, pw, H - 10, 2);
        }
        BarPiece bar[1] = {{"^ V PICK   < > CHANGE   , CLOSE   SAVED AT ONCE", C_WHITE, 0}};
        draw_bar(&c, bar, 1);
        canvas_text(&sheet, 6 + v * (W + 6), 3, v == 0 ? "A  OVER THE ROOM" : "B  BESIDE THE ROOM (IT KEEPS RUNNING)", RGB(220, 220, 235));
        canvas_blit(&sheet, &c, 6 + v * (W + 6), 12);
        canvas_free(&c);
    }
    png_write("out/06-settings/settings-panel.png", &sheet, 3);
    return 0;
}
