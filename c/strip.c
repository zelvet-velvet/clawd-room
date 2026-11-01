/* The room in a pane too small for it: a band of Clawds at laptops, or faces on tiles (see strip.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "clawd.h"
#include "strip.h"
#include "theme.h"
#include "ui.h"

const SpriteFile *draw_frames(void);

static const Rgb EYE = {20, 20, 24}, PLATE = {14, 14, 18}, TILE_BG = {12, 12, 16};
#define PICK_LIGHT 14.0 /* room pixels: the light around the picked Clawd (the rest dims) */
#define PICK_DARK 0.6   /* how much darker the rest gets (as draw.c PICK_DIM) */

int strip_wanted(int cols, int rows) { return cols < MIN_COLS || rows < MIN_ROWS; }

/* (picked 2026-11-01: an empty room saying "+1 MORE" tells nothing; the strip shows the Clawds) */
int strip_no_seat(Room *room, int w, int h) {
    static struct { int w, h, n, gates; const Theme *th; int none; } C = {-1, -1, -1, -1, NULL, 0};
    room_order(room);
    if (!room->n) return 0;
    int gates = 0;
    for (int i = 0; i < room->nnb; i++) gates |= 1 << room->nb[i].side;
    if (C.w != w || C.h != h || C.n != room->n || C.gates != gates || C.th != room->theme) { /* (geo_init is not free: once per change) */
        static Geo g;
        geo_init(&g, room, w, h);
        C.w = w; C.h = h; C.n = room->n; C.gates = gates; C.th = room->theme; C.none = g.shown == 0;
    }
    return C.none;
}

static Rgb state_col(const char *st) {
    if (!strcmp(st, "wait")) return RGB(240, 200, 60);
    if (!strcmp(st, "think")) return RGB(150, 140, 230);
    if (!strcmp(st, "idle") || !strcmp(st, "sleep")) return RGB(120, 120, 136);
    return RGB(110, 230, 120); /* work (and cheer: just done) */
}
void strip_laptop(Canvas *c, int cx, int feet, const char *st) { /* over its right arm, the screen in the state's colour */
    canvas_rect(c, cx + 8, feet - 4, 6, 3, RGB(70, 72, 84));
    canvas_rect(c, cx + 9, feet - 3, 4, 1, state_col(st));
    canvas_rect(c, cx + 7, feet - 1, 8, 1, RGB(150, 152, 164));
}
static int resting(const char *st) { return !strcmp(st, "idle") || !strcmp(st, "sleep"); }

/* the sessions the strip shows, in the room's order (not the ones walking out or visiting another room) */
static int sessions(Room *room, Session **out) {
    room_order(room);
    int n = 0;
    for (int i = 0; i < room->n && n < STRIP_MAX; i++)
        if (!room->s[i]->has_left && !room->s[i]->has_away) out[n++] = room->s[i];
    return n;
}
static int index_of(Session **s, int n, const char *sid) {
    for (int i = 0; sid[0] && i < n; i++) if (!strcmp(s[i]->sid, sid)) return i;
    return -1;
}

/* how many fit, and the window kept around the picked one; a Clawd out of view that starts waiting is picked */
static int fit_window(Strip *st, Session **s, int n, int fit, double now) {
    for (int k = 0; k < st->nwaited;) { /* forget the waits that are over */
        int i = index_of(s, n, st->waited[k]);
        if (i < 0 || strcmp(session_shown(s[i], now), "wait")) { memmove(st->waited[k], st->waited[k + 1], sizeof st->waited[0] * (size_t)(st->nwaited - k - 1)); st->nwaited--; }
        else k++;
    }
    if (n > fit)
        for (int i = 0; i < n; i++) {
            if (strcmp(session_shown(s[i], now), "wait")) continue;
            int seen = 0;
            for (int k = 0; k < st->nwaited; k++) if (!strcmp(st->waited[k], s[i]->sid)) seen = 1;
            if (seen) continue;
            if (st->nwaited < STRIP_MAX) snprintf(st->waited[st->nwaited++], sizeof st->waited[0], "%s", s[i]->sid);
            if (i < st->first || i >= st->first + fit) { snprintf(st->pick, sizeof st->pick, "%s", s[i]->sid); break; } /* (once per wait: Tab may move on) */
        }
    if (index_of(s, n, st->pick) < 0) st->pick[0] = 0; /* (it left) */
    int p = index_of(s, n, st->pick);
    if (p >= 0 && p < st->first) st->first = p;
    if (p >= 0 && p >= st->first + fit) st->first = p - fit + 1;
    if (st->first > n - fit) st->first = n - fit;
    if (st->first < 0) st->first = 0;
    return n < fit ? n : fit;
}

