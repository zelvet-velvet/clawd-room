/* Drawing the room: banner, gates, desks, Clawds, props and labels (port of draw_* in room_tui.py). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clawd.h"
#include "room.h"

static const SpriteFile *g_frames, *g_items;
void draw_set_sprites(const SpriteFile *frames, const SpriteFile *items) { g_frames = frames; g_items = items; }

#define C_GREEN RGB(110, 230, 120)
#define C_BLUE RGB(110, 200, 240)
#define WOOD RGB(78, 62, 50)
#define ROPE RGB(186, 166, 124)
#define RUNG RGB(150, 130, 96)
#define HOLE RGB(18, 16, 20)

static int fdiv(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; } /* Python's // */
static int pyround(double v) { return (int)nearbyint(v); }                                              /* round half to even */
static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

/* longest start of s (upper case) that fits in `width` pixels */
static void fit(char *out, size_t n, const char *s, int width) {
    int k = imax(0, fdiv(width + 1, 4)), i = 0;
    for (; s[i] && i < k && (size_t)i < n - 1; i++) out[i] = (char)((s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i]);
    out[i] = 0;
}

/* ---- gates ----------------------------------------------------------------------------------- */
static int sc(int v, double g) { return imax(1, pyround(v * g)); }

static void draw_door(Canvas *cv, int left, double door_y, const char *theme_name, double g) {
    const Theme *theme = theme_get(theme_name);
    int w = sc(8, g), x = left ? 0 : cv->w - w, cy = (int)(door_y + 5.5);
    int up = sc(7, g), down = sc(7, g), lint = sc(2, g), post = sc(1, g);
    int outer = left ? x : x + w - post, inner = left ? x + w - post : x;
    canvas_rect(cv, x, cy - up - lint, w, lint, WOOD); /* lintel */
    canvas_rect(cv, inner, cy - up, post, up + down, WOOD); /* post on the room side */
    canvas_rect(cv, outer, cy - up, post, up + down, RGB(54, 42, 34)); /* dark gap at the edge of the room */
    canvas_rect(cv, x, cy + down, w, post, WOOD); /* sill */
    int mat_x = left ? x + post : x;
    canvas_rect(cv, mat_x, cy + down - sc(3, g), w - post, sc(3, g), theme->floor); /* doormat */
    canvas_rect(cv, mat_x, cy + down - sc(2, g), w - post, post, theme->floor2);
}

static void draw_rope(Canvas *cv, int x, int top, int end, double g) {
    int half = sc(9, g), rail = sc(7, g), thick = sc(1, g);
    canvas_rect(cv, x - half, top, 2 * half + 1, sc(3, g), WOOD); /* hatch frame */
    canvas_rect(cv, x - half + thick, top + thick, 2 * half + 1 - 2 * thick, thick, HOLE);
    int rx[2] = {x - rail, x + rail};
    for (int i = 0; i < 2; i++) canvas_rect(cv, rx[i], top + sc(3, g), thick, end - top - sc(3, g), ROPE); /* the two ropes */
    for (int y = top + sc(6, g); y < end; y += sc(5, g)) canvas_rect(cv, x - rail + thick, y, 2 * rail - thick, thick, RUNG); /* rungs */
}

static void draw_stairs(Canvas *cv, int x, int h, double g) {
    int half = sc(9, g), rail = sc(6, g), thick = sc(1, g), depth = sc(6, g);
    canvas_rect(cv, x - half, h - depth - thick, 2 * half + 1, thick, WOOD); /* frame */
    canvas_rect(cv, x - half, h - depth, sc(2, g), depth, WOOD);
    canvas_rect(cv, x + half - sc(2, g) + 1, h - depth, sc(2, g), depth, WOOD);
    canvas_rect(cv, x - half + sc(2, g), h - depth, 2 * (half - sc(2, g)) + 1, depth, HOLE); /* the dark shaft */
    int rx[2] = {x - rail, x + rail};
    for (int i = 0; i < 2; i++) canvas_rect(cv, rx[i], h - depth, thick, depth, ROPE); /* the ladder goes down into it */
    for (int y = h - sc(1, g); y > h - depth; y -= sc(2, g)) canvas_rect(cv, x - rail + thick, y, 2 * rail - thick, thick, RUNG);
}

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

static const char *draw_desk(Canvas *cv, int cx, int by, const char *state, const char *tool, double t, const Theme *th, const char *role, char *buf, size_t n) {
    Rgb color;
    const char *kind = tool_kind(tool, &color, buf, n);
    theme_furniture(th, cv, cx, by, !strcmp(state, "work"), color, t, role);
    return kind;
}

