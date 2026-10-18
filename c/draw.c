/* Drawing the room: banner, gates, desks, Clawds, props and labels (port of draw_* in room_tui.py). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clawd.h"
#include "room.h"
#include "scene.h"
#include "place.h"
#include "ui.h"

static const SpriteFile *g_frames, *g_items;
void draw_set_sprites(const SpriteFile *frames, const SpriteFile *items) { g_frames = frames; g_items = items; }
const SpriteFile *draw_frames(void) { return g_frames; }

#define C_GREEN RGB(110, 230, 120)
#define C_BLUE RGB(110, 200, 240)
#define WOOD RGB(78, 62, 50)
#define ROPE RGB(186, 166, 124)
#define RUNG RGB(150, 130, 96)
#define HOLE RGB(18, 16, 20)
#define GATE_ANIM 0.8   /* seconds a gate takes to appear (door rises and opens, rope unrolls, ladder rises) */
#define RUNG_STEP 3    /* room pixels between ladder rungs, the same in both windows */
#define LADDER_HALF 3  /* the ladder rails are this far from its centre (a slim ladder; a Clawd is 14 wide) */
#define WOOD_LIP RGB(110, 86, 64)
#define DOOR_NEAR 18   /* a Clawd this close (room pixels) to a door opens it */
#define DOOR_SWING 0.3 /* seconds the leaf takes to open or close */

static int fdiv(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; } /* Python's // */
static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

/* the words over the room (names, states, "+N MORE"): drawn straight away, or, while the camera is zoomed in, kept in this
 * list and written afterwards at the normal size where they belong on screen (only the room is magnified, not the text) */
typedef struct { int x, y; char s[48]; Rgb c; } WText;
#define MAX_WTEXT 256
static WText *g_wt;
static int g_nwt;
static void wtext(Canvas *cv, int x, int y, const char *s, Rgb c) {
    if (!g_wt) { canvas_text(cv, x, y, s, c); return; }
    if (g_nwt >= MAX_WTEXT) return;
    WText *w = &g_wt[g_nwt++];
    w->x = x; w->y = y; w->c = c;
    snprintf(w->s, sizeof w->s, "%s", s);
}

/* longest start of s (upper case) that fits in `width` pixels; counts characters, not bytes */
static void fit(char *out, size_t n, const char *s, int width) {
    int k = imax(0, fdiv(width + 1, 4)), chars = 0;
    size_t o = 0;
    while (*s && chars < k) {
        int l = utf8_len((unsigned char)*s);
        for (int i = 1; i < l; i++) if (!s[i]) { l = i; break; }
        if (o + (size_t)l + 1 > n) break;
        for (int i = 0; i < l; i++) out[o++] = (l == 1 && s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
        s += l;
        chars++;
    }
    out[o] = 0;
}

/* ---- gates ----------------------------------------------------------------------------------- */

/* ---- desks, bubbles, props ------------------------------------------------------------------- */
static const char *tool_kind(const char *tool, Rgb *color, char *buf, size_t n) {
    static const struct { const char *tool, *label; Rgb c; } K[] = {
        {"Bash", "BASH", {110, 230, 120}}, {"Edit", "EDIT", {250, 210, 70}}, {"Write", "EDIT", {250, 210, 70}},
        {"MultiEdit", "EDIT", {250, 210, 70}}, {"NotebookEdit", "EDIT", {250, 210, 70}}, {"Read", "READ", {110, 200, 240}},
        {"Grep", "READ", {110, 200, 240}}, {"Glob", "READ", {110, 200, 240}}, {"WebFetch", "WEB", {220, 120, 230}},
        {"WebSearch", "WEB", {220, 120, 230}}, {"Task", "AGENT", {240, 150, 80}}, {"Agent", "AGENT", {240, 150, 80}},
    };
    if (!strncmp(tool, "mcp__", 5)) { *color = RGB(150, 220, 160); return "MCP"; }
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++)
        if (!strcmp(tool, K[i].tool)) { *color = K[i].c; return K[i].label; }
    *color = RGB(170, 170, 190);
    snprintf(buf, n, "%.5s", tool);
    for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    return buf[0] ? buf : "TOOL";
}

static void draw_desk(Canvas *cv, int cx, int by, const char *state, const char *tool, double t, const Theme *th, const char *role, unsigned variant) {
    Rgb color;
    char buf[16];
    tool_kind(tool, &color, buf, sizeof buf); /* (the screen shows the tool's colour) */
    theme_furniture(th, cv, cx, by, !strcmp(state, "work"), color, t, role, variant);
}

static int item(Canvas *cv, const char *name, int x, int y) {
    const Sprite *sp = g_items ? sprites_by_name(g_items, name) : NULL;
    if (sp) sprite_blit(cv, sp, x, y, C_CLAWD);
    return sp != NULL;
}

void draw_bubble_code(Canvas *cv, int cx, int y, const char *kind, double ph);
static void draw_bubble(Canvas *cv, int cx, int y, const char *kind, double ph) {
    char name[32];
    if (!strcmp(kind, "!")) snprintf(name, sizeof name, "bubble-excl");
    else if (!strcmp(kind, "?")) snprintf(name, sizeof name, "bubble-ques");
    else if (!strcmp(kind, "cup")) snprintf(name, sizeof name, "bubble-cup");
    else snprintf(name, sizeof name, "bubble-dots-%d", (int)(ph * 3) % 4);
    if (item(cv, name, cx - 4, y)) return; /* an edited bubble from sprites/items.txt */
    canvas_rect(cv, cx - 3, y, 7, 1, C_BUBBLE_EDGE);
    canvas_rect(cv, cx - 3, y + 6, 7, 1, C_BUBBLE_EDGE);
    canvas_rect(cv, cx - 4, y + 1, 1, 5, C_BUBBLE_EDGE);
    canvas_rect(cv, cx + 4, y + 1, 1, 5, C_BUBBLE_EDGE);
    canvas_rect(cv, cx - 3, y + 1, 7, 5, C_WHITE);
    canvas_px(cv, cx - 2, y + 7, C_BUBBLE_EDGE);
    if (!strcmp(kind, "dots")) {
        for (int i = 0; i < 3; i++) if (i < (int)(ph * 3) % 4) canvas_px(cv, cx - 2 + i * 2, y + 3, C_INK);
    } else if (!strcmp(kind, "cup")) {
        canvas_rect(cv, cx - 1, y + 2, 3, 2, RGB(190, 120, 80));
        canvas_px(cv, cx + 2, y + 2, RGB(190, 120, 80));
        canvas_px(cv, cx, y + 1, RGB(160, 160, 170));
    } else canvas_text(cv, cx - 1, y + 1, kind, C_INK);
}

void draw_bubble_code(Canvas *cv, int cx, int y, const char *kind, double ph) { draw_bubble(cv, cx, y, kind, ph); } /* (for the sprite export) */

typedef struct { int x0, y0, x1, y1; } IRect;

/* where a Clawd is drawn: on the stairs its feet are on the steps' front edges, parallel to the hand rail (its route runs
 * straight from the foot of the stairs to the top) */