static void plate(Canvas *c, int x, int y, const char *name, Rgb col) { /* a name on a dark plate */
    char up[24];
    snprintf(up, sizeof up, "%.12s", name[0] ? name : "CLAWD");
    for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
    int w = text_width(up) + 2;
    if (x + w > c->w) x = c->w - w;
    if (x < 0) x = 0;
    if (y + 6 > c->h) y = c->h - 6;
    if (y < 0) y = 0;
    canvas_rect(c, x, y, w, 6, PLATE);
    canvas_text(c, x + 1, y, up, col);
}

/* the picked one stands out as in the room (draw.c draw_pick): the rest dims outside a round soft light, four yellow corners
 * frame the Clawd only (its laptop stays outside), its name beside them; cx is the Clawd's middle, top the top of its sprite */
static void picked(Canvas *c, int cx, int top, const Session *s, double now, int name) {
    double lx = cx, ly = top + 4; /* (on the Clawd: the strip has no bubble above it to keep lit) */
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) {
            double dx = (x + 0.5 - lx) / PICK_LIGHT, dy = (y + 0.5 - ly) / PICK_LIGHT, r = sqrt(dx * dx + dy * dy);
            double k = r < 0.45 ? 0 : r > 1 ? 1 : (r - 0.45) / 0.55, f = 1.0 - PICK_DARK * k * k * (3 - 2 * k);
            if (f >= 0.999) continue;
            Rgb *q = &c->p[y * c->w + x];
            q->r = (uint8_t)(q->r * f + 0.5); q->g = (uint8_t)(q->g * f + 0.5); q->b = (uint8_t)(q->b * f + 0.5);
        }
    int x0 = cx - 9, x1 = cx + 8, y0 = top - 3, y1 = top + 10; /* (the room's frame: the Clawd's box plus 2) */
    if (y0 < 0) y0 = 0;
    if (y1 > c->h - 1) y1 = c->h - 1;
    for (int i = 0; i < 4; i++) {
        canvas_px(c, x0 + i, y0, C_YELLOW); canvas_px(c, x0, y0 + i, C_YELLOW); canvas_px(c, x1 - i, y0, C_YELLOW); canvas_px(c, x1, y0 + i, C_YELLOW);
        canvas_px(c, x0 + i, y1, C_YELLOW); canvas_px(c, x0, y1 - i, C_YELLOW); canvas_px(c, x1 - i, y1, C_YELLOW); canvas_px(c, x1, y1 - i, C_YELLOW);
    }
    if (name) {
        int w = text_width(s->name[0] ? s->name : "CLAWD") + 2;
        if (y0 >= 7) plate(c, cx - w / 2, y0 - 7, s->name, state_col(session_shown(s, now))); /* above the corners when there is room ... */
        else plate(c, x1 + 2 + w <= c->w ? x1 + 2 : x0 - 2 - w, y0, s->name, state_col(session_shown(s, now))); /* ... else beside them, or on their left */
    }
}

static void band(Canvas *c, const Theme *th, Session **s, int n, int first, int shown, const char *pick, double t, double now) {
    canvas_rect(c, 0, 0, c->w, c->h, th->wall);
    canvas_rect(c, 0, c->h - 3, c->w, 3, th->floor);
    canvas_rect(c, 0, c->h - 3, c->w, 1, th->base);
    int feet = c->h - 1, x0 = (c->w - shown * STRIP_SLOT) / 2, pcx = -1;
    const Session *ps = NULL;
    for (int k = 0; k < shown && first + k < n; k++) {
        const Session *q = s[first + k];
        const char *st = session_shown(q, now);
        int cx = x0 + k * STRIP_SLOT + 7;
        Pose p = pose_for(q, st, t, now);
        if (!strcmp(st, "wait")) p.arms[0] = p.arms[1] = 2; /* waving: needs you */
        if (!strcmp(st, "sleep")) p.eyes = "shut";
        draw_clawd(c, cx, feet - 7, &p, draw_frames(), C_CLAWD, th, "");
        if (!strcmp(st, "sleep")) /* the room's Zs (draw.c), rising from its head */
            for (int z = 0; z < 3; z++) {
                double k = fmod(t * 0.7 + z / 3.0, 1.0);
                canvas_text(c, (int)(cx + 6 + k * 3), (int)(feet - 13 - k * 5), "Z", k < 0.7 ? RGB(180, 190, 240) : RGB(110, 115, 150));
            }
        if (!resting(st)) strip_laptop(c, cx, feet, st);
        if (!strcmp(q->sid, pick)) { pcx = cx; ps = q; }
    }
    if (ps) picked(c, pcx, feet - 7, ps, now, 1);
}

/* under STRIP_BAND_ROWS rows (or with v): a face per session STRIP_FACE_ROWS rows tall at most on the room's wall, a dot in the
 * state's colour in the gap on its left, halfway up (41 N), and tall eyes whose place says the state (43 P): waiting raised,
 * thinking up and to the side, idle half shut, sleeping shut */
