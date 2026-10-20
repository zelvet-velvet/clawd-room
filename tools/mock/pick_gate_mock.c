/* Mockups (tools/mock, built and run by build.sh): 1) how the Clawd picked with Tab could stand out, 2) how gates (doors, stairs, the
 * well) could appear. Real rooms from the room's own renderer; the designs are drawn over them here.
 *   build and run: tools/mock/build.sh (writes gallery/mock/out/) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "art.h"
#include "fixture.h"
#include "theme.h"
static double hash(int x, int y);
static int TREE_FADE;
static double ease_out(double x);

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
#define OUT "out"
#define BG RGB(24, 24, 30)
#define YEL RGB(255, 214, 90)

static Rgb mix(Rgb a, Rgb b, double t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return RGB((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t)); }
static Rgb px(const Canvas *c, int x, int y) { return c->p[y * c->w + x]; }
static int in(const Canvas *c, int x, int y) { return x >= 0 && y >= 0 && x < c->w && y < c->h; }
static Canvas copy(const Canvas *c) { Canvas d = canvas_new(c->w, c->h, BG); memcpy(d.p, c->p, sizeof(Rgb) * (size_t)(c->w * c->h)); return d; }
static Canvas crop(const Canvas *c, int x0, int y0, int w, int h) {
    Canvas d = canvas_new(w, h, BG);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (in(c, x0 + x, y0 + y)) d.p[y * w + x] = px(c, x0 + x, y0 + y);
    return d;
}

static Fixture *load(const char *body) {
    char path[256];
    snprintf(path, sizeof path, "/tmp/clawd-mock-pgm-%d.fx", (int)getpid());
    FILE *f = fopen(path, "w");
    fputs(body, f); fclose(f);
    static Fixture fx;
    if (fixture_load(&fx, path)) { fprintf(stderr, "bad fixture\n"); exit(1); }
    unlink(path);
    return &fx;
}
static Canvas frame_at(Fixture *fx, double dt) { char note[64]; return render(fx->cols, fx->rows, &fx->room, fx->now + dt, fx->t + dt, note, sizeof note); }

/* ================= 1. the picked Clawd ================= */
/* the picked Clawd's own pixels: the frame with it minus the frame without it, near where it stands */
static void silhouette(const Canvas *with, const Canvas *without, const Session *s, unsigned char *m) {
    memset(m, 0, (size_t)(with->w * with->h));
    for (int y = (int)s->py - 2; y < (int)s->py + 11; y++) for (int x = (int)s->px - 8; x <= (int)s->px + 8; x++)
        if (in(with, x, y) && !rgb_eq(px(with, x, y), px(without, x, y))) m[y * with->w + x] = 1;
}
static void style_outline(Canvas *c, const unsigned char *m, Rgb col) {
    Canvas o = copy(c);
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) if (!m[y * c->w + x]) {
        int n = 0;
        for (int d = 0; d < 4; d++) { int xx = x + (d == 0) - (d == 1), yy = y + (d == 2) - (d == 3); if (in(c, xx, yy) && m[yy * c->w + xx]) n = 1; }
        if (n) o.p[y * c->w + x] = col;
    }
    memcpy(c->p, o.p, sizeof(Rgb) * (size_t)(c->w * c->h)); canvas_free(&o);
}
static void top_of(const unsigned char *m, int w, int h, int *x0, int *x1, int *y0, int *y1) {
    *x0 = w; *x1 = -1; *y0 = h; *y1 = -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (m[y * w + x]) { if (x < *x0) *x0 = x; if (x > *x1) *x1 = x; if (y < *y0) *y0 = y; if (y > *y1) *y1 = y; }
}
static void style_arrow(Canvas *c, const Session *s, double t) { /* a bobbing yellow arrow over the head (above the state dot) */
    int bob = (int)(fmod(t, 0.8) < 0.4), cx = (int)s->px, y = (int)s->py - 13 + bob;
    for (int r = 0; r < 3; r++) canvas_rect(c, cx - 2 + r, y + r, 5 - 2 * r, 1, YEL);
    canvas_rect(c, cx - 3, y - 1, 7, 1, RGB(120, 90, 30));
}
static void style_ring(Canvas *c, const Session *s, int x0, int x1, int y1, double t) { /* a ring on the floor around its feet, blinking softly */
    double k = 0.65 + 0.35 * sin(t * 6);
    int cx = (x0 + x1) / 2, rx = (x1 - x0) / 2 + 3, fy = y1 + 1;
    (void)s;
    for (int x = cx - rx; x <= cx + rx; x++) {
        double u = (double)(x - cx) / rx, d = sqrt(fmax(0, 1 - u * u)) * 2;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            int y = fy + (int)lround(sgn * d);
            if (!in(c, x, y)) continue;
            if (sgn < 0 && y <= y1) continue; /* (the back half is behind the Clawd) */
            c->p[y * c->w + x] = mix(px(c, x, y), YEL, k);
        }
    }
}
static void style_brackets(Canvas *c, int x0, int x1, int y0, int y1) { /* camera-viewfinder corners */
    int a = x0 - 2, b = x1 + 2, u = y0 - 2, d = y1 + 2;
    canvas_rect(c, a, u, 3, 1, YEL); canvas_rect(c, a, u, 1, 3, YEL);
    canvas_rect(c, b - 2, u, 3, 1, YEL); canvas_rect(c, b, u, 1, 3, YEL);
    canvas_rect(c, a, d, 3, 1, YEL); canvas_rect(c, a, d - 2, 1, 3, YEL);
    canvas_rect(c, b - 2, d, 3, 1, YEL); canvas_rect(c, b, d - 2, 1, 3, YEL);
}
static void style_spot(Canvas *c, int x0, int x1, int y0, int y1) { /* everything else a little darker: a spotlight on it */
    double cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0, r = 13;
    for (int y = 0; y < c->h - 10; y++) for (int x = 0; x < c->w; x++) {
        double d = hypot((x - cx) / 1.0, (y - cy) * 1.0);
        double k = d < r ? 0 : d < r + 6 ? (d - r) / 6 * 0.45 : 0.45;
        c->p[y * c->w + x] = mix(px(c, x, y), RGB(0, 0, 0), k);
    }
}

