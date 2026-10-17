/* The three rooms in perspective (style A, see persp.h): the backgrounds, the workspaces, the gates, the room's sign,
 * the barista's apron and the playground spots. */
#include "theme.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "persp.h"
#include "room.h"

#define STEEL RGB(196, 196, 206)
#define STEEL_DARK RGB(150, 150, 160)
#define SCREEN_OFF RGB(16, 18, 24)
#define SCREEN_ON RGB(12, 26, 22)
#define WOOD RGB(78, 62, 50)
#define WOOD_LT RGB(120, 94, 70)
#define LEAF RGB(150, 108, 72)
#define LEAF_DK RGB(112, 80, 54)
#define KNOB RGB(230, 190, 90)
#define ROPE RGB(186, 166, 124)
#define RUNG RGB(150, 130, 96)
#define CUSHION RGB(70, 140, 130)
#define CHAIR_WOOD RGB(120, 84, 58)

/* name, horizon (unused in style A), visit kinds, solo acts, then wall, wall2, floor, floor2, base, desk, desk_top, counter, counter_top */
static const Theme OFFICE = {"office", 0.47, 6, {0, 1, 2, 3, 4, 5}, 4, {{"stretch", 2.0}, {"dance", 3.5}, {"jump", 2.2}, {"wave", 2.0}}, {196, 204, 218}, {186, 194, 210}, {176, 150, 118}, {156, 130, 100}, {120, 110, 104}, {112, 92, 76}, {140, 116, 96}, {0}, {0}};
static const Theme COFFEE = {"coffee", 0.47, 8, {0, 0, 1, 2, 4, 6, 6, 6}, 3, {{"stretch", 2.0}, {"dance", 3.5}, {"wave", 2.0}}, {168, 128, 98}, {178, 138, 106}, {222, 206, 182}, {158, 128, 104}, {110, 80, 60}, {176, 132, 92}, {150, 112, 78}, {128, 88, 60}, {186, 140, 98}};
static const Theme PARK = {"park", 0.47, 6, {0, 1, 2, 3, 4, 5}, 7, {{"stretch", 2.0}, {"dance", 3.5}, {"wave", 2.0}, {"swing", 7.0}, {"swing", 7.0}, {"slide", 7.0}, {"slide", 7.0}}, {150, 200, 236}, {200, 228, 246}, {122, 182, 98}, {110, 168, 88}, {92, 140, 80}, {132, 98, 66}, {176, 132, 90}, {0}, {0}};

const Theme *theme_get(const char *name) {
    if (name && !strcmp(name, "coffee")) return &COFFEE;
    if (name && !strcmp(name, "park")) return &PARK;
    return &OFFICE;
}

static int has_word(const char *s, const char *w) { return s && strstr(s, w) != NULL; }

const Theme *theme_pick(const char *room_name, const char *theme_name) {
    if (theme_name && (!strcmp(theme_name, "office") || !strcmp(theme_name, "coffee") || !strcmp(theme_name, "park"))) return theme_get(theme_name);
    char low[128] = "";
    if (room_name) for (size_t i = 0; room_name[i] && i < 127; i++) low[i] = (char)((room_name[i] >= 'A' && room_name[i] <= 'Z') ? room_name[i] + 32 : room_name[i]);
    if (has_word(low, "coffee") || has_word(low, "cafe")) return &COFFEE;
    if (has_word(low, "park") || has_word(low, "garden")) return &PARK;
    return &OFFICE;
}

static Rgb dk(Rgb c, double f) { return rgb_mix(c, RGB(0, 0, 0), f); }
static Rgb lt(Rgb c, double f) { return rgb_mix(c, RGB(255, 255, 255), f); }
static void quad(Canvas *c, double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy, Rgb col) {
    double p[8] = {ax, ay, bx, by, cx, cy, dx, dy};
    poly_fill(c, p, 4, col);
}
static void wq(Canvas *c, const View *v, double X0, double Z0, double E0, double X1, double Z1, double E1, double X2, double Z2, double E2, double X3, double Z3, double E3, Rgb col) {
    double p[12] = {X0, Z0, E0, X1, Z1, E1, X2, Z2, E2, X3, Z3, E3};
    view_quad(c, v, p, col);
}
static void blob(Canvas *c, int x, int y, int r, Rgb col) { /* a round bush / tree top with a lighter top */
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r - 2; dx <= r + 2; dx++) {
            double e = (dx * dx) / (double)((r + 2) * (r + 2)) + (dy * dy) / (double)(r * r);
            if (e <= 1.0) canvas_px(c, x + dx, y + dy, e < 0.35 && dy < 0 ? lt(col, 0.18) : col);
        }
}