#define WELL_SINK 11 /* how far a Clawd goes down the well's steps, from its right end (floor level) to its left end */
static double well_sink(const Geo *g, const Session *s) { /* how far down the well's steps it is (0: not in the well) */
    double my = g->gate_out[SIDE_DOWN][1], top = g->gate_mid[SIDE_DOWN][0], end = g->gate_out[SIDE_DOWN][0];
    if (fabs(s->py + 8 - my) > 2.5 || s->px < end - 1 || s->px > top + 1 || top - end < 1) return 0;
    double k = (top - s->px) / (top - end);
    return WELL_SINK * (k < 0 ? 0 : k > 1 ? 1 : k);
}
static double shown_py(const Geo *g, int stairs, int well, const Session *s) {
    if (well) { double d = well_sink(g, s); if (d > 0) return s->py + d; } /* down the well's steps */
    if (!stairs) return s->py;
    double cx = s->px + 0.5, feet = s->py + 8;
    double fx0, fx1, fyl;
    stairs_geometry(&g->view, &fx0, &fx1, &fyl);
    if (feet > g->view.bb + 3 || feet < fyl - 3) return s->py; /* not on the stairs */
    int fy = stairs_feet_y(&g->view, cx);
    return fy < 0 ? s->py : fy - 8;
}

/* what goes with a Clawd: part 0 its marks (a held cup, swing ropes, thinking dots, signs, Z Z Z, bubbles, the dot over
 * its head), drawn right after it; part 1 its name when it needs you, written over everything */
static void draw_extras(Canvas *cv, const Session *s, const char *state, double t, double now, int seated, const IRect *rects, int nrects, const Spots *spots, int part) {
    int cx = (int)s->px, y = (int)s->py;
    if (part == 1) {
        if (!s->has_left && !strcmp(state, "wait")) {
            char name[24];
            fit(name, sizeof name, s->name[0] ? s->name : s->sid, 19);
            int tw = text_width(name), x0 = cx - tw / 2, y0 = y - 18;
            wtext(cv, x0 + 1, y0 + 1, name, RGB(20, 20, 28));
            wtext(cv, x0, y0, name, C_WHITE);
        }
        return;
    }
    if (now < s->cup_until) { /* carrying a coffee, with a little steam */
        if (!item(cv, "cup-held", cx + 7, y + 4)) {
            canvas_rect(cv, cx + 7, y + 4, 3, 2, C_WHITE);
            canvas_px(cv, cx + 10, y + 4, C_WHITE);
        }
        canvas_px(cv, cx + 8, y + 3 - (int)(t * 2) % 3, RGB(200, 200, 210));
    }
    if (!strcmp(s->emote, "swing") && spots->has) { /* ropes from the bar to the hands */
        canvas_line(cv, spots->swing.x - 6, spots->swing.bar + 1, cx - 6, y + 4, RGB(164, 164, 176));
        canvas_line(cv, spots->swing.x + 6, spots->swing.bar + 1, cx + 6, y + 4, RGB(164, 164, 176));
        canvas_rect(cv, cx - 7, y + 8, 15, 1, RGB(196, 76, 62)); /* the seat */
        canvas_rect(cv, cx - 7, y + 9, 15, 1, RGB(137, 53, 43));
    }
    if (!strcmp(state, "think")) {
        for (int i = 0; i < 3; i++) canvas_rect(cv, cx + 8 + i * 3, y - 5 - i, 2, 2, i < (int)(t * 3) % 4 ? C_WHITE : RGB(90, 90, 120));
    } else if (!strcmp(state, "wait")) {
        if (!item(cv, "sign-wait", cx - 4, y - 11)) {
            canvas_rect(cv, cx - 4, y - 11, 9, 9, C_YELLOW);
            canvas_rect(cv, cx - 3, y - 10, 7, 7, RGB(250, 235, 150));
            canvas_text(cv, cx - 1, y - 9, "!", C_INK);
            canvas_rect(cv, cx, y - 2, 1, 1, RGB(120, 90, 40));
        }
    } else if (!strcmp(state, "sleep")) {
        for (int i = 0; i < 3; i++) {
            double k = fmod(t * 0.7 + i / 3.0, 1.0);
            canvas_text(cv, (int)(cx + 8 + k * 4), (int)(y - 2 - k * 7), "Z", k < 0.7 ? RGB(180, 190, 240) : RGB(110, 115, 150));
        }
    }
    int talk = s->chat_with[0] && !strcmp(s->stage, "talk");
    if (talk) {
        double ph = t + s->seed * 10;
        if (s->chat_kind == 0) { /* only one of the two "speaks" at a time */
            if ((int)(t * 0.7) % 2 == (strcmp(s->sid, s->chat_with) < 0 ? 0 : 1)) draw_bubble(cv, cx, y - 10, "dots", ph);
        } else if (s->chat_kind == 2 || s->chat_kind == 3) draw_bubble(cv, cx, y - 10, "!", ph);
        else if (s->chat_kind == 6 && !strcmp(s->part, "visitor")) draw_bubble(cv, cx, y - 10, "cup", ph); /* ordering a coffee */
    } else if (now < s->react_until && !strcmp(state, "idle")) draw_bubble(cv, cx, y - 10, "!", t);
    else if (s->peek_on && s->peek_has_end) draw_bubble(cv, cx, y - 10, "?", t);
    (void)seated; (void)rects; (void)nrects;
    /* over the head: a dot for the state (green busy, yellow needs you, grey idle, blue a visitor); the name only when it needs you */
    if (!s->has_left) {
        Rgb dot = s->guest ? C_BLUE : !strcmp(state, "wait") ? C_YELLOW : (!strcmp(state, "work") || !strcmp(state, "think") || !strcmp(state, "cheer")) ? C_GREEN : C_GREY;
        if (!strcmp(state, "wait")) { /* (its name instead: part 1) */
        } else {
            canvas_rect(cv, cx - 1, y - 4, 3, 3, RGB(20, 20, 28));
            canvas_rect(cv, cx - 1, y - 5, 3, 3, dot);
        }
    }
}

/* ---- layout ---------------------------------------------------------------------------------- */

/* rows of workspaces on the floor, from the back: each row's front edge (y) and how many fit across it */
typedef struct { int n; int by[16], count[16]; double x0[16], x1[16]; } Rows;
static void floor_rows(const View *v, int w, int h, Rows *r) {
    r->n = 0;
    for (double by = v->bb + fmax(9.0, (h - v->bb) * 0.24); by <= h - WALK_MIN && r->n < 16; by += ROW_PITCH) {
        double xl, xr;
        view_floor_span(v, by, &xl, &xr);
        double x0 = xl + ROW_MARGIN, x1 = xr - ROW_MARGIN;
        int cnt = imin(PER_ROW, (int)((x1 - x0) / SLOT_MIN_W));
        if (cnt < 1) continue;
        r->by[r->n] = (int)by; r->count[r->n] = cnt; r->x0[r->n] = x0; r->x1[r->n] = x1; r->n++;
    }
    (void)w;
}

/* (auto scale asks how many fit: all the rows' places as one row) */
void layout_slots(int w, int h, int n, int *banner, int *per_row, int *slot_w, int *rows_shown, int *ys) {
    View v;
    view_init(&v, w, h);
    Rows r;
    floor_rows(&v, w, h, &r);
    int cap = 0;
    for (int i = 0; i < r.n; i++) cap += r.count[i];
    *banner = 0; *per_row = cap; *rows_shown = r.n ? 1 : 0; *slot_w = SLOT_MIN_W;
    if (r.n) ys[0] = r.by[0];
    (void)n;
}

