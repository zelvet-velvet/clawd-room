/* Mockup (tools/mock, build.sh): how a rotated part could look. The swing (a bar, two chains, a seat, a Clawd on it) turned about
 * the bar by -20 .. +20 degrees, three ways:
 *   A  shapes turn, the Clawd leans: the chains and the seat are lines and rectangles, so they turn exactly; the Clawd, a drawn
 *      sprite, uses its lean pose (one step to the side) instead of being turned;
 *   B  everything turns, pixel by pixel: the Clawd's pixels are turned too (nearest pixel), so its edges break up;
 *   C  everything turns, the Clawd by tilt frames: rows of the sprite slide sideways (as a hand-drawn tilt frame would be).
 *   rotate_mock OUT_DIR SPRITES_DIR   (writes rotate.png) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "art.h"
#include "clawd.h"
#include "sprites.h"
#include "theme.h"

#define CW 46
#define CH 44
#define KEYC RGB(255, 0, 255)
#define GRASS RGB(112, 178, 98)
#define GRASS2 RGB(100, 164, 88)

static Rgb tintc(Rgb c, double f) { return RGB((int)(c.r * f), (int)(c.g * f), (int)(c.b * f)); }
static void seat_line(Canvas *c, double x0, double y0, double x1, double y1, Rgb k) { canvas_line(c, (int)lround(x0), (int)lround(y0), (int)lround(x1), (int)lround(y1), k); }

/* the Clawd on its own (14 x 12 or so), upright, on a key-coloured ground */
static Canvas clawd_upright(const SpriteFile *frames, const Theme *th, int lean) {
    Canvas c = canvas_new(24, 22, KEYC);
    Pose p = {0, lean, {1, 1}, 1, "happy", {0, 0}};
    draw_clawd(&c, 12, 5, &p, frames, C_CLAWD, th, "");
    return c;
}
/* nearest-pixel turn about the middle */
static void blit_turned(Canvas *dst, const Canvas *src, double cx, double cy, double deg, int shear) {
    double a = deg * M_PI / 180, ca = cos(a), sa = sin(a);
    for (int y = -20; y < 20; y++) for (int x = -20; x < 20; x++) {
        double sx, sy;
        if (shear) { /* tilt frame: a row slides sideways in proportion to its height above the feet */
            sy = y + src->h / 2.0; sx = x + src->w / 2.0 - (-(y) * sa * 0.9) * 1.0; sx = x + src->w / 2.0 + y * sa * 0.9; sy = y + src->h / 2.0;
        } else { sx = x * ca + y * sa + src->w / 2.0; sy = -x * sa + y * ca + src->h / 2.0; }
        int ix = (int)floor(sx), iy = (int)floor(sy);
        if (ix < 0 || iy < 0 || ix >= src->w || iy >= src->h) continue;
        Rgb k = src->p[iy * src->w + ix];
        if (k.r == 255 && k.g == 0 && k.b == 255) continue;
        canvas_px(dst, (int)lround(cx + x), (int)lround(cy + y), k);
    }
}
static Canvas cell(const SpriteFile *frames, const Theme *th, int option, double deg) {
    Canvas c = canvas_new(CW, CH, GRASS);
    for (int y = 0; y < CH; y += 6) canvas_rect(&c, 0, y, CW, 3, GRASS2);
    double a = deg * M_PI / 180, px = CW / 2.0, py = 5, L = 24;
    Rgb post = RGB(120, 84, 56), bar = RGB(92, 64, 44), chain = RGB(164, 164, 176), seat = RGB(196, 76, 62);
    canvas_line(&c, (int)px - 13, (int)py + 1, (int)px - 16, CH - 2, post); canvas_line(&c, (int)px + 13, (int)py + 1, (int)px + 16, CH - 2, post);
    canvas_rect(&c, (int)px - 14, (int)py - 1, 29, 2, bar);
    double sx = px + L * sin(a), sy = py + L * cos(a); /* (the seat's middle: the chains turn about the bar) */
    double ux = cos(a), uy = -sin(a);                   /* (along the seat) */
    for (int sd = -1; sd <= 1; sd += 2) seat_line(&c, px + sd * 6, py + 1, sx + sd * 6 * ux, sy + sd * 6 * uy, chain);
    for (int t = 0; t < 2; t++) seat_line(&c, sx - 7 * ux, sy - 7 * uy + t, sx + 7 * ux, sy + 7 * uy + t, t ? tintc(seat, 0.7) : seat);
    int lean = option == 0 ? (deg > 7 ? 1 : deg < -7 ? -1 : 0) : 0;
    Canvas cl = clawd_upright(frames, th, lean);
    if (option == 0) blit_turned(&c, &cl, sx, sy - 5, 0, 0);               /* A: upright, leaning one step */
    else if (option == 1) blit_turned(&c, &cl, sx, sy - 5, deg, 0);        /* B: turned pixel by pixel */
    else blit_turned(&c, &cl, sx, sy - 5, deg, 1);                         /* C: a tilt frame (rows slid) */
    canvas_free(&cl);
    return c;
}
int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/19-rotate", *dir = argc > 2 ? argv[2] : "sprites";
    char p[700];
    snprintf(p, sizeof p, "%s/clawd.txt", dir);
    SpriteFile frames = sprites_load(p);
    const Theme *th = theme_get("park");
    static const double ANG[5] = {-20, -10, 0, 10, 20};
    static const char *const NAME[3] = {"A  SHAPES TURN, THE CLAWD LEANS", "B  EVERYTHING TURNS, PIXEL BY PIXEL", "C  EVERYTHING TURNS, THE CLAWD BY A TILT FRAME"};
    int lw = 6, W = lw + 5 * (CW + 4), H = 3 * (CH + 20) + 8;
    Canvas sheet = canvas_new(W, H, RGB(18, 18, 24));
    for (int r = 0; r < 3; r++) {
        int y = 4 + r * (CH + 20);
        canvas_text(&sheet, lw, y, NAME[r], C_YELLOW);
        for (int i = 0; i < 5; i++) {
            Canvas c = cell(&frames, th, r, ANG[i]);
            int x = lw + i * (CW + 4);
            canvas_blit(&sheet, &c, x, y + 8);
            canvas_free(&c);
            char t[8]; snprintf(t, sizeof t, "%+d", (int)ANG[i]);
            canvas_text(&sheet, x + (CW - text_width(t)) / 2, y + 8 + CH + 1, ANG[i] == 0 ? "0" : t, C_GREY);
        }
    }
    snprintf(p, sizeof p, "%s/rotate.png", o);
    png_write(p, &sheet, 3);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