/* ---- the room box: stylised ceiling, back wall, side walls; the floor in true perspective ------------------------------- */
static void room_box(Canvas *c, const View *v, Rgb ceil_c, Rgb wall, Rgb wall2, Rgb side, Rgb fl, Rgb fl2, int checker, Rgb base) {
    int w = c->w, h = c->h;
    quad(c, 0, 0, w, 0, v->br, v->bt, v->bl, v->bt, ceil_c);
    quad(c, v->bl, v->bt, v->br, v->bt, v->br, v->bb, v->bl, v->bb, wall);
    for (int y = (int)v->bt + 6; y < v->bb - 2; y += 7) canvas_rect(c, (int)v->bl, y, (int)(v->br - v->bl), 1, wall2);
    quad(c, 0, 0, v->bl, v->bt, v->bl, v->bb, 0, h, side);
    quad(c, w, 0, v->br, v->bt, v->br, v->bb, w, h, dk(side, 0.08));
    double tile = v->xw / 4.5; /* about nine tiles across */
    for (int y = (int)v->bb; y < h; y++)
        for (int x = 0; x < w; x++) {
            double X, Z;
            view_world(v, x + 0.5, y + 0.5, &X, &Z);
            if (fabs(X) > v->xw) continue;
            Rgb col;
            if (checker) col = (((int)floor((X + 8) / tile) + (int)floor(Z / tile)) & 1) ? fl : fl2;
            else col = fmod(Z / (tile * 0.7), 1.0) < 0.14 ? fl2 : fl; /* floor boards */
            canvas_px(c, x, y, col);
        }
    quad(c, v->bl, v->bb - 1, v->br, v->bb - 1, v->br, v->bb, v->bl, v->bb, base);
}

static void menu_board(Canvas *c, int x, int y) {
    canvas_rect(c, x, y, 16, 11, RGB(110, 84, 60));
    canvas_rect(c, x + 1, y + 1, 14, 9, RGB(40, 60, 46));
    canvas_text(c, x + 1, y + 2, "MENU", RGB(235, 235, 210));
    canvas_rect(c, x + 2, y + 8, 10, 1, RGB(150, 150, 140));
}

/* ---- backgrounds ----------------------------------------------------------------------------------------------------- */
static void paint_office(const Theme *th, Canvas *c, const View *v) {
    room_box(c, v, RGB(150, 160, 178), th->wall, th->wall2, RGB(170, 178, 196), th->floor, th->floor2, 0, th->base);
    int wx = (int)(v->bl + (v->br - v->bl) * 0.12), wy = (int)(v->bt + 4); /* a window on the back wall */
    if (v->bb - v->bt > 16) {
        canvas_rect(c, wx, wy, 14, 9, RGB(240, 244, 250));
        canvas_rect(c, wx + 1, wy + 1, 12, 7, RGB(150, 200, 236));
        canvas_rect(c, wx + 6, wy, 1, 9, RGB(240, 244, 250));
        int px = (int)(v->br - (v->br - v->bl) * 0.1); /* a plant */
        canvas_rect(c, px - 3, (int)v->bb - 13, 6, 8, RGB(90, 150, 90));
        canvas_rect(c, px - 2, (int)v->bb - 5, 4, 4, RGB(170, 110, 80));
    }
}

static void paint_coffee(const Theme *th, Canvas *c, const View *v) {
    room_box(c, v, RGB(118, 86, 66), th->wall, th->wall2, RGB(140, 104, 80), th->floor, th->floor2, 1, th->base);
    for (int i = 0; i < 2; i++) { /* hanging lamps with a warm glow */
        int lx = (int)(v->bl + (v->br - v->bl) * (0.2 + 0.3 * i));
        canvas_rect(c, lx, (int)v->bt, 1, 4, RGB(80, 60, 48));
        canvas_rect(c, lx - 2, (int)v->bt + 4, 5, 2, RGB(230, 190, 110));
        canvas_px(c, lx, (int)v->bt + 6, RGB(255, 230, 150));
    }
    if (v->bb - v->bt > 16) {
        int sx0 = (int)(v->bl + (v->br - v->bl) * 0.36), sy0 = (int)v->bb - 9; /* a shelf of cups and beans */
        canvas_rect(c, sx0, sy0, 24, 1, RGB(120, 92, 66));
        for (int i = 0; i < 5; i++) canvas_rect(c, sx0 + 1 + i * 5, sy0 - 3, 3, 3, i % 2 ? C_WHITE : RGB(120, 80, 50));
        menu_board(c, (int)(v->bl + (v->br - v->bl) * 0.76), (int)v->bt + 4); /* (the stairs cover it when they are there) */
    }
}