static void pick_sheet(void) {
    Fixture *fx = load("theme office\ntitle office\nterminal 150 44\ntime 1000.0 3.4\n"
                       "session s00 name=clawd-room created=900 state=work tool=Bash\n"
                       "session s01 name=park created=910 state=think tool=Edit hold=1000.4\n"
                       "session s02 name=api created=920 state=wait tool=mcp__x__y\n"
                       "session s03 name=web created=930 state=idle tool=Read\n"
                       "session s04 name=docs created=940 state=work tool=Foo\n");
    for (int i = 0; i < fx->room.npool; i++) { fx->room.pool[i].has_pos = 0; fx->room.pool[i].created = fx->now - 100; fx->room.pool[i].last = fx->now - 1; }
    fx->room.freeze = 0;
    for (int k = 0; k < 40; k++) { Canvas c = frame_at(fx, k * 0.0625); canvas_free(&c); }
    fx->room.freeze = 1;
    double T = 40 * 0.0625;
    const char *NAMES[] = {"NOW: ONLY THE HINT TEXT", "A  OUTLINE", "B  ARROW OVER THE HEAD", "C  RING ON THE FLOOR", "D  CORNER BRACKETS", "E  SPOTLIGHT + OUTLINE"};
    int NS = 6;
    for (int pick = 0; pick < 2; pick++) { /* a seated Clawd and one standing (the idle one) */
        Session *s = fx->room.s[pick == 0 ? 1 : 3];
        Canvas with = frame_at(fx, T);
        int keep = s->has_away; s->has_away = 1;
        Canvas without = frame_at(fx, T);
        s->has_away = keep;
        unsigned char *m = malloc((size_t)(with.w * with.h));
        silhouette(&with, &without, s, m);
        int x0, x1, y0, y1;
        top_of(m, with.w, with.h, &x0, &x1, &y0, &y1);
        int cw = with.w, chh = with.h, zw = 44, zh = 30, Z = 3;
        Canvas sheet = canvas_new(NS * (cw + 6) + 6, 9 + chh + 6 + zh * Z + 8, BG);
        for (int st = 0; st < NS; st++) {
            Canvas c = copy(&with);
            if (st == 1) style_outline(&c, m, RGB(255, 255, 255));
            if (st == 2) style_arrow(&c, s, 0);
            if (st == 3) style_ring(&c, s, x0, x1, y1, 0.3);
            if (st == 4) style_brackets(&c, x0, x1, y0, y1);
            if (st == 5) { style_spot(&c, x0, x1, y0, y1); style_outline(&c, m, YEL); }
            if (st > 0) { /* the bottom bar names the picked one */
                char bar[64]; snprintf(bar, sizeof bar, "PICKED %s", s->name);
                for (char *q = bar; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
                canvas_rect(&c, 0, c.h - 8, c.w, 8, RGB(20, 20, 26)); canvas_text(&c, 2, c.h - 7, bar, YEL);
            } else { canvas_rect(&c, 0, c.h - 8, c.w, 8, RGB(20, 20, 26)); canvas_text(&c, 2, c.h - 7, "M MOVE PARK", RGB(200, 200, 210)); }
            int sx = 6 + st * (cw + 6);
            canvas_text(&sheet, sx, 2, NAMES[st], RGB(220, 220, 235));
            canvas_blit(&sheet, &c, sx, 9);
            Canvas z = crop(&c, (x0 + x1) / 2 - zw / 2, (y0 + y1) / 2 - zh / 2, zw, zh), zs = canvas_scaled(&z, Z);
            canvas_blit(&sheet, &zs, sx, 9 + chh + 6);
            canvas_free(&z); canvas_free(&zs); canvas_free(&c);
        }
        char path[128]; snprintf(path, sizeof path, OUT "/01-pick-highlight/pick-%s.png", pick == 0 ? "seated" : "standing");
        png_write(path, &sheet, 3); fprintf(stderr, "wrote %s\n", path);
        canvas_free(&sheet); canvas_free(&with); canvas_free(&without); free(m);
    }
}

/* ================= 2. gates appearing ================= */
/* a gate before (p 0) and after (p 1) with the same layout: the gate's pixels are where they differ */
typedef struct { Canvas before, after, now[8]; unsigned char *m, *part[5], *tree; Canvas bld, notree; int x0, x1, y0, y1, floor_y; } Gate; /* part: steps, platform, opening, building, rail */
static const double NOWT[8] = {0, 0.1, 0.2, 0.3, 0.45, 0.6, 0.8, 1.0}; /* (parts of GATE_ANIM, 0.8 s) */
static void gate_get(const char *theme, int side, Gate *g) {
    char body[512];
    static const char *SN[] = {"left", "right", "up", "down"};
    snprintf(body, sizeof body, "theme %s\ntitle %s\nterminal 150 44\ntime 1000.0 3.4\nneighbor %s office\n", theme, theme, SN[side]);
    Fixture *fx = load(body);
    fx->room.freeze = 0;
    Canvas c = frame_at(fx, 0); canvas_free(&c);
    for (int i = 0; i < 8; i++) { /* the gate appears now: the program's own animation */
        fx->room.gate_seen[side] = 0; fx->room.has_gate_sync = 0;
        memset(&fx->room.anim, 0, sizeof fx->room.anim);
        Canvas tmp = frame_at(fx, 10); canvas_free(&tmp);
        g->now[i] = frame_at(fx, 10 + NOWT[i] * 0.8);
    }
    g->before = copy(&g->now[0]);
    g->after = copy(&g->now[7]);
    g->tree = NULL;
    extern int g_mock_no_tree;
    if (TREE_FADE && !strcmp(theme, "park") && side == SIDE_UP) { /* the big tree goes when the building comes: drawn again without it */
        g_mock_no_tree = 1;
        fx->room.gate_seen[side] = 0; fx->room.has_gate_sync = 0; memset(&fx->room.anim, 0, sizeof fx->room.anim);
        Canvas tmp = frame_at(fx, 10); canvas_free(&tmp);
        g->notree = frame_at(fx, 10);
        canvas_free(&g->after); g->after = frame_at(fx, 10 + 0.8);
        g_mock_no_tree = 0;
        int n = g->after.w * g->after.h;
        g->tree = calloc((size_t)n, 1);
        for (int i = 0; i < n; i++) if (!rgb_eq(g->before.p[i], g->notree.p[i])) g->tree[i] = 1;
    }
    int w = g->after.w, h = g->after.h;
    g->m = calloc((size_t)(w * h), 1);
    g->x0 = w; g->x1 = -1; g->y0 = h; g->y1 = -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!rgb_eq(px(g->tree ? &g->notree : &g->before, x, y), px(&g->after, x, y))) {
        g->m[y * w + x] = 1;
        if (x < g->x0) g->x0 = x; if (x > g->x1) g->x1 = x; if (y < g->y0) g->y0 = y; if (y > g->y1) g->y1 = y;
    }
    g->floor_y = (int)fx->room.geo.gate_out[side][1];
    for (int k = 0; k < 5; k++) { /* each part of the stairs on its own (where it shows in the finished frame) */
        g->part[k] = calloc((size_t)(w * h), 1);
        if (side != SIDE_UP) continue;
        Canvas d = copy(&g->before);
        if (k < 4) theme_gate(fx->room.theme, &d, &fx->room.geo, side, 1.0, 0, k); else theme_stair_rail(fx->room.theme, &d, &fx->room.geo, 1.0);
        for (int i = 0; i < w * h; i++) if (g->m[i] && !rgb_eq(d.p[i], g->before.p[i])) g->part[k][i] = 1;
        if (k == 3) g->bld = d; else canvas_free(&d);
    }
    for (int i = 0; i < w * h; i++) if (g->part[4][i]) for (int k = 0; k < 4; k++) g->part[k][i] = 0; /* (the rail in front wins) */
}
/* the stairs, one thing after another: (the park: the building rises behind,) the steps drop in one by one from the
 * foot up, the platform slides out of the right-hand wall, the railing's posts pop up from the foot, the opening's light
 * comes on (a flicker) */
