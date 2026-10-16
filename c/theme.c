#include "theme.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STEEL RGB(140, 140, 150)
#define STEEL_DARK RGB(96, 96, 108)
#define SCREEN_OFF RGB(14, 18, 26)
#define SCREEN_ON RGB(10, 24, 20)

static const Theme OFFICE = {"office", 0.42, {34, 36, 46}, {38, 41, 52}, {62, 58, 58}, {57, 53, 53}, {48, 45, 46}, {112, 92, 76}, {128, 106, 88}, {0}, {0}};
static const Theme COFFEE = {"coffee", 0.40, {58, 48, 44}, {64, 54, 49}, {110, 98, 88}, {96, 85, 76}, {44, 34, 30}, {120, 94, 70}, {146, 118, 90}, {82, 60, 46}, {104, 78, 58}};
static const Theme PARK = {"park", 0.40, {88, 110, 126}, {96, 120, 136}, {66, 92, 68}, {58, 82, 60}, {52, 72, 52}, {98, 78, 58}, {130, 104, 76}, {0}, {0}};

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

static int noise(int x, int y) { return (int)((((int64_t)x * 73856093) ^ ((int64_t)y * 19349663)) & 255); }

/* ---- office --------------------------------------------------------------------------------- */
static void paint_office(const Theme *th, Canvas *cv, int banner, int floor_y) {
    canvas_rect(cv, 0, 0, cv->w, cv->h, th->wall);
    for (int y = banner + 4; y < floor_y; y += 8) canvas_rect(cv, 0, y, cv->w, 1, th->wall2); /* faint wall panels */
    canvas_rect(cv, 0, floor_y, cv->w, cv->h - floor_y, th->floor);
    for (int y = floor_y + 4; y < cv->h; y += 8) canvas_rect(cv, 0, y, cv->w, 1, th->floor2);
    canvas_rect(cv, 0, floor_y - 1, cv->w, 2, th->base);
}

static void table(const Theme *th, Canvas *cv, int cx, int by) {
    canvas_rect(cv, cx - 14, by + 9, 29, 2, th->desk_top);
    canvas_rect(cv, cx - 14, by + 11, 29, 4, th->desk);
}

static void laptop(Canvas *cv, int cx, int by, int on, Rgb color, double t) {
    int mx = cx + 6;
    canvas_rect(cv, mx, by + 2, 9, 7, RGB(60, 60, 70));
    canvas_rect(cv, mx + 1, by + 3, 7, 5, on ? SCREEN_ON : SCREEN_OFF);
    if (on)
        for (int i = 0; i < 3; i++) {
            int ly = (i * 2 + (int)(t * 8)) % 5;
            canvas_rect(cv, mx + 2, by + 3 + ly, 5 - (i % 2) * 2, 1, color);
        }
    canvas_rect(cv, mx - 1, by + 9, 11, 1, STEEL_DARK);
}

static void furniture_office(const Theme *th, Canvas *cv, int cx, int by, int on, Rgb color, double t) {
    table(th, cv, cx, by);
    int mx = cx + 6;
    canvas_rect(cv, mx, by, 9, 9, RGB(60, 60, 70));
    canvas_rect(cv, mx + 1, by + 1, 7, 7, on ? SCREEN_ON : SCREEN_OFF);
    if (on)
        for (int i = 0; i < 4; i++) { /* scrolling lines */
            int ly = (i * 2 + (int)(t * 8)) % 7;
            canvas_rect(cv, mx + 2, by + 1 + ly, 5 - (i % 2) * 2, 1, color);
        }
    canvas_rect(cv, mx + 3, by + 9, 3, 1, RGB(60, 60, 70));
}

/* ---- coffee shop ---------------------------------------------------------------------------- */
static void menu_board(Canvas *cv, int x, int y, int h) {
    canvas_rect(cv, x, y, 22, h, RGB(110, 84, 60));
    canvas_rect(cv, x + 1, y + 1, 20, h - 2, RGB(30, 40, 36));
    canvas_text(cv, x + 3, y + 2, "MENU", RGB(220, 220, 200));
    for (int i = 0; i < 2; i++) canvas_rect(cv, x + 3, y + 9 + i * 3, 14 - i * 4, 1, RGB(150, 150, 140));
}