static void paint_park(const Theme *th, Canvas *c, const View *v) {
    int w = c->w, h = c->h;
    for (int y = 0; y < (int)v->bb; y++) canvas_rect(c, 0, y, w, 1, rgb_mix(th->wall, th->wall2, (double)y / v->bb)); /* sky */
    int cl[2][2] = {{w / 7, (int)v->bt}, {w / 2, (int)v->bt - 3}};
    for (int i = 0; i < 2; i++) { canvas_rect(c, cl[i][0], cl[i][1], 14, 3, RGB(250, 250, 255)); canvas_rect(c, cl[i][0] + 3, cl[i][1] - 2, 7, 2, RGB(250, 250, 255)); }
    quad(c, 0, v->bb, w * 0.24, v->bb - 8, w * 0.47, v->bb, 0, v->bb, RGB(130, 176, 124)); /* hills */
    quad(c, w * 0.3, v->bb, w * 0.62, v->bb - 11, w * 0.97, v->bb, w * 0.3, v->bb, RGB(116, 164, 112));
    for (int y = (int)v->bb; y < h; y++)
        for (int x = 0; x < w; x++) { /* mowed stripes running to the back */
            double X, Z;
            view_world(v, x + 0.5, y + 0.5, &X, &Z);
            canvas_px(c, x, y, ((int)floor((X + 8) / (v->xw / 4)) & 1) ? th->floor : th->floor2);
        }
    for (int i = 0; i < 4; i++) { /* bushes along both sides, nearer = bigger */
        double y = v->bb + 2 + (h - v->bb) * i / 4.0, xl, xr;
        view_floor_span(v, y, &xl, &xr);
        blob(c, (int)xl, (int)y, 2 + i, RGB(60, 120, 62));
        blob(c, (int)xr, (int)y, 2 + i, RGB(56, 114, 58));
    }
    int tx = (int)(w * TREE_X), ty = (int)v->bb + 2; /* the big tree at the back (its treehouse comes with the gate up) */
    canvas_rect(c, tx - 2, ty - 26, 5, 26, RGB(120, 86, 56));
    blob(c, tx, ty - 30, 6, RGB(70, 140, 70));
    blob(c, tx - 8, ty - 25, 4, RGB(66, 132, 66));
    Spots sp = theme_spots(th, w, h); /* the playground: a swing and a slide on the grass */
    Rgb pw = RGB(112, 88, 64);
    canvas_rect(c, sp.swing.x - 11, sp.swing.bar, 23, 2, pw);
    canvas_rect(c, sp.swing.x - 11, sp.swing.bar, 2, sp.swing.y + 8 - sp.swing.bar, pw);
    canvas_rect(c, sp.swing.x + 10, sp.swing.bar, 2, sp.swing.y + 8 - sp.swing.bar, pw);
    int stx = sp.slide.top[0], sty = sp.slide.top[1], sby = sp.slide.base[1] + 8;
    canvas_rect(c, stx - 5, sty + 8, 2, sby - sty - 8, pw);
    canvas_rect(c, stx - 1, sty + 8, 2, sby - sty - 8, pw);
    for (int ry = sty + 11; ry < sby; ry += 3) canvas_rect(c, stx - 5, ry, 6, 1, pw);
    canvas_rect(c, stx - 5, sty + 8, 12, 2, pw);
    for (int i = 0, steps = sp.slide.bottom[0] - stx - 4; i < steps; i++) {
        int off = (int)((double)(sp.slide.bottom[1] + 6 - sty - 8) * i / (steps > 1 ? steps : 1));
        canvas_rect(c, stx + 4 + i, sty + 10 + off, 1, 2, RGB(170, 170, 180));
    }
}