static double seg(double t, double a, double b) { return t <= a ? 0 : t >= b ? 1 : (t - a) / (b - a); }
static void stairs_sequence(Canvas *c, const Gate *g, double t) {
    int w = c->w;
    int bx0 = w, bx1 = -1; /* each part's columns */
    static const int ORDER[4] = {3, 0, 1, 2};
    for (int oi = 0; oi < 4; oi++) { int k = ORDER[oi];
        int a = w, b = -1;
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->part[k][y * w + x]) { if (x < a) a = x; if (x > b) b = x; }
        if (k == 0) { bx0 = a; bx1 = b; }
        (void)bx0; (void)bx1;
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->part[k][y * w + x]) {
            Rgb col = px(&g->after, x, y);
            double u = b > a ? (double)(x - a) / (b - a) : 0;
            if (k == 3) col = px(&g->bld, x, y); /* (the whole building, also behind the steps) */
            if (k == 3) { double q = ease_out(seg(t, 0, 0.3)); int dy = (int)lround((g->y1 - g->y0) * (1 - q)); int yy = y + dy; if (q > 0 && yy <= g->y1) c->p[yy * w + x] = col; }
            if (k == 0) { double q = seg(t, 0.05 + u * 0.3, 0.15 + u * 0.3); if (q > 0) { int yy = y - (int)lround(3 * (1 - ease_out(q))); if (in(c, x, yy)) c->p[yy * w + x] = q < 1 ? mix(col, RGB(255, 255, 255), 0.35 * (1 - q)) : col; } }
            if (k == 1) { double q = ease_out(seg(t, 0.4, 0.65)); if ((1 - u) <= q) { int xx = x + (int)lround((b - a) * (1 - q)); if (xx <= b) c->p[y * w + xx] = col; } }
            if (k == 2) { double q = seg(t, 0.82, 1.0); double fl = q <= 0 ? 0 : q < 0.25 ? 1 : q < 0.4 ? 0.2 : q < 0.6 ? 1 : q < 0.7 ? 0.5 : 1; c->p[y * w + x] = mix(px(c, x, y), col, fl); }
        }
    }
    { /* the railing: post by post from the left, the hand rail joins them as they come */
        int a = w, b = -1;
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->part[4][y * w + x]) { if (x < a) a = x; if (x > b) b = x; }
        double q = seg(t, 0.6, 0.85);
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->part[4][y * w + x]) {
            double u = b > a ? (double)(x - a) / (b - a) : 0;
            if (u <= q) c->p[y * w + x] = px(&g->after, x, y);
        }
    }
    if (t > 0.05 && t < 0.4) { /* dust where the step just landed */
        int bb = g->y1;
        for (int i = 0; i < 5; i++) { int x = g->x0 + (int)((g->x1 - g->x0) * 0.25 * (t / 0.4)) + i * 2 - 4, y = bb - (int)(hash(i, 9) * 3 + t * 5); canvas_px(c, x, y, mix(RGB(210, 200, 185), px(c, x, y), t * 2)); }
    }
}
static double hash(int x, int y) { unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return (h & 0xffff) / 65536.0; }
static const int BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static double ease_out(double x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return 1 - (1 - x) * (1 - x) * (1 - x); }
static double ease_back(double x) { x = x < 0 ? 0 : x > 1 ? 1 : x; double c = 1.70158; return 1 + (c + 1) * pow(x - 1, 3) + c * pow(x - 1, 2); }

/* A  BUILD: rises / builds from the floor in rows with a small overshoot, a dust puff where it meets the floor
 *    (the well instead opens from its middle out, with dust at its rim) */
static Canvas style_build(const Gate *g, int side, double t) {
    Canvas c = copy(&g->before);
    int w = c.w;
    if (side == SIDE_DOWN) {
        double cx = (g->x0 + g->x1) / 2.0, hw = (g->x1 - g->x0) / 2.0 * ease_back(t);
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x] && fabs(x - cx) <= hw) c.p[y * w + x] = px(&g->after, x, y);
    } else if (side == SIDE_UP) { stairs_sequence(&c, g, t); return c;
    } else if (0) {
        double H = g->y1 - g->y0 + 1, front = g->y1 - H * ease_out(t) * 1.0;
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x]) {
            if (y < front) continue;
            double age = (y - front) / 6.0; /* rows just shown are still settling */
            int dy = age < 1 ? (int)lround(3 * (1 - age)) : 0, yy = y - dy;
            if (in(&c, x, yy)) c.p[yy * w + x] = age < 0.5 ? mix(px(&g->after, x, y), RGB(255, 255, 255), 0.5) : px(&g->after, x, y);
        }
    } else { /* a door: rises out of the floor with an overshoot */
        double H = g->y1 - g->y0 + 1, lift = H * (1 - ease_back(t));
        for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x]) {
            int yy = y + (int)lround(lift);
            if (yy <= g->y1 && yy >= 0) c.p[yy * w + x] = px(&g->after, x, y);
        }
    }
    if (t > 0.05 && t < 0.95) { /* dust puffs along the base */
        double life = 1 - fabs(t - 0.5) * 2;
        int base = side == SIDE_DOWN ? g->y1 : g->y1, n = 7;
        for (int i = 0; i < n; i++) {
            double fx = g->x0 - 3 + (g->x1 - g->x0 + 6) * (i + 0.5) / n + (hash(i, 7) - 0.5) * 4, rise = t * 6 * (0.5 + hash(i, 3));
            int x = (int)fx, y = base - (int)rise, r = life > 0.6 ? 2 : 1;
            if (hash(i, (int)(t * 10)) < 0.25) continue;
            canvas_rect(&c, x, y, r, r, mix(RGB(200, 190, 175), px(&c, x < 0 ? 0 : x, y < 0 ? 0 : y), 1 - life));
        }
    }
    return c;
}
/* B  SHIMMER: pixels fade in through a dither (ordered, so it reads as a pattern, not noise), lit bright as they arrive,
 *    with a few sparkles */
static Canvas style_shimmer(const Gate *g, int side, double t) {
    (void)side;
    Canvas c = copy(&g->before);
    int w = c.w;
    for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x]) {
        double th = (BAYER[y & 3][x & 3] + 0.5) / 16.0 * 0.7, a = (t - th) / 0.3; /* this pixel arrives at th */
        if (a <= 0) continue;
        c.p[y * w + x] = mix(px(&g->after, x, y), RGB(255, 250, 220), a < 1 ? 0.7 * (1 - a) : 0);
    }
    for (int i = 0; i < 6 && t < 0.95; i++) {
        double ph = fmod(t * 2.2 + hash(i, 1), 1.0);
        int x = g->x0 + (int)(hash(i, 2) * (g->x1 - g->x0)), y = g->y0 + (int)(hash(i, 5) * (g->y1 - g->y0));
        Rgb s = mix(YEL, RGB(255, 255, 255), ph);
        if (ph < 0.6) { canvas_px(&c, x, y, s); if (ph > 0.15 && ph < 0.45) { canvas_px(&c, x - 1, y, s); canvas_px(&c, x + 1, y, s); canvas_px(&c, x, y - 1, s); canvas_px(&c, x, y + 1, s); } }
    }
    return c;
}
/* C  BLUEPRINT: its outline is drawn first in light blue (traced from the floor up), then it fills in from the floor,
 *    under a bright line */
static Canvas style_blueprint(const Gate *g, int side, double t) {
    (void)side;
    Canvas c = copy(&g->before);
    int w = c.w;
    Rgb line = RGB(120, 200, 255);
    double H = g->y1 - g->y0 + 1, trace = g->y1 - H * fmin(1, t / 0.45), fill = g->y1 - H * (t < 0.4 ? 0 : ease_out((t - 0.4) / 0.6)) - 0.5;
    for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x]) {
        int edge = 0;
        for (int d = 0; d < 4; d++) { int xx = x + (d == 0) - (d == 1), yy = y + (d == 2) - (d == 3); if (!in(&c, xx, yy) || !g->m[yy * w + xx]) edge = 1; }
        /* (inside lines too: where the colour changes a lot, the steps' edges and the door's frame) */
        Rgb a = px(&g->after, x, y), b = in(&c, x + 1, y) ? px(&g->after, x + 1, y) : a, d2 = in(&c, x, y + 1) ? px(&g->after, x, y + 1) : a;
        if (abs(a.r - b.r) + abs(a.g - b.g) + abs(a.b - b.b) > 90 || abs(a.r - d2.r) + abs(a.g - d2.g) + abs(a.b - d2.b) > 90) edge = 1;
        if (y >= fill) c.p[y * w + x] = (int)y == (int)ceil(fill) && t < 1 ? RGB(230, 245, 255) : a;
        else if (edge && y >= trace) c.p[y * w + x] = mix(px(&g->before, x, y), line, 0.85);
        else if (y >= trace && t > 0.1) c.p[y * w + x] = mix(px(&g->before, x, y), line, 0.12);
    }
    return c;
}