/* ---- the placer's part of the layout (Engine Design, step 2) ----------------------------------------------------------
 * After the rows and the gate points: what must stay clear (the gates, then the wall items placed around them, the park's
 * playground and posts, the bushes that fit), then the workspaces put only where they keep clear of all that. */
#define PER_ROW_MAX PER_ROW
static void add_keep(Geo *g, Box b, int mask) { if (g->nkeep < 48) { g->keep[g->nkeep] = b; g->keep_mask[g->nkeep++] = mask; } }
static int floor_clear_soft(const Geo *g, Box b, int soft) { /* soft 0: the walkways count too */
    for (int k = 0; k < g->nkeep; k++) if ((g->keep_mask[k] & 1) && !(soft && (g->keep_mask[k] & 4)) && box_hit(b, g->keep[k])) return 0;
    return 1;
}
static int floor_clear(const Geo *g, Box b) { return floor_clear_soft(g, b, 0); }
static void place_room(Geo *g, const Room *room, const Rows *r, int n) {
    const Theme *th = room->theme;
    const View *v = &g->view;
    int w = g->w, h = g->h, park = !strcmp(th->name, "park"), coffee = !strcmp(th->name, "coffee");
    for (int i = 0; i < room->nnb; i++) { int sd = room->nb[i].side; if (sd == SIDE_UP) g->has_up = 1; if (park && sd == SIDE_LEFT) g->fence_sides |= 1; if (park && sd == SIDE_RIGHT) g->fence_sides |= 2; }
    /* measured once per room, size and gates (drawing on an empty canvas costs a little) */
    static struct { const Theme *th; int w, h, gates, door_y; Box keep[48]; int mask[48], n; Box fp, bar, play[2]; } C;
    int gates = 0;
    for (int i = 0; i < room->nnb; i++) gates |= 1 << room->nb[i].side;
    if (C.th != th || C.w != w || C.h != h || C.gates != gates || C.door_y != (int)g->gate_out[SIDE_LEFT][1]) {
        C.th = th; C.w = w; C.h = h; C.gates = gates; C.door_y = (int)g->gate_out[SIDE_LEFT][1]; C.n = 0;
        for (int sd = 0; sd < SIDES; sd++) if (gates & (1 << sd)) C.n += theme_keep_clear(th, g, sd, C.keep + C.n, C.mask + C.n, 48 - C.n);
        C.fp = theme_footprint(th, w, h, "");
        if (coffee) C.bar = theme_footprint(th, w, h, "barista");
        if (park) { C.play[0] = theme_play_box(th, w, h, 0); C.play[1] = theme_play_box(th, w, h, 1); }
    }
    for (int k = 0; k < C.n; k++) add_keep(g, C.keep[k], C.mask[k]);
    /* 1 the wall items, around the gates */
    WallSpec spec[MAX_PLACED];
    Box wk[48];
    int nspec = theme_wall_specs(th, room->title[0] ? room->title : th->name, spec, MAX_PLACED), nwk = 0;
    for (int k = 0; k < g->nkeep; k++) if (g->keep_mask[k] & 2) wk[nwk++] = g->keep[k];
    place_wall(spec, nspec, g->has_up, w, v->bl, v->br, v->bt, v->bb, wk, nwk, &g->wall);
    for (int k = 0; k < g->wall.n; k++) if (g->wall.p[k].box.y1 > v->bb - 14) add_keep(g, box_grow(g->wall.p[k].box, 1), 1); /* (standing on the floor: the plant, the signpost) */
    /* 2 the park's bushes that fit */
    int cand[8][4], nc = theme_bushes(th, v, g->fence_sides, cand, 8);
    for (int k = 0; k < nc; k++) if (floor_clear(g, theme_bush_box(cand[k]))) memcpy(g->bush[g->nbush++], cand[k], sizeof cand[k]);
    for (int k = 0; k < g->nbush; k++) add_keep(g, box_grow(theme_bush_box(g->bush[k]), 1), 1);
    /* 3 the coffee shop's bar (the barista's place) first, at the back, from the left */
    int first = 0;
    if (coffee && n > 0)
        for (int row = 0; row < r->n && !first; row++) {
            double xl, xr;
            view_floor_span(v, r->by[row], &xl, &xr);
            for (int cx = (int)r->x0[row] - C.bar.x0; cx + C.bar.x1 <= (int)r->x1[row] && !first; cx += 2) {
                Box b = {cx + C.bar.x0, r->by[row] + C.bar.y0, cx + C.bar.x1, r->by[row] + C.bar.y1};
                if (b.x0 < (int)xl + 1 || b.x1 > (int)xr - 1 || !floor_clear_soft(g, b, 1)) continue;
                g->home[0][0] = cx; g->home[0][1] = r->by[row]; first = 1;
                add_keep(g, (Box){b.x0 - 1, r->by[row] - 4, b.x1 + 1, r->by[row] + 5}, 1); /* (only its own row: things in front stand in front of it) */
            }
        }
    /* 4 the workspaces: places along each row, as far apart as a workspace is wide, kept only where they are clear */
    Box fp = C.fp;
    int fw = fp.x1 - fp.x0, fc = (fp.x0 + fp.x1) / 2, need = n - first;
    int px[16][PER_ROW_MAX], pn[16] = {0}, total = 0;
    for (int soft = 0; soft < 2 && total < need; soft++) { /* (too full: try again over the walkways, keeping clear of what shows) */
        total = 0;
        for (int row = 0; row < r->n; row++) {
            pn[row] = 0;
            double x0 = r->x0[row], x1 = r->x1[row], mid = (x0 + x1) / 2;
            int k = (int)((x1 - x0) / (fw + 2));
            if (k < 1) continue;
            if (k > PER_ROW_MAX) k = PER_ROW_MAX;
            double sw = fmax(fw + 2, fmin(SLOT_MAX_W, (x1 - x0) / k)), xl, xr;
            view_floor_span(v, r->by[row], &xl, &xr);
            int by = r->by[row], lo = (int)fmax(x0, xl + 1), hi = (int)fmin(x1, xr - 1);
#define FITS(cx) ((cx) + fp.x0 >= lo && (cx) + fp.x1 <= hi && floor_clear_soft(g, (Box){(cx) + fp.x0, by + fp.y0, (cx) + fp.x1, by + fp.y1}, soft))
            for (int j = 0; j < k; j++) { /* evenly spread places ... */
                int cx = (int)(mid - sw * k / 2 + sw * (j + 0.5)) - fc;
                if (FITS(cx)) px[row][pn[row]++] = cx;
            }
            if (pn[row] < k) { /* ... or, when something is in the way, packed around it, if that gives more */
                int gx[PER_ROW_MAX], gn = 0;
                for (int cx = lo - fp.x0; cx + fp.x1 <= hi && gn < PER_ROW_MAX; cx++)
                    if (FITS(cx)) { gx[gn++] = cx; cx += fw + 1; }
                if (gn > pn[row]) { memcpy(px[row], gx, sizeof gx[0] * (size_t)gn); pn[row] = gn; }
            }
#undef FITS
            total += pn[row];
        }
    }
    int keep = need;
    if (keep > total) keep = total > 1 ? total - 1 : total; /* the last place says "+N MORE" */
    g->shown = first + keep; g->more = n - g->shown; g->has_more_pos = 0;
    int extra = (g->more && total > 1) ? 1 : 0, count = need < keep + extra ? need : keep + extra, i = 0, last_by = first ? g->home[0][1] : 0;
    for (int row = 0; row < r->n && i < count; row++) { /* fill the rows from the back; a row not filled uses its middle places */
        int in_row = pn[row] < count - i ? pn[row] : count - i;
        if (in_row <= 0) continue;
        int use[PER_ROW_MAX] = {0}, mid = (int)((r->x0[row] + r->x1[row]) / 2);
        for (int c = 0; c < in_row; c++) { /* the free place nearest the middle not taken yet */
            int best = -1;
            for (int j = 0; j < pn[row]; j++) if (!use[j] && (best < 0 || abs(px[row][j] + fc - mid) < abs(px[row][best] + fc - mid))) best = j;
            use[best] = 1;
        }
        for (int j = 0; j < pn[row]; j++) if (use[j]) {
            if (i < keep) { g->home[first + i][0] = px[row][j]; g->home[first + i][1] = r->by[row]; }
            else { g->has_more_pos = 1; g->more_pos[0] = px[row][j]; g->more_pos[1] = r->by[row] - 8; }
            add_keep(g, box_grow((Box){px[row][j] + fp.x0, r->by[row] + fp.y0, px[row][j] + fp.x1, r->by[row] + fp.y1}, 1), 1);
            i++;
        }
        if (r->by[row] > last_by) last_by = r->by[row];
    }
    if (last_by && last_by - 6 > g->ymin) { g->ymin = last_by - 6; g->ymax = g->ymax > g->ymin ? g->ymax : g->ymin; } /* (the walking strip: in front of the last row) */
    /* 5 the park's swing set and slide in the floor that is left: moved across to the nearest clear place, or left out */
    if (park) {
        for (int k = 0; k < 2; k++) {
            Box b0 = C.play[k], b = b0;
            int placed = 0;
            for (int d = 0; d <= w / 2 && !placed; d++)
                for (int sg = 0; sg < (d ? 2 : 1) && !placed; sg++) {
                    int dx = sg ? -d : d;
                    Box t = {b0.x0 + dx, b0.y0, b0.x1 + dx, b0.y1};
                    if (t.x0 < 1 || t.x1 > w - 1 || !floor_clear(g, box_grow(t, 1))) continue;
                    b = t; g->play_dx[k] = dx; placed = 1;
                }
            g->play_on[k] = placed;
            g->play[k] = b;
            if (placed) add_keep(g, box_grow(b, 1), 1);
        }
        g->nplay = 2;
        g->spots = theme_spots(th, w, h, g->play_dx);
        g->spots.no_swing = !g->play_on[0]; g->spots.no_slide = !g->play_on[1];
    }
}