static void shelf(Canvas *cv, int x, int y) {
    static const Rgb C[5] = {{200, 200, 205}, {170, 120, 90}, {200, 200, 205}, {120, 150, 130}, {200, 200, 205}};
    canvas_rect(cv, x, y + 6, 28, 1, RGB(110, 84, 60));
    for (int i = 0; i < 5; i++) {
        canvas_rect(cv, x + 2 + i * 5, y + 3, 3, 3, C[i]);
        canvas_px(cv, x + 5 + i * 5, y + 4, C[i]); /* cup handle */
    }
}

static void paint_coffee(const Theme *th, Canvas *cv, int banner, int floor_y) {
    canvas_rect(cv, 0, 0, cv->w, cv->h, th->wall);
    for (int x = 0; x < cv->w; x += 6) canvas_rect(cv, x, floor_y - 9, 1, 9, th->wall2); /* wainscot panelling */
    canvas_rect(cv, 0, floor_y - 10, cv->w, 1, RGB(74, 62, 56));
    for (int ty = floor_y; ty < cv->h; ty += 4) /* chequered floor tiles */
        for (int tx = 0; tx < cv->w; tx += 4) canvas_rect(cv, tx, ty, 4, 4, ((tx / 4 + ty / 4) % 2 == 0) ? th->floor : th->floor2);
    canvas_rect(cv, 0, floor_y - 1, cv->w, 2, th->base);
    int top = banner + 3;
    if (cv->w >= 90 && floor_y - top >= 9) {
        menu_board(cv, 6, top, floor_y - top < 24 ? floor_y - top : 24);
        shelf(cv, cv->w - 34, top + 3);
        int lamps[2] = {cv->w / 3, 2 * cv->w / 3};
        for (int i = 0; i < 2; i++) { /* hanging lamps with a warm glow */
            int lx = lamps[i];
            canvas_rect(cv, lx, top, 1, 3, RGB(50, 44, 40));
            canvas_rect(cv, lx - 2, top + 3, 5, 2, RGB(120, 100, 60));
            canvas_px(cv, lx, top + 5, RGB(200, 170, 100));
        }
    }
}

static void cup(Canvas *cv, int x, int y, int on, double t) {
    canvas_rect(cv, x, y, 3, 2, C_WHITE);
    canvas_px(cv, x + 3, y, C_WHITE);
    canvas_rect(cv, x, y + 2, 3, 1, RGB(190, 190, 198));
    if (on) canvas_px(cv, x + 1, y - 1 - (int)(t * 2) % 3, RGB(200, 200, 210)); /* a little steam */
}

static void counter_with_machine(const Theme *th, Canvas *cv, int cx, int by, int on, double t) {
    canvas_rect(cv, cx - 14, by + 9, 29, 2, th->counter_top);
    canvas_rect(cv, cx - 14, by + 11, 29, 4, th->counter);
    for (int x = cx - 12; x < cx + 14; x += 4) canvas_rect(cv, x, by + 11, 1, 4, RGB(70, 50, 38)); /* plank lines */
    int mx = cx + 3;
    canvas_rect(cv, mx, by + 1, 12, 8, STEEL); /* espresso machine */
    canvas_rect(cv, mx, by + 1, 12, 2, STEEL_DARK);
    canvas_px(cv, mx + 2, by + 4, (on && (int)(t * 4) % 2) ? RGB(220, 70, 60) : RGB(110, 40, 40));
    canvas_rect(cv, mx + 4, by + 5, 4, 1, STEEL_DARK);
    canvas_rect(cv, mx + 5, by + 8, 2, 1, STEEL_DARK);
    cup(cv, mx + 5, by + 7, 0, t);
    if (on)
        for (int i = 0; i < 3; i++) { /* steam */
            double k = fmod(t * 1.2 + i / 3.0, 1.0);
            canvas_px(cv, mx + 8 + (int)(sin(k * 6 + i) * 1.5), by - 1 - (int)(k * 5), RGB(220, 220, 228));
        }
}