static void gate_sheets(void) {
    static const struct { const char *theme, *what; int side; } G[] = {
        {"office", "DOOR (OFFICE)", SIDE_LEFT}, {"office", "STAIRS (OFFICE)", SIDE_UP}, {"coffee", "STAIR WELL (COFFEE SHOP)", SIDE_DOWN}, {"park", "BUILDING DOOR (PARK)", SIDE_RIGHT}, {"park", "STOOP (PARK)", SIDE_UP},
    };
    static const char *SNAME[] = {"NOW", "A  BUILD", "B  SHIMMER", "C  BLUEPRINT"};
    static const double TS[8] = {0, 0.1, 0.2, 0.3, 0.45, 0.6, 0.8, 1.0};
    for (size_t gi = 0; gi < sizeof G / sizeof G[0]; gi++) {
        Gate g; gate_get(G[gi].theme, G[gi].side, &g);
        int m = 6, cx0 = g.x0 - m, cy0 = g.y0 - m - 4, cw = g.x1 - g.x0 + 1 + 2 * m, ch = g.y1 - g.y0 + 1 + 2 * m + 4;
        if (cw < 36) { cx0 -= (36 - cw) / 2; cw = 36; }
        int Z = cw > 90 ? 1 : 2, fw = cw * Z, fh = ch * Z;
        Canvas sheet = canvas_new(60 + 8 * (fw + 4), 12 + 4 * (fh + 4), BG);
        canvas_text(&sheet, 2, 2, G[gi].what, YEL);
        for (int f = 0; f < 8; f++) { char lab[16]; snprintf(lab, sizeof lab, "%.2fS", TS[f] * 0.8); canvas_text(&sheet, 60 + f * (fw + 4), 2, lab, RGB(150, 150, 170)); }
        /* GIFs of the whole room, each style one after another */
        int nfr = 4 * 18, fw2 = g.after.w, fh2 = g.after.h;
        Rgb **gif = malloc(sizeof(Rgb *) * (size_t)nfr);
        for (int st = 0; st < 4; st++) {
            canvas_text(&sheet, 2, 12 + st * (fh + 4) + fh / 2 - 2, SNAME[st], RGB(220, 220, 235));
            for (int f = 0; f < 8; f++) {
                Canvas c = st == 0 ? copy(&g.now[f]) : st == 1 ? style_build(&g, G[gi].side, TS[f]) : st == 2 ? style_shimmer(&g, G[gi].side, TS[f]) : style_blueprint(&g, G[gi].side, TS[f]);
                Canvas z = crop(&c, cx0, cy0, cw, ch), zs = canvas_scaled(&z, Z);
                canvas_blit(&sheet, &zs, 60 + f * (fw + 4), 12 + st * (fh + 4));
                canvas_free(&z); canvas_free(&zs); canvas_free(&c);
            }
            for (int f = 0; f < 18; f++) { /* 0.8 s at 16 FPS = 13 frames, then a short hold */
                double tt = fmin(1, f / 13.0);
                Canvas c;
                if (st == 0) { int k = 0; while (k < 7 && NOWT[k + 1] <= tt) k++; c = copy(&g.now[k]); }
                else c = st == 1 ? style_build(&g, G[gi].side, tt) : st == 2 ? style_shimmer(&g, G[gi].side, tt) : style_blueprint(&g, G[gi].side, tt);
                canvas_rect(&c, 0, c.h - 8, c.w, 8, RGB(20, 20, 26)); canvas_text(&c, 2, c.h - 7, SNAME[st], YEL);
                Canvas z = crop(&c, cx0 - 30, cy0 - 10, cw + 60, ch + 20);
                Canvas zz = canvas_new(z.w, z.h + 8, BG); canvas_blit(&zz, &z, 0, 0); canvas_text(&zz, 2, z.h + 1, SNAME[st], YEL);
                gif[st * 18 + f] = zz.p; fw2 = zz.w; fh2 = zz.h;
                canvas_free(&z); canvas_free(&c);
            }
        }
        char path[160], name[64];
        snprintf(name, sizeof name, "%s-%d", G[gi].theme, G[gi].side);
        snprintf(path, sizeof path, OUT "/02-gate-appear-first-round/gate-%s.png", name);
        png_write(path, &sheet, 3); fprintf(stderr, "wrote %s\n", path);
        /* (the GIF bigger: every pixel 4x) */
        Rgb **big = malloc(sizeof(Rgb *) * (size_t)nfr);
        for (int f = 0; f < nfr; f++) { Canvas s = {fw2, fh2, gif[f]}, b = canvas_scaled(&s, 4); big[f] = b.p; }
        snprintf(path, sizeof path, OUT "/02-gate-appear-first-round/gate-%s.gif", name);
        gif_write(path, (const Rgb *const *)big, nfr, fw2 * 4, fh2 * 4, 6); fprintf(stderr, "wrote %s\n", path);
        for (int f = 0; f < nfr; f++) { free(gif[f]); free(big[f]); }
        free(gif); free(big); canvas_free(&sheet);
    }
}


/* ================= 3. the construct: a glowing blue build of the gate whose pieces fly in and lock together in the air
 * (like a Green Lantern construct), then it turns solid ================= */