static int item(Canvas *cv, const char *name, int x, int y) {
    const Sprite *sp = g_items ? sprites_by_name(g_items, name) : NULL;
    if (sp) sprite_blit(cv, sp, x, y, C_CLAWD);
    return sp != NULL;
}

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
    else if (!seated && !s->has_left && strcmp(state, "wait")) { /* away from the desk: say who it is */
        char name[24];
        fit(name, sizeof name, s->name[0] ? s->name : "", 19);
        if (!s->name[0]) { char id8[9]; snprintf(id8, sizeof id8, "%.8s", s->sid); fit(name, sizeof name, id8, 19); }
        int tw = text_width(name), x0 = cx - tw / 2, y0 = y - 8, hit = 0;
        for (int i = 0; i < nrects; i++) /* skip the tag where it would sit on top of a desk label */
            if (x0 < rects[i].x1 && x0 + tw > rects[i].x0 && y0 < rects[i].y1 && y0 + 5 > rects[i].y0) hit = 1;
        if (!hit) canvas_text(cv, x0, y0, name, s->guest ? C_BLUE : C_GREY);
    }
}

/* ---- layout ---------------------------------------------------------------------------------- */

static void layout(int w, int h, int n, int *banner, int *per_row, int *slot_w, int *rows_shown, int *ys) {
    *per_row = imax(1, imin(PER_ROW, fdiv(w, SLOT_MIN_W)));
    *slot_w = imin(SLOT_MAX_W, fdiv(w, *per_row));
    int rows_needed = imax(1, -fdiv(-n, *per_row));
    *banner = fdiv(h - BANNER_H, SLOT_H) >= rows_needed ? BANNER_H : 0;
    *rows_shown = imax(0, imin(rows_needed, fdiv(h - *banner, SLOT_H)));
    int gap = fdiv(h - *banner - *rows_shown * SLOT_H, *rows_shown + 1); /* spread spare height evenly */
    for (int r = 0; r < *rows_shown && r < 16; r++) ys[r] = *banner + gap * (r + 1) + SLOT_H * r;
}

void geo_init(Geo *g, const Room *room, int w, int h) {
    int ys[16], per_row, rows_shown, n = room->n;
    memset(g, 0, sizeof *g);
    g->w = w; g->h = h;
    double up = room->has_link[SIDE_UP] ? room->link_pos[SIDE_UP] : w * 0.72, down = room->has_link[SIDE_DOWN] ? room->link_pos[SIDE_DOWN] : w * 0.28;
    g->rope_x = imin(imax(pyround(up), 10), w - 10);
    g->stairs_x = imin(imax(pyround(down), 10), w - 10);
    g->spots = theme_spots(room->theme, w, h);
    g->floor_y = (int)(h * room->theme->horizon);
    layout(w, h, n, &g->banner, &per_row, &g->slot_w, &rows_shown, ys);
    int slots = per_row * rows_shown, keep = n;
    if (keep > slots) keep = slots > 1 ? slots - 1 : slots; /* the last slot says "+N MORE" */
    g->shown = keep;
    g->more = n - keep;
    int extra = (g->more && slots > 1) ? 1 : 0, count = imin(n, keep + extra);
    for (int i = 0; i < count; i++) {
        int r = i / per_row, c = i % per_row, n_in_row = imin(per_row, keep + extra - r * per_row);
        int cx = fdiv(w - g->slot_w * n_in_row, 2) + c * g->slot_w + g->slot_w / 2;
        if (i < keep) { g->home[i][0] = cx; g->home[i][1] = ys[r] + 8; }
        else { g->has_more_pos = 1; g->more_pos[0] = cx; g->more_pos[1] = ys[r] + 14; }
    }
    g->ymin = rows_shown ? ys[rows_shown - 1] + 8 + SEAT_DY + 4 : 0;
    g->ymax = imax(g->ymin, h - 8);
    g->rope_end = g->ymin + 8; /* the rope hangs down to where the Clawds' feet are */
    for (int side = 0; side < 2; side++) {
        double v = room->has_link[side] ? room->link_pos[side] : (g->ymin + g->ymax) / 2.0;
        g->door_y[side] = v < g->ymin ? g->ymin : v > g->ymax ? g->ymax : v;
    }
}

static const char *LABEL(const char *state) {
    return !strcmp(state, "idle") ? "IDLE" : !strcmp(state, "think") ? "THINKING" : !strcmp(state, "wait") ? "NEED OK" : !strcmp(state, "cheer") ? "DONE!" : !strcmp(state, "sleep") ? "ZZZ" : "";
}