/* AUTO room size: the scale the old estimate picks, or smaller until the placer seats all of this room's Clawds */
int room_auto_scale(int pw, int ph, const Room *room) { /* (never bigger pixels than the old estimate; smaller when the placer needs it) */
    static Geo g;
    for (int k = auto_scale(pw, ph, room->n); k >= 1; k--) {
        int w = pw / k, h = ph / k / 2 * 2;
        if (w < SLOT_MIN_W || h < SLOT_H || w < ROAM_W || h < ROAM_H) continue;
        geo_init(&g, room, w, h);
        if (g.more == 0) return k;
    }
    return 1;
}

void geo_init(Geo *g, const Room *room, int w, int h) {
    int n = room->n, park = !strcmp(room->theme->name, "park");
    memset(g, 0, sizeof *g);
    g->w = w; g->h = h;
    View *v = &g->view;
    view_init(v, w, h);
    g->spots = theme_spots(room->theme, w, h, NULL);
    g->floor_y = (int)v->bb;
    Rows r;
    floor_rows(v, w, h, &r);
    int slots = 0;
    for (int i = 0; i < r.n; i++) slots += r.count[i];
    int keep = n;
    if (keep > slots) keep = slots > 1 ? slots - 1 : slots; /* the last place says "+N MORE" */
    g->shown = keep;
    g->more = n - keep;
    int extra = (g->more && slots > 1) ? 1 : 0, count = imin(n, keep + extra), i = 0, last_by = 0;
    g->slot_w = r.n ? (int)((r.x1[0] - r.x0[0]) / r.count[0]) : SLOT_MIN_W;
    for (int row = 0; row < r.n && i < count; row++) { /* fill the rows from the back */
        int in_row = imin(r.count[row], count - i);
        double sw = fmin(SLOT_MAX_W, (r.x1[row] - r.x0[row]) / r.count[row]), mid = (r.x0[row] + r.x1[row]) / 2;
        for (int c = 0; c < in_row; c++, i++) {
            int cx = (int)(mid - sw * in_row / 2 + sw * (c + 0.5));
            if (i < keep) { g->home[i][0] = cx; g->home[i][1] = r.by[row]; }
            else { g->has_more_pos = 1; g->more_pos[0] = cx; g->more_pos[1] = r.by[row] - 8; }
        }
        last_by = r.by[row];
    }
    g->ymin = last_by ? last_by - 6 : (int)v->bb + 4; /* the walking strip: in front of the last row, to the front edge */
    g->ymax = imax(g->ymin, h - 9);
    /* the gates (feet positions): side doors at the foot of the side walls; up: the stairs on the back wall (indoors) or the
     * treehouse ladder (park); down: the stair well (indoors) or the stone steps (park), at the front */
    double fy = g->ymin + 8 + (g->ymax - g->ymin) * 0.3, xl, xr;
    view_floor_span(v, fy, &xl, &xr);
    g->gate_out[SIDE_LEFT][0] = xl + 2; g->gate_out[SIDE_LEFT][1] = fy; g->gate_in[SIDE_LEFT][0] = xl + 14; g->gate_in[SIDE_LEFT][1] = fy;
    g->gate_out[SIDE_RIGHT][0] = xr - 2; g->gate_out[SIDE_RIGHT][1] = fy; g->gate_in[SIDE_RIGHT][0] = xr - 14; g->gate_in[SIDE_RIGHT][1] = fy;
    if (park) {
        double lx = w * TREE_X + 4;
        g->gate_in[SIDE_UP][0] = lx; g->gate_in[SIDE_UP][1] = v->bb + 6;
        g->gate_out[SIDE_UP][0] = lx; g->gate_out[SIDE_UP][1] = v->bb - TREEHOUSE_UP;
    } else {
        double x0, x1, yl;
        stairs_geometry(v, &x0, &x1, &yl);
        g->gate_in[SIDE_UP][0] = x0 - 4; g->gate_in[SIDE_UP][1] = v->bb + 2;
        g->gate_has_mid[SIDE_UP] = 1; g->gate_mid[SIDE_UP][0] = x1 + 2; g->gate_mid[SIDE_UP][1] = yl - 1; /* up the stairs, then along the platform */
        g->gate_out[SIDE_UP][0] = v->br - ARCH_IN - ARCH_W / 2.0; g->gate_out[SIDE_UP][1] = yl - 1;       /* along it to the opening at its right end */
    }
    { /* the stair well at the front left (the park's is stone): its steps go down from its right end to its left, carrying
       * on the stairs that go up to the right below; in at the right, onto the top step, down to the left end */
        double fy, wx, wx0, wx1;
        well_place(v, &fy, &wx, &wx0, &wx1);
        double my = fy - WELL_DEEP / 2.0;
        g->gate_in[SIDE_DOWN][0] = wx1 + 6; g->gate_in[SIDE_DOWN][1] = my;
        g->gate_has_mid[SIDE_DOWN] = 1; g->gate_mid[SIDE_DOWN][0] = wx1 - 3; g->gate_mid[SIDE_DOWN][1] = my;
        g->gate_out[SIDE_DOWN][0] = wx0 + 3; g->gate_out[SIDE_DOWN][1] = my;
    }
    for (int side = 0; side < 2; side++) g->door_y[side] = fy - 8;
    g->rope_x = (int)g->gate_in[SIDE_UP][0]; g->stairs_x = (int)g->gate_in[SIDE_DOWN][0];
    g->rope_end = (int)g->gate_in[SIDE_UP][1];
    place_room(g, room, &r, n);
}