typedef struct { int n, x0, y0, x1, y1, *pix; double start, ox, oy, spin; } Piece;
#define MAXP 600
static Piece P[MAXP]; static int NP; static int *LAB;
static int cdist(Rgb a, Rgb b) { return abs(a.r - b.r) + abs(a.g - b.g) + abs(a.b - b.b); }
static void pieces(const Gate *g, int cell) {
    int w = g->after.w, h = g->after.h, *q = malloc(sizeof(int) * (size_t)(w * h));
    free(LAB); LAB = malloc(sizeof(int) * (size_t)(w * h));
    for (int i = 0; i < w * h; i++) LAB[i] = -1;
    NP = 0;
    for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x] && LAB[y * w + x] < 0 && NP < MAXP) {
        Rgb seed = px(&g->after, x, y);
        int qn = 0, cx = x / cell, cy = y / cell;
        q[qn++] = y * w + x; LAB[y * w + x] = NP;
        for (int k = 0; k < qn; k++) {
            int ix = q[k] % w, iy = q[k] / w;
            for (int d = 0; d < 4; d++) {
                int xx = ix + (d == 0) - (d == 1), yy = iy + (d == 2) - (d == 3), j = yy * w + xx;
                if (!in(&g->after, xx, yy) || !g->m[j] || LAB[j] >= 0 || xx / cell != cx || yy / cell != cy) continue;
                if (cdist(px(&g->after, xx, yy), seed) > 70) continue;
                LAB[j] = NP; q[qn++] = j;
            }
        }
        NP++;
    }
    for (int pass = 0; pass < 3; pass++) { /* crumbs (under 6 pixels) join a neighbour */
        int *cnt = calloc(MAXP, sizeof(int));
        for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) cnt[LAB[i]]++;
        for (int i = 0; i < w * h; i++) if (LAB[i] >= 0 && cnt[LAB[i]] < 6) {
            int ix = i % w, iy = i / w;
            for (int d = 0; d < 4; d++) { int xx = ix + (d == 0) - (d == 1), yy = iy + (d == 2) - (d == 3); if (in(&g->after, xx, yy) && LAB[yy * w + xx] >= 0 && LAB[yy * w + xx] != LAB[i] && cnt[LAB[yy * w + xx]] >= cnt[LAB[i]]) { LAB[i] = LAB[yy * w + xx]; break; } }
        }
        free(cnt);
    }
    for (int k = 0; k < NP; k++) { P[k].n = 0; P[k].x0 = w; P[k].y0 = h; P[k].x1 = -1; P[k].y1 = -1; free(P[k].pix); P[k].pix = NULL; }
    for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) { Piece *p = &P[LAB[i]]; p->n++; int x = i % w, y = i / w; if (x < p->x0) p->x0 = x; if (x > p->x1) p->x1 = x; if (y < p->y0) p->y0 = y; if (y > p->y1) p->y1 = y; }
    for (int k = 0; k < NP; k++) P[k].pix = malloc(sizeof(int) * (size_t)(P[k].n + 1)), P[k].n = 0;
    for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) P[LAB[i]].pix[P[LAB[i]].n++] = i;
    /* the order: from the floor up (what holds the rest comes first), a little shuffled; each flies in from somewhere
     * above and around, turning into place */
    double ymin = g->y0, ymax = g->y1;
    for (int k = 0; k < NP; k++) if (P[k].n) {
        double r = (ymax - P[k].y1) / (ymax - ymin + 1);
        P[k].start = 0.04 + 0.42 * (r * 0.75 + hash(k, 11) * 0.25);
        double a = M_PI * (1.1 + 0.8 * hash(k, 13)), dist = 26 + 26 * hash(k, 17); /* (up-left to up-right) */
        P[k].ox = cos(a) * dist; P[k].oy = sin(a) * dist * 0.8 - 6;
        P[k].spin = (hash(k, 19) - 0.5) * 2.4;
    }
    free(q);
}
#define FLY 0.26   /* part of the whole a piece takes to fly in */
#define SOLID0 0.80 /* from here it turns solid, from the floor up */
static Rgb holo(double l) { /* the construct's blues, by the real colour's brightness */
    static const Rgb R[5] = {{14, 30, 92}, {28, 78, 190}, {60, 140, 245}, {130, 210, 255}, {228, 248, 255}};
    l = l < 0 ? 0 : l > 1 ? 0.999 : l;
    double f = l * 4; int i = (int)f;
    return mix(R[i], R[i + 1], f - i);
}
static double lum(Rgb c) { return (0.3 * c.r + 0.59 * c.g + 0.11 * c.b) / 255.0; }
static Canvas style_construct(const Gate *g, double t) {
    Canvas c = copy(&g->before), glow = copy(&g->before);
    int w = c.w;
    unsigned char *built = calloc((size_t)(w * c.h), 1);
    double solid = t <= SOLID0 ? -1 : (t - SOLID0) / (1 - SOLID0); /* 0..1 up the gate */
    double front = g->y1 + 1 - (g->y1 - g->y0 + 2) * ease_out(solid < 0 ? 0 : solid);
    for (int k = 0; k < NP; k++) {
        Piece *p = &P[k];
        if (!p->n) continue;
        double q = (t - p->start) / FLY;
        if (q <= 0) continue;
        for (int trail = 2; trail >= 0; trail--) { /* two fading copies behind it while it flies */
            double qq = q - trail * 0.09;
            if (trail && (q >= 1 || qq <= 0)) continue;
            double e = ease_out(qq), ang = p->spin * (1 - e), dx = p->ox * (1 - e), dy = p->oy * (1 - e);
            double pcx = (p->x0 + p->x1) / 2.0, pcy = (p->y0 + p->y1) / 2.0, sc = 0.5 + 0.5 * e; /* (it grows as it comes) */
            for (int i = 0; i < p->n; i++) {
                int x = p->pix[i] % w, y = p->pix[i] / w;
                double rx = (x - pcx) * sc, ry = (y - pcy) * sc;
                int X = (int)lround(pcx + dx + rx * cos(ang) - ry * sin(ang)), Y = (int)lround(pcy + dy + rx * sin(ang) + ry * cos(ang));
                if (!in(&c, X, Y)) continue;
                Rgb real = px(&g->after, x, y);
                int edge = 0;
                for (int d = 0; d < 4; d++) { int xx = x + (d == 0) - (d == 1), yy = y + (d == 2) - (d == 3); if (!in(&c, xx, yy) || LAB[yy * w + xx] != k) edge = 1; }
                Rgb col = holo(lum(real) * 0.75 + (edge ? 0.35 : 0.1));
                if (qq < 0.25) { if (!edge) continue; col = holo(0.95); } /* it starts as its outline */
                if (q >= 1 && q < 1.18) col = mix(col, RGB(255, 255, 255), 1 - (q - 1) / 0.18); /* the click when it locks in */
                if (trail) { c.p[Y * w + X] = mix(px(&c, X, Y), holo(0.6), trail == 1 ? 0.35 : 0.18); continue; }
                double a = q >= 1 ? 0.92 : 0.8;
                c.p[Y * w + X] = mix(px(&g->before, X, Y), col, a);
                if (q >= 1) built[Y * w + X] = 1;
                if (solid >= 0 && q >= 1 && y >= front) c.p[Y * w + X] = (y < front + 1.5 && solid < 1) ? RGB(235, 250, 255) : real;
            }
        }
    }
    if (solid < 1) for (int y = 1; y < c.h - 1; y++) for (int x = 1; x < w - 1; x++) if (!g->m[y * w + x]) { /* a soft blue glow around what is built */
        int n = 0;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) if (in(&c, x + dx, y + dy) && built[(y + dy) * w + x + dx] && !(y + dy >= front && solid >= 0)) n++;
        if (n) c.p[y * w + x] = mix(px(&c, x, y), RGB(70, 150, 255), fmin(0.38, n * 0.05) * (solid < 0 ? 1 : 1 - solid));
    }
    free(built); canvas_free(&glow);
    return c;
}
static void construct_sheets(void) {
    static const struct { const char *theme, *what; int side, cell; } G[] = {
        {"office", "DOOR (OFFICE)", SIDE_LEFT, 6}, {"office", "STAIRS (OFFICE)", SIDE_UP, 8}, {"coffee", "STAIRS (COFFEE SHOP)", SIDE_UP, 8}, {"coffee", "STAIR WELL (COFFEE SHOP)", SIDE_DOWN, 5}, {"park", "STOOP (PARK)", SIDE_UP, 12},
    };
    static const double TS[10] = {0, 0.08, 0.16, 0.26, 0.36, 0.48, 0.62, 0.8, 0.9, 1.0};
    const double LEN = 1.8; /* seconds, the whole thing */
    for (size_t gi = 0; gi < sizeof G / sizeof G[0]; gi++) {
        Gate g; gate_get(G[gi].theme, G[gi].side, &g);
        pieces(&g, G[gi].cell);
        int m = 10, cx0 = g.x0 - m - 6, cy0 = g.y0 - m - 14, cw = g.x1 - g.x0 + 1 + 2 * m + 12, ch = g.y1 - g.y0 + 1 + 2 * m + 18;
        if (cw < 50) { cx0 -= (50 - cw) / 2; cw = 50; }
        if (ch < 40) { cy0 -= (40 - ch) / 2; ch = 40; }
        int Z = cw > 100 ? 1 : 2, fw = cw * Z, fh = ch * Z, per = 5;
        Canvas sheet = canvas_new(4 + per * (fw + 4), 12 + 2 * (fh + 12), BG);
        canvas_text(&sheet, 2, 2, G[gi].what, YEL);
        for (int f = 0; f < 10; f++) {
            Canvas c = style_construct(&g, TS[f]);
            Canvas z = crop(&c, cx0, cy0, cw, ch), zs = canvas_scaled(&z, Z);
            int sx = 4 + (f % per) * (fw + 4), sy = 12 + (f / per) * (fh + 12);
            char lab[16]; snprintf(lab, sizeof lab, "%.2fS", TS[f] * LEN);
            canvas_text(&sheet, sx, sy, lab, RGB(150, 150, 170));
            canvas_blit(&sheet, &zs, sx, sy + 7);
            canvas_free(&z); canvas_free(&zs); canvas_free(&c);
        }
        char name[64], path[160];
        snprintf(name, sizeof name, "%s-%d", G[gi].theme, G[gi].side);
        snprintf(path, sizeof path, OUT "/03-gate-construct/construct-%s.png", name);
        png_write(path, &sheet, 3); fprintf(stderr, "wrote %s\n", path);
        int nf = (int)(LEN * 16) + 12; /* 16 FPS, then a hold */
        Rgb **fr = malloc(sizeof(Rgb *) * (size_t)nf);
        int W = 0, H = 0;
        for (int f = 0; f < nf; f++) {
            Canvas c = style_construct(&g, fmin(1, f / (LEN * 16)));
            Canvas z = crop(&c, cx0 - 20, cy0 - 6, cw + 40, ch + 12), b = canvas_scaled(&z, 4);
            fr[f] = b.p; W = b.w; H = b.h;
            canvas_free(&z); canvas_free(&c);
        }
        snprintf(path, sizeof path, OUT "/03-gate-construct/construct-%s.gif", name);
        for (int step = 4; gif_write(path, (const Rgb *const *)fr, nf, W, H, 6); step *= 2) /* (a GIF has 256 colours: round them until they fit) */
            for (int f = 0; f < nf; f++) for (int i = 0; i < W * H; i++) { Rgb *c = &fr[f][i]; c->r = (uint8_t)(c->r / step * step); c->g = (uint8_t)(c->g / step * step); c->b = (uint8_t)(c->b / step * step); }
        fprintf(stderr, "wrote %s\n", path);
        for (int f = 0; f < nf; f++) free(fr[f]);
        free(fr); canvas_free(&sheet);
    }
}