/* ---- the workspaces (at the front edge (cx, by); the Clawd sits behind, its body top at by + SEAT_DY) -------------------- */
static void monitor(Canvas *c, int x, int y, int on, Rgb color, double t) {
    canvas_rect(c, x, y, 9, 7, RGB(60, 60, 70));
    canvas_rect(c, x + 1, y + 1, 7, 5, on ? SCREEN_ON : SCREEN_OFF);
    if (on) for (int i = 0; i < 3; i++) canvas_rect(c, x + 2, y + 1 + (i * 2 + (int)(t * 8)) % 5, 5 - (i % 2) * 2, 1, color); /* scrolling lines */
    canvas_rect(c, x + 4, y + 7, 1, 1, RGB(60, 60, 70));
}
static void laptop(Canvas *c, int x, int y, int on, Rgb color, double t) {
    canvas_rect(c, x, y, 7, 5, RGB(70, 72, 84));
    canvas_rect(c, x + 1, y + 1, 5, 3, on ? SCREEN_ON : RGB(120, 200, 230));
    if (on) canvas_rect(c, x + 2, y + 1 + (int)(t * 6) % 3, 3, 1, color);
    canvas_rect(c, x - 1, y + 5, 9, 1, RGB(150, 152, 164));
}
static void phone(Canvas *c, int x, int y, int on) { canvas_rect(c, x, y, 2, 3, RGB(40, 40, 48)); canvas_px(c, x, y + 1, on ? RGB(150, 230, 250) : RGB(110, 180, 220)); }
static void tablet(Canvas *c, int x, int y, int on) { canvas_rect(c, x, y, 5, 4, RGB(40, 40, 48)); canvas_rect(c, x + 1, y + 1, 3, 2, on ? RGB(170, 230, 250) : RGB(130, 200, 230)); }
static void drink(Canvas *c, int x, int y, int matcha, int on, double t) {
    canvas_rect(c, x, y, 3, 3, C_WHITE);
    canvas_rect(c, x, y, 3, 1, matcha ? RGB(120, 190, 90) : RGB(120, 76, 50));
    canvas_px(c, x + 3, y + 1, C_WHITE);
    if (on) canvas_px(c, x + 1, y - 1 - (int)(t * 2) % 3, RGB(230, 230, 236)); /* a little steam */
}
/* a box standing on the floor whose front edge is the screen point (cx, by): w px wide, hgt px high, a top face of top_px */
static void floor_box(Canvas *c, const View *v, int cx, int by, double w, double hgt, double top_px, Rgb top, Rgb front, Rgb side) {
    double X, Z;
    view_world(v, cx, by, &X, &Z);
    double ww = w * Z / v->f / 2, dz = top_px * Z * Z / v->f;
    view_box(c, v, X - ww, X + ww, Z, Z + dz, hgt * Z / v->f, top, front, side); /* (the same size on screen at any depth, like the sprites) */
}

static void office_desk(const Theme *th, Canvas *c, const View *v, int cx, int by, int on, Rgb color, double t) {
    floor_box(c, v, cx, by, 24, 3, 2, th->desk_top, th->desk, dk(th->desk, 0.2));
    monitor(c, cx + 3, by - 12, on, color, t);
}
static void bar(const Theme *th, Canvas *c, const View *v, int cx, int by, int on, double t) { /* low, so the barista shows above it */
    floor_box(c, v, cx, by, 36, 4, 2, th->counter_top, th->counter, dk(th->counter, 0.15));
    int mx = cx + 6, my = by - 11; /* the espresso machine on the bar */
    canvas_rect(c, mx, my, 9, 7, STEEL);
    canvas_rect(c, mx + 1, my + 1, 7, 2, STEEL_DARK);
    canvas_rect(c, mx + 3, my + 4, 3, 1, RGB(60, 60, 66));
    canvas_px(c, mx + 7, my + 1, (on && (int)(t * 4) % 2) ? RGB(240, 80, 70) : RGB(140, 50, 50));
    drink(c, mx + 3, my + 5 - 2, 0, on, t);
    drink(c, cx - 14, by - 7, 1, 0, t);
}
static void cafe_table(Canvas *c, const View *v, int cx, int by) { /* a round table on one leg */
    double X, Z;
    view_world(v, cx, by, &X, &Z);
    int ty = by - (int)(6 / Z), r = (int)(7 / Z);
    canvas_rect(c, cx, ty + 1, 1, by - ty - 1, RGB(80, 70, 66));
    canvas_rect(c, cx - 2, by - 1, 5, 1, RGB(80, 70, 66));
    canvas_rect(c, cx - r + 1, ty - 1, 2 * r - 1, 1, RGB(236, 226, 210));
    canvas_rect(c, cx - r, ty, 2 * r + 1, 1, RGB(214, 200, 182));
    canvas_rect(c, cx - r + 1, ty + 1, 2 * r - 1, 1, RGB(150, 136, 120));
}
static void square_chair(Canvas *c, int cx, int by, int back_left) { /* a cushioned seat, a solid backrest, short legs */
    int s0 = by - 4;
    canvas_rect(c, cx - 3, s0 - 1, 7, 2, CUSHION);
    canvas_rect(c, cx - 3, s0 + 1, 7, 1, CHAIR_WOOD);
    canvas_rect(c, cx - 3, s0 + 2, 1, 2, CHAIR_WOOD);
    canvas_rect(c, cx + 3, s0 + 2, 1, 2, CHAIR_WOOD);
    int bx = back_left ? cx - 4 : cx + 3;
    canvas_rect(c, bx, s0 - 7, 2, 7, CHAIR_WOOD);
    canvas_rect(c, bx, s0 - 6, 2, 4, CUSHION);
}
static void coffee_table(Canvas *c, const View *v, int cx, int by, int on, double t, unsigned variant) {
    square_chair(c, cx + 13, by - 1, 0);                                               /* the empty chair across the table */
    canvas_rect(c, cx - 9, by - 4, 9, 2, CUSHION); canvas_rect(c, cx - 9, by - 2, 9, 1, CHAIR_WOOD); /* the customer's own seat */
    cafe_table(c, v, cx + 3, by);
    int ty = by - 6 - 1;
    switch (variant % 3) { /* a laptop, a tablet or a phone */
    case 0: laptop(c, cx + 2, ty - 5, on, RGB(110, 230, 120), t); break;
    case 1: tablet(c, cx + 2, ty - 3, on); break;
    default: phone(c, cx + 3, ty - 2, on); break;
    }
    drink(c, cx - 2, ty - 2, (variant / 3) % 2, on, t); /* a coffee or a matcha */
}
static void park_spot(const Theme *th, Canvas *c, const View *v, int cx, int by, int on, double t, unsigned variant) {
    int k = variant % 3;
    if (k == 0) { /* a bench, a phone in hand */
        floor_box(c, v, cx, by, 26, 3, 2, th->desk_top, th->desk, dk(th->desk, 0.2));
        canvas_rect(c, cx - 12, by - 1, 2, 2, th->desk); canvas_rect(c, cx + 10, by - 1, 2, 2, th->desk);
        phone(c, cx + 1, by - 9, on);
    } else if (k == 1) { /* a swing, a tablet */
        Rgb pw = RGB(112, 88, 64);
        int top = by - 22;
        canvas_rect(c, cx - 16, top, 23, 2, pw);
        canvas_rect(c, cx - 16, top, 2, by - top, pw);
        canvas_rect(c, cx + 5, top, 2, by - top, pw);
        canvas_rect(c, cx - 11, top + 2, 1, 13, RGB(150, 140, 120));
        canvas_rect(c, cx + 1, top + 2, 1, 13, RGB(150, 140, 120));
        canvas_rect(c, cx - 12, by - 5, 15, 1, pw);
        tablet(c, cx + 1, by - 10, on);
    } else { /* a picnic table, a tablet on it */
        floor_box(c, v, cx, by, 26, 4, 3, th->desk_top, th->desk, dk(th->desk, 0.2));
        canvas_rect(c, cx - 12, by - 1, 2, 2, th->desk); canvas_rect(c, cx + 10, by - 1, 2, 2, th->desk);
        tablet(c, cx + 3, by - 9, on);
        canvas_px(c, cx + 10, by - 8, RGB(200, 100, 120)); canvas_px(c, cx + 10, by - 7, RGB(60, 100, 60)); /* a flower */
    }
    (void)t;
}

