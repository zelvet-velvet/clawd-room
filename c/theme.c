/* The three rooms in perspective (style A, see persp.h): the backgrounds, the workspaces, the gates, the room's sign,
 * the barista's apron and the playground spots. */
#include "theme.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

/* ---- the playground --------------------------------------------------------------------------------------------------
 * A small swing set seen from the front, and a slide at 45 degrees (mirrored: its ladder at the back right, sliding down to
 * the front left, built from world points around the floor point (fx, fy)); `sp` (if given) gets where a Clawd sits /
 * climbs / slides, for theme_spots. */
static void wline(Canvas *c, const View *v, double X0, double Z0, double E0, double X1, double Z1, double E1, Rgb col) {
    canvas_line(c, (int)view_sx(v, X0, Z0), (int)view_sy(v, Z0, E0), (int)view_sx(v, X1, Z1), (int)view_sy(v, Z1, E1), col);
}
/* a small swing set, seen from the front (like the sprites, the same size at any depth): splayed posts, a bar, two
 * chains and a seat whose top is at seat_y; the posts stand on gy */
static void swing_set(Canvas *c, int cx, int gy, int half, int seat_y) {
    Rgb post = RGB(120, 84, 56), post_lt = RGB(160, 118, 80), bar = RGB(92, 64, 44), chain = RGB(164, 164, 176), seat = RGB(196, 76, 62);
    int top = seat_y - 13;
    for (int sd = -1; sd <= 1; sd += 2) {
        int x = cx + sd * half;
        canvas_line(c, x, top, x + sd * 3, gy, post);         /* the front leg, splayed out */
        canvas_line(c, x - sd, top, x + sd * 2 - sd, gy, post_lt);
        canvas_line(c, x, top + 1, x - sd * 2, gy - 1, dk(post, 0.25)); /* the back leg */
    }
    canvas_rect(c, cx - half - 1, top - 1, 2 * half + 3, 1, lt(bar, 0.25)); /* the bar */
    canvas_rect(c, cx - half - 1, top, 2 * half + 3, 1, bar);
    for (int sd = -1; sd <= 1; sd += 2) for (int y = top + 1; y < seat_y; y++) canvas_px(c, cx + sd * 6, y, y % 2 ? chain : dk(chain, 0.2));
    canvas_rect(c, cx - 7, seat_y, 15, 1, seat);
    canvas_rect(c, cx - 7, seat_y + 1, 15, 1, dk(seat, 0.3));
}
/* the playground's swing set: where it stands and where a Clawd on it is (for theme_spots) */
static void play_swing(Canvas *c, const View *v, int w, int h, Spots *sp, int dx) {
    int cx = (int)(w * PLAY_SWING_X) + dx, gy = (int)(v->bb + (h - v->bb) * PLAY_Y), seat_y = gy - 4;
    if (sp) { sp->swing.x = cx; sp->swing.y = seat_y - 8; sp->swing.bar = seat_y - 13; return; }
    swing_set(c, cx, gy, 12, seat_y);
}
static void slide_45(Canvas *c, const View *v, double fx, double fy, Spots *sp) {
    double X, Z;
    view_world(v, fx, fy, &X, &Z);
    double lx = X + 0.04 * Z, lz = Z + 0.04 * Z, bx = X - 0.06 * Z, bz = Z - 0.05 * Z, et = view_e(v, 11) * Z, w2 = 0.012 * Z;
    if (sp) {
        sp->slide.base[0] = (int)view_sx(v, lx, lz); sp->slide.base[1] = (int)view_sy(v, lz, 0) - 8;
        sp->slide.top[0] = sp->slide.base[0]; sp->slide.top[1] = (int)view_sy(v, lz, et) - 8;
        sp->slide.bottom[0] = (int)view_sx(v, bx, bz); sp->slide.bottom[1] = (int)view_sy(v, bz, 0) - 8;
        return;
    }
    Rgb wood = RGB(112, 88, 64), metal = RGB(180, 180, 192);
    for (int sd = -1; sd <= 1; sd += 2) wline(c, v, lx + sd * w2, lz - sd * w2, 0, lx + sd * w2, lz - sd * w2, et, wood); /* the ladder */
    for (int r = 1; r <= 2; r++) { double e = et * r / 3; wline(c, v, lx - w2, lz + w2, e, lx + w2, lz - w2, e, wood); }
    double q[12] = {lx - w2, lz + w2, et, lx + w2, lz - w2, et, bx + w2, bz - w2, 0, bx - w2, bz + w2, 0};
    view_quad(c, v, q, metal);                                                                       /* the chute */
}

/* ---- backgrounds ----------------------------------------------------------------------------------------------------- */
static void paint_office(const Theme *th, Canvas *c, const View *v) {
    room_box(c, v, RGB(150, 160, 178), th->wall, th->wall2, RGB(170, 178, 196), th->floor, th->floor2, 0, th->base);
    (void)v; /* (the window and the plant are wall items, placed by the placer) */
}