/* ================= 4. D + spotlight, fading: Tab picks one (the light and the corners come in, the corners closing in
 * from wider), Tab again moves them to the next, then they fade out ================= */
static void spot_k(Canvas *c, double cx, double cy, double dim, int bar_h) {
    double r = 13;
    for (int y = 0; y < c->h - bar_h; y++) for (int x = 0; x < c->w; x++) {
        double d = hypot(x - cx, y - cy), k = d < r ? 0 : d < r + 8 ? (d - r) / 8 : 1;
        c->p[y * c->w + x] = mix(px(c, x, y), RGB(10, 10, 24), k * dim);
    }
}
static void brackets_k(Canvas *c, int x0, int x1, int y0, int y1, int grow, double a) {
    Rgb col = mix(px(c, x0, y0), YEL, a);
    int A = x0 - 2 - grow, B = x1 + 2 + grow, U = y0 - 2 - grow, D = y1 + 2 + grow;
    for (int i = 0; i < 3; i++) {
        canvas_px(c, A + i, U, col); canvas_px(c, A, U + i, col); canvas_px(c, B - i, U, col); canvas_px(c, B, U + i, col);
        canvas_px(c, A + i, D, col); canvas_px(c, A, D - i, col); canvas_px(c, B - i, D, col); canvas_px(c, B, D - i, col);
    }
}
static void pick_d(void) {
    Fixture *fx = load("theme office\ntitle office\nterminal 150 44\ntime 1000.0 3.4\n"
                       "session s00 name=clawd-room created=900 state=work tool=Bash\n"
                       "session s01 name=park created=910 state=think tool=Edit hold=1000.4\n"
                       "session s02 name=api created=920 state=wait tool=mcp__x__y\n"
                       "session s03 name=web created=930 state=idle tool=Read\n"
                       "session s04 name=docs created=940 state=work tool=Foo\n");
    for (int i = 0; i < fx->room.npool; i++) { fx->room.pool[i].has_pos = 0; fx->room.pool[i].created = fx->now - 100; fx->room.pool[i].last = fx->now - 1; }
    fx->room.freeze = 0;
    for (int k = 0; k < 40; k++) { Canvas c = frame_at(fx, k * 0.0625); canvas_free(&c); }
    fx->room.freeze = 1;
    double T = 40 * 0.0625;
    Canvas base = frame_at(fx, T);
    int bx[2][4];
    for (int k = 0; k < 2; k++) { /* each picked Clawd's box */
        Session *s = fx->room.s[k == 0 ? 1 : 3];
        int keep = s->has_away; s->has_away = 1;
        Canvas without = frame_at(fx, T);
        s->has_away = keep;
        unsigned char *m = malloc((size_t)(base.w * base.h));
        silhouette(&base, &without, s, m);
        top_of(m, base.w, base.h, &bx[k][0], &bx[k][1], &bx[k][2], &bx[k][3]);
        free(m); canvas_free(&without);
    }
    /* the strengths side by side */
    static const double DIM[3] = {0.18, 0.28, 0.4};
    Canvas sheet = canvas_new(3 * (base.w + 6) + 6, base.h + 18, BG);
    for (int i = 0; i < 3; i++) {
        Canvas c = copy(&base);
        spot_k(&c, (bx[0][0] + bx[0][1]) / 2.0, (bx[0][2] + bx[0][3]) / 2.0, DIM[i], 0);
        brackets_k(&c, bx[0][0], bx[0][1], bx[0][2], bx[0][3], 0, 1);
        char t[32]; snprintf(t, sizeof t, "%c  DIMMED %d IN 100", 'A' + i, (int)(DIM[i] * 100));
        canvas_text(&sheet, 6 + i * (base.w + 6), 3, t, RGB(220, 220, 235));
        canvas_blit(&sheet, &c, 6 + i * (base.w + 6), 12);
        canvas_free(&c);
    }
    png_write(OUT "/05-pick-d-spotlight/dim-strengths.png", &sheet, 3); canvas_free(&sheet);
    /* the GIF: nothing, Tab (fade in 0.25 s, corners close in), hold, Tab (the light glides to the next, 0.3 s), hold,
     * fade out (0.4 s), nothing */
    double FIN = 0.25, GLIDE = 0.3, FOUT = 0.4, t1 = 0.5, t2 = t1 + FIN + 1.2, t3 = t2 + GLIDE + 1.2, t4 = t3 + FOUT, LEN = t4 + 0.5, dim = 0.28;
    int nf = (int)(LEN * 16);
    Rgb **fr = malloc(sizeof(Rgb *) * (size_t)nf);
    int W = 0, H = 0;
    for (int f = 0; f < nf; f++) {
        double t = f / 16.0, a = t < t1 ? 0 : t < t1 + FIN ? ease_out((t - t1) / FIN) : t < t3 ? 1 : t < t4 ? 1 - (t - t3) / FOUT : 0;
        double g = t < t2 ? 0 : t < t2 + GLIDE ? ease_out((t - t2) / GLIDE) : 1; /* from the first to the second */
        int b[4]; for (int i = 0; i < 4; i++) b[i] = (int)lround(bx[0][i] + (bx[1][i] - bx[0][i]) * g);
        Canvas c = copy(&base);
        const char *bar = "TAB PICK  M MOVE";
        if (a > 0) {
            spot_k(&c, (b[0] + b[1]) / 2.0, (b[2] + b[3]) / 2.0, dim * a, 0);
            int grow = t < t1 + FIN ? (int)lround(4 * (1 - ease_out((t - t1) / FIN))) : (t >= t2 && t < t2 + GLIDE) ? (int)lround(2 * sin(M_PI * g)) : 0;
            brackets_k(&c, b[0], b[1], b[2], b[3], grow, a);
            bar = t < t2 ? "PICKED PARK" : "PICKED WEB";
        }
        canvas_rect(&c, 0, c.h - 8, c.w, 8, RGB(20, 20, 28)); canvas_text(&c, 2, c.h - 7, bar, a > 0 ? YEL : RGB(200, 200, 210));
        Canvas big = canvas_scaled(&c, 3);
        fr[f] = big.p; W = big.w; H = big.h;
        canvas_free(&c);
    }
    const char *path = OUT "/05-pick-d-spotlight/tab-fade.gif";
    for (int step = 4; gif_write(path, (const Rgb *const *)fr, nf, W, H, 6); step *= 2)
        for (int f = 0; f < nf; f++) for (int i = 0; i < W * H; i++) { Rgb *q = &fr[f][i]; q->r = (uint8_t)(q->r / step * step); q->g = (uint8_t)(q->g / step * step); q->b = (uint8_t)(q->b / step * step); }
    for (int f = 0; f < nf; f++) free(fr[f]);
    free(fr); canvas_free(&base);
    fprintf(stderr, "wrote 05\n");
}