void theme_furniture(const Theme *th, Canvas *cv, int cx, int by, int on, Rgb color, double t, const char *role, unsigned variant) {
    View v;
    view_init(&v, cv->w, cv->h);
    if (th == &COFFEE) { if (!strcmp(role, "barista")) bar(th, cv, &v, cx, by, on, t); else coffee_table(cv, &v, cx, by, on, t, variant); }
    else if (th == &PARK) park_spot(th, cv, &v, cx, by, on, t, variant);
    else office_desk(th, cv, &v, cx, by, on, color, t);
}

/* ---- the gates ------------------------------------------------------------------------------------------------------- */
/* a door on a side wall, in the wall's plane: on the right-hand wall the door itself (frame, panelled leaf, knob; the leaf
 * opens towards the room, `open`), on the left-hand wall only the frame round a dark doorway; p: it rises from the floor */
static void wall_door(Canvas *c, const View *v, int right, double fx, double fy, double p, double open) {
    double X, Z;
    view_world(v, fx, fy, &X, &Z);
    double xw = right ? v->xw : -v->xw, z0 = Z - 0.06 * Z, z1 = Z + 0.06 * Z, e = view_e(v, 17) * p;
    if (e <= 0) return;
    wq(c, v, xw, z0, 0, xw, z1, 0, xw, z1, e + view_e(v, 1), xw, z0, e + view_e(v, 1), WOOD_LT);
    double i0 = z0 + 0.012 * Z, i1 = z1 - 0.012 * Z;
    if (!right) { wq(c, v, xw, i0, 0, xw, i1, 0, xw, i1, e, xw, i0, e, RGB(60, 50, 44)); return; }
    wq(c, v, xw, i0, 0, xw, i1, 0, xw, i1, e, xw, i0, e, RGB(60, 50, 44)); /* the doorway behind the leaf */
    double lz = i0 + (i1 - i0) * (1 - open * 0.85); /* the leaf narrows towards its hinge (at the back) as it opens */
    wq(c, v, xw, lz, 0, xw, i1, 0, xw, i1, e, xw, lz, e, LEAF);
    if (open < 0.5 && p >= 1) {
        wq(c, v, xw, i0 + 0.02 * Z, e * 0.55, xw, i1 - 0.02 * Z, e * 0.55, xw, i1 - 0.02 * Z, e * 0.88, xw, i0 + 0.02 * Z, e * 0.88, LEAF_DK);
        wq(c, v, xw, i0 + 0.02 * Z, e * 0.12, xw, i1 - 0.02 * Z, e * 0.12, xw, i1 - 0.02 * Z, e * 0.45, xw, i0 + 0.02 * Z, e * 0.45, LEAF_DK);
        canvas_px(c, (int)view_sx(v, xw, i0 + 0.025 * Z), (int)view_sy(v, i0 + 0.025 * Z, e * 0.5), KNOB);
    }
}
static void fence_gate(Canvas *c, const View *v, int right, double fx, double fy, double p) { /* the park: a fence along that side with a gate */
    double xw = right ? v->xw : -v->xw, X, Z, zf = 1.0, zb = v->zb, e = view_e(v, 8) * p;
    view_world(v, fx, fy, &X, &Z);
    if (e <= 0) return;
    for (double z = zf; z <= zb; z += 0.05) canvas_rect(c, (int)view_sx(v, xw, z), (int)view_sy(v, z, e), 1, (int)(view_sy(v, z, 0) - view_sy(v, z, e)) + 1, RGB(214, 186, 138));
    wq(c, v, xw, zf, e * 0.8, xw, zb, e * 0.8, xw, zb, e * 0.9, xw, zf, e * 0.9, RGB(214, 186, 138));
    double g0 = Z - 0.06 * Z, g1 = Z + 0.06 * Z, ge = view_e(v, 11) * p;
    wq(c, v, xw, g0, 0, xw, g1, 0, xw, g1, ge, xw, g0, ge, RGB(150, 118, 80));
    wq(c, v, xw, g0 + 0.01, view_e(v, 1), xw, g1 - 0.01, view_e(v, 1), xw, g1 - 0.01, ge * 0.85, xw, g0 + 0.01, ge * 0.85, RGB(226, 200, 150));
}
/* indoors, up: stairs against the back wall from two thirds along it up to the right-hand wall and an opening in the
 * ceiling (the coffee shop's menu on their side wall); p: the steps appear one by one */
