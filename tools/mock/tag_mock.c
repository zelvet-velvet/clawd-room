/* Mockup (tools/mock, build.sh): the g / m hint as tags on the gates, four looks without a black box */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "art.h"
#include "fixture.h"
#include "ui.h"
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
static Rgb mix(Rgb a, Rgb b, double t) { return RGB((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t)); }
static void outlined(Canvas *c, int x, int y, const char *s, Rgb col, Rgb line) {
    Canvas t = canvas_new(text_width(s) + 2, 7, RGB(0, 0, 0));
    canvas_text(&t, 1, 1, s, RGB(255, 255, 255));
    for (int yy = 0; yy < t.h; yy++) for (int xx = 0; xx < t.w; xx++) {
        int on = t.p[yy * t.w + xx].r > 0, near = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int a = xx + dx, b = yy + dy; if (a >= 0 && b >= 0 && a < t.w && b < t.h && t.p[b * t.w + a].r) near = 1; }
        if (!on && near) canvas_px(c, x - 1 + xx, y - 1 + yy, line);
    }
    canvas_text(c, x, y, s, col); canvas_free(&t);
}
static void shadowed(Canvas *c, int x, int y, const char *s, Rgb col) {
    Canvas t = canvas_new(text_width(s) + 1, 6, RGB(0, 0, 0));
    canvas_text(&t, 0, 0, s, RGB(255, 255, 255));
    for (int yy = 0; yy < t.h; yy++) for (int xx = 0; xx < t.w; xx++) if (t.p[yy * t.w + xx].r) { int X = x + xx + 1, Y = y + yy + 1; if (X < c->w && Y < c->h) c->p[Y * c->w + X] = mix(c->p[Y * c->w + X], RGB(0, 0, 0), 0.55); }
    canvas_text(c, x, y, s, col); canvas_free(&t);
}
static void keycap(Canvas *c, int x, int y, const char *k) { /* the arrow on a little key */
    canvas_rect(c, x, y, 7, 8, RGB(70, 70, 86)); canvas_rect(c, x, y, 7, 7, RGB(225, 225, 235));
    canvas_text(c, x + 2, y + 1, k, RGB(30, 30, 40));
}
static void chevron(Canvas *c, int x, int y, int side, Rgb col) { /* a pixel arrow pointing the way, 5 px */
    for (int i = 0; i < 3; i++) {
        if (side == 0) canvas_rect(c, x + i, y - i, 1, 2 * i + 1, col);
        if (side == 1) canvas_rect(c, x + 2 - i, y - i, 1, 2 * i + 1, col);
        if (side == 2) canvas_rect(c, x - i, y + i, 2 * i + 1, 1, col);
        if (side == 3) canvas_rect(c, x - i, y + 2 - i, 2 * i + 1, 1, col);
    }
}
#include "theme.h"

/* the arrows drawn as pixels (the font's ^ and V look odd): 0 the font, 1 an arrow with a shaft (5x5), 2 a triangle */
static const char *ARROWS[2][4][5] = {
    {{"..#..", ".##..", "#####", ".##..", "..#.."}, {"..#..", "..##.", "#####", "..##.", "..#.."},
     {"..#..", ".###.", "#####", "..#..", "..#.."}, {"..#..", "..#..", "#####", ".###.", "..#.."}},
    {{"..#..", ".##..", "###..", ".##..", "..#.."}, {"..#..", "..##.", "..###", "..##.", "..#.."},
     {".....", "..#..", ".###.", "#####", "....."}, {".....", "#####", ".###.", "..#..", "....."}},
};
static int ARROW_STYLE;
static int arrow_w(void) { return ARROW_STYLE ? 5 : 3; }
static void arrow(Canvas *c, int x, int y, int sd, Rgb col, Rgb line) {
    if (!ARROW_STYLE) { static const char *K[4] = {"<", ">", "^", "V"}; outlined(c, x, y, K[sd], col, line); return; }
    const char *const *g = ARROWS[ARROW_STYLE - 1][sd];
    for (int yy = -1; yy <= 5; yy++) for (int xx = -1; xx <= 5; xx++) {
        int on = yy >= 0 && yy < 5 && xx >= 0 && xx < 5 && g[yy][xx] == '#', near = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int a = xx + dx, b = yy + dy; if (a >= 0 && b >= 0 && a < 5 && b < 5 && g[b][a] == '#') near = 1; }
        if (on) canvas_px(c, x + xx, y + yy, col); else if (near) canvas_px(c, x + xx, y + yy, line);
    }
}
/* where a gate's door is (its box in room pixels): drawn alone on an empty canvas (up: the opening at the platform's end,
 * the park: the building's door) */
