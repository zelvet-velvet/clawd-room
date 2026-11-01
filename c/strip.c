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

static Rgb state_col(const char *st) {
    if (!strcmp(st, "wait")) return RGB(240, 200, 60);
    if (!strcmp(st, "think")) return RGB(150, 140, 230);
    if (!strcmp(st, "idle") || !strcmp(st, "sleep")) return RGB(120, 120, 136);
    return RGB(110, 230, 120); /* work (and cheer: just done) */
}
static Rgb dim(Rgb c) { return RGB((c.r + 3 * 14) / 4, (c.g + 3 * 14) / 4, (c.b + 3 * 18) / 4); }
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

static void plate(Canvas *c, int x, const char *name, Rgb col) { /* a name on a dark plate along the top */
    char up[24];
    snprintf(up, sizeof up, "%.12s", name[0] ? name : "CLAWD");
    for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
    int w = text_width(up) + 2;
    if (x + w > c->w) x = c->w - w;
    if (x < 0) x = 0;
    canvas_rect(c, x, 0, w, 6, PLATE);
    canvas_text(c, x + 1, 0, up, col);
}

/* the picked one stands out: the rest dims outside a soft light, yellow corners at the band's edges, its name beside it */
static void picked(Canvas *c, int cx, const Session *s, double now, int name) {
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) {
            double r = fabs(x + 0.5 - cx) / PICK_LIGHT, k = r < 0.45 ? 0 : r > 1 ? 1 : (r - 0.45) / 0.55, f = 1.0 - PICK_DARK * k * k * (3 - 2 * k);
            if (f >= 0.999) continue;
            Rgb *q = &c->p[y * c->w + x];
            q->r = (uint8_t)(q->r * f + 0.5); q->g = (uint8_t)(q->g * f + 0.5); q->b = (uint8_t)(q->b * f + 0.5);
        }
    int x0 = cx - 9, x1 = cx + 8, y1 = c->h - 1;
    for (int k = 0; k < 3; k++) {
        canvas_px(c, x0 + k, 0, C_YELLOW); canvas_px(c, x0, k, C_YELLOW); canvas_px(c, x1 - k, 0, C_YELLOW); canvas_px(c, x1, k, C_YELLOW);
        canvas_px(c, x0 + k, y1, C_YELLOW); canvas_px(c, x0, y1 - k, C_YELLOW); canvas_px(c, x1 - k, y1, C_YELLOW); canvas_px(c, x1, y1 - k, C_YELLOW);
    }
    if (name) {
        int w = text_width(s->name[0] ? s->name : "CLAWD") + 2;
        plate(c, x1 + 2 + w <= c->w ? x1 + 2 : x0 - 2 - w, s->name, state_col(session_shown(s, now))); /* beside it, else on its left */
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
        if (resting(st)) p.eyes = "shut";
        draw_clawd(c, cx, feet - 7, &p, draw_frames(), C_CLAWD, th, "");
        if (!resting(st)) { /* the laptop, its screen in the state's colour */
            canvas_rect(c, cx + 8, feet - 4, 6, 3, RGB(70, 72, 84));
            canvas_rect(c, cx + 9, feet - 3, 4, 1, state_col(st));
            canvas_rect(c, cx + 7, feet - 1, 8, 1, RGB(150, 152, 164));
        }
        if (!strcmp(q->sid, pick)) { pcx = cx; ps = q; }
    }
    if (ps) picked(c, pcx, ps, now, 1);
}

static void faces(Canvas *c, Session **s, int n, int first, int shown, const char *pick, double now) {
    int x0 = (c->w - shown * STRIP_TILE) / 2, fy = (c->h - 4) / 2;
    for (int k = 0; k < shown && first + k < n; k++) {
        const Session *q = s[first + k];
        const char *st = session_shown(q, now);
        int x = x0 + k * STRIP_TILE, f = x + 3, on = !strcmp(q->sid, pick);
        Rgb col = state_col(st);
        canvas_rect(c, x, 0, STRIP_TILE - 2, c->h, dim(col));
        canvas_rect(c, x, 0, 1, c->h, col);
        canvas_rect(c, f, fy, 8, 4, C_CLAWD); /* the face; its eyes say the state */
        if (resting(st)) { canvas_rect(c, f + 1, fy + 2, 2, 1, EYE); canvas_rect(c, f + 5, fy + 2, 2, 1, EYE); }
        else if (!strcmp(st, "think")) { canvas_px(c, f + 3, fy + 1, EYE); canvas_px(c, f + 6, fy + 1, EYE); }
        else if (!strcmp(st, "wait")) { canvas_rect(c, f + 2, fy + 1, 1, 2, EYE); canvas_rect(c, f + 5, fy + 1, 1, 2, EYE); }
        else { canvas_px(c, f + 2, fy + 2, EYE); canvas_px(c, f + 5, fy + 2, EYE); }
        if (on) { canvas_rect(c, x, 0, STRIP_TILE - 2, 1, C_YELLOW); canvas_rect(c, x, c->h - 1, STRIP_TILE - 2, 1, C_YELLOW); canvas_rect(c, x, 0, 1, c->h, C_YELLOW); canvas_rect(c, x + STRIP_TILE - 3, 0, 1, c->h, C_YELLOW); }
    }
}

Canvas strip_render(Strip *st, Room *room, int cols, int rows, double t, double now) {
    Canvas c = canvas_new(cols > 0 ? cols : 1, rows > 0 ? rows * 2 : 2, TILE_BG);
    Session *s[STRIP_MAX];
    int n = sessions(room, s), is_band = rows >= STRIP_BAND_ROWS;
    int fit = (cols - 2) / (is_band ? STRIP_SLOT : STRIP_TILE);
    if (fit < 1) fit = 1;
    int shown = fit_window(st, s, n, fit, now);
    if (is_band) band(&c, room->theme, s, n, st->first, shown, st->pick, t, now);
    else faces(&c, s, n, st->first, shown, st->pick, now);
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