/* ---- the scene's items (draw_room adds them; each keeps its drawing code) ------------------------------------------ */
typedef struct {
    Room *room;
    const Geo *geo;
    double now, t, rail_p;
    int well;                /* there is a stair well (a gate down) */
    int active, has_up, fence_sides, np, nshown_people;
    int seated[256];
    const Session *people[512];
    const char *pstate[512];
    char roles[256][12];
} DrawCtx;
static Scene g_scene;
static int g_measure;
void draw_set_measure(int on) { g_measure = on; }
const Scene *draw_last_scene(void) { return &g_scene; }
static const char *GATE_KIND[SIDES] = {"gate-left", "gate-right", "gate-up", "gate-down"};

static void draw_background(Canvas *cv, const Room *room, int banner, int count);
static void it_background(Canvas *cv, const SceneItem *it, void *ctx) { DrawCtx *X = ctx; (void)it; draw_background(cv, X->room, X->geo->banner, X->active); }
static void it_gate(Canvas *cv, const SceneItem *it, void *ctx) {
    DrawCtx *X = ctx;
    theme_gate(X->room->theme, cv, X->geo, it->i, it->p, it->i < 2 ? X->room->door_open[it->i] : 0, it->j);
}
static void it_wall(Canvas *cv, const SceneItem *it, void *ctx) {
    DrawCtx *X = ctx;
    theme_wall_item(X->room->theme, cv, &X->geo->wall.p[it->i], X->room->title[0] ? X->room->title : X->room->theme->name, X->has_up);
}
static void it_bush(Canvas *cv, const SceneItem *it, void *ctx) { DrawCtx *X = ctx; theme_bush(cv, X->geo->bush[it->i]); }
static void it_play(Canvas *cv, const SceneItem *it, void *ctx) { DrawCtx *X = ctx; theme_play(X->room->theme, cv, it->i, X->geo->play_dx[it->i]); }
static void it_no_sessions(Canvas *cv, const SceneItem *it, void *ctx) {
    DrawCtx *X = ctx;
    (void)it;
    int mid = (int)(X->geo->view.bb + cv->h) / 2;
    wtext(cv, cv->w / 2 - text_width("NO SESSIONS") / 2, mid - 7, "NO SESSIONS", RGB(150, 150, 180));
    wtext(cv, cv->w / 2 - text_width("START CLAUDE") / 2, mid, "START CLAUDE", RGB(110, 110, 140));
}
static void it_desk(Canvas *cv, const SceneItem *it, void *ctx) {
    DrawCtx *X = ctx;
    const Session *s = X->room->s[it->i];
    draw_desk(cv, X->geo->home[it->i][0], X->geo->home[it->i][1], session_shown(s, X->now), s->tool, X->t, X->room->theme, X->roles[it->i], crc32_str(s->sid));
}
static double shown_py(const Geo *g, int stairs, int well, const Session *s);
static void it_clawd(Canvas *cv, const SceneItem *it, void *ctx) {
    DrawCtx *X = ctx;
    const Session *s = X->people[it->i];
    const char *role = "";
    if (it->i < X->nshown_people) for (int q = 0; q < X->geo->shown; q++) if (X->room->s[q] == s) role = X->roles[q];
    Pose p = pose_for(s, X->pstate[it->i], X->t, X->now);
    int x0 = 0, x1 = 0, fy = cv->h, sink = X->well && well_sink(X->geo, s) > 0;
    static Rgb keep[64 * 256];
    if (sink) { /* going down the well: the rim and the floor in front of the hole stay in front of it (cut off at the hole's edge) */
        theme_well_front(&X->geo->view, &x0, &x1, &fy);
        x0 = x0 < 0 ? 0 : x0; x1 = x1 > cv->w ? cv->w : x1;
        if (x1 - x0 > 256 || cv->h - fy > 64 || fy < 0) sink = 0;
        for (int y = fy; sink && y < cv->h; y++) memcpy(keep + (y - fy) * 256, cv->p + y * cv->w + x0, sizeof(Rgb) * (size_t)(x1 - x0));
    }
    draw_clawd(cv, (int)s->px, (int)shown_py(X->geo, X->rail_p >= 0, X->well, s), &p, g_frames, C_CLAWD, X->room->theme, role);
    for (int y = fy; sink && y < cv->h; y++) memcpy(cv->p + y * cv->w + x0, keep + (y - fy) * 256, sizeof(Rgb) * (size_t)(x1 - x0));
}
static void it_marks(Canvas *cv, const SceneItem *it, void *ctx) { /* the dot, bubbles, signs, swing ropes, a held cup */
    DrawCtx *X = ctx;
    const Session *s = X->people[it->i];
    int is_seated = 0;
    for (int q = 0; q < X->geo->shown; q++) if (X->room->s[q] == s && X->seated[q]) is_seated = 1;
    IRect rects[1];
    double sy = shown_py(X->geo, X->rail_p >= 0, X->well, s);
    if (sy != s->py) { Session tmp = *s; tmp.py = sy; draw_extras(cv, &tmp, X->pstate[it->i], X->t, X->now, is_seated, rects, 0, &X->geo->spots, it->p > 0); }
    else draw_extras(cv, s, X->pstate[it->i], X->t, X->now, is_seated, rects, 0, &X->geo->spots, it->p > 0);
}
static void it_rail(Canvas *cv, const SceneItem *it, void *ctx) { DrawCtx *X = ctx; (void)it; theme_stair_rail(X->room->theme, cv, X->geo, X->rail_p); }
static void it_particles(Canvas *cv, const SceneItem *it, void *ctx) {
    static const int HEART[3][3] = {{1, 0, 1}, {1, 1, 1}, {0, 1, 0}};
    DrawCtx *X = ctx;
    (void)it;
    for (int i = 0; i < X->room->np; i++) {
        const Particle *q = &X->room->parts[i];
        if (q->life > 0.15) {
            if (q->heart) {
                for (int ry = 0; ry < 3; ry++) for (int rx = 0; rx < 3; rx++) if (HEART[ry][rx]) canvas_px(cv, (int)(q->x - 1 + rx), (int)(q->y + ry), q->c);
            } else canvas_px(cv, (int)q->x, (int)q->y, q->c);
        }
    }
}
static void it_more(Canvas *cv, const SceneItem *it, void *ctx) {
    DrawCtx *X = ctx;
    (void)it;
    char m[24];
    snprintf(m, sizeof m, "+%d MORE", X->geo->more);
    wtext(cv, X->geo->more_pos[0] - text_width(m) / 2, X->geo->more_pos[1], m, C_WHITE);
}
/* the debug overlay (key o): each item's box in its layer's colour and its anchor, the gate points (in green, mid yellow,
 * out red) and where each Clawd is going (its goal and route, cyan) */