/* ================= 5. the construct, v2: real parts (each board, post, rail length, the handle) in their own colours fly
 * into a blue outline of the gate, held by a blue glow, and click into place ================= */
static void pieces_real(const Gate *g) {
    int w = g->after.w, h = g->after.h, *q = malloc(sizeof(int) * (size_t)(w * h)), *comp = malloc(sizeof(int) * (size_t)(w * h));
    free(LAB); LAB = malloc(sizeof(int) * (size_t)(w * h));
    for (int i = 0; i < w * h; i++) { LAB[i] = -1; comp[i] = -1; }
    int nc = 0;
    /* 1 runs of one colour: a board, a post, a rail, a tread's lit edge */
    for (int y = g->y0; y <= g->y1; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x] && comp[y * w + x] < 0) {
        Rgb seed = px(&g->after, x, y);
        int qn = 0, bx0 = x, bx1 = x, by0 = y, by1 = y;
        q[qn++] = y * w + x; comp[y * w + x] = nc;
        for (int k = 0; k < qn; k++) {
            int ix = q[k] % w, iy = q[k] / w;
            if (ix < bx0) bx0 = ix; if (ix > bx1) bx1 = ix; if (iy < by0) by0 = iy; if (iy > by1) by1 = iy;
            for (int d = 0; d < 4; d++) {
                int xx = ix + (d == 0) - (d == 1), yy = iy + (d == 2) - (d == 3), j = yy * w + xx;
                if (in(&g->after, xx, yy) && g->m[j] && comp[j] < 0 && rgb_eq(px(&g->after, xx, yy), seed)) { comp[j] = nc; q[qn++] = j; }
            }
        }
        /* 2 cut into parts of a real size: long runs into lengths, big blocks into boards (staggered, like planks) */
        int bw = bx1 - bx0 + 1, bh = by1 - by0 + 1, off = (int)(hash(nc, 23) * 12);
        for (int k = 0; k < qn; k++) {
            int ix = q[k] % w, iy = q[k] / w, sub;
            if (bh >= 4 && bw >= 6) { int band = (iy - by0) / 3; sub = band * 64 + (ix - bx0 + off + band * 5) / 13; } /* a block: boards 3 high, about 13 long */
            else if (bw > bh) sub = (ix - bx0 + off) / 11;  /* a long run: lengths */
            else sub = (iy - by0) / 12;                     /* a post: whole (very tall ones in two) */
            LAB[q[k]] = nc * 4096 + sub;
        }
        nc++;
    }
    /* 3 number the parts */
    NP = 0;
    int *map = NULL, nmap = 0, cap = 0, *key = NULL;
    for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) {
        int f = -1;
        for (int k = 0; k < nmap; k++) if (key[k] == LAB[i]) { f = k; break; }
        if (f < 0) { if (nmap == cap) { cap = cap ? cap * 2 : 256; key = realloc(key, sizeof(int) * (size_t)cap); map = realloc(map, sizeof(int) * (size_t)cap); } key[nmap] = LAB[i]; map[nmap] = nmap; f = nmap++; }
        LAB[i] = f;
    }
    /* 4 single pixels join the part they touch (a pixel of shading is not a part), unless they stand out (the handle) */
    for (int pass = 0; pass < 2; pass++) {
        int *cnt = calloc((size_t)nmap + 1, sizeof(int));
        for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) cnt[LAB[i]]++;
        for (int i = 0; i < w * h; i++) if (LAB[i] >= 0 && cnt[LAB[i]] <= 2) {
            Rgb c = g->after.p[i];
            int sat = abs(c.r - c.g) + abs(c.g - c.b) + abs(c.r - c.b);
            if (sat > 200 && (c.r + c.g) > 300) continue; /* (bright and coloured: a handle, a lamp) */
            int ix = i % w, iy = i / w;
            for (int d = 0; d < 4; d++) { int xx = ix + (d == 0) - (d == 1), yy = iy + (d == 2) - (d == 3); if (in(&g->after, xx, yy) && LAB[yy * w + xx] >= 0 && LAB[yy * w + xx] != LAB[i] && cnt[LAB[yy * w + xx]] > 2) { LAB[i] = LAB[yy * w + xx]; break; } }
        }
        free(cnt);
    }
    NP = nmap < MAXP ? nmap : MAXP;
    for (int k = 0; k < NP; k++) { P[k].n = 0; P[k].x0 = w; P[k].y0 = h; P[k].x1 = -1; P[k].y1 = -1; free(P[k].pix); P[k].pix = NULL; }
    for (int i = 0; i < w * h; i++) if (LAB[i] >= NP) LAB[i] = NP - 1;
    for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) { Piece *p = &P[LAB[i]]; p->n++; int x = i % w, y = i / w; if (x < p->x0) p->x0 = x; if (x > p->x1) p->x1 = x; if (y < p->y0) p->y0 = y; if (y > p->y1) p->y1 = y; }
    for (int k = 0; k < NP; k++) P[k].pix = malloc(sizeof(int) * (size_t)(P[k].n + 1)), P[k].n = 0;
    for (int i = 0; i < w * h; i++) if (LAB[i] >= 0) P[LAB[i]].pix[P[LAB[i]].n++] = i;
    /* the order: big parts from the floor up first, then posts and rails, the small things (handle, light) last */
    double ymin = g->y0, ymax = g->y1;
    for (int k = 0; k < NP; k++) if (P[k].n) {
        double up = (ymax - P[k].y1) / (ymax - ymin + 1), small = 1 - fmin(1, P[k].n / 30.0);
        P[k].start = 0.08 + 0.52 * fmin(1, up * 0.6 + small * 0.3 + hash(k, 11) * 0.15);
        double a = M_PI * (1.05 + 0.9 * hash(k, 13)), dist = 30 + 24 * hash(k, 17);
        P[k].ox = cos(a) * dist; P[k].oy = sin(a) * dist * 0.7 - 8;
        P[k].spin = (hash(k, 19) - 0.5) * 3.0;
    }
    free(q); free(comp); free(map); free(key);
}
#define FLY2 0.24
static Canvas style_construct2(const Gate *g, double t) {
    Canvas c = copy(&g->before);
    int w = c.w;
    Rgb BLUE = RGB(80, 170, 255), PALE = RGB(170, 225, 255);
    if (g->tree) { /* the tree fades out (a little blue as it goes) while the outline comes */
        double f = seg(t, 0.0, 0.22);
        for (int i = 0; i < w * c.h; i++) if (g->tree[i]) c.p[i] = mix(mix(g->before.p[i], BLUE, 0.5 * sin(M_PI * f)), g->notree.p[i], f);
    }
    double wire = seg(t, 0, 0.12) * (1 - seg(t, 0.82, 1.0)); /* the blue outline of what is to come */
    unsigned char *aura = calloc((size_t)(w * c.h), 1);
    for (int y = g->y0; y <= g->y1 && wire > 0; y++) for (int x = g->x0; x <= g->x1; x++) if (g->m[y * w + x]) {
        int k = LAB[y * w + x], edge = 0;
        for (int d = 0; d < 4; d++) { int xx = x + (d == 0) - (d == 1), yy = y + (d == 2) - (d == 3); if (!in(&c, xx, yy) || !g->m[yy * w + xx]) edge = 2; else if (k >= 0 && LAB[yy * w + xx] != k && !edge) edge = 1; }
        double a = edge == 2 ? 0.75 : edge == 1 ? 0.35 : 0.1;
        c.p[y * w + x] = mix(px(&c, x, y), edge ? PALE : BLUE, a * wire);
    }
    for (int k = 0; k < NP; k++) {
        Piece *p = &P[k];
        if (!p->n) continue;
        double q = (t - p->start) / FLY2;
        if (q <= 0) continue;
        double pcx = (p->x0 + p->x1) / 2.0, pcy = (p->y0 + p->y1) / 2.0;
        for (int trail = 2; trail >= 0; trail--) {
            double qq = q - trail * 0.1;
            if (trail && (q >= 1 || qq <= 0)) continue;
            double e = ease_out(qq), ang = p->spin * (1 - e) * (1 - e), dx = p->ox * (1 - e), dy = p->oy * (1 - e);
            for (int i = 0; i < p->n; i++) {
                int x = p->pix[i] % w, y = p->pix[i] / w;
                double rx = x - pcx, ry = y - pcy;
                int X = (int)lround(pcx + dx + rx * cos(ang) - ry * sin(ang)), Y = (int)lround(pcy + dy + rx * sin(ang) + ry * cos(ang));
                if (!in(&c, X, Y)) continue;
                if (trail) { c.p[Y * w + X] = mix(px(&c, X, Y), BLUE, trail == 1 ? 0.3 : 0.15); continue; }
                Rgb col = px(&g->after, x, y);
                if (q >= 1 && q < 1.25) col = mix(col, RGB(255, 255, 255), 0.8 * (1 - (q - 1) / 0.25)); /* the click */
                c.p[Y * w + X] = col;
                if (q < 1.6) aura[Y * w + X] = (unsigned char)(q < 1 ? 255 : 255 * (1.6 - q) / 0.6);
            }
        }
    }
    /* the glow that holds each part while it flies, fading once it is in */
    Canvas o = copy(&c);
    for (int y = 1; y < c.h - 1; y++) for (int x = 1; x < w - 1; x++) if (!aura[y * w + x]) {
        int a = 0;
        for (int d = 0; d < 8; d++) { static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1}; int v = aura[(y + DY[d]) * w + x + DX[d]]; if (v > a) a = v; }
        if (a) o.p[y * w + x] = mix(px(&c, x, y), PALE, 0.7 * a / 255.0);
    }
    free(aura); canvas_free(&c);
    return o;
}
static void construct2_sheets(void) {
    static const struct { const char *theme, *what; int side; } G[] = {
        {"office", "DOOR (OFFICE)", SIDE_LEFT}, {"office", "STAIRS (OFFICE)", SIDE_UP}, {"coffee", "STAIRS (COFFEE SHOP)", SIDE_UP}, {"coffee", "STAIR WELL (COFFEE SHOP)", SIDE_DOWN}, {"park", "STOOP (PARK)", SIDE_UP},
    };
    static const double TS[10] = {0, 0.08, 0.16, 0.26, 0.36, 0.46, 0.56, 0.68, 0.84, 1.0};
    const double LEN = 1.4;
    for (size_t gi = 0; gi < sizeof G / sizeof G[0]; gi++) {
        Gate g; gate_get(G[gi].theme, G[gi].side, &g);
        pieces_real(&g);
        fprintf(stderr, "%s: %d parts\n", G[gi].what, NP);
        int m = 10, cx0 = g.x0 - m - 6, cy0 = g.y0 - m - 14, cw = g.x1 - g.x0 + 1 + 2 * m + 12, ch = g.y1 - g.y0 + 1 + 2 * m + 18;
        if (cw < 50) { cx0 -= (50 - cw) / 2; cw = 50; }
        if (ch < 40) { cy0 -= (40 - ch) / 2; ch = 40; }
        int Z = cw > 100 ? 1 : 2, fw = cw * Z, fh = ch * Z, per = 5;
        Canvas sheet = canvas_new(4 + per * (fw + 4), 12 + 2 * (fh + 12), BG);
        canvas_text(&sheet, 2, 2, G[gi].what, YEL);
        for (int f = 0; f < 10; f++) {
            Canvas c = style_construct2(&g, TS[f]);
            Canvas z = crop(&c, cx0, cy0, cw, ch), zs = canvas_scaled(&z, Z);
            int sx = 4 + (f % per) * (fw + 4), sy = 12 + (f / per) * (fh + 12);
            char lab[16]; snprintf(lab, sizeof lab, "%.2fS", TS[f] * LEN);
            canvas_text(&sheet, sx, sy, lab, RGB(150, 150, 170));
            canvas_blit(&sheet, &zs, sx, sy + 7);
            canvas_free(&z); canvas_free(&zs); canvas_free(&c);
        }
        char name[64], path[160];
        snprintf(name, sizeof name, "%s-%s", G[gi].theme, G[gi].side == SIDE_UP ? "stairs" : G[gi].side == SIDE_DOWN ? "well" : "door");
        snprintf(path, sizeof path, OUT "/07-construct-real-parts/construct-%s.png", name);
        png_write(path, &sheet, 3);
        int nf = (int)(LEN * 16) + 14;
        Rgb **fr = malloc(sizeof(Rgb *) * (size_t)nf);
        int W = 0, H = 0;
        for (int f = 0; f < nf; f++) {
            Canvas c = style_construct2(&g, fmin(1, f / (LEN * 16)));
            Canvas z = crop(&c, cx0 - 20, cy0 - 6, cw + 40, ch + 12), b = canvas_scaled(&z, 4);
            fr[f] = b.p; W = b.w; H = b.h;
            canvas_free(&z); canvas_free(&c);
        }
        snprintf(path, sizeof path, OUT "/07-construct-real-parts/construct-%s.gif", name);
        for (int step = 4; gif_write(path, (const Rgb *const *)fr, nf, W, H, 6); step *= 2)
            for (int f = 0; f < nf; f++) for (int i = 0; i < W * H; i++) { Rgb *q = &fr[f][i]; q->r = (uint8_t)(q->r / step * step); q->g = (uint8_t)(q->g / step * step); q->b = (uint8_t)(q->b / step * step); }
        for (int f = 0; f < nf; f++) free(fr[f]);
        free(fr); canvas_free(&sheet);
    }
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "construct2")) { TREE_FADE = 1; construct2_sheets(); return 0; }
    if (argc > 1 && !strcmp(argv[1], "pickd")) { (void)!system("mkdir -p " OUT "/05-pick-d-spotlight"); pick_d(); return 0; }
    if (argc > 1 && !strcmp(argv[1], "construct")) { (void)!system("mkdir -p " OUT); construct_sheets(); return 0; }
    (void)!system("mkdir -p " OUT);
    pick_sheet();
    gate_sheets();
    return 0;
}