static int gate_box(const Room *r, int side, int w, int h, int *x0, int *y0, int *x1, int *y1) {
    Rgb key = RGB(255, 0, 255);
    Canvas c = canvas_new(w, h, key);
    int park = r->theme == theme_get("park");
    theme_gate(r->theme, &c, &r->geo, side, 1.0, 0, side == SIDE_UP ? 2 : side < 2 && park ? 1 : -1);
    *x0 = w; *y0 = h; *x1 = -1; *y1 = -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!rgb_eq(c.p[y * w + x], key)) { if (x < *x0) *x0 = x; if (x > *x1) *x1 = x; if (y < *y0) *y0 = y; if (y > *y1) *y1 = y; }
    canvas_free(&c);
    return *x1 >= 0;
}
int main(int argc, char **argv) {
    ARROW_STYLE = argc > 1 ? atoi(argv[1]) : 0;
    static const struct { const char *theme; int cols, rows; } S[] = {{"coffee", 110, 32}, {"coffee", 150, 44}, {"office", 150, 44}, {"park", 150, 44}};
    static const char *KEY[4] = {"<", ">", "^", "V"}, *NAME[4] = {"PARK", "OFFICE", "OFFICE", "PARK"};
    int n = 4, W = 156, H = 96;
    Canvas sheet = canvas_new(2 * (W + 6) + 6, 2 * (H + 12) + 6, RGB(24, 24, 30));
    for (int si = 0; si < n; si++) {
        FILE *f = fopen("/tmp/clawd-mock-tm.fx", "w");
        fprintf(f, "theme %s\ntitle %s\nterminal %d %d\ntime 1000.0 3.4\nneighbor left park\nneighbor down park\nneighbor right office\nneighbor up office\n"
                   "session s00 name=clawd-room created=900 state=work tool=Bash\nsession s01 name=api created=910 state=idle tool=Read\n", S[si].theme, S[si].theme, S[si].cols, S[si].rows - 4);
        fclose(f);
        static Fixture fx; fixture_load(&fx, "/tmp/clawd-mock-tm.fx");
        for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; }
        fx.room.freeze = 0;
        char note[64]; Canvas room = {0};
        for (int k = 0; k < 40; k++) { if (room.p) canvas_free(&room); room = render(fx.cols, fx.rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note); }
        Canvas c = canvas_new(room.w, S[si].rows * 2, RGB(20, 20, 28)); /* the room above a one-line bar */
        canvas_blit(&c, &room, 0, 0);
        int tb[4][4], nt = 0; /* the tags placed so far (x0 y0 x1 y1) */
        static const int ORDER[4] = {SIDE_DOWN, SIDE_LEFT, SIDE_RIGHT, SIDE_UP};
        for (int oi = 0; oi < 4; oi++) {
            int sd = ORDER[oi], x0, y0, x1, y1;
            if (!gate_box(&fx.room, sd, room.w, room.h, &x0, &y0, &x1, &y1)) continue;
            int cx = (x0 + x1 + 1) / 2, y = y0 - 8 < 2 ? 2 : y0 - 8;
            for (int full = 1; full >= 0; full--) { /* the arrow and the name, centred over the door; on top of another tag: the arrow alone */
                int tw = full ? arrow_w() + 3 + text_width(NAME[sd]) : arrow_w(), x = cx - tw / 2, ok = 1;
                if (x < 2) x = 2; if (x + tw > room.w - 2) x = room.w - 2 - tw; /* (kept inside the room) */
                for (int k = 0; k < nt && ok; k++) if (x - 2 < tb[k][2] && x + tw + 2 > tb[k][0] && y - 2 < tb[k][3] && y + 7 > tb[k][1]) ok = 0;
                if (!ok && full) continue;
                if (x < 2) x = 2; if (x + tw > room.w - 2) x = room.w - 2 - tw;
                arrow(&c, x, y, sd, C_YELLOW, RGB(36, 24, 18));
                if (full) outlined(&c, x + arrow_w() + 3, y, NAME[sd], RGB(250, 246, 235), RGB(36, 24, 18));
                tb[nt][0] = x; tb[nt][1] = y; tb[nt][2] = x + tw; tb[nt][3] = y + 5; nt++;
                break;
            }
        }
        BarPiece bar[1] = {{"G SEND: PRESS AN ARROW", C_YELLOW, 0}};
        draw_bar(&c, bar, 1);
        int sx = 6 + (si % 2) * (W + 6), sy = 6 + (si / 2) * (H + 12);
        char t[48]; snprintf(t, sizeof t, "%s %dX%d", S[si].theme, S[si].cols, S[si].rows);
        for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        canvas_text(&sheet, sx, sy, t, RGB(220, 220, 235));
        canvas_blit(&sheet, &c, sx, sy + 8);
        canvas_free(&c); canvas_free(&room);
    }
    char out[128];
    if (ARROW_STYLE) snprintf(out, sizeof out, "out/07-construct-real-parts/tag-arrows-%c.png", ARROW_STYLE == 1 ? 'a' : 'b');
    else snprintf(out, sizeof out, "out/04-bar-and-gate-tags/gate-tags-a-centred.png");
    png_write(out, &sheet, 4);
    return 0;
}