static void back_stairs(Canvas *c, const View *v, int coffee, double p) {
    int n = 6, x0 = (int)(v->bl + (v->br - v->bl) * 2 / 3), y0 = (int)v->bb - 1, x1 = (int)v->br, y1 = (int)v->bt + 2;
    int shown = (int)ceil(n * p);
    if (shown <= 0) return;
    for (int x = x0; x < x1; x++) { int ty = y0 - (y0 - y1) * (x - x0) / (x1 - x0); if ((x - x0) * n < (x1 - x0) * shown) canvas_rect(c, x, ty, 1, y0 - ty + 1, RGB(146, 108, 82)); }
    if (p >= 1) quad(c, x1 - 10, v->bt - 3, x1 + 8, v->bt - 3, x1 + 4, v->bt + 2, x1 - 10, v->bt + 2, RGB(40, 30, 26)); /* the opening */
    for (int i = 0; i < shown; i++) {
        int x = x0 + (x1 - x0) * i / n, w = (x1 - x0) / n + 1, y = y0 - (y0 - y1) * (i + 1) / n;
        canvas_rect(c, x, y, w, 1, RGB(214, 186, 150));
        canvas_rect(c, x, y + 1, w, 2, RGB(170, 134, 102));
    }
    if (p >= 1) {
        for (int k = 0; k <= 30; k++) { double f = k / 30.0; canvas_px(c, (int)(x0 + (x1 - x0) * f), (int)(y0 - 7 - (y0 - y1) * f), RGB(90, 64, 46)); } /* hand rail */
        if (coffee && x1 - x0 > 20) menu_board(c, x1 - 17, y0 - 12);
    }
}
/* indoors, down: a stair well at the front: a trapezoid opening, its side walls going down, the steps dropping away and
 * getting darker, a railing on the far three sides; p: the opening grows */