static void furniture_coffee(const Theme *th, Canvas *cv, int cx, int by, int on, Rgb color, double t, const char *role) {
    if (!strcmp(role, "barista")) { counter_with_machine(th, cv, cx, by, on, t); return; }
    canvas_rect(cv, cx - 14, by + 9, 29, 2, th->desk_top); /* a small cafe table */
    canvas_rect(cv, cx - 14, by + 11, 29, 3, th->desk);
    canvas_rect(cv, cx - 2, by + 14, 5, 1, th->desk);
    laptop(cv, cx, by, on, color, t);
    cup(cv, cx - 11, by + 7, on, t);
}

/* ---- park ----------------------------------------------------------------------------------- */
Spots theme_spots(const Theme *th, int w, int h) {
    Spots s = {0};
    if (th != &PARK) return s;
    int floor_y = (int)(h * th->horizon);
    s.has = 1;
    s.swing = (SwingSpot){22, floor_y - 2, floor_y - 15 > 10 ? floor_y - 15 : 10};
    s.slide = (SlideSpot){{w - 30, floor_y + 4}, {w - 30, floor_y - 17 > 10 ? floor_y - 17 : 10}, {w - 10, floor_y + 2}};
    return s;
}

static void swing_set(Canvas *cv, const SwingSpot *sp, int floor_y) {
    Rgb wood = RGB(112, 88, 64);
    canvas_rect(cv, sp->x - 11, sp->bar, 23, 2, wood); /* top bar */
    int dx[2] = {-11, 10};
    for (int i = 0; i < 2; i++) canvas_rect(cv, sp->x + dx[i], sp->bar, 2, floor_y - sp->bar + 1, wood); /* A-frame legs */
}

static void slide_set(Canvas *cv, const SlideSpot *sp, int floor_y) {
    int tx = sp->top[0], ty = sp->top[1], bx = sp->bottom[0], by = sp->bottom[1];
    Rgb wood = RGB(112, 88, 64), metal = RGB(150, 150, 160);
    canvas_rect(cv, tx - 5, ty + 8, 2, floor_y - ty - 4, wood); /* ladder rails */
    canvas_rect(cv, tx - 1, ty + 8, 2, floor_y - ty - 4, wood);
    for (int ry = ty + 11; ry < floor_y + 3; ry += 3) canvas_rect(cv, tx - 5, ry, 6, 1, wood);
    canvas_rect(cv, tx - 5, ty + 8, 14, 2, wood); /* platform */
    int steps = bx - tx - 4 > 1 ? bx - tx - 4 : 1;
    for (int i = 0; i < steps; i++) { /* slanted slide */
        int off = (int)((double)(by - ty - 8) * i / steps);
        canvas_px(cv, tx + 4 + i, ty + 10 + off + 1, metal);
        canvas_px(cv, tx + 4 + i, ty + 11 + off + 1, metal);
    }
}