static void frame_rect(Canvas *cv, int x0, int y0, int x1, int y1, Rgb c) {
    for (int x = x0; x < x1; x++) { canvas_px(cv, x, y0, c); canvas_px(cv, x, y1 - 1, c); }
    for (int y = y0; y < y1; y++) { canvas_px(cv, x0, y, c); canvas_px(cv, x1 - 1, y, c); }
}
static void cross(Canvas *cv, int x, int y, Rgb c) { canvas_px(cv, x, y, c); canvas_px(cv, x - 1, y, c); canvas_px(cv, x + 1, y, c); canvas_px(cv, x, y - 1, c); canvas_px(cv, x, y + 1, c); }
static void draw_overlay(Canvas *cv, const Scene *sc, const Geo *g, const Room *room) {
    static const Rgb LC[N_LAYERS] = {{0, 0, 0}, {120, 160, 255}, {255, 150, 60}, {200, 120, 255}, {255, 80, 160}, {255, 255, 120}, {255, 255, 255}};
    for (int i = 0; i < sc->n; i++) {
        const SceneItem *it = &sc->it[i];
        if (!it->has_box || it->layer == LAYER_SKY || it->owner >= 0) continue;
        if (!strcmp(it->kind, "particles")) continue;
        frame_rect(cv, it->bx0, it->by0, it->bx1, it->by1, LC[it->layer]);
        if (it->ax || it->ay) cross(cv, (int)it->ax, (int)it->ay, LC[it->layer]);
    }
    for (int i = 0; i < room->nnb; i++) {
        int sd = room->nb[i].side;
        cross(cv, (int)g->gate_in[sd][0], (int)g->gate_in[sd][1], RGB(80, 255, 80));
        if (g->gate_has_mid[sd]) cross(cv, (int)g->gate_mid[sd][0], (int)g->gate_mid[sd][1], RGB(255, 230, 60));
        cross(cv, (int)g->gate_out[sd][0], (int)g->gate_out[sd][1], RGB(255, 60, 60));
    }
    for (int i = 0; i < room->n + room->ng; i++) {
        const Session *s = i < room->n ? room->s[i] : room->guests[i - room->n];
        if (!s->has_pos || s->has_away || !s->has_goal) continue;
        double x = s->px, y = s->py + 8;
        canvas_line(cv, (int)x, (int)y, (int)s->gx, (int)s->gy + 8, RGB(80, 230, 255));
        x = s->gx; y = s->gy + 8;
        for (int k = 0; k < s->nroute; k++) { canvas_line(cv, (int)x, (int)y, (int)s->rx[k], (int)s->ry[k] + 8, RGB(80, 230, 255)); x = s->rx[k]; y = s->ry[k] + 8; }
    }
}

static void draw_background(Canvas *cv, const Room *room, int banner, int count) {
    (void)count;
    theme_background(room->theme, cv, banner);

}