static int tile_w(int h) { return 2 * h + 6; }
static int face_h(int ch) { return ch < STRIP_FACE_ROWS * 2 ? ch : STRIP_FACE_ROWS * 2; }
static void faces(Canvas *c, const Theme *th, Session **s, int n, int first, int shown, const char *pick, double now) {
    int h = face_h(c->h), y0 = (c->h - h) / 2, tw = tile_w(h), x0 = (c->w - shown * tw) / 2, fw = 2 * h - 2, ey = y0 + (h >= 6 ? 2 : 1), dy = y0 + h / 2 - 1;
    canvas_rect(c, 0, 0, c->w, c->h, th->wall);
    for (int k = 0; k < shown && first + k < n; k++) {
        const Session *q = s[first + k];
        const char *st = session_shown(q, now);
        int x = x0 + k * tw, f = x + 4, l = f + fw / 2 - (h >= 6 ? 3 : 2), r = f + fw / 2 + (h >= 6 ? 2 : 1);
        canvas_rect(c, x + 1, dy < y0 ? y0 : dy, 2, 2, state_col(st));
        canvas_rect(c, f, y0, fw, h, C_CLAWD);
        if (!strcmp(st, "sleep")) { canvas_rect(c, l - 1, ey + 1, 2, 1, EYE); canvas_rect(c, r, ey + 1, 2, 1, EYE); }
        else if (!strcmp(st, "idle")) { canvas_px(c, l, ey + 1, EYE); canvas_px(c, r, ey + 1, EYE); }
        else if (!strcmp(st, "think")) { canvas_rect(c, l + 1, ey - 1, 1, 2, EYE); canvas_rect(c, r + 1, ey - 1, 1, 2, EYE); }
        else if (!strcmp(st, "wait")) { canvas_rect(c, l, ey - 1, 1, 2, EYE); canvas_rect(c, r, ey - 1, 1, 2, EYE); }
        else { canvas_rect(c, l, ey, 1, 2, EYE); canvas_rect(c, r, ey, 1, 2, EYE); }
        if (!strcmp(q->sid, pick)) { /* picked: yellow brackets around the dot and the face */
            int a = x, z = f + fw, yb = y0 + h - 1;
            for (int i = 0; i < 2; i++) {
                canvas_px(c, a + i, y0, C_YELLOW); canvas_px(c, z - i, y0, C_YELLOW); canvas_px(c, a + i, yb, C_YELLOW); canvas_px(c, z - i, yb, C_YELLOW);
            }
            canvas_rect(c, a, y0, 1, h, C_YELLOW); canvas_rect(c, z, y0, 1, h, C_YELLOW);
        }
    }
}

Canvas strip_render(Strip *st, Room *room, int cols, int rows, double t, double now) {
    Canvas c = canvas_new(cols > 0 ? cols : 1, rows > 0 ? rows * 2 : 2, TILE_BG);
    Session *s[STRIP_MAX];
    int n = sessions(room, s), is_band = rows >= STRIP_BAND_ROWS && st->force != 2;
    int h0 = face_h(c.h), k = is_band ? 1 : c.h / h0; /* faces fill the height: the 3-row design k times bigger (its shape kept), */
    if (!is_band && k > 1 && tile_w(h0) * k > cols - 2) k = (cols - 2) / tile_w(h0) > 1 ? (cols - 2) / tile_w(h0) : 1; /* ... a whole face across at least */
    int fit = (cols - 2) / (is_band ? STRIP_SLOT : tile_w(h0) * k);
    if (fit < 1) fit = 1;
    int shown = fit_window(st, s, n, fit, now);
    if (is_band) band(&c, room->theme, s, n, st->first, shown, st->pick, t, now);
    else if (k <= 1) faces(&c, room->theme, s, n, st->first, shown, st->pick, now);
    else {
        Canvas small = canvas_new((cols + k - 1) / k, h0, TILE_BG), big;
        faces(&small, room->theme, s, n, st->first, shown, st->pick, now);
        big = canvas_scaled(&small, k);
        canvas_rect(&c, 0, 0, c.w, c.h, room->theme->wall);
        canvas_blit(&c, &big, (c.w - big.w) / 2, (c.h - big.h) / 2);
        canvas_free(&small); canvas_free(&big);
    }
    return c;
}

const char *strip_step(Strip *st, Room *room, double now, int dir) {
    (void)now;
    Session *s[STRIP_MAX];
    int n = sessions(room, s);
    if (!n) { st->pick[0] = 0; return NULL; }
    int i = index_of(s, n, st->pick);
    i = i < 0 ? (dir > 0 ? 0 : n - 1) : ((i + dir) % n + n) % n;
    snprintf(st->pick, sizeof st->pick, "%s", s[i]->sid);
    return s[i]->name[0] ? s[i]->name : "CLAWD";
}

void strip_unpick(Strip *st) { st->pick[0] = 0; }