static void draw_background(Canvas *cv, const Room *room, int banner, int count) {
    theme_background(room->theme, cv, banner);
    if (!banner) return;
    char title[64], label[32];
    snprintf(title, sizeof title, "%s", room->title[0] ? room->title : "CLAWD ROOM");
    for (char *p = title; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 2, 1, title, C_YELLOW);
    snprintf(label, sizeof label, "%d SESSION%s", count, count == 1 ? "" : "S");
    canvas_text(cv, cv->w - 2 - text_width(label), 1, label, C_WHITE);
    if (room->npeers) { /* linked to other windows; an arrow shows which side one is close enough */
        char link[256] = "LINK";
        for (int i = 0; i < room->npeers; i++) {
            const char *side = room->peers[i].side, *arrow = !strcmp(side, "right") ? " >" : !strcmp(side, "left") ? " <" : !strcmp(side, "up") ? " ^" : !strcmp(side, "down") ? " V" : "";
            strncat(link, " ", sizeof link - strlen(link) - 1);
            strncat(link, room->peers[i].name, sizeof link - strlen(link) - 1);
            strncat(link, arrow, sizeof link - strlen(link) - 1);
        }
        char fitted[256];
        int tw = imax(text_width(label), text_width(room->title)); /* (the title as given, not upper-cased: same length) */
        fit(fitted, sizeof fitted, link, cv->w - 2 * (tw + 6));
        canvas_text(cv, cv->w / 2 - text_width(fitted) / 2, 1, fitted, C_GREEN);
    }
}