void draw_room(Canvas *cv, Room *room, double now, double t, char *note, size_t note_n) {
    Geo geo;
    room_order(room);
    geo_init(&geo, room, cv->w, cv->h);
    if (!room->freeze) { room->geo = geo; room->has_geo = 1; room_step(room, &room->geo, now); } /* everybody moves a little before they are drawn */
    room_order(room); /* (visitors may have come or gone) */
    note[0] = 0;
    int active = 0;
    for (int i = 0; i < room->n; i++) if (!room->s[i]->has_left) active++;
    { /* gates that appear while the room is running animate in; the ones there from the first frame do not */
        int present[SIDES] = {0};
        for (int i = 0; i < room->nnb; i++) present[room->nb[i].side] = 1;
        for (int sd = 0; sd < SIDES; sd++) {
            if (present[sd] && !room->gate_seen[sd]) { /* starts when the windows stopped moving (+1 s), the same moment for both rooms */
                room->gate_seen[sd] = 1;
                room->gate_t0[sd] = !room->drawn ? t - GATE_ANIM : room->has_gate_sync && room->gate_sync_t > t - GATE_ANIM ? room->gate_sync_t : t;
                char key[8];
                snprintf(key, sizeof key, "gate%d", sd);
                anim_start(&room->anim, key, 0, 1, room->gate_t0[sd], GATE_ANIM, EASE_OUT); /* (rises fast, settles) */
            }
            else if (!present[sd]) room->gate_seen[sd] = 0;
        }
        room->drawn = 1;
        room->door_t = t; /* the leaves swing open while a Clawd is near the door, and closed after (eased both ways) */
        for (int sd = SIDE_LEFT; sd <= SIDE_RIGHT; sd++) {
            int near = 0;
            for (int i = 0; i < room->n + room->ng && !near; i++) {
                const Session *q = i < room->n ? room->s[i] : room->guests[i - room->n];
                if (q->has_pos && !q->has_away && fabs(q->px - geo.gate_out[sd][0]) < DOOR_NEAR && fabs(q->py + 8 - geo.gate_out[sd][1]) < 6) near = 1;
            }
            char key[8];
            snprintf(key, sizeof key, "door%d", sd);
            if (anim_target(&room->anim, key, 0) != near) anim_to(&room->anim, key, near, t, DOOR_SWING, EASE_IN_OUT);
            room->door_open[sd] = anim_value(&room->anim, key, t, 0);
        }
    }
    /* ---- the scene: every item on its layer, then drawn back to front (scene.h) -------------------------------------- */
    theme_set_time(t);
    Scene *sc = &g_scene;
    scene_clear(sc);
    sc->measure = room->debug_overlay || g_measure;
    DrawCtx X;
    memset(&X, 0, sizeof X);
    X.room = room; X.geo = &geo; X.now = now; X.t = t; X.active = active;
    scene_add(sc, (SceneItem){.kind = "background", .layer = LAYER_SKY, .owner = -1, .draw = it_background});
    for (int i = 0; i < room->nnb; i++) {
        int side = room->nb[i].side;
        char gk[8];
        snprintf(gk, sizeof gk, "gate%d", side);
        double p = fmin(1.0, fmax(0.0, anim_value(&room->anim, gk, t, 1))); /* (eased by the animator; clamped: EASE_BACK may pass 1) */
        int park = room->theme == theme_get("park");
        if (side == SIDE_UP && !park) { /* the stairs: the steps, the platform, the opening (things may stand in front of the platform) */
            scene_add(sc, (SceneItem){.kind = "gate-up", .layer = LAYER_STRUCT, .owner = -1, .ax = geo.gate_in[side][0], .ay = geo.gate_in[side][1], .draw = it_gate, .i = side, .j = 0, .p = p});
            scene_add(sc, (SceneItem){.kind = "platform", .layer = LAYER_STRUCT, .owner = -1, .draw = it_gate, .i = side, .j = 1, .p = p});
            scene_add(sc, (SceneItem){.kind = "opening", .layer = LAYER_STRUCT, .owner = -1, .ax = geo.gate_out[side][0], .ay = geo.gate_out[side][1], .draw = it_gate, .i = side, .j = 2, .p = p});
        } else if (side < 2 && park) { /* the park's building along the side, and its door */
            scene_add(sc, (SceneItem){.kind = "building", .layer = LAYER_STRUCT, .owner = -1, .draw = it_gate, .i = side, .j = 0, .p = p});
            scene_add(sc, (SceneItem){.kind = GATE_KIND[side], .layer = LAYER_STRUCT, .owner = -1, .ax = geo.gate_out[side][0], .ay = geo.gate_out[side][1], .draw = it_gate, .i = side, .j = 1, .p = p});
        } else scene_add(sc, (SceneItem){.kind = GATE_KIND[side], .layer = LAYER_STRUCT, .owner = -1, .ax = geo.gate_out[side][0], .ay = geo.gate_out[side][1], .draw = it_gate, .i = side, .j = -1, .p = p});
    }
    /* the room's name as a sign in the room (after the gates: it hangs in front of the stairs), the park's bushes, the
     * things on the back wall */
    X.has_up = geo.has_up; X.fence_sides = geo.fence_sides;
    for (int k = 0; k < geo.wall.n; k++) /* the wall items where the placer put them (the sign hangs in front of the stairs) */
        scene_add(sc, (SceneItem){.kind = geo.wall.p[k].kind, .layer = LAYER_STRUCT, .owner = -1, .ax = (geo.wall.p[k].box.x0 + geo.wall.p[k].box.x1) / 2.0, .ay = geo.wall.p[k].box.y1, .draw = it_wall, .i = k});
    for (int k = 0; k < geo.nbush; k++) scene_add(sc, (SceneItem){.kind = "bush", .layer = LAYER_FLOOR, .owner = -1, .ax = geo.bush[k][0], .ay = geo.bush[k][1], .draw = it_bush, .i = k});
    int anyone = 0; /* any live session at all: linked rooms share them out, so this room may have none of its own */
    for (int i = 0; i < room->npool; i++) if (!room->pool[i].has_left) anyone = 1;
    if (room->n == 0 && !anyone) /* no Claude Code running anywhere: say so (visitors from other windows are still drawn) */
        scene_add(sc, (SceneItem){.kind = "no-sessions", .layer = LAYER_FLOOR, .owner = -1, .draw = it_no_sessions});
    /* who is seated at their desk, and who is in the room right now */
    for (int i = 0; i < geo.shown; i++) snprintf(X.roles[i], sizeof X.roles[i], "%s", theme_role(room->theme, i));
    for (int i = 0; i < geo.shown; i++) {
        const Session *s = room->s[i];
        if (s->has_away) continue;
        X.seated[i] = !s->moving && !s->has_left && fabs(s->px - (geo.home[i][0] - 5)) < 1 && fabs(s->py - (geo.home[i][1] + SEAT_DY)) < 1;
        X.people[X.np] = s; X.pstate[X.np++] = session_shown(s, now);
    }
    X.nshown_people = X.np;
    for (int i = 0; i < room->ng; i++) { X.people[X.np] = room->guests[i]; X.pstate[X.np++] = session_shown(room->guests[i], now); }
    /* the workspaces and the Clawds, back to front by where they stand on the floor (a desk's front edge, a Clawd's feet);
     * the stairs' railing is one more item at the back wall's foot: Clawds on the stairs and the platform are behind it */
    X.rail_p = -1;
    for (int i = 0; i < room->nnb; i++) if (room->nb[i].side == SIDE_UP) X.rail_p = fmin(1.0, fmax(0.0, anim_value(&room->anim, "gate2", t, 1)));
    double rail_key = geo.view.bb - 0.25;
    for (int i = 0; i < room->nnb; i++) if (room->nb[i].side == SIDE_DOWN) X.well = 1;
    for (int i = 0; i < geo.shown; i++)
        scene_add(sc, (SceneItem){.kind = "desk", .layer = LAYER_DEPTH, .depth = geo.home[i][1] + 0.5, .owner = -1, .ax = geo.home[i][0], .ay = geo.home[i][1], .draw = it_desk, .i = i});
    for (int i = 0; i < X.np; i++) {
        const Session *s = X.people[i];
        int me = scene_add(sc, (SceneItem){.kind = "clawd", .layer = LAYER_DEPTH, .depth = s->py + 8, .owner = -1, .ax = s->px, .ay = s->py + 8, .draw = it_clawd, .i = i});
        scene_add(sc, (SceneItem){.kind = "marks", .layer = LAYER_DEPTH, .owner = me, .draw = it_marks, .i = i}); /* (right after it: behind whatever stands in front) */
    }
    for (int k = 0; k < geo.nplay; k++) if (geo.play_on[k]) /* the park's swing set and slide, in among the Clawds */
        scene_add(sc, (SceneItem){.kind = k ? "slide" : "swing", .layer = LAYER_DEPTH, .depth = theme_play_ground(&geo.view, k), .owner = -1, .draw = it_play, .i = k});
    if (X.rail_p >= 0) scene_add(sc, (SceneItem){.kind = "rail", .layer = LAYER_DEPTH, .depth = rail_key, .owner = -1, .draw = it_rail});
    scene_add(sc, (SceneItem){.kind = "particles", .layer = LAYER_PARTICLE, .owner = -1, .draw = it_particles});
    for (int i = 0; i < X.np; i++) if (!X.people[i]->has_left && !strcmp(X.pstate[i], "wait")) /* the name of one that needs you, over everything */
        scene_add(sc, (SceneItem){.kind = "name", .layer = LAYER_HUD, .owner = -1, .draw = it_marks, .i = i, .p = 1});
    if (geo.has_more_pos) scene_add(sc, (SceneItem){.kind = "more", .layer = LAYER_HUD, .owner = -1, .ax = geo.more_pos[0], .ay = geo.more_pos[1], .draw = it_more});
    scene_draw(sc, cv, &X);
    if (room->debug_overlay) draw_overlay(cv, sc, &geo, room);
    if (geo.more && !geo.has_more_pos) snprintf(note, note_n, "+%d MORE", geo.more);
}

/* ---- the whole terminal ----------------------------------------------------------------------- */
static int ratio_size(const char *r, int *w, int *h) {
    static const struct { const char *k; int w, h; } R[] = {{"3:1", 144, 48}, {"2:1", 120, 60}, {"16:9", 128, 72}, {"4:3", 128, 96}, {"1:1", 120, 120}, {"3:4", 96, 128}};
    for (size_t i = 0; i < sizeof R / sizeof R[0]; i++) if (!strcmp(R[i].k, r)) { *w = R[i].w; *h = R[i].h; return 1; }
    return 0;
}

typedef struct { int x0, y0, x1, y1; } SRect;
/* text at k times the pixel size (the font is 3x5), only the lit pixels, clipped to [cx0, cx1) x [cy0, cy1), and not over
 * the Clawds (`behind`: their boxes on screen), so Clawds walking past stay in front of the words as they do at 1x */
static void text_scaled(Canvas *cv, int x, int y, const char *s, Rgb c, int k, int cx0, int cy0, int cx1, int cy1, const SRect *behind, int nb) {
    const Rgb KEY = RGB(1, 2, 3);
    int tw = text_width(s);
    if (tw <= 0) return;
    Canvas t = canvas_new(tw, 5, KEY);
    canvas_text(&t, 0, 0, s, c);
    for (int ty = 0; ty < 5; ty++) for (int tx = 0; tx < tw; tx++) {
        Rgb p = t.p[ty * tw + tx];
        if (rgb_eq(p, KEY)) continue;
        for (int dy = 0; dy < k; dy++) for (int dx = 0; dx < k; dx++) {
            int X = x + tx * k + dx, Y = y + ty * k + dy;
            if (X < cx0 || X >= cx1 || Y < cy0 || Y >= cy1) continue;
            int hidden = 0;
            for (int b = 0; b < nb && !hidden; b++) hidden = X >= behind[b].x0 && X < behind[b].x1 && Y >= behind[b].y0 && Y < behind[b].y1;
            if (!hidden) cv->p[Y * cv->w + X] = p;
        }
    }
    canvas_free(&t);
}

