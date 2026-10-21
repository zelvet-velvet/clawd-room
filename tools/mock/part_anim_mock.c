/* Mockup (tools/mock, build.sh): the pixel editor's stage page for part animations, beyond gates: a Clawd on the swing and the
 * barista's espresso machine brewing. A thing is made of parts; each part has keyframes (where, turned, shows, glow) and the engine
 * moves it in between. It is the current editor's timeline (play, onion skin) with a stage above it.
 * A: a dope sheet (a row per part, keyframes as diamonds); B: one track for the picked part, a bigger stage.
 *   part_anim_mock OUT_DIR   (writes part-anim-swing.png and part-anim-barista.png) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "art.h"
#include "fixture.h"
#include "theme.h"

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
#define BG RGB(18, 18, 24)
#define DIM RGB(110, 110, 130)
#define TXT RGB(200, 200, 215)
#define YEL RGB(250, 210, 70)
#define FLD RGB(34, 34, 44)
static void box(Canvas *c, int x, int y, int w, int h, Rgb k) { canvas_rect(c, x - 1, y - 1, w + 2, 1, k); canvas_rect(c, x - 1, y + h, w + 2, 1, k); canvas_rect(c, x - 1, y - 1, 1, h + 2, k); canvas_rect(c, x + w, y - 1, 1, h + 2, k); }
static void button(Canvas *c, int x, int y, const char *t, int on) { int w = text_width(t) + 6; canvas_rect(c, x, y, w, 9, on ? RGB(90, 90, 120) : FLD); if (on) box(c, x, y, w, 9, YEL); canvas_text(c, x + 3, y + 2, t, on ? RGB(235, 235, 245) : RGB(150, 155, 185)); }
static int bw(const char *t) { return text_width(t) + 6; }
static void diamond(Canvas *c, int x, int y, Rgb k) { canvas_px(c, x, y - 2, k); canvas_rect(c, x - 1, y - 1, 3, 1, k); canvas_rect(c, x - 2, y, 5, 1, k); canvas_rect(c, x - 1, y + 1, 3, 1, k); canvas_px(c, x, y + 2, k); }

typedef struct { int x, y, w, h; const char *name; } Part; /* a part's box on the stage (room pixels) */
typedef struct {
    const char *title, *fixture, *seat;                  /* the fixture (and whether to seat the Clawds first) */
    int cols, rows, steps;
    int ax, ay, aw, ah;                                  /* A: the crop (1x) */
    int bx, by, bw_, bh;                                 /* B: the crop (2x) */
    const Part *parts; int nparts, picked;               /* the parts outlined; the picked one */
    const char *things[4]; int thing;                    /* the buttons of what is edited */
    const char *rows_[6]; int nrows; const double keys[6][5]; /* the dope sheet rows (the picked part is `picked`) and their keyframes */
    const char *pname, *pgroup; const char *K[5], *V[5]; /* the picked part's values at this frame */
    int py_dot, px_dot, ndots;                           /* the picked part's path, dotted from its box: step in x and y, count */
    const char *len, *frame; int loop;
} Case;

