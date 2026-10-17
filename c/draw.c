/* Drawing the room: banner, gates, desks, Clawds, props and labels (port of draw_* in room_tui.py). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clawd.h"
#include "room.h"

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

static void draw_extras(Canvas *cv, const Session *s, const char *state, double t, double now, int seated, const IRect *rects, int nrects, const Spots *spots) {
    int cx = (int)s->px, y = (int)s->py;
    if (now < s->cup_until) { /* carrying a coffee, with a little steam */
        if (!item(cv, "cup-held", cx + 7, y + 4)) {
            canvas_rect(cv, cx + 7, y + 4, 3, 2, C_WHITE);
            canvas_px(cv, cx + 10, y + 4, C_WHITE);
        }
        canvas_px(cv, cx + 8, y + 3 - (int)(t * 2) % 3, RGB(200, 200, 210));
    }
    if (!strcmp(s->emote, "swing") && spots->has) { /* ropes from the bar to the hands */
        canvas_line(cv, spots->swing.x - 6, spots->swing.bar + 2, cx - 6, y + 3, RGB(150, 140, 120));
        canvas_line(cv, spots->swing.x + 6, spots->swing.bar + 2, cx + 6, y + 3, RGB(150, 140, 120));
        canvas_rect(cv, cx - 6, y + 8, 13, 1, RGB(112, 88, 64)); /* the seat */
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
        if (!strcmp(state, "wait")) {
            char name[24];
            fit(name, sizeof name, s->name[0] ? s->name : s->sid, 19);
            int tw = text_width(name), x0 = cx - tw / 2, y0 = y - 18;
            wtext(cv, x0 + 1, y0 + 1, name, RGB(20, 20, 28));
            wtext(cv, x0, y0, name, C_WHITE);
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

void geo_init(Geo *g, const Room *room, int w, int h) {
    int n = room->n, park = !strcmp(room->theme->name, "park");
    memset(g, 0, sizeof *g);
    g->w = w; g->h = h;
    View *v = &g->view;
    view_init(v, w, h);
    g->spots = theme_spots(room->theme, w, h);
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
        g->gate_out[SIDE_UP][0] = x1 + ARCH_IN + ARCH_W / 2.0; g->gate_out[SIDE_UP][1] = yl - 1;          /* to the archway */
    }
    double wx = w * (park ? 0.5 : WELL_X), wh = (h - v->bb) * WELL_H;
    g->gate_in[SIDE_DOWN][0] = wx; g->gate_in[SIDE_DOWN][1] = h - 1;
    g->gate_out[SIDE_DOWN][0] = wx; g->gate_out[SIDE_DOWN][1] = h - 1 - wh + 2;
    for (int side = 0; side < 2; side++) g->door_y[side] = fy - 8;
    g->rope_x = (int)g->gate_in[SIDE_UP][0]; g->stairs_x = (int)g->gate_in[SIDE_DOWN][0];
    g->rope_end = (int)g->gate_in[SIDE_UP][1];
}


static void draw_background(Canvas *cv, const Room *room, int banner, int count) {
    (void)count;
    theme_background(room->theme, cv, banner);

}

void draw_room(Canvas *cv, Room *room, double now, double t, char *note, size_t note_n) {
    static const int HEART[3][3] = {{1, 0, 1}, {1, 1, 1}, {0, 1, 0}};
    Geo geo;
    room_order(room);
    geo_init(&geo, room, cv->w, cv->h);
    if (!room->freeze) { room->geo = geo; room->has_geo = 1; room_step(room, &room->geo, now); } /* everybody moves a little before they are drawn */
    room_order(room); /* (visitors may have come or gone) */
    note[0] = 0;
    int active = 0;
    for (int i = 0; i < room->n; i++) if (!room->s[i]->has_left) active++;
    draw_background(cv, room, geo.banner, active);
    { /* gates that appear while the room is running animate in; the ones there from the first frame do not */
        int present[SIDES] = {0};
        for (int i = 0; i < room->nnb; i++) present[room->nb[i].side] = 1;
        for (int sd = 0; sd < SIDES; sd++) {
            if (present[sd] && !room->gate_seen[sd]) { /* starts when the windows stopped moving (+1 s), the same moment for both rooms */
                room->gate_seen[sd] = 1;
                room->gate_t0[sd] = !room->drawn ? t - GATE_ANIM : room->has_gate_sync && room->gate_sync_t > t - GATE_ANIM ? room->gate_sync_t : t;
            }
            else if (!present[sd]) room->gate_seen[sd] = 0;
        }
        room->drawn = 1;
        double dt = fmin(0.25, fmax(0.0, t - room->door_t)); /* the leaves swing towards open while a Clawd is near the door */
        room->door_t = t;
        for (int sd = SIDE_LEFT; sd <= SIDE_RIGHT; sd++) {
            int near = 0;
            for (int i = 0; i < room->n + room->ng && !near; i++) {
                const Session *q = i < room->n ? room->s[i] : room->guests[i - room->n];
                if (q->has_pos && !q->has_away && fabs(q->px - geo.gate_out[sd][0]) < DOOR_NEAR && fabs(q->py + 8 - geo.gate_out[sd][1]) < 6) near = 1;
            }
            double step = dt / DOOR_SWING;
            room->door_open[sd] = near ? fmin(1.0, room->door_open[sd] + step) : fmax(0.0, room->door_open[sd] - step);
        }
    }
    for (int i = 0; i < room->nnb; i++) {
        int side = room->nb[i].side;
        double p = fmin(1.0, fmax(0.0, (t - room->gate_t0[side]) / GATE_ANIM));
        theme_gate(room->theme, cv, &geo, side, p, side < 2 ? room->door_open[side] : 0);
    }
    { /* the room's name as a sign in the room (after the gates: it hangs in front of the stairs) */
        int has_up = 0, sides = 0; /* (sides: where the park has a fence instead of bushes) */
        for (int i = 0; i < room->nnb; i++) { if (room->nb[i].side == SIDE_UP) has_up = 1; if (room->nb[i].side == SIDE_LEFT) sides |= 1; if (room->nb[i].side == SIDE_RIGHT) sides |= 2; }
        theme_sign(room->theme, cv, room->title[0] ? room->title : room->theme->name, has_up, sides);
    }
    int anyone = 0; /* any live session at all: linked rooms share them out, so this room may have none of its own */
    for (int i = 0; i < room->npool; i++) if (!room->pool[i].has_left) anyone = 1;
    if (room->n == 0 && !anyone) { /* no Claude Code running anywhere: say so (visitors from other windows are still drawn below) */
        int mid = (int)(geo.view.bb + cv->h) / 2;
        wtext(cv, cv->w / 2 - text_width("NO SESSIONS") / 2, mid - 7, "NO SESSIONS", RGB(150, 150, 180));
        wtext(cv, cv->w / 2 - text_width("START CLAUDE") / 2, mid, "START CLAUDE", RGB(110, 110, 140));
    }
    /* who is seated at their desk, and who is in the room right now */
    int seated[256] = {0};
    const Session *people[512];
    const char *pstate[512];
    int np = 0;
    char roles[256][12];
    for (int i = 0; i < geo.shown; i++) snprintf(roles[i], sizeof roles[i], "%s", theme_role(room->theme, i));
    for (int i = 0; i < geo.shown; i++) {
        const Session *s = room->s[i];
        if (s->has_away) continue;
        seated[i] = !s->moving && !s->has_left && fabs(s->px - (geo.home[i][0] - 5)) < 1 && fabs(s->py - (geo.home[i][1] + SEAT_DY)) < 1;
        people[np] = s; pstate[np++] = session_shown(s, now);
    }
    int nshown_people = np;
    for (int i = 0; i < room->ng; i++) { people[np] = room->guests[i]; pstate[np++] = session_shown(room->guests[i], now); }
    IRect rects[1];
    int nrects = 0;
    /* the workspaces and the Clawds, back to front by where they stand on the floor (a desk's front edge, a Clawd's feet) */
    int nd = geo.shown, total = nd + np, order[1024];
    double key[1024];
    for (int i = 0; i < nd; i++) { key[i] = geo.home[i][1] + 0.5; order[i] = i; }
    for (int i = 0; i < np; i++) { key[nd + i] = people[i]->py + 8; order[nd + i] = nd + i; }
    for (int i = 1; i < total; i++) { /* stable insertion sort */
        int o = order[i], j = i;
        while (j > 0 && key[order[j - 1]] > key[o]) { order[j] = order[j - 1]; j--; }
        order[j] = o;
    }
    for (int k = 0; k < total; k++) {
        int o = order[k];
        if (o < nd) {
            const Session *s = room->s[o];
            draw_desk(cv, geo.home[o][0], geo.home[o][1], session_shown(s, now), s->tool, t, room->theme, roles[o], crc32_str(s->sid));
            continue;
        }
        int i = o - nd;
        const Session *s = people[i];
        const char *role = "";
        if (i < nshown_people) for (int q = 0; q < geo.shown; q++) if (room->s[q] == s) role = roles[q];
        Pose p = pose_for(s, pstate[i], t, now);
        draw_clawd(cv, (int)s->px, (int)s->py, &p, g_frames, C_CLAWD, room->theme, role);
    }
    for (int i = 0; i < room->np; i++) {
        const Particle *q = &room->parts[i];
        if (q->life > 0.15) {
            if (q->heart) {
                for (int ry = 0; ry < 3; ry++) for (int rx = 0; rx < 3; rx++) if (HEART[ry][rx]) canvas_px(cv, (int)(q->x - 1 + rx), (int)(q->y + ry), q->c);
            } else canvas_px(cv, (int)q->x, (int)q->y, q->c);
        }
    }
    for (int i = 0; i < np; i++) {
        int is_seated = 0;
        for (int q = 0; q < geo.shown; q++) if (room->s[q] == people[i] && seated[q]) is_seated = 1;
        draw_extras(cv, people[i], pstate[i], t, now, is_seated, rects, nrects, &geo.spots);
    }
    if (geo.has_more_pos) {
        char m[24];
        snprintf(m, sizeof m, "+%d MORE", geo.more);
        wtext(cv, geo.more_pos[0] - text_width(m) / 2, geo.more_pos[1], m, C_WHITE);
    }
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