static void stair_well(Canvas *c, const View *v, double cx, double front_y, double hgt, double p) {
    if (p <= 0) return;
    double X, Z0, Z1, Xb;
    view_world(v, cx, front_y, &X, &Z0);
    view_world(v, cx, front_y - hgt * p, &Xb, &Z1);
    double hw = 0.14 * p, x0 = X - hw, x1 = X + hw, dep = 0.045;
    wq(c, v, x0 - 0.02, Z0, 0, x1 + 0.02, Z0, 0, x1 + 0.02, Z1 + 0.01, 0, x0 - 0.02, Z1 + 0.01, 0, RGB(150, 128, 104)); /* rim */
    wq(c, v, x0, Z0, 0, x1, Z0, 0, x1, Z1, 0, x0, Z1, 0, RGB(24, 20, 18));
    wq(c, v, x0, Z0, 0, x0, Z1, 0, x0, Z1, -dep, x0, Z0, -dep, RGB(52, 44, 40));
    wq(c, v, x1, Z0, 0, x1, Z1, 0, x1, Z1, -dep, x1, Z0, -dep, RGB(52, 44, 40));
    int n = 5;
    for (int i = n - 1; i >= 0; i--) {
        double za = Z0 + (Z1 - Z0) * i / n, zb = Z0 + (Z1 - Z0) * (i + 1) / n, e = -dep * (i + 1) / n;
        wq(c, v, x0, za, e + dep / n, x1, za, e + dep / n, x1, za, e, x0, za, e, dk(RGB(150, 126, 102), 0.11 * i));
        wq(c, v, x0, za, e, x1, za, e, x1, zb, e, x0, zb, e, dk(RGB(206, 182, 150), 0.11 * i));
    }
    if (p < 1) return;
    double rh = view_e(v, 6);
    for (int sd = 0; sd < 2; sd++) {
        double xx = sd ? x1 + 0.02 : x0 - 0.02;
        for (int k = 0; k <= 2; k++) { double z = Z0 + (Z1 - Z0) * k / 2; canvas_rect(c, (int)view_sx(v, xx, z), (int)view_sy(v, z, rh), 1, (int)(view_sy(v, z, 0) - view_sy(v, z, rh)) + 1, WOOD); }
        wq(c, v, xx, Z0, rh * 0.9, xx, Z1 + 0.01, rh * 0.9, xx, Z1 + 0.01, rh, xx, Z0, rh, WOOD);
    }
    wq(c, v, x0 - 0.02, Z1 + 0.01, rh * 0.9, x1 + 0.02, Z1 + 0.01, rh * 0.9, x1 + 0.02, Z1 + 0.01, rh, x0 - 0.02, Z1 + 0.01, rh, WOOD);
}
/* the park, down: stone steps going into the ground, low stone walls round them */
static void stone_steps(Canvas *c, const View *v, double cx, double front_y, double hgt, double p) {
    if (p <= 0) return;
    double X, Z0, Z1, Xb;
    view_world(v, cx, front_y, &X, &Z0);
    view_world(v, cx, front_y - hgt * p, &Xb, &Z1);
    double hw = 0.12 * p, x0 = X - hw, x1 = X + hw, e = view_e(v, 3);
    for (int i = 0; i < 4; i++) { double za = Z0 + (Z1 - Z0) * i / 4, zb = Z0 + (Z1 - Z0) * (i + 1) / 4; wq(c, v, x0, za, 0, x1, za, 0, x1, zb, 0, x0, zb, 0, dk(RGB(170, 170, 176), 0.1 + 0.16 * (4 - i))); }
    view_box(c, v, x0 - 0.03, x0, Z0, Z1 + 0.02, e, RGB(176, 176, 182), RGB(140, 140, 148), RGB(120, 120, 128));
    view_box(c, v, x1, x1 + 0.03, Z0, Z1 + 0.02, e, RGB(176, 176, 182), RGB(140, 140, 148), RGB(120, 120, 128));
    view_box(c, v, x0 - 0.03, x1 + 0.03, Z1, Z1 + 0.03, e, RGB(176, 176, 182), RGB(140, 140, 148), RGB(120, 120, 128));
}
/* the park, up: a little house on a platform in the tree and a ladder up to it; p: the ladder grows */
static void treehouse(Canvas *c, int w, const View *v, double p) {
    int tx = (int)(w * TREE_X), base = (int)v->bb + 2, hy = base - TREEHOUSE_UP - 2, hx = tx + 1;
    canvas_rect(c, hx - 2, hy, 14, 2, RGB(150, 110, 70));
    canvas_rect(c, hx, hy - 7, 10, 7, RGB(176, 128, 82));
    quad(c, hx - 1, hy - 7, hx + 11, hy - 7, hx + 8, hy - 11, hx + 2, hy - 11, RGB(170, 70, 60));
    canvas_rect(c, hx + 6, hy - 5, 3, 5, RGB(70, 50, 36));
    int len = (int)((base + 4 - (hy + 2)) * p);
    for (int y = hy + 2; y < hy + 2 + len; y++) {
        canvas_px(c, hx + 1, y, ROPE); canvas_px(c, hx + 6, y, ROPE);
        if (y % 3 == 0) canvas_rect(c, hx + 2, y, 4, 1, RUNG);
    }
}