static Canvas stage(const Case *cs) {
    FILE *f = fopen("/tmp/clawd-mock-part-anim.fx", "w");
    fputs(cs->fixture, f);
    fclose(f);
    static Fixture fx;
    fixture_load(&fx, "/tmp/clawd-mock-part-anim.fx");
    unlink("/tmp/clawd-mock-part-anim.fx");
    if (cs->seat) { for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; } fx.room.freeze = 0; }
    char note[64];
    Canvas c = {0};
    for (int k = 0; k < cs->steps; k++) { if (c.p) canvas_free(&c); c = render(fx.cols, fx.rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note); }
    return c;
}
static void outlines(Canvas *c, const Case *cs) { /* every part in a quiet outline, the picked one yellow with its path dotted */
    for (int i = 0; i < cs->nparts; i++) {
        const Part *p = &cs->parts[i];
        box(c, p->x, p->y, p->w, p->h, i == cs->picked ? YEL : RGB(95, 105, 135));
    }
    const Part *p = &cs->parts[cs->picked];
    for (int s = 1; s <= cs->ndots; s++) canvas_px(c, p->x + p->w / 2 + s * cs->px_dot, p->y + (cs->py_dot < 0 ? 0 : p->h) + s * cs->py_dot, YEL);
}
static Canvas crop(const Canvas *st, int x, int y, int w, int h) {
    Canvas c = canvas_new(w, h, BG);
    for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) if (x + xx >= 0 && x + xx < st->w && y + yy >= 0 && y + yy < st->h) c.p[yy * w + xx] = st->p[(y + yy) * st->w + x + xx];
    return c;
}
static int title(Canvas *c, const Case *cs, const char *opt) {
    canvas_text(c, 4, 3, cs->title, YEL);
    canvas_text(c, c->w - text_width(opt) - 4, 3, opt, DIM);
    return 12;
}
static void panel(Canvas *c, const Case *cs, int x, int y) {
    canvas_text(c, x, y, "EDIT", DIM); y += 7;
    int bx = x;
    for (int i = 0; i < 4 && cs->things[i]; i++) { if (i == 2) { bx = x; y += 11; } button(c, bx, y, cs->things[i], i == cs->thing); bx += bw(cs->things[i]) + 2; }
    y += 11; bx = x; button(c, bx, y, "PLAYS", 1); bx += bw("PLAYS") + 2; button(c, bx, y, cs->loop ? "LOOP" : "ONCE", 0); y += 13;
    canvas_text(c, x, y, "PART", DIM); y += 7;
    canvas_text(c, x, y, cs->pname, TXT); canvas_text(c, x + 46, y, cs->pgroup, DIM); y += 9;
    for (int i = 0; i < 5; i++) {
        canvas_text(c, x + 3, y, cs->K[i], RGB(170, 170, 190));
        int vx = x + 70 - text_width(cs->V[i]);
        if (i == 1) { canvas_rect(c, x + 44, y - 1, 30, 7, RGB(60, 60, 80)); box(c, x + 44, y - 1, 30, 7, YEL); canvas_text(c, x + 46, y, "<", YEL); canvas_text(c, x + 70, y, ">", YEL); vx -= 6; }
        canvas_text(c, vx, y, cs->V[i], i == 1 ? YEL : RGB(230, 200, 120));
        y += 8;
    }
    y += 3;
    button(c, x, y, "+ KEY", 0); button(c, x + bw("+ KEY") + 2, y, "- KEY", 0); button(c, x + bw("+ KEY") + bw("- KEY") + 4, y, "EASE", 0); y += 13;
    canvas_text(c, x, y, "CLICK A PART ON THE STAGE", DIM);
}
static void ruler(Canvas *c, int x, int y, int w, double at, int frames) {
    canvas_rect(c, x, y + 6, w, 1, RGB(60, 60, 76));
    for (int f = 0; f <= frames; f++) {
        int fx = x + f * (w - 1) / frames;
        canvas_rect(c, fx, y + (f % 4 == 0 ? 3 : 5), 1, f % 4 == 0 ? 3 : 1, RGB(90, 90, 110));
        if (f % 8 == 0) { char t[8]; snprintf(t, sizeof t, "%.1f", f / 16.0); canvas_text(c, fx - text_width(t) / 2, y - 3, t, DIM); }
    }
    canvas_rect(c, x + (int)lround(at * (w - 1)), y + 2, 1, 6, YEL);
}
static void transport(Canvas *c, const Case *cs, int x, int y, int tight) {
    int bx = x; button(c, bx, y, "PLAY", 0); bx += bw("PLAY") + 2; button(c, bx, y, "1X", 0); bx += bw("1X") + 2; button(c, bx, y, "ONION", 0); bx += bw("ONION") + 2;
    char s[96]; snprintf(s, sizeof s, "%s   %s", cs->frame, cs->len);
    canvas_text(c, bx + 4, y + 2, s, TXT);
}
static void dope_sheet(Canvas *c, const Case *cs, int y) {
    int lx = 4, tx = 56, tw = c->w - tx - 6, frames = 26;
    double at = 0.55;
    canvas_rect(c, 0, y - 3, c->w, 1, RGB(44, 44, 56));
    transport(c, cs, lx, y, 0);
    y += 16;
    ruler(c, tx, y, tw, at, frames);
    y += 10;
    int y0 = y;
    for (int r = 0; r < cs->nrows; r++) {
        int on = r == cs->picked;
        if (on) canvas_rect(c, 0, y - 1, c->w, 8, RGB(32, 32, 44));
        canvas_text(c, lx, y, cs->rows_[r], on ? YEL : RGB(170, 170, 190));
        canvas_rect(c, tx, y + 3, tw, 1, RGB(40, 40, 52));
        double k0 = cs->keys[r][0];
        for (int k = 0; k < 5 && cs->keys[r][k] >= 0; k++) {
            int kx = tx + (int)lround(cs->keys[r][k] * (tw - 1));
            if (k > 0) canvas_rect(c, tx + (int)lround(k0 * (tw - 1)), y + 3, kx - (tx + (int)lround(k0 * (tw - 1))), 1, on ? RGB(150, 130, 60) : RGB(80, 80, 100));
            diamond(c, kx, y + 3, on ? YEL : RGB(200, 200, 215));
            k0 = cs->keys[r][k];
        }
        y += 9;
    }
    canvas_rect(c, tx + (int)lround(at * (tw - 1)), y0 - 1, 1, 9 * cs->nrows, YEL);
}
static void one_track(Canvas *c, const Case *cs, int y) {
    int tx = 4, tw = c->w - 8;
    canvas_rect(c, 0, y - 3, c->w, 1, RGB(44, 44, 56));
    transport(c, cs, tx, y, 1);
    y += 16;
    ruler(c, tx, y, tw, 0.55, 26);
    y += 10;
    canvas_rect(c, tx, y + 3, tw, 1, RGB(40, 40, 52));
    const double *K = cs->keys[cs->picked];
    canvas_rect(c, tx + (int)lround(K[0] * (tw - 1)), y + 3, (int)lround((K[1] - K[0]) * (tw - 1)), 1, RGB(150, 130, 60));
    for (int k = 0; k < 5 && K[k] >= 0; k++) diamond(c, tx + (int)lround(K[k] * (tw - 1)), y + 3, YEL);
}
static void make(const Case *cs, const char *path) {
    int W = 330, H = 222;
    Canvas st = stage(cs);
    outlines(&st, cs);
    Canvas a = canvas_new(W, H, BG);
    int y = title(&a, cs, "A  DOPE SHEET");
    Canvas ca = crop(&st, cs->ax, cs->ay, cs->aw, cs->ah);
    canvas_blit(&a, &ca, 4, y); box(&a, 4, y, ca.w, ca.h, RGB(90, 90, 110));
    panel(&a, cs, 4 + ca.w + 10, y);
    dope_sheet(&a, cs, y + (ca.h > 128 ? ca.h : 128) + 8);
    Canvas b = canvas_new(W, H, BG);
    y = title(&b, cs, "B  ONE TRACK");
    Canvas cb = crop(&st, cs->bx, cs->by, cs->bw_, cs->bh), cbs = canvas_scaled(&cb, 2);
    canvas_blit(&b, &cbs, 4, y); box(&b, 4, y, cbs.w, cbs.h, RGB(90, 90, 110));
    panel(&b, cs, 4 + cbs.w + 10, y);
    one_track(&b, cs, y + (cbs.h > 128 ? cbs.h : 128) + 8);
    Canvas sheet = canvas_new(2 * W + 12, H + 4, RGB(24, 24, 30));
    canvas_blit(&sheet, &a, 4, 2); canvas_blit(&sheet, &b, W + 8, 2);
    png_write(path, &sheet, 3);
    fprintf(stderr, "wrote %s\n", path);
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "out/11-part-animations";
    char p[400];
    /* the park's swing: a Clawd on it swinging towards us and back */
    static const Part swing_parts[] = {{27, 60, 31, 3, "BAR"}, {36, 63, 1, 13, "CHAIN L"}, {48, 63, 1, 13, "CHAIN R"}, {35, 75, 16, 2, "SEAT"}, {51, 61, 10, 17, "CLAWD"}};
    static const Case swing = {
        "EDIT PLAY SWING", "theme park\ntitle swing\nterminal 160 50\ntime 1000.0 3.4\nscale 1\nsession s00 name=api created=900 state=idle tool=Bash px=10 py=39 emote=swing@990\n", "seat", 160, 50, 200,
        10, 40, 150, 60, 20, 44, 52, 42,
        swing_parts, 5, 3,
        {"GATE", "SWING", "BARISTA", "DOOR"}, 1,
        {"CHAINS", "BAR", "CLAWD", "SEAT"}, 4, {{0, 0.5, 1, -1}, {0, 1, -1}, {0, 0.25, 0.5, 0.75, 1}, {0, 0.25, 0.5, 0.75, 1}},
        "SEAT", "IN SWING", {"MOVE X", "MOVE Y", "TURN", "SHOW", "GLOW"}, {"0", "2", "-8", "YES", "0"},
        1, 0, 3, "LENGTH 1.6 S", "FRAME 14 OF 26   0.88 S", 1};
    make(&swing, (snprintf(p, sizeof p, "%s/part-anim-swing.png", dir), p));
    /* the coffee shop: the barista at the espresso machine, the cup rising, steam */
    static const Part brew_parts[] = {{42, 36, 10, 8, "MACHINE"}, {44, 40, 4, 3, "CUP"}, {44, 31, 6, 4, "STEAM"}, {27, 32, 11, 11, "CLAWD"}};
    static const Case brew = {
        "EDIT PLAY BARISTA", "theme coffee\ntitle brew\nterminal 120 40\ntime 1000.0 3.4\nscale 1\nsession s00 name=api created=900 state=work tool=Bash hold=1000.4\nsession s01 name=web created=910 state=think tool=Edit hold=1000.4\n", "seat", 120, 40, 40,
        5, 20, 110, 60, 15, 24, 52, 42,
        brew_parts, 4, 1,
        {"GATE", "SWING", "BARISTA", "DOOR"}, 2,
        {"MACHINE", "CUP", "STEAM", "CLAWD"}, 4, {{0, 0.4, 0.8, -1}, {0, 0.3, 0.6, 0.85, -1}, {0.25, 0.5, 0.75, 1, -1}, {0, 0.2, 0.55, 0.85, -1}},
        "CUP", "IN BREWING", {"MOVE X", "MOVE Y", "TURN", "SHOW", "GLOW"}, {"0", "-5", "0", "YES", "20"},
        -1, 0, 4, "LENGTH 1.7 S", "FRAME 9 OF 27   0.56 S", 0};
    make(&brew, (snprintf(p, sizeof p, "%s/part-anim-barista.png", dir), p));
    return 0;
}