static void paint_park(const Theme *th, Canvas *cv, int banner, int floor_y) {
    static const Rgb FLOWER[3] = {{170, 110, 120}, {170, 160, 100}, {140, 120, 170}};
    canvas_rect(cv, 0, 0, cv->w, floor_y, th->wall);
    for (int y = banner + 2; y < floor_y; y += 6) canvas_rect(cv, 0, y, cv->w, 3, th->wall2); /* a gentle sky gradient */
    int cl[2][2] = {{cv->w / 5, banner + 6}, {3 * cv->w / 5, banner + 9}};
    for (int i = 0; i < 2; i++) { /* clouds */
        canvas_rect(cv, cl[i][0], cl[i][1], 14, 3, RGB(118, 138, 152));
        canvas_rect(cv, cl[i][0] + 3, cl[i][1] - 2, 8, 2, RGB(118, 138, 152));
    }
    for (int x = 0; x < cv->w; x += 2) { /* far hills */
        int h = (int)(4 + 3 * sin(x / 14.0) + 2 * sin(x / 5.0));
        canvas_rect(cv, x, floor_y - h, 2, h, RGB(66, 88, 82));
    }
    for (int x = 10; x < cv->w - 10; x += 37) { /* trees */
        int ty = floor_y - 12;
        canvas_rect(cv, x + 3, ty + 6, 2, 6, RGB(84, 64, 48));
        canvas_rect(cv, x, ty, 8, 7, RGB(54, 84, 60));
        canvas_rect(cv, x + 2, ty - 2, 4, 2, RGB(54, 84, 60));
    }
    canvas_rect(cv, 0, floor_y, cv->w, cv->h - floor_y, th->floor);
    for (int y = floor_y; y < cv->h; y++) /* grass texture: tufts and a few flowers */
        for (int x = 0; x < cv->w; x++) {
            int n = noise(x, y);
            if (n < 14) canvas_px(cv, x, y, th->floor2);
            else if (n < 20) canvas_px(cv, x, y, RGB(80, 108, 76));
            else if (n == 255 && (x + y) % 3 == 0) canvas_px(cv, x, y, FLOWER[(x * y) % 3]);
        }
    canvas_rect(cv, 0, floor_y - 1, cv->w, 1, th->base);
    Spots sp = theme_spots(th, cv->w, cv->h);
    swing_set(cv, &sp.swing, floor_y);
    slide_set(cv, &sp.slide, floor_y);
}

static void furniture_park(const Theme *th, Canvas *cv, int cx, int by, int on, Rgb color, double t) {
    canvas_rect(cv, cx - 14, by + 9, 29, 2, th->desk_top); /* picnic table */
    canvas_rect(cv, cx - 14, by + 11, 29, 3, th->desk);
    canvas_rect(cv, cx - 13, by + 14, 2, 3, th->desk);
    canvas_rect(cv, cx + 12, by + 14, 2, 3, th->desk);
    laptop(cv, cx, by, on, color, t);
    canvas_px(cv, cx - 11, by + 7, RGB(60, 100, 60)); /* a flower in a pot */
    canvas_px(cv, cx - 11, by + 8, RGB(60, 100, 60));
    canvas_px(cv, cx - 11, by + 6, RGB(200, 100, 120));
    canvas_rect(cv, cx - 12, by + 8, 3, 1, RGB(150, 90, 70));
}

/* ---- common --------------------------------------------------------------------------------- */
void theme_furniture(const Theme *th, Canvas *cv, int cx, int by, int on, Rgb color, double t, const char *role) {
    if (th == &COFFEE) furniture_coffee(th, cv, cx, by, on, color, t, role);
    else if (th == &PARK) furniture_park(th, cv, cx, by, on, color, t);
    else furniture_office(th, cv, cx, by, on, color, t);
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

typedef struct { const Theme *th; int w, h, banner; Canvas bg; } Cache;
static Cache cache[8];

void theme_background(const Theme *th, Canvas *cv, int banner) {
    Cache *hit = NULL, *free_slot = NULL;
    for (int i = 0; i < 8; i++) {
        if (cache[i].th == th && cache[i].w == cv->w && cache[i].h == cv->h && cache[i].banner == banner) hit = &cache[i];
        if (!cache[i].th && !free_slot) free_slot = &cache[i];
    }
    if (hit) { memcpy(cv->p, hit->bg.p, sizeof(Rgb) * (size_t)cv->w * (size_t)cv->h); return; }
    int floor_y = (int)(cv->h * th->horizon);
    if (th == &COFFEE) paint_coffee(th, cv, banner, floor_y);
    else if (th == &PARK) paint_park(th, cv, banner, floor_y);
    else paint_office(th, cv, banner, floor_y);
    if (!free_slot) { free_slot = &cache[0]; canvas_free(&free_slot->bg); }
    *free_slot = (Cache){th, cv->w, cv->h, banner, canvas_new(cv->w, cv->h, RGB(0, 0, 0))};
    memcpy(free_slot->bg.p, cv->p, sizeof(Rgb) * (size_t)cv->w * (size_t)cv->h);
}