/* the camera: the room is laid out as usual (w x h room pixels at base scale k, where it sits at 1x). The banner stays at
 * the top at the normal size; below it the room is shown zoomed (k * zoom) around (cam_x, cam_y), in the same rectangle the
 * room has at 1x. The words over the room are written at the normal size afterwards (see wtext). */
static Canvas render_camera(int pw, int ph, Room *room, double now, double t, char *note, size_t note_n, int w, int h, int k) {
    const Rgb MARGIN = RGB(14, 14, 18);
    static WText texts[MAX_WTEXT];
    Canvas rc = canvas_new(w, h, RGB(34, 36, 46));
    g_wt = texts; g_nwt = 0;
    draw_room(&rc, room, now, t, note, note_n);
    g_wt = NULL;
    room->size_w = w; room->size_h = h;
    int band = room->has_geo ? imin(room->geo.banner, h) : 0; /* banner rows, not zoomed */
    int z = imax(1, imin(CAM_MAX_ZOOM, room->cam_zoom)), s = k * z;
    int ox = (pw - w * k) / 2, oy = (ph - h * k) / 2 / 2 * 2; /* where the room sits at 1x (whole terminal rows) */
    int ax0 = imax(0, ox), ay0 = imax(0, oy + band * k), ax1 = imin(pw, ox + w * k), ay1 = imin(ph, oy + h * k); /* the room part on screen */
    int vw = imin(w, (ax1 - ax0) / s), vh = imin(h - band, (ay1 - ay0) / s); /* room pixels the camera shows */
    if (!room->cam_set) { room->cam_x = w / 2.0; room->cam_y = (band + h) / 2.0; room->cam_set = 1; }
    room->cam_x = fmax(vw / 2.0, fmin(w - vw / 2.0, room->cam_x)); /* keep the view inside the room, under the banner */
    room->cam_y = fmax(band + vh / 2.0, fmin(h - vh / 2.0, room->cam_y));
    room->cam_vw = vw; room->cam_vh = vh;
    int x0 = (int)(room->cam_x - vw / 2.0), y0 = (int)(room->cam_y - vh / 2.0);
    int vx = ax0 + ((ax1 - ax0) - vw * s) / 2, vy = ay0 + ((ay1 - ay0) - vh * s) / 2 / 2 * 2; /* the view, centred in that part */
    Canvas full = canvas_new(pw, ph, MARGIN);
    for (int y = 0; y < band * k; y++) { /* the banner at 1x */
        int Y = oy + y;
        if (Y < 0 || Y >= ph) continue;
        for (int x = 0; x < w * k; x++) { int X = ox + x; if (X >= 0 && X < pw) full.p[Y * pw + X] = rc.p[(y / k) * w + x / k]; }
    }
    for (int y = 0; y < vh * s; y++) {
        const Rgb *src = rc.p + (size_t)(y0 + y / s) * (size_t)rc.w + x0;
        Rgb *dst = full.p + (size_t)(vy + y) * (size_t)pw + vx;
        for (int x = 0; x < vw * s; x++) dst[x] = src[x / s];
    }
    /* the gates count as not zoomed: they line up with the next window by where the room sits at 1x */
    room->view_x = ox; room->view_y = oy; room->view_k = k;
    double zx = vx - x0 * s, zy = vy - y0 * s; /* screen = (zx, zy) + room pixel * s, for the words */
    static SRect boxes[MAX_SESS + MAX_GUESTS];
    int nbox = 0;
    for (int i = 0; i < room->n + room->ng; i++) { /* where the Clawds are on screen (their 14x8 sprite, arms and legs) */
        const Session *q = i < room->n ? room->s[i] : room->guests[i - room->n];
        if (!q->has_pos || q->has_away) continue;
        boxes[nbox++] = (SRect){(int)(zx + (q->px - 7) * s), (int)(zy + (q->py - 1) * s), (int)(zx + (q->px + 8) * s), (int)(zy + (q->py + 9) * s)};
    }
    for (int i = 0; i < g_nwt; i++) { /* the words: same centre, normal size, behind the Clawds */
        const WText *q = &texts[i];
        int tw = text_width(q->s);
        double cx = zx + (q->x + tw / 2.0) * s, top = zy + q->y * s;
        text_scaled(&full, (int)(cx - tw * k / 2.0), (int)top, q->s, q->c, k, vx, vy, vx + vw * s, vy + vh * s, boxes, nbox);
    }
    canvas_free(&rc);
    return full;
}

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n) {
    const Rgb MARGIN = RGB(14, 14, 18);
    int pw = cols, ph = rows * 2;
    if (!strcmp(room->ratio, "auto")) { /* the room is the whole window, whatever its shape */
        int k = imax(1, room->scale);
        if (room->cam_zoom > 1) return render_camera(pw, ph, room, now, t, note, note_n, pw / k, ph / k / 2 * 2, k);
        Canvas cv = canvas_new(pw / k, ph / k / 2 * 2, MARGIN);
        draw_room(&cv, room, now, t, note, note_n);
        room->size_w = cv.w; room->size_h = cv.h;
        if (k == 1) { room->view_x = room->view_y = 0; room->view_k = 1; return cv; }
        Canvas big = canvas_scaled(&cv, k), full = canvas_new(pw, ph, MARGIN);
        canvas_blit(&full, &big, (pw - big.w) / 2, (ph - big.h) / 2 / 2 * 2);
        room->view_x = (pw - big.w) / 2; room->view_y = (ph - big.h) / 2 / 2 * 2; room->view_k = k;
        canvas_free(&cv); canvas_free(&big);
        return full;
    }
    int sw = 0, sh = 0;
    ratio_size(room->ratio, &sw, &sh);
    int k = imin(pw / sw, ph / sh);
    if (room->cam_zoom > 1) return render_camera(pw, ph, room, now, t, note, note_n, sw, sh, imax(1, k));
    Canvas full = canvas_new(pw, ph, MARGIN);
    if (k >= 1) {
        Canvas rc = canvas_new(sw, sh, RGB(34, 36, 46));
        draw_room(&rc, room, now, t, note, note_n);
        room->size_w = sw; room->size_h = sh;
        Canvas big = canvas_scaled(&rc, k);
        canvas_blit(&full, &big, (pw - big.w) / 2, (ph - big.h) / 2 / 2 * 2);
        room->view_x = (pw - big.w) / 2; room->view_y = (ph - big.h) / 2 / 2 * 2; room->view_k = k;
        canvas_free(&rc); canvas_free(&big);
        return full;
    }
    int rw = imin(pw, ph * sw / sh), rh = rw * sh / sw / 2 * 2; /* whole terminal rows */
    if (rh < SLOT_H) { rw = pw; rh = ph; }
    Canvas rc = canvas_new(rw, rh, RGB(34, 36, 46));
    draw_room(&rc, room, now, t, note, note_n);
    room->size_w = rw; room->size_h = rh;
    canvas_blit(&full, &rc, (pw - rw) / 2, (rows - rh / 2) / 2 * 2);
    room->view_x = (pw - rw) / 2; room->view_y = (rows - rh / 2) / 2 * 2; room->view_k = 1;
    canvas_free(&rc);
    return full;
}