void draw_room(Canvas *cv, Room *room, double now, double t, char *note, size_t note_n) {
    static const int HEART[3][3] = {{1, 0, 1}, {1, 1, 1}, {0, 1, 0}};
    Geo geo;
    room_order(room);
    geo_init(&geo, room, cv->w, cv->h);
    if (!room->freeze) room_step(room, &geo, now); /* everybody moves a little before they are drawn */
    room_order(room); /* (visitors may have come or gone) */
    note[0] = 0;
    int active = 0;
    for (int i = 0; i < room->n; i++) if (!room->s[i]->has_left) active++;
    draw_background(cv, room, geo.banner, active);
    for (int i = 0; i < room->nnb; i++) {
        int side = room->nb[i].side;
        double g = room->has_gate[side] ? room->gate_scale[side] : 1.0;
        if (side == SIDE_LEFT || side == SIDE_RIGHT) draw_door(cv, side == SIDE_LEFT, geo.door_y[side], room->nb[i].theme, g);
        else if (side == SIDE_UP) draw_rope(cv, geo.rope_x, geo.banner, geo.rope_end, g);
        else draw_stairs(cv, geo.stairs_x, cv->h, g);
    }
    if (room->n == 0) {
        int mid = (geo.banner + cv->h) / 2;
        canvas_text(cv, cv->w / 2 - text_width("NO SESSIONS") / 2, mid - 7, "NO SESSIONS", RGB(150, 150, 180));
        canvas_text(cv, cv->w / 2 - text_width("START CLAUDE") / 2, mid, "START CLAUDE", RGB(110, 110, 140));
        return;
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
    char kinds[256][16];
    for (int i = 0; i < geo.shown; i++) {
        const Session *s = room->s[i];
        char buf[16];
        const char *kind = draw_desk(cv, geo.home[i][0], geo.home[i][1], session_shown(s, now), s->tool, t, room->theme, roles[i], buf, sizeof buf);
        snprintf(kinds[i], sizeof kinds[i], "%s", kind);
    }
    int name_w = geo.slot_w - 3;
    IRect rects[256];
    int nrects = 0;
    for (int i = 0; i < geo.shown; i++) { /* desk labels; Clawds walking past are drawn over them */
        const Session *s = room->s[i];
        if (s->has_left) continue;
        int cx = geo.home[i][0], by = geo.home[i][1];
        char nm[48], id8[9];
        snprintf(id8, sizeof id8, "%.8s", s->sid);
        fit(nm, sizeof nm, s->name[0] ? s->name : id8, name_w);
        canvas_text(cv, cx - text_width(nm) / 2, by + 18, nm, C_WHITE);
        const char *state = session_shown(s, now);
        const char *label = !strcmp(state, "work") ? kinds[i] : LABEL(state);
        char lb[16];
        snprintf(lb, sizeof lb, "%.15s", label);
        if (s->has_away && !strcmp(state, "idle")) snprintf(lb, sizeof lb, "AWAY");
        if (text_width(lb) > name_w) lb[5] = 0; /* "THINKING" -> "THINK" in narrow slots */
        canvas_text(cv, cx - text_width(lb) / 2, by + 24, lb, C_GREY);
        rects[nrects++] = (IRect){cx - name_w / 2, by + 18, cx + name_w / 2 + 1, by + 29};
    }
    int order[512];
    for (int i = 0; i < np; i++) { /* nearer to the viewer = drawn later (stable, by py) */
        int j = i;
        while (j > 0 && people[order[j - 1]]->py > people[i]->py) { order[j] = order[j - 1]; j--; }
        order[j] = i;
    }
    for (int k = 0; k < np; k++) {
        int i = order[k];
        const Session *s = people[i];
        int idx = i < nshown_people ? -1 : 0;
        const char *role = "";
        if (idx == -1) for (int q = 0; q < geo.shown; q++) if (room->s[q] == s) role = roles[q];
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
        canvas_text(cv, geo.more_pos[0] - text_width(m) / 2, geo.more_pos[1], m, C_WHITE);
    }
    if (geo.more && !geo.has_more_pos) snprintf(note, note_n, "+%d MORE", geo.more);
}

/* ---- the whole terminal ----------------------------------------------------------------------- */
static Canvas scaled(const Canvas *c, int k) {
    Canvas out = canvas_new(c->w * k, c->h * k, RGB(34, 36, 46));
    for (int y = 0; y < out.h; y++) for (int x = 0; x < out.w; x++) out.p[y * out.w + x] = c->p[(y / k) * c->w + x / k];
    return out;
}

static void blit(Canvas *dst, const Canvas *src, int x, int y) {
    for (int yy = 0; yy < src->h; yy++) {
        if (y + yy < 0 || y + yy >= dst->h) continue;
        for (int xx = 0; xx < src->w && x + xx < dst->w; xx++) if (x + xx >= 0) dst->p[(y + yy) * dst->w + x + xx] = src->p[yy * src->w + xx];
    }
}

static int ratio_size(const char *r, int *w, int *h) {
    static const struct { const char *k; int w, h; } R[] = {{"3:1", 144, 48}, {"2:1", 120, 60}, {"16:9", 128, 72}, {"4:3", 128, 96}, {"1:1", 120, 120}, {"3:4", 96, 128}};
    for (size_t i = 0; i < sizeof R / sizeof R[0]; i++) if (!strcmp(R[i].k, r)) { *w = R[i].w; *h = R[i].h; return 1; }
    return 0;
}

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n) {
    const Rgb MARGIN = RGB(14, 14, 18);
    int pw = cols, ph = rows * 2;
    if (!strcmp(room->ratio, "auto")) { /* the room is the whole window, whatever its shape */
        int k = imax(1, room->scale);
        Canvas cv = canvas_new(pw / k, ph / k / 2 * 2, MARGIN);
        draw_room(&cv, room, now, t, note, note_n);
        room->size_w = cv.w; room->size_h = cv.h;
        if (k == 1) return cv;
        Canvas big = scaled(&cv, k), full = canvas_new(pw, ph, MARGIN);
        blit(&full, &big, (pw - big.w) / 2, (ph - big.h) / 2 / 2 * 2);
        canvas_free(&cv); canvas_free(&big);
        return full;
    }
    int sw = 0, sh = 0;
    ratio_size(room->ratio, &sw, &sh);
    int k = imin(pw / sw, ph / sh);
    Canvas full = canvas_new(pw, ph, MARGIN);
    if (k >= 1) {
        Canvas rc = canvas_new(sw, sh, RGB(34, 36, 46));
        draw_room(&rc, room, now, t, note, note_n);
        room->size_w = sw; room->size_h = sh;
        Canvas big = scaled(&rc, k);
        blit(&full, &big, (pw - big.w) / 2, (ph - big.h) / 2 / 2 * 2);
        canvas_free(&rc); canvas_free(&big);
        return full;
    }
    int rw = imin(pw, ph * sw / sh), rh = rw * sh / sw / 2 * 2; /* whole terminal rows */
    if (rh < SLOT_H) { rw = pw; rh = ph; }
    Canvas rc = canvas_new(rw, rh, RGB(34, 36, 46));
    draw_room(&rc, room, now, t, note, note_n);
    room->size_w = rw; room->size_h = rh;
    blit(&full, &rc, (pw - rw) / 2, (rows - rh / 2) / 2 * 2);
    canvas_free(&rc);
    return full;
}