void theme_gate(const Theme *th, Canvas *cv, const Geo *g, int side, double p, double open) {
    const View *v = &g->view;
    if (side == SIDE_LEFT || side == SIDE_RIGHT) {
        if (th == &PARK) fence_gate(cv, v, side == SIDE_RIGHT, g->gate_out[side][0], g->gate_out[side][1], p);
        else wall_door(cv, v, side == SIDE_RIGHT, g->gate_out[side][0], g->gate_out[side][1], p, open);
    } else if (side == SIDE_UP) {
        if (th == &PARK) treehouse(cv, cv->w, v, p);
        else back_stairs(cv, v, th == &COFFEE, p);
    } else {
        double hgt = g->gate_in[SIDE_DOWN][1] - g->gate_out[SIDE_DOWN][1] + 2;
        if (th == &PARK) stone_steps(cv, v, g->gate_in[SIDE_DOWN][0], g->gate_in[SIDE_DOWN][1] + 1, hgt, p);
        else stair_well(cv, v, g->gate_in[SIDE_DOWN][0], g->gate_in[SIDE_DOWN][1] + 1, hgt, p);
    }
}

/* ---- the room's sign: an office plaque, the coffee shop's sign, a park signpost --------------------------------------- */
void theme_sign(const Theme *th, Canvas *c, const char *title) {
    View v;
    view_init(&v, c->w, c->h);
    char t[24];
    snprintf(t, sizeof t, "%.12s", title);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    int tw = text_width(t), w = tw + 6;
    if (th == &PARK) {
        int gx = (int)(c->w * 0.12), gy = (int)v.bb + 4;
        canvas_rect(c, gx, gy - 12, 1, 12, RGB(120, 86, 56));
        canvas_rect(c, gx - w / 2, gy - 20, w, 9, RGB(150, 112, 72));
        canvas_rect(c, gx - w / 2, gy - 12, w, 1, RGB(110, 80, 52));
        canvas_text(c, gx - w / 2 + 3, gy - 18, t, RGB(250, 236, 200));
        return;
    }
    Rgb board = th == &COFFEE ? RGB(120, 70, 48) : RGB(70, 84, 110), ink = th == &COFFEE ? RGB(250, 226, 170) : RGB(235, 240, 250);
    int cx = th == &COFFEE ? (int)(v.bl + (v.br - v.bl) * 0.32) : c->w / 2, y = (int)v.bt + 2;
    canvas_rect(c, cx - w / 2, y, w, 9, board);
    canvas_rect(c, cx - w / 2, y + 8, w, 1, dk(board, 0.35));
    canvas_text(c, cx - w / 2 + 3, y + 2, t, ink);
}

/* ---- common ---------------------------------------------------------------------------------------------------------- */
Spots theme_spots(const Theme *th, int w, int h) { /* the park's swing and slide, on the grass to the sides of the walking strip */
    Spots s = {0};
    if (th != &PARK) return s;
    View v;
    view_init(&v, w, h);
    int y = (int)(v.bb + (h - v.bb) * 0.62) - 8;
    s.has = 1;
    s.swing = (SwingSpot){(int)(w * 0.13), y, y - 12};
    s.slide = (SlideSpot){{(int)(w * 0.86) - 6, y + 2}, {(int)(w * 0.86) - 6, y - 12}, {(int)(w * 0.86) + 10, y + 2}};
    return s;
}

void theme_accessory(const Theme *th, Canvas *cv, int x, int y, const char *role) {
    if (th == &COFFEE && !strcmp(role, "barista")) { /* white apron with straps */
        canvas_rect(cv, x + 4, y + 3, 6, 3, RGB(226, 226, 232));
        canvas_px(cv, x + 5, y + 2, RGB(226, 226, 232));
        canvas_px(cv, x + 8, y + 2, RGB(226, 226, 232));
    }
}

const char *theme_role(const Theme *th, int index) {
    if (th != &COFFEE) return "";
    return index == 0 ? "barista" : "customer";
}

typedef struct { const Theme *th; int w, h; Canvas bg; } Cache;
static Cache cache[8];

void theme_background(const Theme *th, Canvas *cv, int banner) {
    (void)banner;
    Cache *hit = NULL, *free_slot = NULL;
    for (int i = 0; i < 8; i++) {
        if (cache[i].th == th && cache[i].w == cv->w && cache[i].h == cv->h) hit = &cache[i];
        if (!cache[i].th && !free_slot) free_slot = &cache[i];
    }
    if (hit) { memcpy(cv->p, hit->bg.p, sizeof(Rgb) * (size_t)cv->w * (size_t)cv->h); return; }
    View v;
    view_init(&v, cv->w, cv->h);
    if (th == &COFFEE) paint_coffee(th, cv, &v);
    else if (th == &PARK) paint_park(th, cv, &v);
    else paint_office(th, cv, &v);
    if (!free_slot) { free_slot = &cache[0]; canvas_free(&free_slot->bg); }
    *free_slot = (Cache){th, cv->w, cv->h, canvas_new(cv->w, cv->h, RGB(0, 0, 0))};
    memcpy(free_slot->bg.p, cv->p, sizeof(Rgb) * (size_t)cv->w * (size_t)cv->h);
}