static void paint_coffee(const Theme *th, Canvas *c, const View *v) {
    room_box(c, v, RGB(118, 86, 66), th->wall, th->wall2, RGB(140, 104, 80), th->floor, th->floor2, 1, th->base);
    (void)v; /* (the lamps, the shelf and the menu are wall items, placed by the placer) */
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
    int tx = (int)(w * TREE_X), ty = (int)v->bb + 2; /* the big tree at the back (its treehouse comes with the gate up) */
    canvas_rect(c, tx - 2, ty - 26, 5, 26, RGB(120, 86, 56));
    blob(c, tx, ty - 30, 6, RGB(70, 140, 70));
    blob(c, tx - 8, ty - 25, 4, RGB(66, 132, 66));
    /* (the swing and the slide are items in among the Clawds) */
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
    } else if (k == 1) { /* a swing, the Clawd on its seat with a tablet */
        swing_set(c, cx - 5, by, 11, by + SEAT_DY + 8); /* (a seated Clawd sits at cx - 5) */
        tablet(c, cx + 1, by - 9, on);
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
/* a door on a side wall, drawn column by column along the wall (so its edges are clean at this steep angle): a dark
 * outline, a light wooden frame and lintel, the leaf with a knob near its free (front) edge; the leaf swings into the room
 * while a Clawd passes (`open`), showing the dark doorway; p: it rises from the floor */
/* the first row of the floor in column x, near y (the floor's edge along a side wall is a pixel staircase) */
static int floor_edge(const View *v, int x, int y) {
    double X, Z;
    for (int k = 0; k < 4; k++) { view_world(v, x + 0.5, y - 1 + 0.5, &X, &Z); if (fabs(X) > v->xw) break; y--; }
    for (int k = 0; k < 4; k++) { view_world(v, x + 0.5, y + 0.5, &X, &Z); if (fabs(X) <= v->xw) break; y++; }
    return y;
}
static void wall_door(Canvas *c, const View *v, int right, double fx, double fy, double p, double open) {
    double X, Z;
    view_world(v, fx, fy, &X, &Z);
    double xw = right ? v->xw : -v->xw, z0 = Z * (1 - DOOR_HALF_DEPTH), z1 = Z * (1 + DOOR_HALF_DEPTH), e = view_e(v, DOOR_H_PX) * Z;
    double xa = view_sx(v, xw, z0), xb = view_sx(v, xw, z1);  /* front edge (z0) and back edge (z1, the hinge) */
    double ab = view_sy(v, z0, 0), bb = view_sy(v, z1, 0), at = view_sy(v, z0, e), bt = view_sy(v, z1, e);
    int lo = (int)floor(fmin(xa, xb)), hi = (int)floor(fmax(xa, xb)), n = hi - lo;
    if (n < 4 || p <= 0) return;
    Rgb outline = RGB(58, 40, 28), frame = RGB(176, 132, 90), hole = RGB(28, 22, 18);
    int leaf_cols = (int)((n - 3) * (1 - open * 0.8) + 0.5);
    for (int x = lo; x <= hi; x++) {
        double t = (x + 0.5 - xa) / (xb - xa); /* 0 at the front edge, 1 at the back */
        int bot = (int)(ab + (bb - ab) * t), top = (int)(bot - ((ab + (bb - ab) * t) - (at + (bt - at) * t)) * p), b0 = bot;
        bot = floor_edge(v, x, bot);  /* (down to where the floor starts in this column: no gap under the door) */
        top += bot - b0;
        int k = right ? hi - x : x - lo;      /* columns from the front edge */
        int back = n - k;                     /* columns from the back edge */
        if (k == 0 || back == 0) { canvas_rect(c, x, top - 2, 1, bot - top + 2, outline); continue; }
        canvas_px(c, x, top - 2, outline);
        canvas_rect(c, x, top - 1, 1, 2, frame);                                   /* the lintel */
        if (k == 1 || back == 1) { canvas_rect(c, x, top + 1, 1, bot - top - 1, frame); continue; } /* the posts */
        int in_leaf = back - 1 <= leaf_cols;                                       /* the leaf hangs from the back post */
        canvas_rect(c, x, top + 1, 1, bot - top - 1, in_leaf ? (back - 1 == leaf_cols ? LEAF_DK : LEAF) : hole);
        if (in_leaf && back - 1 == leaf_cols - 1 && open < 0.4 && p >= 1) canvas_px(c, x, (top + bot) / 2, KNOB);
    }
}
static void fence_gate(Canvas *c, const View *v, int right, double fx, double fy, double p, int part) { /* the park: a fence along that side (part 0) with a gate (part 1; -1 both) */
    double xw = right ? v->xw : -v->xw, X, Z, zf = 1.0, zb = v->zb, e = view_e(v, 8) * p;
    view_world(v, fx, fy, &X, &Z);
    if (e <= 0) return;
    if (part <= 0) {
        for (double z = zf; z <= zb; z += 0.05) canvas_rect(c, (int)view_sx(v, xw, z), (int)view_sy(v, z, e), 1, (int)(view_sy(v, z, 0) - view_sy(v, z, e)) + 1, RGB(214, 186, 138));
        wq(c, v, xw, zf, e * 0.8, xw, zb, e * 0.8, xw, zb, e * 0.9, xw, zf, e * 0.9, RGB(214, 186, 138));
    }
    if (part == 0) return;
    double g0 = Z - 0.06 * Z, g1 = Z + 0.06 * Z, ge = view_e(v, 11) * p;
    wq(c, v, xw, g0, 0, xw, g1, 0, xw, g1, ge, xw, g0, ge, RGB(150, 118, 80));
    wq(c, v, xw, g0 + 0.01, view_e(v, 1), xw, g1 - 0.01, view_e(v, 1), xw, g1 - 0.01, ge * 0.85, xw, g0 + 0.01, ge * 0.85, RGB(226, 200, 150));
}
/* indoors, up (as Zaviel drew it): stairs against the back wall climb from about a fifth along it to a platform half way up
 * the wall (a half floor); the platform runs along the back wall to the right-hand wall, its front face down to the floor
 * (the coffee shop's menu hangs on it) and its end meeting the right-hand wall; on the platform, at its right end, a door in
 * the back wall, dark inside: the way up. A banister runs up the stairs and along the platform's edge.
 * p: the steps appear one by one from the bottom, then the platform and the door */
void stairs_geometry(const View *v, double *x0, double *x1, double *yl) {
    *x0 = v->bl + (v->br - v->bl) * STAIRS_FOOT; *x1 = v->bl + (v->br - v->bl) * STAIRS_TOP; *yl = v->bb - (v->bb - v->bt) * PLATFORM_UP;
}
/* the stairs' steps: how many, and the top row of the tread at column x (-1 off the stairs) */
static int stairs_n(const View *v) { int d = (int)v->bb - 1 - (int)(v->bb - (v->bb - v->bt) * PLATFORM_UP); return (d + 2) / 4 < 3 ? 3 : (d + 2) / 4; }
/* the row a Clawd's feet are on at column x on the stairs: the line through the steps' front edges (the nosing), level on
 * the last step and the platform; -1 off the stairs. The hand rail runs parallel to it. */
int stairs_feet_y(const View *v, double x) {
    double fx0, fx1, fyl;
    stairs_geometry(v, &fx0, &fx1, &fyl);
    int x0 = (int)fx0, x1 = (int)fx1, y0 = (int)v->bb - 1, yl = (int)fyl, n = stairs_n(v);
    int lx = x0 + (x1 - x0) * (n - 1) / n, t0 = y0 - (y0 - yl) / n - 1, tl = yl - 1;
    if (x < x0) return -1;
    if (x >= lx) return tl;
    return (int)(t0 + (tl - t0) * (x - x0) / (double)(lx - x0) + 0.5);
}
/* the two stair styles: the coffee shop's wooden stairs with balusters, the office's steel stairs with slim steel balusters */
typedef struct { Rgb side, tread, riser, rail; int baluster_gap; } StairStyle;
static const StairStyle WOOD_STAIRS = {{118, 82, 60}, {214, 186, 150}, {140, 102, 76}, {84, 56, 40}, 7};  /* (darker than the wall and the floor) */   /* wooden balusters */
static const StairStyle STEEL_STAIRS = {{110, 116, 128}, {206, 210, 218}, {140, 146, 158}, {70, 76, 88}, 9}; /* slim steel balusters, further apart */

/* the way up from the platform: an opening in the back wall with the stairs carrying on upwards inside, into the dark,
 * a warm light at the top; its bottom on the platform floor (Zaviel kept this one over the lit archway) */
static void upstairs_opening(Canvas *c, int x, int floor_y, int w, int h, const StairStyle *st) {
    Rgb dark = RGB(22, 18, 16);
    canvas_rect(c, x - 1, floor_y - h - 1, w + 2, h + 1, st->rail);         /* the frame */
    canvas_rect(c, x, floor_y - h, w, h, dark);
    canvas_rect(c, x + 1, floor_y - h, w - 2, 1, RGB(250, 214, 140));         /* light from upstairs */
    int n = 4;
    int rise = (h - 3) / n < 1 ? 1 : (h - 3) / n;
    for (int i = 0; i < n; i++) { /* steps going up inside, fading into the dark: each a block from its tread down to the opening's floor */
        int sy = floor_y - (i + 1) * rise, sx = x + 1 + i * (w - 2) / n, sw = w - 1 - (sx - x);
        if (sy <= floor_y - h + 1) break;
        canvas_rect(c, sx, sy + 1, sw, floor_y - sy - 1, rgb_mix(st->riser, dark, 0.45 + 0.12 * i));
        canvas_rect(c, sx, sy, sw, 1, rgb_mix(st->tread, dark, 0.1 + 0.15 * i));
    }
}

static void back_stairs(Canvas *c, const View *v, int coffee, double p, int part) { /* part: -1 all, 0 the steps, 1 the platform, 2 the opening */
    const StairStyle *st = coffee ? &WOOD_STAIRS : &STEEL_STAIRS;
    double fx0, fx1, fyl;
    stairs_geometry(v, &fx0, &fx1, &fyl);
    int x0 = (int)fx0, x1 = (int)fx1, xr = (int)ceil(v->br), y0 = (int)v->bb - 1, yl = (int)fyl; /* (to the wall's last column) */
    int n = stairs_n(v);
    int shown = (int)ceil(n * p);
    if (shown <= 0) return;
    Rgb side = st->side, tread = st->tread, riser = st->riser;
    /* the steps, seen from the front: each a block from the floor up to its tread (so the side is a sawtooth), the tread a
     * lit strip on top (seen from above), a shadow line where the next step rises */
    for (int i = 0; i < shown && part <= 0; i++) {
        int sx = x0 + (x1 - x0) * i / n, ex = x0 + (x1 - x0) * (i + 1) / n, ty = y0 - (y0 - yl) * (i + 1) / n;
        canvas_rect(c, sx, ty + 1, ex - sx, y0 - ty, side);
        canvas_rect(c, sx, ty - 1, ex - sx, 2, tread);
        canvas_rect(c, sx, ty + 1, ex - sx, 1, riser);
    }
    if (p < 1 || part == 0) return;
    int dw = ARCH_W, dh = (int)fmin(ARCH_H, yl - v->bt - 2), dxl = xr - dw - ARCH_IN; /* at the right end of the platform */
    if (part == 2) { if (dh > 6) upstairs_opening(c, dxl, yl - 1, dw, dh, st); return; }
    /* the platform: its top (a strip, lit), its front face down to the floor, its end against the right-hand wall */
    canvas_rect(c, x1, yl - 1, xr - x1, 2, tread);
    canvas_rect(c, x1, yl + 1, xr - x1, y0 - yl, side);
    canvas_rect(c, x1, yl + 1, xr - x1, 1, riser);
    canvas_rect(c, x0, y0, xr - x0, 1, dk(side, 0.45)); /* a dark skirting along the floor, under the steps and the platform */
    if (part < 0 && dh > 6) upstairs_opening(c, dxl, yl - 1, dw, dh, st);
    if (coffee && xr - x1 > 20 && y0 - yl >= 11) menu_board(c, (x1 + xr) / 2 - 8, yl + 1 + (y0 - yl - 11) / 2); /* the menu on the platform's face */
}

/* the stair well at the front left (every room; the park's is stone): its front edge's y and middle x, and how far it
 * reaches across on screen (rim included) */
void well_place(const View *v, double *fy, double *wx, double *x0, double *x1) {
    double xl, xr, X, Z;
    *fy = v->h - WELL_FRONT;
    view_floor_span(v, *fy, &xl, &xr);
    *wx = xl + WELL_SIDE;
    view_world(v, *wx, *fy, &X, &Z);
    *x0 = view_sx(v, X - WELL_HALF_W - 0.02, Z); *x1 = view_sx(v, X + WELL_HALF_W + 0.02, Z);
}
/* down: a small stair well in the floor: a rim all round (wood, or stone in the park), dark inside, its steps going down
 * from the right end (at floor level, lit) to the left (deeper, darker), so they carry on the stairs that go up to the
 * right in the room below; p: the opening grows */
static void stair_well(Canvas *c, const View *v, double p, int stone) {
    if (p <= 0) return;
    double fy, wx, sx0, sx1, X, Z0, Z1, Xb;
    well_place(v, &fy, &wx, &sx0, &sx1);
    view_world(v, wx, fy + 1, &X, &Z0);
    view_world(v, wx, fy + 1 - WELL_DEEP, &Xb, &Z1);
    double hw = WELL_HALF_W * p, x0 = X - hw, x1 = X + hw, dep = view_e(v, 6) * Z0;
    Rgb rim = stone ? RGB(112, 112, 120) : RGB(96, 70, 50), rim_lt = stone ? RGB(176, 176, 182) : RGB(150, 112, 80);
    Rgb tread = stone ? RGB(190, 190, 196) : RGB(206, 182, 150), riser = stone ? RGB(132, 132, 140) : RGB(150, 126, 102);
    wq(c, v, x0 - 0.02, Z0 - 0.015, 0, x1 + 0.02, Z0 - 0.015, 0, x1 + 0.02, Z1 + 0.01, 0, x0 - 0.02, Z1 + 0.01, 0, rim);
    wq(c, v, x0 - 0.01, Z0 - 0.007, 0, x1 + 0.01, Z0 - 0.007, 0, x1 + 0.01, Z1 + 0.004, 0, x0 - 0.01, Z1 + 0.004, 0, rim_lt);
    wq(c, v, x0, Z0, 0, x1, Z0, 0, x1, Z1, 0, x0, Z1, 0, RGB(24, 20, 18));
    /* inside, seen from above the front: the back wall going down, the steps (each a tread and the riser on its left) */
    int n = 4, top = (int)view_sy(v, Z1, 0), bot = (int)view_sy(v, Z0, 0);
    for (int i = 0; i < n; i++) {
        double xa = x1 - (x1 - x0) * (i + 1) / n, xb = x1 - (x1 - x0) * i / n, e = -dep * (i + 1) / (n + 1);
        double k = 0.16 * i;
        int ca = (int)view_sx(v, xa, Z0), cb = (int)view_sx(v, xb, Z0);
        for (int x = ca; x < cb; x++) {           /* (columns, kept inside the opening) */
            int ty = (int)view_sy(v, Z1, e);      /* the tread starts where the back wall meets it */
            if (ty < top) ty = top;
            canvas_rect(c, x, top, 1, ty - top, dk(riser, 0.35 + k));  /* the back wall above this step */
            if (ty < bot) canvas_rect(c, x, ty, 1, bot - ty, dk(tread, k));
            if (x == ca && i < n - 1) canvas_rect(c, x, ty, 1, bot - ty, dk(riser, 0.2 + k)); /* the riser down to the next step */
        }
    }
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

/* the stairs' railing, drawn after the Clawds on the stairs and the platform (they walk behind it) and before the ones on
 * the floor: a newel post at the foot, a baluster on each step, the hand rail parallel to the steps up to the last one
 * (level with the platform), then level along the platform with balusters at the same spacing on */
void theme_stair_rail(const Theme *th, Canvas *c, const Geo *g, double p) {
    if (th == &PARK || p < 1) return;
    const View *v = &g->view;
    const StairStyle *st = th == &COFFEE ? &WOOD_STAIRS : &STEEL_STAIRS;
    double fx0, fx1, fyl;
    stairs_geometry(v, &fx0, &fx1, &fyl);
    int x0 = (int)fx0, x1 = (int)fx1, xr = (int)ceil(v->br), y0 = (int)v->bb - 1, yl = (int)fyl, n = stairs_n(v);
    Rgb rail = st->rail, hi = lt(rail, 0.15);
    /* 2 rows thick, 3-4 rows above the line the Clawds' feet are on (low enough to pass under their eyes) */
    int last = x0, lx = x0 + (x1 - x0) * (n - 1) / n, T = yl - 1;
    for (int x = x0; x < xr; x++) {
        int f = stairs_feet_y(v, x);
        canvas_px(c, x, f - 3, rail);
        canvas_px(c, x, f - 4, x >= lx ? hi : rail);
    }
    for (int i = 0; i < n - 1; i++) { /* a baluster on each step, from its tread up to the rail */
        int sx = x0 + (x1 - x0) * i / n, ex = x0 + (x1 - x0) * (i + 1) / n, t = y0 - (y0 - yl) * (i + 1) / n - 1;
        last = (sx + ex) / 2;
        int f = stairs_feet_y(v, last);
        canvas_rect(c, last, f - 3, 1, t - (f - 3), rail);
    }
    for (int x = last + st->baluster_gap; x < xr; x += st->baluster_gap) canvas_rect(c, x, T - 3, 1, 3, rail);
    canvas_rect(c, x0, stairs_feet_y(v, x0) - 4, 1, y0 - 1 - (stairs_feet_y(v, x0) - 4) - (y0 - 1 - (y0 - (y0 - yl) / n - 1)), rail); /* the newel post at the foot */
    (void)x1;
}

void theme_gate(const Theme *th, Canvas *cv, const Geo *g, int side, double p, double open, int part) {
    const View *v = &g->view;
    if (side == SIDE_LEFT || side == SIDE_RIGHT) {
        if (th == &PARK) fence_gate(cv, v, side == SIDE_RIGHT, g->gate_out[side][0], g->gate_out[side][1], p, part);
        else wall_door(cv, v, side == SIDE_RIGHT, g->gate_out[side][0], g->gate_out[side][1], p, open);
    } else if (side == SIDE_UP) {
        if (th == &PARK) treehouse(cv, cv->w, v, p);
        else back_stairs(cv, v, th == &COFFEE, p, part);
    } else {
        stair_well(cv, v, p, th == &PARK);
    }
}

/* ---- props drawn from sprites/props.txt when it has them (else by the code below) ---------------------------------- */
static const SpriteFile *g_props;
void theme_set_props(const SpriteFile *props) { g_props = props; }
static double g_time;
void theme_set_time(double t) { g_time = t; }
static const Sprite *prop(const char *kind) { return g_props ? sprites_by_name(g_props, kind) : NULL; }
/* a prop's frame now: "lamp", then "lamp~2", "lamp~3" ... if the file has them, about three a second */
static const Sprite *prop_frame(const char *kind) {
    const Sprite *base = prop(kind);
    if (!base) return NULL;
    int n = 1;
    char nm[80];
    for (;; n++) { snprintf(nm, sizeof nm, "%s~%d", kind, n + 1); if (!prop(nm)) break; }
    int k = n > 1 ? (int)(g_time / 0.3) % n : 0;
    if (k == 0) return base;
    snprintf(nm, sizeof nm, "%s~%d", kind, k + 1);
    return prop(nm);
}

/* ---- the rooms' wall items, as data (place.h): what, how big, where it would like to be, how much it matters ----------- */
static const WallSpec OFFICE_WALL[] = {
    {"sign", 0, 9, BAND_TOP, 0.5, 0.2, 9, WHEN_ALWAYS},
    {"window", 14, 9, BAND_TOP, 0.12, 0.6, 5, WHEN_ALWAYS},
    {"plant", 6, 12, BAND_FLOOR, 0.9, 0.1, 2, WHEN_ALWAYS},
};
static const WallSpec COFFEE_WALL[] = {
    {"sign", 0, 9, BAND_TOP, 0.32, 0.2, 9, WHEN_ALWAYS},
    {"menu", 16, 11, BAND_TOP, 0.82, 0.82, 6, WHEN_NO_UP}, /* (with stairs it hangs on the platform's face) */
    {"lamp", 5, 7, BAND_CEIL, 0.6, 0.6, 3, WHEN_ALWAYS},
    {"lamp", 5, 7, BAND_CEIL, 0.76, 0.76, 3, WHEN_ALWAYS},
    {"shelf", 24, 4, BAND_SHELF, 0.46, 0.1, 2, WHEN_ALWAYS},
};
static const WallSpec PARK_WALL[] = {
    {"signpost", 0, 20, BAND_POST, 0.12, 0.12, 9, WHEN_ALWAYS},
};
static void sign_words(const char *title, char *t, size_t n) {
    snprintf(t, n, "%.12s", title);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
}
/* rooms/<room>.txt: the same, as a text file you can edit (read again when it changes; missing: the tables above) */
static char g_rooms_dir[512];
void theme_set_rooms_dir(const char *dir) { snprintf(g_rooms_dir, sizeof g_rooms_dir, "%s", dir ? dir : ""); }
static int room_file(const Theme *th, const WallSpec **out) {
    static struct { const Theme *th; time_t mt; double checked; WallSpec spec[MAX_PLACED]; char kind[MAX_PLACED][16]; int n, ok; } F[3];
    int k = th == &COFFEE ? 1 : th == &PARK ? 2 : 0;
    if (!g_rooms_dir[0]) return -1;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;
    if (F[k].th == th && now < F[k].checked + 0.5) { *out = F[k].spec; return F[k].ok ? F[k].n : -1; }
    F[k].th = th; F[k].checked = now;
    char path[600];
    snprintf(path, sizeof path, "%s/%s.txt", g_rooms_dir, th->name);
    struct stat st;
    if (stat(path, &st) != 0) { F[k].ok = 0; return -1; }
    if (F[k].ok && st.st_mtime == F[k].mt) { *out = F[k].spec; return F[k].n; }
    FILE *f = fopen(path, "r");
    if (!f) { F[k].ok = 0; return -1; }
    F[k].mt = st.st_mtime; F[k].n = 0;
    char line[256];
    while (fgets(line, sizeof line, f) && F[k].n < MAX_PLACED) {
        char kind[32], band[16], when[16];
        int w, h, pri;
        double pr, pu;
        if (line[0] == '#' || sscanf(line, "%31s %d %d %15s %lf %lf %d %15s", kind, &w, &h, band, &pr, &pu, &pri, when) != 8) continue;
        WallSpec *sp = &F[k].spec[F[k].n];
        snprintf(F[k].kind[F[k].n], sizeof F[k].kind[0], "%s", kind);
        sp->kind = F[k].kind[F[k].n]; sp->w = w; sp->h = h; sp->prefer = pr; sp->prefer_up = pu; sp->priority = pri;
        sp->band = !strcmp(band, "ceil") ? BAND_CEIL : !strcmp(band, "floor") ? BAND_FLOOR : !strcmp(band, "shelf") ? BAND_SHELF : !strcmp(band, "post") ? BAND_POST : BAND_TOP;
        sp->when = !strcmp(when, "up") ? WHEN_UP : !strcmp(when, "no-up") ? WHEN_NO_UP : WHEN_ALWAYS;
        F[k].n++;
    }
    fclose(f);
    F[k].ok = 1;
    *out = F[k].spec;
    return F[k].n;
}
int theme_wall_specs(const Theme *th, const char *title, WallSpec *out, int max) {
    const WallSpec *src = th == &COFFEE ? COFFEE_WALL : th == &PARK ? PARK_WALL : OFFICE_WALL;
    int n = th == &COFFEE ? (int)(sizeof COFFEE_WALL / sizeof *COFFEE_WALL) : th == &PARK ? (int)(sizeof PARK_WALL / sizeof *PARK_WALL) : (int)(sizeof OFFICE_WALL / sizeof *OFFICE_WALL);
    const WallSpec *filed;
    int nf = room_file(th, &filed);
    if (nf >= 0) { src = filed; n = nf; }
    char t[24];
    sign_words(title, t, sizeof t);
    for (int i = 0; i < n && i < max; i++) {
        out[i] = src[i];
        if (out[i].w == 0 && (!strcmp(out[i].kind, "sign") || !strcmp(out[i].kind, "signpost"))) out[i].w = text_width(t) + 6; /* the sign: as wide as its words */
        const Sprite *sp = prop(out[i].kind);
        if (sp) { out[i].w = sp->w; out[i].h = sp->h; } /* (an edited prop: its own size) */
        if (out[i].w == 0 || out[i].h == 0) for (size_t q = 0; q < sizeof OFFICE_WALL / sizeof *OFFICE_WALL + sizeof COFFEE_WALL / sizeof *COFFEE_WALL; q++) { /* (no sprite: the code's own size) */
            const WallSpec *d = q < sizeof OFFICE_WALL / sizeof *OFFICE_WALL ? &OFFICE_WALL[q] : &COFFEE_WALL[q - sizeof OFFICE_WALL / sizeof *OFFICE_WALL];
            if (!strcmp(d->kind, out[i].kind)) { if (!out[i].w) out[i].w = d->w; if (!out[i].h) out[i].h = d->h; break; }
        }
    }
    return n < max ? n : max;
}
/* a placed wall item, drawn in its box */
void theme_wall_item(const Theme *th, Canvas *c, const Placed *pl, const char *title, int has_up) {
    int x0 = pl->box.x0, y0 = pl->box.y0, w = pl->box.x1 - pl->box.x0;
    char t[24];
    sign_words(title, t, sizeof t);
    if (!strcmp(pl->kind, "sign")) {
        Rgb board = th == &COFFEE ? RGB(120, 70, 48) : RGB(70, 84, 110), ink = th == &COFFEE ? RGB(250, 226, 170) : RGB(235, 240, 250);
        if (has_up) for (int k = 1; k <= 2; k++) { canvas_px(c, x0 + 2, y0 - k, RGB(70, 56, 46)); canvas_px(c, x0 + w - 3, y0 - k, RGB(70, 56, 46)); } /* cords: it hangs */
        canvas_rect(c, x0, y0, w, 9, board);
        canvas_rect(c, x0, y0 + 8, w, 1, dk(board, 0.35));
        canvas_text(c, x0 + 3, y0 + 2, t, ink);
    } else if (!strcmp(pl->kind, "signpost")) { /* the park: a board on a post */
        int gx = x0 + w / 2, gy = pl->box.y1;
        canvas_rect(c, gx, gy - 12, 1, 12, RGB(120, 86, 56));
        canvas_rect(c, x0, gy - 20, w, 9, RGB(150, 112, 72));
        canvas_rect(c, x0, gy - 12, w, 1, RGB(110, 80, 52));
        canvas_text(c, x0 + 3, gy - 18, t, RGB(250, 236, 200));
    } else if (prop(pl->kind)) sprite_blit(c, prop_frame(pl->kind), x0, y0, C_CLAWD); /* edited in sprites/props.txt (with frames: animated) */
    else if (!strcmp(pl->kind, "window")) {
        canvas_rect(c, x0, y0, 14, 9, RGB(240, 244, 250));
        canvas_rect(c, x0 + 1, y0 + 1, 12, 7, RGB(150, 200, 236));
        canvas_rect(c, x0 + 6, y0, 1, 9, RGB(240, 244, 250));
    } else if (!strcmp(pl->kind, "plant")) {
        canvas_rect(c, x0, y0, 6, 8, RGB(90, 150, 90));
        canvas_rect(c, x0 + 1, y0 + 8, 4, 4, RGB(170, 110, 80));
    } else if (!strcmp(pl->kind, "menu")) menu_board(c, x0, y0);
    else if (!strcmp(pl->kind, "lamp")) { /* hanging from the ceiling, a warm glow */
        int lx = x0 + 2;
        canvas_rect(c, lx, y0, 1, 4, RGB(80, 60, 48));
        canvas_rect(c, lx - 2, y0 + 4, 5, 2, RGB(230, 190, 110));
        canvas_px(c, lx, y0 + 6, RGB(255, 230, 150));
    } else if (!strcmp(pl->kind, "shelf")) { /* cups and beans */
        canvas_rect(c, x0, y0 + 3, 24, 1, RGB(120, 92, 66));
        for (int i = 0; i < 5; i++) canvas_rect(c, x0 + 1 + i * 5, y0, 3, 3, i % 2 ? C_WHITE : RGB(120, 80, 50));
    }
}

/* the park's bushes along the sides without a fence (nearer = bigger): where they would go; the room leaves out any that
 * would cover something */
int theme_bushes(const Theme *th, const View *v, int fence_sides, int (*out)[4], int max) {
    int n = 0;
    if (th != &PARK) return 0;
    for (int i = 0; i < 4; i++) {
        double y = v->bb + 2 + (v->h - v->bb) * i / 4.0, xl, xr;
        view_floor_span(v, y, &xl, &xr);
        if (!(fence_sides & 1) && n < max) { out[n][0] = (int)xl; out[n][1] = (int)y; out[n][2] = 2 + i; out[n][3] = 0; n++; }
        if (!(fence_sides & 2) && n < max) { out[n][0] = (int)xr; out[n][1] = (int)y; out[n][2] = 2 + i; out[n][3] = 1; n++; }
    }
    return n;
}
Box theme_bush_box(const int *b) { return (Box){b[0] - b[2] - 2, b[1] - b[2], b[0] + b[2] + 3, b[1] + b[2] + 1}; }
void theme_bush(Canvas *c, const int *b) { blob(c, b[0], b[1], b[2], b[3] ? RGB(56, 114, 58) : RGB(60, 120, 62)); }

/* the park's playground: 0 the swing set, 1 the slide; where it stands (its ground y, for the depth) */
void theme_play(const Theme *th, Canvas *c, int which, int dx) {
    if (th != &PARK) return;
    View v;
    view_init(&v, c->w, c->h);
    if (which == 0) play_swing(c, &v, c->w, c->h, NULL, dx);
    else slide_45(c, &v, c->w * PLAY_SLIDE_X + dx, v.bb + (c->h - v.bb) * PLAY_Y, NULL);
}
double theme_play_ground(const View *v, int which) { return which == 0 ? (int)(v->bb + (v->h - v->bb) * PLAY_Y) - 6 : v->bb + (v->h - v->bb) * PLAY_Y + 1; }

/* ---- measuring: what a drawing covers (drawn once on an empty canvas of the room's size) ------------------------------ */
static const Rgb KEY = {1, 2, 3};
static Box changed(const Canvas *c) {
    Box b = {c->w, c->h, -1, -1};
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) {
        if (rgb_eq(c->p[y * c->w + x], KEY)) continue;
        if (x < b.x0) b.x0 = x;
        if (y < b.y0) b.y0 = y;
        if (x + 1 > b.x1) b.x1 = x + 1;
        if (y + 1 > b.y1) b.y1 = y + 1;
    }
    if (b.x1 < 0) b = (Box){0, 0, 0, 0};
    return b;
}
Box theme_play_box(const Theme *th, int w, int h, int which) { /* (where it stands without moving it; dx shifts the box) */
    Canvas c = canvas_new(w, h, KEY);
    theme_play(th, &c, which, 0);
    Box b = changed(&c);
    canvas_free(&c);
    return b;
}
/* a workspace with its Clawd seated, relative to its front edge (cx, by): the union over its variants */
Box theme_footprint(const Theme *th, int w, int h, const char *role) {
    View v;
    view_init(&v, w, h);
    int cx = w / 2, by = (int)(v.bb + (h - v.bb) * 0.5);
    Canvas c = canvas_new(w, h, KEY);
    for (unsigned k = 0; k < 6; k++) theme_furniture(th, &c, cx, by, 1, RGB(110, 230, 120), 0, role, k);
    Box b = changed(&c);
    canvas_free(&c);
    Box me = {cx - 5 - 7, by + SEAT_DY - 5, cx - 5 + 8, by + SEAT_DY + 9}; /* the seated Clawd and the dot over it */
    if (me.x0 < b.x0) b.x0 = me.x0;
    if (me.y0 < b.y0) b.y0 = me.y0;
    if (me.x1 > b.x1) b.x1 = me.x1;
    if (me.y1 > b.y1) b.y1 = me.y1;
    return (Box){b.x0 - cx, b.y0 - by, b.x1 - cx, b.y1 - by};
}
/* what must stay clear around a gate: the steps, their foot and the opening (not the platform's face: things in front of it
 * are fine); the door, the gate in the fence, the well, the treehouse, and the floor between a gate's in and out points.
 * mask: 1 for the floor (workspaces, bushes), 2 for the wall (wall items), 4 soft (only walked on: given up when the room
 * is too full) */
int theme_keep_clear(const Theme *th, const Geo *g, int side, Box *out, int *mask, int max) {
    const View *v = &g->view;
    int n = 0;
#define KEEP(B, M) do { if (n < max) { out[n] = (B); mask[n] = (M); n++; } } while (0)
    double ix = g->gate_in[side][0], iy = g->gate_in[side][1], ox = g->gate_out[side][0], oy = g->gate_out[side][1];
    if (side == SIDE_UP && th != &PARK) {
        double fx0, fx1, fyl;
        stairs_geometry(v, &fx0, &fx1, &fyl);
        int x0 = (int)fx0, x1 = (int)fx1, xr = (int)ceil(v->br), yl = (int)fyl, bb = (int)v->bb;
        int dw = ARCH_W, dh = (int)fmin(ARCH_H, yl - v->bt - 2), dxl = xr - dw - ARCH_IN;
        KEEP(((Box){x0 - 3, yl - 9, x1 + 2, bb + 1}), 3);              /* the steps and their railing */
        KEEP(((Box){x1, yl - 9, xr + 1, bb + 1}), 2);                  /* the platform (only for the wall items) */
        KEEP(((Box){dxl - 2, yl - 2 - dh, dxl + dw + 2, yl + 1}), 3);  /* the opening */
        KEEP(((Box){x0 - 14, bb - 1, x0 + 4, bb + 10}), 5);            /* the foot of the stairs */
        return n;
    }
    Canvas c = canvas_new(v->w, v->h, KEY);
    theme_gate(th, &c, g, side, 1.0, 0.0, (th == &PARK && side < 2) ? 1 : -1); /* (the park: only the gate in the fence) */
    Box b = changed(&c);
    canvas_free(&c);
    if (b.x1 > b.x0) KEEP(box_grow(b, 1), 3);
    KEEP(((Box){(int)fmin(ix, ox) - 8, (int)fmin(iy, oy) - 10, (int)fmax(ix, ox) + 8, (int)fmax(iy, oy) + 4}), 5); /* stepping on and off */
#undef KEEP
    return n;
}

/* ---- common ---------------------------------------------------------------------------------------------------------- */
Spots theme_spots(const Theme *th, int w, int h, const int *dx) { /* the park's swing and slide (where a Clawd sits, climbs and slides), moved by dx[] */
    Spots s = {0};
    if (th != &PARK) return s;
    View v;
    view_init(&v, w, h);
    s.has = 1;
    play_swing(NULL, &v, w, h, &s, dx ? dx[0] : 0);
    slide_45(NULL, &v, w * PLAY_SLIDE_X + (dx ? dx[1] : 0), v.bb + (h - v.bb) * PLAY_Y, &s);
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
