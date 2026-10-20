/* Mockup (tools/mock, build.sh): the stair well (the gate down) redrawn four ways, in the three rooms, with a Clawd halfway down */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "art.h"
#include "fixture.h"
#include "clawd.h"
#include "theme.h"
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
static Rgb mix(Rgb a, Rgb b, double t) { return RGB((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t)); }
static Rgb dk(Rgb a, double k) { return mix(a, RGB(16, 12, 12), k); }
typedef struct { Rgb rim, rim_lt, tread, riser, rail, dark; } Pal;
static unsigned char HOLE[400 * 200]; static int CW;
static void hole(Canvas *c, int x, int y, Rgb col) { if (x >= 0 && y >= 0 && x < c->w && y < c->h) { c->p[y * c->w + x] = col; HOLE[y * CW + x] = 1; } }
static void rect(Canvas *c, int x, int y, int w, int h, Rgb col) { canvas_rect(c, x, y, w, h, col); }
static void hrect(Canvas *c, int x, int y, int w, int h, Rgb col) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) hole(c, x + i, y + j, col); }

/* A  a stairwell with a railing: a bigger opening, steps down from the right (floor level) to the left, side walls, a
 *    guard rail along the back and the left, open on the right where you step in */
static void well_a(Canvas *c, int cx, int fy, const Pal *p) {
    int W = 34, H = 12, x0 = cx - W / 2, top = fy - H;
    rect(c, x0 - 2, top - 1, W + 4, H + 3, p->rim); rect(c, x0 - 1, top, W + 2, H + 1, p->rim_lt);
    hrect(c, x0, top + 1, W, H - 1, p->dark);
    hrect(c, x0, top + 1, W, 3, dk(p->riser, 0.5)); /* the back wall going down */
    int n = 5, sw = W / n;
    for (int i = 0; i < n; i++) { /* step 0 at the right, at floor level */
        int sx = x0 + W - (i + 1) * sw, ty = top + 3 + i * 2;
        double k = 0.15 * i;
        hrect(c, sx, ty, sw, 2, dk(p->tread, k));
        hrect(c, sx, ty + 2, sw, fy - ty - 2, dk(p->riser, 0.25 + k));
        hrect(c, sx + sw - 1, ty, 1, fy - ty, dk(p->riser, 0.45 + k)); /* the riser up to the step before */
    }
    hrect(c, x0, top + 1, 1, H - 1, dk(p->riser, 0.7)); /* the left wall in shadow */
    for (int x = x0 - 1; x <= x0 + W - 6; x += 6) rect(c, x, top - 6, 1, 6, p->rail); /* posts along the back */
    rect(c, x0 - 1, top - 7, W - 4, 1, p->rail);
    for (int y = top + 2; y < fy; y += 4) rect(c, x0 - 2, y - 6, 1, 6, p->rail); /* and the left side */
    rect(c, x0 - 2, top - 7, 1, H + 2, p->rail);
    rect(c, x0 - 2, fy - 1, W + 4, 2, dk(p->rim, 0.2)); /* the front lip (the floor's thickness) */
}
/* B  an open hatch: a wooden trapdoor standing up behind the opening, a ladder of steps going down */
static void well_b(Canvas *c, int cx, int fy, const Pal *p) {
    int W = 22, H = 9, x0 = cx - W / 2, top = fy - H;
    rect(c, x0 - 1, top - 1, W + 2, H + 2, p->rim);
    hrect(c, x0, top, W, H - 1, p->dark);
    for (int i = 0; i < 4; i++) hrect(c, x0 + 3, top + 1 + i * 2, W - 6, 1, dk(p->tread, 0.12 * i)); /* the steps, going away down */
    hrect(c, x0 + 2, top, 1, H - 1, dk(p->riser, 0.3)); hrect(c, x0 + W - 3, top, 1, H - 1, dk(p->riser, 0.3)); /* stringers */
    /* the lid, up and leaning back a little: planks, a dark edge, an iron ring */
    for (int y = 0; y < 13; y++) {
        int sh = y / 4, yy = top - 2 - y;
        rect(c, x0 - 1 + sh, yy, W + 2, 1, (y % 4 == 0) ? dk(p->rim, 0.35) : p->rim_lt);
        rect(c, x0 - 1 + sh, yy, 1, 1, dk(p->rim, 0.4)); rect(c, x0 + W + sh, yy, 1, 1, dk(p->rim, 0.4));
    }
    for (int x = 4; x < W; x += 5) for (int y = 1; y < 12; y++) canvas_px(c, x0 + x + (y + 2) / 4, top - 2 - y, dk(p->rim_lt, 0.25));
    rect(c, x0 + W / 2, top - 8, 3, 1, RGB(60, 60, 66)); rect(c, x0 + W / 2, top - 6, 3, 1, RGB(60, 60, 66)); canvas_px(c, x0 + W / 2, top - 7, RGB(60, 60, 66)); canvas_px(c, x0 + W / 2 + 2, top - 7, RGB(60, 60, 66));
    rect(c, x0 - 1, fy - 1, W + 2, 1, dk(p->rim, 0.2));
}
/* C  stairs going down out of the picture: the floor cut away at the front, steps going down towards you, a rail each side */
static void well_c(Canvas *c, int cx, int fy, const Pal *p) {
    int W = 26, x0 = cx - W / 2, top = fy - 8, bot = c->h;
    hrect(c, x0, top, W, bot - top, p->dark);
    for (int i = 0; i < 6; i++) { int ty = top + i * 3; double k = 0.08 * i; hrect(c, x0 + 1, ty, W - 2, 2, dk(p->tread, k)); hrect(c, x0 + 1, ty + 2, W - 2, 1, dk(p->riser, 0.3 + k)); }
    hrect(c, x0, top, 2, bot - top, dk(p->riser, 0.5)); hrect(c, x0 + W - 2, top, 2, bot - top, dk(p->riser, 0.35)); /* the cut sides */
    rect(c, x0 - 1, top - 1, W + 2, 1, p->rim_lt); rect(c, x0 - 1, top - 1, 1, bot - top + 1, p->rim); rect(c, x0 + W, top - 1, 1, bot - top + 1, p->rim);
    for (int s = 0; s < 2; s++) { int rx = s ? x0 + W : x0 - 1; for (int y = top - 1; y < bot; y += 4) rect(c, rx, y - 6, 1, 6, p->rail); rect(c, rx, top - 7, 1, 1, p->rail); for (int y = top - 7; y < bot - 6; y++) canvas_px(c, rx, y, p->rail); }
}
/* D  a spiral stair: a round hole, a post in the middle, wedge steps winding down, a round rail on three quarters */
static void well_d(Canvas *c, int cx, int fy, const Pal *p) {
    double rx = 15, ry = 6, cy = fy - ry - 1;
    for (int y = (int)(cy - ry - 2); y <= (int)(cy + ry + 2); y++) for (int x = (int)(cx - rx - 2); x <= (int)(cx + rx + 2); x++) {
        double u = (x - cx) / rx, v = (y - cy) / ry, d = u * u + v * v;
        if (d <= 1) {
            double a = atan2(v, u), k = fmod(a + M_PI + 10 * M_PI, 2 * M_PI) / (2 * M_PI); /* 0..1 round the hole: how deep */
            int step = (int)(k * 8);
            Rgb col = dk(p->tread, 0.08 * step);
            if (fmod(k * 8, 1.0) < 0.18) col = dk(p->riser, 0.3 + 0.06 * step);
            if (d < 0.08) col = p->rail;           /* the post */
            if (d > 0.82 && v < 0) col = dk(p->riser, 0.6); /* the back of the hole */
            hole(c, x, y, col);
        } else if (d <= 1.35) canvas_px(c, x, y, d < 1.15 ? p->rim_lt : p->rim);
    }
    for (int i = 0; i <= 12; i++) { /* the rail: round the back and the left, open at the front right */
        double a = M_PI * (0.25 + 1.5 * i / 12.0) + M_PI / 2;
        int x = (int)lround(cx + cos(a) * (rx + 1)), y = (int)lround(cy + sin(a) * (ry + 1));
        rect(c, x, y - 6, 1, 6, p->rail);
        if (i < 12) { double b = M_PI * (0.25 + 1.5 * (i + 1) / 12.0) + M_PI / 2; canvas_line(c, x, y - 6, (int)lround(cx + cos(b) * (rx + 1)), (int)lround(cy + sin(b) * (ry + 1)) - 6, p->rail); }
    }
}
typedef void (*WellFn)(Canvas *, int, int, const Pal *);
int main(void) {
    static const char *TH[3] = {"coffee", "office", "park"};
    static const Pal PAL[3] = {
        {{96, 70, 50}, {150, 112, 80}, {206, 182, 150}, {150, 126, 102}, {84, 56, 40}, {26, 20, 18}},
        {{92, 98, 112}, {170, 176, 190}, {206, 210, 218}, {130, 136, 150}, {70, 76, 90}, {22, 24, 30}},
        {{112, 112, 120}, {176, 176, 182}, {190, 190, 196}, {132, 132, 140}, {48, 52, 70}, {22, 22, 26}},
    };
    static const WellFn FN[4] = {well_a, well_b, well_c, well_d};
    static const char *NAME[5] = {"NOW", "A  RAILED WELL", "B  OPEN HATCH", "C  STAIRS OUT FRONT", "D  SPIRAL"};
    int cw = 64, chh = 44, Z = 3;
    Canvas sheet = canvas_new(70 + 3 * (cw * Z + 6) + 4 * 2, 5 * (chh * Z + 6) + 14, RGB(24, 24, 30));
    for (int t = 0; t < 3; t++) canvas_text(&sheet, 70 + t * (cw * Z + 6), 3, TH[t][0] == 'c' ? "COFFEE SHOP" : TH[t][0] == 'o' ? "OFFICE" : "PARK", RGB(220, 220, 235));
    for (int o = 0; o < 5; o++) {
        canvas_text(&sheet, 2, 14 + o * (chh * Z + 6) + chh * Z / 2 - 3, NAME[o] + (o ? 3 : 0), RGB(220, 220, 235));
        if (o) { char k[2] = {NAME[o][0], 0}; canvas_text(&sheet, 2, 14 + o * (chh * Z + 6) + chh * Z / 2 - 12, k, C_YELLOW); }
        for (int t = 0; t < 3; t++) {
            FILE *f = fopen("/tmp/clawd-mock-wm.fx", "w");
            fprintf(f, "theme %s\ntitle %s\nterminal 150 40\ntime 1000.0 3.4\nscale 1\n%s", TH[t], TH[t], o == 0 ? "neighbor down park\n" : "");
            fclose(f);
            static Fixture fx; fixture_load(&fx, "/tmp/clawd-mock-wm.fx");
            fx.room.freeze = 0;
            char note[64];
            Canvas c = render(fx.cols, fx.rows, &fx.room, fx.now, fx.t, note, sizeof note);
            double fy, wx, sx0, sx1;
            well_place(&fx.room.geo.view, &fy, &wx, &sx0, &sx1);
            CW = c.w; memset(HOLE, 0, sizeof HOLE);
            int cx = (int)wx + 6, y0 = (int)fy;
            if (o) FN[o - 1](&c, cx, y0, &PAL[t]);
            if (o) { /* a Clawd halfway down: only what is above the floor or inside the hole shows */
                Canvas k = canvas_new(c.w, c.h, RGB(255, 0, 255));
                Pose ps; memset(&ps, 0, sizeof ps); ps.eyes = "open";
                int kx = o == 3 ? cx : cx - 2, ky = o == 3 ? y0 - 4 : o == 2 ? y0 - 12 : y0 - 13;
                draw_clawd(&k, kx, ky, &ps, NULL, RGB(205, 110, 88), NULL, "");
                int lid = o == 2 ? y0 - 9 : o == 4 ? y0 - 13 : o == 1 ? y0 - 11 : y0 - 8; /* (the hole's top edge) */
                for (int i = 0; i < c.w * c.h; i++) {
                    if (rgb_eq(k.p[i], RGB(255, 0, 255))) continue;
                    int y = i / c.w;
                    if (HOLE[i] || y < lid) c.p[i] = k.p[i];
                }
                canvas_free(&k);
            }
            int x0 = (int)wx - cw / 2 + 6, ys = c.h - chh;
            Canvas z = canvas_new(cw, chh, RGB(24, 24, 30));
            for (int y = 0; y < chh; y++) for (int x = 0; x < cw; x++) if (x0 + x >= 0 && x0 + x < c.w && ys + y >= 0) z.p[y * cw + x] = c.p[(ys + y) * c.w + x0 + x];
            Canvas zs = canvas_scaled(&z, Z);
            canvas_blit(&sheet, &zs, 70 + t * (cw * Z + 6), 14 + o * (chh * Z + 6));
            canvas_free(&z); canvas_free(&zs); canvas_free(&c);
        }
    }
    png_write("out/08-well-redesign/well-options.png", &sheet, 2);
    return 0;
}
