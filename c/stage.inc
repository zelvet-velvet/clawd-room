/* The stage page of the pixel editor (Interaction, "Part animations in the pixel editor"): clawd-room --edit anims/NAME opens an
 * animation of anims/ (parts.c). The canvas shows the room with the thing on its stage and every part outlined; the panel holds the
 * picked part and its values at this frame; the timeline shows the frames as pictures with a diamond under each keyframe of the
 * picked part. Included by editor.c (it uses the editor's drawing helpers and its terminal code); what you change is saved as you go
 * to ~/.clawd-room/anims/NAME.txt (yours over the default), and a running room reads it again.
 *   mouse: a part on the stage or in the list picks it; SELECT / MOVE / ROTATE / KEY tools: MOVE drags the picked part, ROTATE drags
 *          its angle, KEY adds a key at this frame; < > beside a value change it at this frame (a key is made if there is none)
 *   keys:  left / right frame, Space play, up / down value, + - change it, [ ] part, k add key, x delete key, e ease of the key,
 *          u / r undo / redo, q quit */
#include "fixture.h"
#include "parts.h"

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n); /* draw.c */

enum { R_SSTAGE = 64, R_SVAL, R_SPART, R_SKEY, R_SKEYDEL, R_SEASE, R_STOOL, R_SPANEL, R_SSCROLL, R_SKEYSHOW, R_SGROW };
#define ST_UNDO 24
#define ST_FPS 16          /* the frames: 16 a second, like the room */
#define ST_MAXF 300
#define ST_TW 44           /* a frame as a picture */
#define ST_TH 26
#define ST_BG RGB(18, 18, 24)
#define ST_ROWS 5          /* the values: move x, move y, rotate, show, glow (grow is the bar under the panel) */
static const char *const ST_ROW[ST_ROWS] = {"MOVE X", "MOVE Y", "ROTATE", "SHOW", "GLOW"};
static const char *const ST_EASE[] = {"LINEAR", "OUT", "IN OUT", "BACK", "IN", "SINE", "SINE OUT", "SINE IN"};

typedef struct {
    PartAnim a, undo[ST_UNDO];
    int nundo, nredo;
    char name[24];
    int pick, row, frame, tool, playing, first, help, vx, vy, vw, vh, asking, changed;
    int onion;                /* o / ONION: the frame before shows faintly red, the one after blue, where they differ (not while playing) */
    int hide_keys;            /* the keys' marks on the stage are hidden (v, or the KEYS button under the stage) */
    int hscroll;              /* the help box scrolls when the window is too short for it */
    int growdrag;             /* dragging the grow bar */
    int focus;                /* the view goes to the picked part (put in the middle) at the next drawing */
    int zoom;                 /* the stage's zoom (0: as the window fits); vx, vy: the room pixel at its top left (vw, vh: what it shows) */
    int pscroll, pfollow;     /* the panel scrolls: how far, and whether it should bring the picked value into view */
    int pan; double pan_x0, pan_y0; int pan_vx, pan_vy; /* dragging the view with the right button */
    int rsz, rsz_box[4], rsz_off[2]; /* resizing the picked part's border: which edges (1 left, 2 right, 4 top, 8 bottom), its box before, where it is at this frame */
    int sbdrag, seen, cap, sb_x0, sb_w; /* the frame strip's scroll bar: being dragged; the frame last seen (the strip follows it when it changes); frames in view; the bar's place */
    char *orig; /* your file as it was when the page opened (NULL: you had none), for a discard */ /* (vx, vy: the room pixel at the stage's top left, as last drawn) */
    double play_t0, speed;
    int drag; double px0, py0; PartPose dpose; /* dragging the picked part: where the mouse and the key were */
    char msg[96];
    Canvas thumb[ST_MAXF]; unsigned char thumb_ok[ST_MAXF]; int thumb_pick; /* the frame pictures, drawn when they are first seen */
} Stage;

static int st_nf(const PartAnim *a) { int n = (int)lround(a->length * ST_FPS); if (n < 1) n = 1; n = a->loop ? n : n + 1; return n > ST_MAXF ? ST_MAXF : n; }
static double st_t(int f) { return (double)f / ST_FPS; }
static double st_now_t(const Stage *s, double wall) { return s->playing ? fmod((wall - s->play_t0) * s->speed, s->a.length) : st_t(s->frame); }
static int st_key_at(const Part *p, double t) { for (int k = 0; k < p->nkey; k++) if (fabs(p->key[k].t - t) < 0.5 / ST_FPS) return k; return -1; }

/* the room of the scene at time t of the animation: the scene lines as a fixture (a line `emote SID KIND` starts that emote when the
 * animation began, t ago; `appear SIDE` makes that gate begin to appear then), rendered once to settle the room, then at that moment */
static Canvas st_shot(const PartAnim *a, double t) {
    static Fixture fx;
    char text[PARTS_SCENE_LINES * 200 + 16], sid[PARTS_SCENE_LINES][24], kind[PARTS_SCENE_LINES][16];
    int nemote = 0, ngate = 0, gside[4], still = 0;
    text[0] = 0;
    for (int i = 0; i < a->nscene; i++) {
        if (!strcmp(a->scene[i], "still")) { still = 1; continue; } /* still: nobody moves (a gate scene: the room keeps the gates the fixture gives) */
        if (!strncmp(a->scene[i], "appear ", 7)) { char sd[16]; if (ngate < 4 && sscanf(a->scene[i] + 7, "%15s", sd) == 1) gside[ngate++] = side_index(sd); continue; } /* appear SIDE: that gate begins to appear when the animation does */
        if (!strncmp(a->scene[i], "emote ", 6)) { if (nemote < PARTS_SCENE_LINES && sscanf(a->scene[i] + 6, "%23s %15s", sid[nemote], kind[nemote]) == 2) nemote++; continue; }
        size_t n = strlen(text);
        snprintf(text + n, sizeof text - n, "%s\n", a->scene[i]);
    }
    if (fixture_load_text(&fx, text)) return canvas_new(8, 8, ST_BG);
    fx.room.freeze = still; /* (it moves: the swing's Clawd by the animation; a still scene does not) */
    char note[64];
    Canvas settle = render(fx.cols, fx.rows, &fx.room, fx.now, fx.t, note, sizeof note);
    canvas_free(&settle);
    for (int i = 0; i < nemote; i++) room_emote(&fx.room, sid[i], kind[i], fx.now - t, 100000.0);
    for (int i = 0; i < ngate; i++) if (gside[i] >= 0) { /* (a gate: it began to appear t ago) */
        char key[8];
        fx.room.gate_t0[gside[i]] = fx.t - t; /* (gates run on the drawing's clock, fx.t, not the events' one) */
        snprintf(key, sizeof key, "gate%d", gside[i]);
        anim_start(&fx.room.anim, key, 0, 1, fx.t - t, 0.8, EASE_OUT);
    }
    return render(fx.cols, fx.rows, &fx.room, fx.now, fx.t, note, sizeof note);
}
static void st_crop(Canvas *dst, const Canvas *src, int sx, int sy, int w, int h, int dx, int dy, int z) { /* a piece of a picture, z times, at (dx, dy) */
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int X = sx + x, Y = sy + y;
        canvas_rect(dst, dx + x * z, dy + y * z, z, z, X >= 0 && X < src->w && Y >= 0 && Y < src->h ? src->p[Y * src->w + X] : ST_BG);
    }
}

/* ---- editing ---------------------------------------------------------------------------------------------------------- */
static void st_msg(Stage *s, const char *m) { snprintf(s->msg, sizeof s->msg, "%s", m); }
static void st_changed(Stage *s) { memset(s->thumb_ok, 0, sizeof s->thumb_ok); }
static int st_save(Stage *s) {
    char path[700];
    if (!parts_mine_dir()[0]) { st_msg(s, "NOWHERE TO SAVE"); return -1; }
    snprintf(path, sizeof path, "%s/%s.txt", parts_mine_dir(), s->name);
    if (parts_save(&s->a, path)) { st_msg(s, "COULD NOT SAVE"); return -1; }
    s->changed = 1;
    return 0;
}
static void st_discard(Stage *s) { /* your file back as it was when the page opened (none: gone) */
    char path[700];
    snprintf(path, sizeof path, "%s/%s.txt", parts_mine_dir(), s->name);
    if (s->orig) { FILE *f = fopen(path, "w"); if (f) { fputs(s->orig, f); fclose(f); } }
    else unlink(path);
}
static void st_push(Stage *s) { /* (before a change: undo goes back to here) */
    if (s->nundo == ST_UNDO) { memmove(s->undo, s->undo + 1, sizeof s->undo[0] * (ST_UNDO - 1)); s->nundo--; }
    s->undo[s->nundo++] = s->a;
    s->nredo = 0;
}
static void st_swap(Stage *s, int at) { static PartAnim t; t = s->a; s->a = s->undo[at]; s->undo[at] = t; st_changed(s); }
/* a key of the part at time t: the one there, or a new one with the pose the part has there */
static int st_ensure_key(Stage *s, int part, double t) {
    Part *p = &s->a.part[part];
    int k = st_key_at(p, t);
    if (k >= 0) return k;
    if (p->nkey >= PARTS_MAX_KEYS) { st_msg(s, "TOO MANY KEYS"); return -1; }
    PartKey nk = {t, parts_pose(&s->a, part, t), EASE_LINEAR};
    int at = 0;
    while (at < p->nkey && p->key[at].t < t) at++;
    memmove(&p->key[at + 1], &p->key[at], sizeof p->key[0] * (size_t)(p->nkey - at));
    p->key[at] = nk;
    p->nkey++;
    return at;
}
static void st_value(PartPose *q, int row, int delta) {
    if (row == 0) q->x += delta;
    else if (row == 1) q->y += delta;
    else if (row == 2) q->turn += delta * 5;
    else if (row == 3) q->show = !q->show;
    else { q->glow += delta * 10; q->glow = q->glow < 0 ? 0 : q->glow > 100 ? 100 : q->glow; }
}
static void st_change(Stage *s, int row, int delta) { /* the value of the picked part at this frame, one step (a key is made if there is none) */
    if (row == 2 && s->a.part[s->pick].sprite) { st_msg(s, "A SPRITE MOVES, IT DOES NOT ROTATE"); return; }
    st_push(s);
    int k = st_ensure_key(s, s->pick, st_t(s->frame));
    if (k < 0) return;
    st_value(&s->a.part[s->pick].key[k].pose, row, delta);
    st_changed(s);
    st_save(s);
}
static void st_add_key(Stage *s) {
    Part *p = &s->a.part[s->pick];
    if (st_key_at(p, st_t(s->frame)) >= 0) return;
    st_push(s);
    if (st_ensure_key(s, s->pick, st_t(s->frame)) >= 0) { st_changed(s); st_save(s); } /* (no message: the KEY cap shows it) */
}
static void st_del_key(Stage *s) {
    Part *p = &s->a.part[s->pick];
    int k = st_key_at(p, st_t(s->frame));
    if (k < 0) { st_msg(s, "NO KEY HERE"); return; }
    if (p->nkey < 2) { st_msg(s, "A PART KEEPS ONE KEY"); return; }
    st_push(s);
    memmove(&p->key[k], &p->key[k + 1], sizeof p->key[0] * (size_t)(p->nkey - k - 1));
    p->nkey--;
    st_changed(s); st_save(s);
}
static void st_toggle_key(Stage *s) { /* KEY: a key at this frame for the picked part, or none (a part keeps one) */
    if (st_key_at(&s->a.part[s->pick], st_t(s->frame)) >= 0) {
        st_del_key(s);
    } else st_add_key(s);
}
static void st_ease(Stage *s) {
    Part *p = &s->a.part[s->pick];
    int k = st_key_at(p, st_t(s->frame));
    if (k < 0) { st_msg(s, "NO KEY HERE"); return; }
    st_push(s);
    p->key[k].ease = (p->key[k].ease + 1) % (int)(sizeof ST_EASE / sizeof *ST_EASE);
    st_changed(s); st_save(s);
}

/* ---- drawing ---------------------------------------------------------------------------------------------------------- */
static const char *const STAGE_ICON[4][9] = {
    {"k........", "kwk......", "kwwk.....", "kwwwk....", "kwwwwk...", "kwwkkk...", "kwkwk....", "kk.kwk...", "....kk..."}, /* SELECT: a pointer */
    {"....w....", "...www...", "....w....", ".w..w..w.", "wwwwwwwww", ".w..w..w.", "....w....", "...www...", "....w...."}, /* MOVE: four arrows */
    {"..bbbbb..", ".b.....b.", "b.......b", "b.......b", "b......bbb", ".b......b.", "..bbbbb.b.", ".......b..", "........."},  /* ROTATE: an arrow round */
    {"....y....", "...yyy...", "..yyyyy..", ".yyyyyyy.", "yyyyyyyyy", ".yyyyyyy.", "..yyyyy..", "...yyy...", "....y...."},  /* KEY: a diamond */
};
static void stage_tool(Canvas *cv, int x, int y, int which, int on) { /* the editor's key caps (tool_button), with the stage's icons */
    int w = 13, h = 13, dy = on ? 1 : 0;
    canvas_rect(cv, x + 1, y + 1, w - 2, h, RGB(14, 14, 20)); canvas_rect(cv, x, y + 2, w, h - 2, RGB(14, 14, 20));
    Rgb cap = on ? RGB(70, 72, 100) : RGB(52, 54, 70);
    canvas_rect(cv, x + 1, y + dy, w - 2, h - dy, cap); canvas_rect(cv, x, y + dy + 1, w, h - dy - 2, cap);
    canvas_rect(cv, x + 2, y + dy, w - 4, 1, on ? RGB(110, 112, 145) : RGB(86, 88, 110));
    for (int yy = 0; yy < 9; yy++) for (int xx = 0; xx < 9 && STAGE_ICON[which][yy][xx]; xx++)
        if (STAGE_ICON[which][yy][xx] != '.') canvas_px(cv, x + 2 + xx, y + 2 + dy + yy, tool_colour(STAGE_ICON[which][yy][xx]));
    if (on) canvas_rect(cv, x + 2, y + h + 1, w - 4, 1, C_YELLOW);
}
static void st_diamond(Canvas *c, int x, int y, Rgb k) { canvas_px(c, x, y - 2, k); canvas_rect(c, x - 1, y - 1, 3, 1, k); canvas_rect(c, x - 2, y, 5, 1, k); canvas_rect(c, x - 1, y + 1, 3, 1, k); canvas_px(c, x, y + 2, k); }
static void st_value_box(Canvas *cv, int x, int y, int w, const char *v, int on) {
    canvas_rect(cv, x, y, w, 7, on ? RGB(60, 60, 80) : RGB(34, 34, 44));
    if (on) frame(cv, x, y, w, 7, C_YELLOW);
    canvas_text(cv, x + w - 2 - text_width(v), y + 1, v, on ? C_YELLOW : C_WHITE);
}
static void st_box(const Part *p, PartPose q, int *x, int *y, int *w, int *h) { *x = p->box[0] + (int)lround(q.x); *y = p->box[1] + (int)lround(q.y); *w = p->box[2]; *h = p->box[3]; }

typedef struct { int gx, ty, cw, ah, z, px, fw, th, y0; } StLayout; /* the stage's corner and size, the panel's left and width, the timeline's height and top */
static StLayout st_layout(const Stage *s, int W, int H) {
    StLayout L;
    L.th = H >= 150 ? 3 + 12 + ST_TH + 22 : 15; /* (too short: the timeline's line only) */
    L.y0 = H - L.th;
    L.gx = 4; L.ty = 12;
    L.fw = W >= 280 ? 104 : W >= 200 ? 96 : 84; /* (a narrow window: a narrower panel) */
    L.px = W - 4 - L.fw;
    L.cw = L.px - 10 - L.gx;
    L.ah = L.y0 - 12 - L.ty;
    L.z = s->zoom > 0 ? s->zoom : L.cw >= 120 ? 2 : 1;
    return L;
}

static void st_dim(Canvas *cv, int x, int y, int w, int h) { /* greyed out: every pixel halfway to the ground */
    for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++) if (xx >= 0 && yy >= 0 && xx < cv->w && yy < cv->h) { Rgb c = cv->p[yy * cv->w + xx]; cv->p[yy * cv->w + xx] = RGB((c.r + 18) / 2, (c.g + 18) / 2, (c.b + 24) / 2); }
}
static void st_mark(Canvas *cv, int x, int y, Rgb k) { /* a key's mark: a small plus with a dark edge, readable on any ground */
    Rgb d = RGB(20, 20, 28);
    canvas_rect(cv, x - 2, y - 1, 5, 3, d); canvas_rect(cv, x - 1, y - 2, 3, 5, d);
    canvas_rect(cv, x - 1, y, 3, 1, k); canvas_rect(cv, x, y - 1, 1, 3, k);
}
static void st_line_in(Canvas *cv, int x0, int y0, int x1, int y1, Rgb k, int cx0, int cy0, int cx1, int cy1) { /* a line, only inside the box (cx0, cy0)-(cx1, cy1) */
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int n = 0; n < 4000; n++) {
        if (x0 >= cx0 && x0 <= cx1 && y0 >= cy0 && y0 <= cy1) canvas_px(cv, x0, y0, k);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void stage_draw(Stage *s, Canvas *cv, double wall) {
    g_nreg = 0;
    canvas_rect(cv, 0, 0, cv->w, cv->h, ST_BG);
    StLayout L = st_layout(s, cv->w, cv->h);
    const PartAnim *a = &s->a;
    double t = st_now_t(s, wall);
    int nf = st_nf(a), cur = (int)lround(t * ST_FPS) % nf;
    char buf[160];
    snprintf(buf, sizeof buf, "EDIT %s  ANIMS/%s.TXT  %d PART%s", a->name, s->name, a->npart, a->npart == 1 ? "" : "S");
    for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 4, 3, buf, C_YELLOW);
    /* the stage: the room at this time, a crop of it, every part outlined (the picked one yellow), the picked part's keys */
    Canvas shot = st_shot(a, t);
    if (s->onion && !s->playing) { /* onion skin: where the frame before / after differs from this one, it shows faintly red / blue */
        int fb = (cur + nf - 1) % nf, fa = (cur + 1) % nf;
        if (!a->loop) { fb = cur > 0 ? cur - 1 : -1; fa = cur + 1 < nf ? cur + 1 : -1; }
        for (int side = 0; side < 2; side++) {
            int f = side ? fa : fb;
            if (f < 0) continue;
            Canvas o = st_shot(a, st_t(f));
            Rgb tint = side ? RGB(90, 150, 240) : RGB(230, 90, 90);
            for (int i = 0; i < shot.w * shot.h && i < o.w * o.h; i++) if (!rgb_eq(o.p[i], shot.p[i])) {
                Rgb c = shot.p[i], g = o.p[i];
                Rgb gt = RGB((g.r + tint.r) / 2, (g.g + tint.g) / 2, (g.b + tint.b) / 2);
                shot.p[i] = RGB((c.r * 6 + gt.r * 4) / 10, (c.g * 6 + gt.g * 4) / 10, (c.b * 6 + gt.b * 4) / 10);
            }
            canvas_free(&o);
        }
    }
    int vw = L.cw / L.z, vh = L.ah / L.z;
    if (s->focus && s->pick >= 0 && s->pick < a->npart && a->part[s->pick].has_box) { /* the picked part in the middle of the stage (as far as the room lets it) */
        const Part *fp = &a->part[s->pick];
        PartPose fq = parts_pose(a, s->pick, t);
        s->vx = fp->box[0] + (int)lround(fq.x) + fp->box[2] / 2 - vw / 2;
        s->vy = fp->box[1] + (int)lround(fq.y) + fp->box[3] / 2 - vh / 2;
    }
    s->focus = 0;
    int vx = s->vx, vy = s->vy; /* (the view may go past the room's edge by half of itself, so any part can be in its middle) */
    if (vx + vw / 2 > shot.w) vx = shot.w - vw / 2;
    if (vy + vh / 2 > shot.h) vy = shot.h - vh / 2;
    if (vx < -vw / 2) vx = -vw / 2;
    if (vy < -vh / 2) vy = -vh / 2;
    s->vx = vx; s->vy = vy; s->vw = vw; s->vh = vh;
    canvas_rect(cv, L.gx, L.ty, L.cw, L.ah, ST_BG);
    st_crop(cv, &shot, vx, vy, vw, vh, L.gx, L.ty, L.z);
    for (int i = 0; i < a->npart; i++) {
        const Part *p = &a->part[i];
        if (!p->has_box) continue;
        int bx, by, bw, bh;
        st_box(p, parts_pose(a, i, t), &bx, &by, &bw, &bh);
        Rgb oc = i == s->pick ? C_YELLOW : s->tool == 2 && p->sprite ? RGB(58, 62, 78) : RGB(95, 105, 135); /* (ROTATE: a sprite's outline dim, it cannot be picked) */
        double turn = parts_pose(a, i, t).turn;
        int cx0 = L.gx, cy0 = L.ty, cx1 = L.gx + L.cw - 1, cy1 = L.ty + L.ah - 1; /* (outlines stay inside the stage: a part out of view draws nothing over the panel) */
        if (fabs(turn) < 0.5) { /* (frame()'s border, one pixel outside the box, line by line inside the stage) */
            int x0 = L.gx + (bx - vx) * L.z - 1, y0 = L.ty + (by - vy) * L.z - 1, x1 = x0 + bw * L.z + 1, y1 = y0 + bh * L.z + 1;
            st_line_in(cv, x0, y0, x1, y0, oc, cx0, cy0, cx1, cy1); st_line_in(cv, x0, y1, x1, y1, oc, cx0, cy0, cx1, cy1);
            st_line_in(cv, x0, y0, x0, y1, oc, cx0, cy0, cx1, cy1); st_line_in(cv, x1, y0, x1, y1, oc, cx0, cy0, cx1, cy1);
        }
        else { /* turned: the outline turned about its middle, and a mark on its top edge */
            double cxp = L.gx + (bx - vx + bw / 2.0) * L.z, cyp = L.ty + (by - vy + bh / 2.0) * L.z, w2 = bw * L.z / 2.0 + 1, h2 = bh * L.z / 2.0 + 1, ca = cos(turn * M_PI / 180), sa = sin(turn * M_PI / 180);
            double cx4[4] = {-w2, w2, w2, -w2}, cy4[4] = {-h2, -h2, h2, h2};
            int px4[4], py4[4];
            for (int k = 0; k < 4; k++) { px4[k] = (int)lround(cxp + cx4[k] * ca - cy4[k] * sa); py4[k] = (int)lround(cyp + cx4[k] * sa + cy4[k] * ca); }
            for (int k = 0; k < 4; k++) st_line_in(cv, px4[k], py4[k], px4[(k + 1) % 4], py4[(k + 1) % 4], oc, cx0, cy0, cx1, cy1);
            st_line_in(cv, (px4[0] + px4[1]) / 2, (py4[0] + py4[1]) / 2, (int)lround(cxp), (int)lround(cyp), oc, cx0, cy0, cx1, cy1); /* (top middle to the middle: which way is up) */
        }
    }
    if (s->tool == 0 && s->pick < a->npart && a->part[s->pick].has_box) { /* SELECT: handles on the picked part's border (drag one to resize it) */
        int bx, by, bw, bh;
        st_box(&a->part[s->pick], parts_pose(a, s->pick, t), &bx, &by, &bw, &bh);
        int x0 = L.gx + (bx - vx) * L.z, y0 = L.ty + (by - vy) * L.z, x1 = x0 + bw * L.z, y1 = y0 + bh * L.z, xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
        int hx[8] = {x0, xm, x1, x0, x1, x0, xm, x1}, hy[8] = {y0, y0, y0, ym, ym, y1, y1, y1};
        for (int k = 0; k < 8; k++) if (hx[k] > L.gx && hx[k] < L.gx + L.cw - 1 && hy[k] > L.ty && hy[k] < L.ty + L.ah - 1) { canvas_rect(cv, hx[k] - 1, hy[k] - 1, 3, 3, C_YELLOW); canvas_px(cv, hx[k], hy[k], ST_BG); }
    }
    if (!s->hide_keys && s->pick < a->npart && a->part[s->pick].has_box) { /* the keys of the picked part: a line through where it is at each, in time order, and a small mark on each */
        const Part *p = &a->part[s->pick];
        int kx[PARTS_MAX_KEYS], ky[PARTS_MAX_KEYS];
        for (int k = 0; k < p->nkey; k++) {
            kx[k] = L.gx + (p->box[0] + p->box[2] / 2 + (int)lround(p->key[k].pose.x) - vx) * L.z + L.z / 2;
            ky[k] = L.ty + (p->box[1] + p->box[3] / 2 + (int)lround(p->key[k].pose.y) - vy) * L.z + L.z / 2;
        }
        for (int k = 1; k < p->nkey; k++) st_line_in(cv, kx[k - 1], ky[k - 1], kx[k], ky[k], RGB(130, 112, 52), L.gx + 1, L.ty + 1, L.gx + L.cw - 2, L.ty + L.ah - 2);
        for (int k = 0; k < p->nkey; k++) {
            if (kx[k] < L.gx + 3 || kx[k] > L.gx + L.cw - 4 || ky[k] < L.ty + 3 || ky[k] > L.ty + L.ah - 4) continue;
            int stacked = 0, here = fabs(p->key[k].t - t) < 0.5 / ST_FPS;
            for (int m = 0; m < p->nkey; m++) if (m != k && abs(kx[m] - kx[k]) <= 2 && abs(ky[m] - ky[k]) <= 2) stacked = 1;
            if (stacked) frame(cv, kx[k] - 3, ky[k] - 3, 7, 7, RGB(200, 170, 60)); /* (keys on top of each other: a ring round them) */
            st_mark(cv, kx[k], ky[k], here ? C_WHITE : C_YELLOW);
        }
    }
    frame(cv, L.gx, L.ty, L.cw, L.ah, RGB(90, 90, 110));
    region(R_SSTAGE, 0, L.gx, L.ty, L.cw, L.ah, RGB(0, 0, 0));
    if (s->pick < a->npart) {
        PartPose q = parts_pose(a, s->pick, t);
        snprintf(buf, sizeof buf, "PART %s  X %d  Y %d", a->part[s->pick].name, a->part[s->pick].box[0] + (int)lround(q.x), a->part[s->pick].box[1] + (int)lround(q.y));
        for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    }
    { /* the line under the stage: the part and where it is, or a message in its place; KEYS ON / OFF at the right (the text cut short so they never touch) */
        const char *kl = s->hide_keys ? "KEYS OFF" : "KEYS ON";
        int kx = L.gx + L.cw - text_width(kl) - 6, room = kx - 8 - L.gx;
        char line[160];
        snprintf(line, sizeof line, "%s", s->msg[0] ? s->msg : s->pick < a->npart ? buf : "");
        while (line[0] && text_width(line) > room) line[strlen(line) - 1] = 0;
        canvas_text(cv, L.gx, L.ty + L.ah + 4, line, s->msg[0] ? C_YELLOW : C_GREY);
        button(cv, kx, L.ty + L.ah + 2, kl, !s->hide_keys, R_SKEYSHOW, 0);
    }
    if (s->pick >= 0 && s->pick < a->npart) { /* GROW: how much of the picked part has come in at this frame (a running figure, a bar; click or drag it) */
        static const char *const RUN[9] = {"....ww...", "....ww...", "..wwww.w.", ".w.ww.w..", "...www...", "..w...w..", ".w.....w.", "w.......w", "........."};
        int gx0 = L.px, gy0 = L.ty + L.ah + 2, bx0 = gx0 + 13, bw = L.fw - 13, g = (int)lround(parts_pose(a, s->pick, t).grow);
        g = g < 0 ? 0 : g > 100 ? 100 : g;
        for (int yy = 0; yy < 9; yy++) for (int xx = 0; xx < 9; xx++) if (RUN[yy][xx] == 'w') canvas_px(cv, gx0 + 1 + xx, gy0 + yy, s->growdrag ? C_YELLOW : RGB(170, 170, 190));
        canvas_rect(cv, bx0, gy0 + 2, bw, 5, RGB(34, 34, 44));
        canvas_rect(cv, bx0, gy0 + 2, bw * g / 100, 5, C_YELLOW);
        frame(cv, bx0, gy0 + 2, bw, 5, RGB(60, 60, 76));
        region(R_SGROW, 0, bx0 - 2, gy0, bw + 4, 9, RGB(0, 0, 0));
    }
    /* the panel: the tools, the picked part, its values at this frame, the keys' buttons, the parts */
    int x = L.px, y = L.ty, fw = L.fw, pbot = L.ty + L.ah;
    static const char *LONG[4] = {"SELECT", "MOVE", "ROTATE", "KEY"}, *SHORT[4] = {"SEL", "MOVE", "ROT", "KEY"};
    const char *const *TOOL = LONG;
    int roww = 0;
    for (int i = 0; i < 4; i++) roww += text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13;
    if (roww + 3 * 4 > fw) { TOOL = SHORT; roww = 0; for (int i = 0; i < 4; i++) roww += text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13; } /* (a narrow window: the names never touch) */
    for (int i = 0, used = 0; i < 4; i++) {
        int cw = text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13, tx = x + used + (fw - roww) * i / 3;
        used += cw;
        int off = i == 2 && s->pick < a->npart && a->part[s->pick].sprite; /* (a sprite does not rotate) */
        int keyed = i == 3 && s->pick < a->npart && st_key_at(&a->part[s->pick], st_t(cur)) >= 0; /* (KEY is not a tool: a toggle, pressed in when this frame is a key of the picked part) */
        stage_tool(cv, tx + (cw - 13) / 2, y, i, i == 3 ? keyed : s->tool == i && !off);
        if (off) st_dim(cv, tx + (cw - 13) / 2, y, 13, 15);
        canvas_text(cv, tx + (cw - text_width(TOOL[i])) / 2, y + 16, TOOL[i], off ? RGB(70, 70, 86) : keyed ? C_YELLOW : i != 3 && s->tool == i ? C_WHITE : C_GREY);
        region(R_STOOL, i, tx, y, cw, 21, RGB(0, 0, 0));
    }
    int top = L.ty + 27, pvis = pbot - top;
    if (pvis < 16) pvis = 16;
    region(R_SPANEL, 0, x, top, fw + 2, pvis, RGB(0, 0, 0));
    int n0 = g_nreg;
    Canvas pc = canvas_new(fw + 2, 300, ST_BG); /* (the rest of the panel is drawn whole, then the part that is in view is copied: it scrolls) */
    const Part *pp = s->pick < a->npart ? &a->part[s->pick] : NULL;
    PartPose q = pp ? parts_pose(a, s->pick, t) : (PartPose){0, 0, 0, 0, 1, 100};
    int kat = pp ? st_key_at(pp, st_t(cur)) : -1, py = 0, row_y[ST_ROWS] = {0};
    if (pp) {
        canvas_text(&pc, 0, py, "PART", RGB(84, 84, 104)); canvas_rect(&pc, 0, py + 7, fw, 1, RGB(40, 40, 52));
        py += 12;
        canvas_rect(&pc, 0, py, 52, 7, RGB(34, 34, 44));
        snprintf(buf, sizeof buf, "%s", pp->name); for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        canvas_text(&pc, 2, py + 1, buf, C_WHITE);
        snprintf(buf, sizeof buf, "%s", pp->group); for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        canvas_text(&pc, 56, py + 1, buf, C_GREY);
        py += 11;
        canvas_text(&pc, 0, py, kat >= 0 ? "AT THIS FRAME  (A KEY)" : "AT THIS FRAME", RGB(84, 84, 104)); canvas_rect(&pc, 0, py + 7, fw, 1, RGB(40, 40, 52));
        py += 12;
        double vals[ST_ROWS] = {q.x, q.y, q.turn, q.show, q.glow};
        for (int i = 0; i < ST_ROWS; i++, py += 9) {
            int on = i == s->row, off = i == 2 && pp->sprite;
            row_y[i] = py;
            canvas_text(&pc, 0, py + 1, ST_ROW[i], off ? RGB(70, 70, 86) : on ? C_WHITE : C_GREY);
            char v[16];
            if (i == 3) snprintf(v, sizeof v, "%s", q.show ? "YES" : "NO"); else snprintf(v, sizeof v, "%d", (int)lround(vals[i]));
            int vw2 = fw >= 96 ? 26 : 22, vbx = fw - vw2 - 8;
            st_value_box(&pc, vbx, py, vw2, v, on);
            if (off) { st_dim(&pc, vbx, py, vw2, 7); continue; } /* (a sprite: no rotation, no buttons) */
            region(R_SVAL, i * 3, vbx, py, vw2, 7, RGB(0, 0, 0));
            canvas_text(&pc, vbx - 6, py + 1, "<", on ? C_YELLOW : RGB(84, 84, 104)); region(R_SVAL, i * 3 + 1, vbx - 7, py, 7, 7, RGB(0, 0, 0));
            canvas_text(&pc, vbx + vw2 + 2, py + 1, ">", on ? C_YELLOW : RGB(84, 84, 104)); region(R_SVAL, i * 3 + 2, vbx + vw2 + 1, py, 7, 7, RGB(0, 0, 0));
        }
        py += 3;
        int bx = 0;
        snprintf(buf, sizeof buf, "%s", kat >= 0 ? ST_EASE[pp->key[kat].ease] : "EASE");
        button(&pc, bx, py, buf, 0, R_SEASE, 0);
        py += 13;
    }
    canvas_text(&pc, 0, py, "PARTS", RGB(84, 84, 104)); canvas_rect(&pc, 0, py + 7, fw, 1, RGB(40, 40, 52));
    py += 12;
    for (int i = 0, bx = 0; i < a->npart && py + 9 < pc.h; i++) {
        snprintf(buf, sizeof buf, "%s", a->part[i].name); for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        int bw = text_width(buf) + 6;
        if (bx + bw > fw) { bx = 0; py += 11; }
        if (s->tool == 2 && a->part[i].sprite) { canvas_rect(&pc, bx, py, bw, 9, RGB(28, 28, 36)); canvas_text(&pc, bx + 3, py + 2, buf, RGB(70, 70, 86)); } /* (ROTATE: a sprite cannot be picked) */
        else button(&pc, bx, py, buf, i == s->pick, R_SPART, i);
        bx += bw + 3;
    }
    py += 11;
    int most = py > pvis ? py - pvis : 0; /* how far it can scroll */
    if (s->pfollow && pp) { /* (a value picked with the arrows: scrolled into view) */
        if (row_y[s->row] - 14 < s->pscroll) s->pscroll = row_y[s->row] - 14;
        if (row_y[s->row] + 10 > s->pscroll + pvis) s->pscroll = row_y[s->row] + 10 - pvis;
    }
    s->pfollow = 0;
    if (s->pscroll > most) s->pscroll = most;
    if (s->pscroll < 0) s->pscroll = 0;
    for (int k = n0; k < g_nreg; k++) { /* (what was registered in the panel's own coordinates: to the screen, and only what is in view) */
        Region *r = &g_reg[k];
        int y0 = r->y0 + top - s->pscroll, y1 = r->y1 + top - s->pscroll;
        if (y1 <= top || y0 >= top + pvis) { r->x0 = r->x1 = -100; continue; }
        r->x0 += x; r->x1 += x; r->y0 = y0 < top ? top : y0; r->y1 = y1 > top + pvis ? top + pvis : y1;
    }
    st_crop(cv, &pc, 0, s->pscroll, fw + 2, pvis, x, top, 1);
    if (most) { /* a scroll bar at the panel's right edge: where what is shown is in all of it */
        int bx2 = x + fw + 1, k0 = top + pvis * s->pscroll / py, k1 = top + pvis * (s->pscroll + pvis) / py;
        canvas_rect(cv, bx2, top, 1, pvis, RGB(40, 40, 52));
        canvas_rect(cv, bx2, k0, 1, k1 - k0 > 4 ? k1 - k0 : 4, RGB(140, 140, 160));
    }
    canvas_free(&pc);
    /* the timeline: its line, the thing as in the room at its left, the frames as pictures with the picked part's keys under them */
    int ty0 = L.y0, ly = ty0 + 3;
    canvas_rect(cv, 0, ty0, cv->w, 1, RGB(44, 44, 56));
    const char *sh = cv->w >= 220 ? "? SHORTCUTS  Q QUIT" : "? HELP  Q QUIT";
    int lw = text_width(sh) + 16, sx0 = lw, sx1 = cv->w - 4;
    canvas_rect(cv, sx0 - 8, ty0 + 1, 1, L.th - 1, RGB(40, 40, 52));
    canvas_text(cv, 4, ly + 2, sh, RGB(150, 150, 170));
    const char *spd = s->speed < 0.75 ? "0.5X" : s->speed > 1.5 ? "2X" : "1X", *play = s->playing ? "STOP" : "PLAY";
    int bw = text_width("ONION") + 9 + text_width(spd) + 9 + text_width(play) + 6, bx = sx1 - bw;
    snprintf(buf, sizeof buf, "[ %s ]  FRAME %d OF %d   %.1f S", a->name, cur + 1, nf, a->length);
    if (sx0 + text_width(buf) + 8 > bx) snprintf(buf, sizeof buf, "[ %s ] %d/%d", a->name, cur + 1, nf); /* (no room: the short way, like the editor) */
    while (buf[0] && sx0 + text_width(buf) + 8 > bx) buf[strlen(buf) - 1] = 0;
    for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, sx0, ly + 2, buf, C_YELLOW);
    button(cv, bx, ly, "ONION", s->onion, R_ONION, 0); bx += text_width("ONION") + 9;
    button(cv, bx, ly, spd, 0, R_SPEED, 0); bx += text_width(spd) + 9;
    button(cv, bx, ly, play, s->playing, R_PLAY, 0);
    if (L.th > 15) {
        int ty = ly + 12, ah = L.th - 15, aw = lw - 16;
        st_crop(cv, &shot, vx, vy, aw, ah > 40 ? 40 : ah, 4, ty, 1); /* the thing as in the room, 1x */
        frame(cv, 4, ty, aw, ah > 40 ? 40 : ah, RGB(70, 70, 92));
        int cap = (sx1 - sx0 + 6) / (ST_TW + 6);
        if (cap < 1) cap = 1;
        if (cur != s->seen) { /* (the frame changed: the strip brings it into view; scrolling the strip by hand is left alone) */
            if (cur < s->first) s->first = cur;
            if (cur >= s->first + cap) s->first = cur - cap + 1;
            s->seen = cur;
        }
        if (s->first > nf - cap) s->first = nf - cap;
        if (s->first < 0) s->first = 0;
        s->cap = cap;
        int tx = sx0, shown = 0;
        region(R_STRIP, 0, sx0, ty - 1, sx1 - sx0, ST_TH + 12, RGB(0, 0, 0));
        if (s->thumb_pick != s->pick) { memset(s->thumb_ok, 0, sizeof s->thumb_ok); s->thumb_pick = s->pick; }
        for (int f = s->first; f < nf && tx + ST_TW <= sx1; f++, tx += ST_TW + 6, shown++) {
            if (!s->thumb_ok[f]) { /* (a picture of the frame around the picked part, drawn the first time it is seen) */
                if (s->thumb[f].p) canvas_free(&s->thumb[f]);
                Canvas full = st_shot(a, st_t(f));
                int cx = vx + 4, cy = vy + 6;
                if (pp && pp->has_box) { cx = pp->box[0] + pp->box[2] / 2 - ST_TW / 2; cy = pp->box[1] + pp->box[3] / 2 - ST_TH / 2; }
                cx = cx > full.w - ST_TW ? full.w - ST_TW : cx; cy = cy > full.h - ST_TH ? full.h - ST_TH : cy; /* (inside the room: no black edge) */
                cx = cx < 0 ? 0 : cx; cy = cy < 0 ? 0 : cy;
                s->thumb[f] = canvas_new(ST_TW, ST_TH, ST_BG);
                st_crop(&s->thumb[f], &full, cx, cy, ST_TW, ST_TH, 0, 0, 1);
                canvas_free(&full);
                s->thumb_ok[f] = 1;
            }
            int on = f == cur;
            canvas_rect(cv, tx - 1, ty - 1, ST_TW + 2, ST_TH + 2, on ? RGB(70, 70, 92) : RGB(34, 34, 44));
            canvas_blit(cv, &s->thumb[f], tx, ty);
            if (on) frame(cv, tx - 1, ty - 1, ST_TW + 2, ST_TH + 2, C_YELLOW);
            region(R_FRAME, f, tx - 1, ty - 1, ST_TW + 2, ST_TH + 2, RGB(0, 0, 0));
            char nb[8]; snprintf(nb, sizeof nb, "%d", f + 1);
            canvas_text(cv, tx + ST_TW / 2 - text_width(nb) / 2, ty + ST_TH + 2, nb, on ? C_YELLOW : C_GREY); /* (the number and the key's diamond on one middle) */
            if (pp && st_key_at(pp, st_t(f)) >= 0) st_diamond(cv, tx + ST_TW / 2, ty + ST_TH + 11, C_YELLOW);
        }
        if (shown < nf) { /* a scroll bar: where the frames shown are among all of them; click or drag it, or use the wheel over the frames */
            int by = ty + ST_TH + 16, tw = sx1 - sx0, k0 = sx0 + tw * s->first / nf, k1 = sx0 + tw * (s->first + shown) / nf;
            canvas_rect(cv, sx0, by, tw, 3, RGB(40, 40, 52));
            canvas_rect(cv, k0, by, k1 - k0 > 6 ? k1 - k0 : 6, 3, s->sbdrag ? C_WHITE : RGB(140, 140, 160));
            region(R_SSCROLL, 0, sx0, by - 3, tw, 9, RGB(0, 0, 0));
            s->sb_x0 = sx0; s->sb_w = tw;
        }
    }
    if (s->asking) { /* q: the editor's own window (draw_asking): save or discard if something changed, else just quit */
        char a[96];
        snprintf(a, sizeof a, s->changed ? "SAVE THE CHANGES TO %s?" : "NOTHING CHANGED IN %s: QUIT?", s->name);
        for (char *q = a; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        const char *b = s->changed ? "S SAVE    D DISCARD    ESC BACK" : "S QUIT    ESC BACK";
        int w = (text_width(b) > text_width(a) ? text_width(b) : text_width(a)) + 12, h = 24, bx = (cv->w - w) / 2, by = (cv->h - h) / 2;
        canvas_rect(cv, bx - 1, by - 1, w + 2, h + 2, C_GREY);
        canvas_rect(cv, bx, by, w, h, RGB(20, 20, 28));
        canvas_text(cv, bx + (w - text_width(a)) / 2, by + 4, a, C_YELLOW);
        canvas_text(cv, bx + (w - text_width(b)) / 2, by + 14, b, C_WHITE);
    }
    if (s->help) { /* ?: the keys and the mouse, in a box over the stage; a window too short for it scrolls it (up / down, the wheel) */
        static const char *const H[] = {"HELP  (UP DOWN SCROLLS)", "MOUSE", "CLICK A PART: PICK IT", "SELECT: DRAG A BORDER TO RESIZE", "MOVE / ROTATE: DRAG ON THE STAGE", "KEY: A KEY AT THIS FRAME, OR NONE", "< > BESIDE A VALUE: CHANGE IT", "THE BAR BY THE RUNNER: HOW MUCH HAS COME IN", "CLICK A FRAME: GO THERE", "WHEEL: ZOOM / SCROLL THE PANEL", "RIGHT DRAG: MOVE THE VIEW",
                                        "KEYS", "LEFT RIGHT  FRAME", "UP DOWN  A VALUE", "< >  CHANGE IT", "+ -  ZOOM THE STAGE", "W A S D  MOVE THE VIEW", "0  ZOOM AS IT WAS, THE PART IN THE MIDDLE", "V  KEYS ON THE STAGE: ON / OFF", "SPACE  PLAY", "O  ONION SKIN (BEFORE RED, AFTER BLUE)", "[ ]  OTHER PART", "K  A KEY HERE OR NONE", "X  DELETE THE KEY", "E  KEY EASE", "U R  UNDO REDO", "Q  QUIT"};
        int n = (int)(sizeof H / sizeof *H), bw2 = 0;
        for (int i = 0; i < n; i++) if (text_width(H[i]) > bw2) bw2 = text_width(H[i]);
        int vis = (cv->h - 12 - 4 - 8) / 7, w = bw2 + 14; /* (as many lines as the window has room for) */
        if (vis > n) vis = n;
        if (vis < 3) vis = 3;
        int most = n - vis;
        if (s->hscroll > most) s->hscroll = most;
        if (s->hscroll < 0) s->hscroll = 0;
        int hh = vis * 7 + 8, hx = L.gx + (L.cw - w) / 2, hy = L.ty + (L.ah - hh) / 2;
        if (hy < 2) hy = 2;
        if (hy + hh > cv->h - 2) hy = cv->h - 2 - hh;
        if (hx + w > cv->w - 2) hx = cv->w - 2 - w;
        if (hx < 2) hx = 2;
        canvas_rect(cv, hx, hy, w, hh, RGB(26, 26, 36)); frame(cv, hx, hy, w, hh, RGB(110, 110, 140));
        for (int i = 0; i < vis; i++) { int k = i + s->hscroll; canvas_text(cv, hx + 6, hy + 4 + i * 7, H[k], k == 0 || !strcmp(H[k], "MOUSE") || !strcmp(H[k], "KEYS") ? C_YELLOW : C_WHITE); }
        if (most) { /* a scroll bar inside the box's right edge */
            int k0 = hy + 3 + (hh - 6) * s->hscroll / n, k1 = hy + 3 + (hh - 6) * (s->hscroll + vis) / n;
            canvas_rect(cv, hx + w - 4, hy + 3, 1, hh - 6, RGB(40, 40, 52));
            canvas_rect(cv, hx + w - 4, k0, 1, k1 - k0 > 4 ? k1 - k0 : 4, RGB(150, 150, 170));
        }
    }
    canvas_free(&shot);
}

/* the picker's card of a part animation: its room at time t, around its parts, as big as fits */
static void picker_anim(Canvas *cv, int ai, double t, int x, int y, int w, int h) {
    const PartAnim *a = parts_get(g_anim[ai]);
    if (!a || !a->nscene) { canvas_text(cv, x + w / 2 - 1, y + h / 2 - 2, "?", C_GREY); return; }
    t = a->loop ? fmod(t, a->length) : fmod(t, a->length + 0.5);
    if (t > a->length) t = a->length;
    Canvas shot = st_shot(a, t);
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (int i = 0; i < a->npart; i++) if (a->part[i].has_box) {
        const int *b = a->part[i].box;
        x0 = b[0] < x0 ? b[0] : x0; y0 = b[1] < y0 ? b[1] : y0;
        x1 = b[0] + b[2] > x1 ? b[0] + b[2] : x1; y1 = b[1] + b[3] > y1 ? b[1] + b[3] : y1;
    }
    if (x1 < x0) { x0 = a->view_x; y0 = a->view_y; x1 = x0 + 40; y1 = y0 + 26; }
    x0 -= 4; y0 -= 4; x1 += 4; y1 += 4;
    int z = w / (x1 - x0) < h / (y1 - y0) ? w / (x1 - x0) : h / (y1 - y0);
    z = z < 1 ? 1 : z > 4 ? 4 : z;
    int cw = w / z, ch = h / z, cx = (x0 + x1) / 2 - cw / 2, cy = (y0 + y1) / 2 - ch / 2;
    if (cx + cw > shot.w) cx = shot.w - cw;
    if (cy + ch > shot.h) cy = shot.h - ch;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    st_crop(cv, &shot, cx, cy, cw, ch, x + (w - cw * z) / 2, y + (h - ch * z) / 2, z);
    canvas_free(&shot);
}

/* ---- the window --------------------------------------------------------------------------------------------------------- */
static int stage_open(Stage *s, const char *name) {
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    s->speed = 1; s->row = 1; s->thumb_pick = -1; s->onion = 1;
    s->seen = -1;
    char path[700];
    snprintf(path, sizeof path, "%s/%s.txt", parts_mine_dir(), name);
    s->orig = parts_mine_dir()[0] ? slurp(path) : NULL;
    if (!parts_mine_dir()[0] || access(path, R_OK)) snprintf(path, sizeof path, "%s/%s.txt", parts_default_dir(), name);
    if (parts_load(&s->a, path)) return 1;
    parts_inherit(&s->a, name);
    if (!s->a.npart) { fprintf(stderr, "%s: no parts\n", path); return 1; }
    s->vx = s->a.view_x; s->vy = s->a.view_y; s->focus = 1; /* (it opens on its first part, in the middle) */
    if (!s->a.nscene) { fprintf(stderr, "%s: no scene lines: the editor's stage has nothing to show\n", path); return 1; }
    return 0;
}
static int st_part_at(const Stage *s, double t, int mx, int my, const StLayout *L, int sprites) { /* the part under the pointer, the smallest first (sprites: or not); -1 none */
    const PartAnim *a = &s->a;
    int best = -1, barea = 1 << 30;
    for (int i = 0; i < a->npart; i++) {
        const Part *p = &a->part[i];
        if (!p->has_box || (!sprites && p->sprite)) continue;
        int bx, by, bw, bh;
        st_box(p, parts_pose(a, i, t), &bx, &by, &bw, &bh);
        int X = (mx - L->gx) / L->z + s->vx, Y = (my - L->ty) / L->z + s->vy;
        if (X >= bx - 1 && X <= bx + bw && Y >= by - 1 && Y <= by + bh && bw * bh < barea) { best = i; barea = bw * bh; }
    }
    return best;
}
/* which borders of the picked part the pointer is on (1 left, 2 right, 4 top, 8 bottom; two for a corner), 0 if none */
static int st_edge_at(const Stage *s, const StLayout *L, double t, int mx, int my) {
    if (s->pick >= s->a.npart || !s->a.part[s->pick].has_box) return 0;
    int bx, by, bw, bh;
    st_box(&s->a.part[s->pick], parts_pose(&s->a, s->pick, t), &bx, &by, &bw, &bh);
    int x0 = L->gx + (bx - s->vx) * L->z, y0 = L->ty + (by - s->vy) * L->z, x1 = x0 + bw * L->z, y1 = y0 + bh * L->z, r = 3, m = 0;
    int inx = mx >= x0 - r && mx <= x1 + r, iny = my >= y0 - r && my <= y1 + r;
    if (iny && abs(mx - x0) <= r) m |= 1;
    if (iny && abs(mx - x1) <= r) m |= 2;
    if (inx && abs(my - y0) <= r) m |= 4;
    if (inx && abs(my - y1) <= r) m |= 8;
    if ((m & 3) == 3) m &= abs(mx - x0) < abs(mx - x1) ? ~2 : ~1; /* (a thin part: the nearer side) */
    if ((m & 12) == 12) m &= abs(my - y0) < abs(my - y1) ? ~8 : ~4;
    return m;
}
static void st_resize(Stage *s, const StLayout *L, int mx, int my) { /* the dragged borders follow the pointer (room pixels, inside what the stage shows) */
    Part *p = &s->a.part[s->pick];
    int X = s->vx + (mx - L->gx) / L->z, Y = s->vy + (my - L->ty) / L->z;
    X = X < s->vx ? s->vx : X > s->vx + s->vw ? s->vx + s->vw : X;
    Y = Y < s->vy ? s->vy : Y > s->vy + s->vh ? s->vy + s->vh : Y;
    int l = s->rsz_box[0] + s->rsz_off[0], t0 = s->rsz_box[1] + s->rsz_off[1], r = l + s->rsz_box[2], b = t0 + s->rsz_box[3]; /* (as they were, where the part is now) */
    if (s->rsz & 1) l = X < r ? X : r - 1;
    if (s->rsz & 2) r = X > l ? X : l + 1;
    if (s->rsz & 4) t0 = Y < b ? Y : b - 1;
    if (s->rsz & 8) b = Y > t0 ? Y : t0 + 1;
    p->box[0] = l - s->rsz_off[0]; p->box[1] = t0 - s->rsz_off[1]; p->box[2] = r - l; p->box[3] = b - t0;
    st_changed(s);
}
static void st_grow_to(Stage *s, const StLayout *L, int mx) { /* the grow bar: the picked part's grow at this frame where the pointer is (5% steps) */
    int bx0 = L->px + 13, bw = L->fw - 13, k = st_ensure_key(s, s->pick, st_t(s->frame));
    if (k < 0) return;
    double g = (double)(mx - bx0) * 100 / (bw - 1);
    g = 5 * lround(g / 5);
    s->a.part[s->pick].key[k].pose.grow = g < 0 ? 0 : g > 100 ? 100 : g;
    st_changed(s);
}
static void st_zoom(Stage *s, const StLayout *L, int d) { /* the stage's zoom one step, about the middle of what it shows */
    int z = L->z + d;
    if (z < 1) z = 1;
    if (z > 8) z = 8;
    if (z == L->z) return;
    double cx = s->vx + s->vw / 2.0, cy = s->vy + s->vh / 2.0;
    s->zoom = z;
    s->vx = (int)lround(cx - (L->cw / z) / 2.0); s->vy = (int)lround(cy - (L->ah / z) / 2.0);
}
static void st_pan(Stage *s, int dx, int dy) { int st = s->vw / 8 > 2 ? s->vw / 8 : 2; s->vx += dx * st; s->vy += dy * st; }
static void stage_dirs(const char *dir) { /* the folders of the animations: the defaults next to sprites/, yours next to your sprites */
    if (!parts_default_dir()[0]) { char rd[700]; snprintf(rd, sizeof rd, "%s", dir); char *sl = strrchr(rd, '/'); strcpy(sl ? sl : rd, "/anims"); parts_set_dir(rd); }
    if (!parts_mine_dir()[0]) { char md[700]; snprintf(md, sizeof md, "%s", getenv("CLAWD_MY_ANIMS") ? getenv("CLAWD_MY_ANIMS") : sprites_mine_dir()); if (!getenv("CLAWD_MY_ANIMS")) { char *sl = strrchr(md, '/'); if (sl) strcpy(sl, "/anims"); } parts_set_mine_dir(md); }
}
/* the stage page in a terminal that is already set up (the editor's sprite picker opens it too): until q; 1 if it cannot open */
static int stage_loop(const char *dir, const char *name) {
    static Stage s;
    (void)dir;
    if (stage_open(&s, name)) return 1;
    Cells prev = {0, 0, NULL};
    int quit = 0, cols = 0, rows = 0;
    char in[256]; int nin = 0;
    while (!quit) {
        struct winsize ws;
        int c = 120, r = 40;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col) { c = ws.ws_col; r = ws.ws_row; }
        if (c != cols || r != rows) { cols = c; rows = r; cells_free(&prev); prev.c = NULL; put("\x1b[H"); }
        double wall = wall_now();
        Canvas cv = canvas_new(cols, rows * 2, ST_BG);
        stage_draw(&s, &cv, wall);
        StLayout L = st_layout(&s, cv.w, cv.h);
        Cells cur = cells_from_canvas(&cv, NULL);
        size_t len;
        char *out = diff_frame(&cur, prev.c ? &prev : NULL, &len);
        put(out); put("\x1b[H");
        free(out);
        cells_free(&prev); prev = cur;
        canvas_free(&cv);
        fd_set fs; FD_ZERO(&fs); FD_SET(STDIN_FILENO, &fs);
        struct timeval tv = {0, s.playing ? 62500 : 200000};
        if (select(STDIN_FILENO + 1, &fs, NULL, NULL, &tv) <= 0) { if (nin == 1 && in[0] == 0x1b) { nin = 0; s.asking = 0; s.help = 0; } continue; }
        ssize_t k = read(STDIN_FILENO, in + nin, sizeof in - 1 - (size_t)nin);
        if (k <= 0) continue;
        nin += (int)k; in[nin] = 0;
        int nf = st_nf(&s.a), i = 0;
        double t = st_now_t(&s, wall);
        while (i < nin) {
            if (in[i] == 0x1b) {
                if (i + 1 >= nin) break;
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] == '<') { /* SGR mouse: ESC [ < b ; x ; y (M | m) */
                    int b, x, y, used = 0;
                    char fin;
                    if (sscanf(in + i, "\x1b[<%d;%d;%d%c%n", &b, &x, &y, &fin, &used) < 4 || !used) { if (strchr(in + i, 'M') || strchr(in + i, 'm')) { i++; continue; } break; }
                    i += used;
                    if (s.asking) continue;
                    int px = x - 1, py = (y - 1) * 2, btn = b & 3, motion = b & 32, release = fin == 'm';
                    const Region *rg = hit(px, py);
                    if (b & 64) { /* the wheel: over the stage it zooms, over the panel it scrolls it, over the frames it scrolls them */
                        int up = (b & 3) == 0 || (b & 3) == 2;
                        if (s.help) s.hscroll += up ? -2 : 2;
                        else if (px >= L.px && py >= L.ty + 27 && py < L.ty + L.ah) s.pscroll += up ? -9 : 9;
                        else if (px >= L.gx && px < L.gx + L.cw && py >= L.ty && py < L.ty + L.ah) st_zoom(&s, &L, up ? 1 : -1);
                        else if (rg && (rg->kind == R_STRIP || rg->kind == R_FRAME)) { s.first += up ? -1 : 1; if (s.first < 0) s.first = 0; if (s.first > nf - 1) s.first = nf - 1; }
                        continue;
                    }
                    if (s.sbdrag) { /* dragging the frame strip's scroll bar */
                        if (release) s.sbdrag = 0;
                        else if (motion && s.sb_w > 0) { s.first = (int)((long)(px - s.sb_x0) * nf / s.sb_w) - s.cap / 2; if (s.first > nf - s.cap) s.first = nf - s.cap; if (s.first < 0) s.first = 0; }
                        continue;
                    }
                    if (s.growdrag) { if (release) { s.growdrag = 0; st_save(&s); } else if (motion) st_grow_to(&s, &L, px); continue; } /* (dragging the grow bar) */
                    if (s.rsz && (motion || release)) { /* dragging a border of the picked part */
                        if (release) { s.rsz = 0; st_save(&s); } else st_resize(&s, &L, px, py);
                        continue;
                    }
                    if (s.pan) { /* dragging the view with the right button */
                        if (release) s.pan = 0;
                        else if (motion) { s.vx = s.pan_vx - (int)lround((px - s.pan_x0) / L.z); s.vy = s.pan_vy - (int)lround((py - s.pan_y0) / L.z); }
                        continue;
                    }
                    if (btn == 2 && !motion && !release && rg && rg->kind == R_SSTAGE) { s.pan = 1; s.pan_x0 = px; s.pan_y0 = py; s.pan_vx = s.vx; s.pan_vy = s.vy; continue; }
                    if (s.drag && (motion || release)) { /* dragging the picked part: MOVE moves it, ROTATE rotates it */
                        if (release) { s.drag = 0; st_save(&s); continue; }
                        int k2 = st_ensure_key(&s, s.pick, st_t(s.frame));
                        if (k2 >= 0) {
                            PartPose *q = &s.a.part[s.pick].key[k2].pose;
                            if (s.tool == 1) { /* MOVE: the part stays in what the stage shows */
                                const Part *pt = &s.a.part[s.pick];
                                double nx = s.dpose.x + lround((px - s.px0) / L.z), ny = s.dpose.y + lround((py - s.py0) / L.z);
                                if (pt->has_box) {
                                    double lox = s.vx - pt->box[0], hix = s.vx + s.vw - pt->box[2] - pt->box[0], loy = s.vy - pt->box[1], hiy = s.vy + s.vh - pt->box[3] - pt->box[1];
                                    if (hix >= lox) nx = nx < lox ? lox : nx > hix ? hix : nx;
                                    if (hiy >= loy) ny = ny < loy ? loy : ny > hiy ? hiy : ny;
                                }
                                q->x = nx; q->y = ny;
                            }
                            else q->turn = s.dpose.turn + (px - s.px0) * 2;
                            st_changed(&s);
                        }
                        continue;
                    }
                    if (release || motion || btn != 0 || !rg) continue;
                    s.msg[0] = 0;
                    if (rg->kind == R_SGROW) { s.playing = 0; st_push(&s); s.growdrag = 1; st_grow_to(&s, &L, px); continue; }
                    if (rg->kind == R_SSCROLL) { s.sbdrag = 1; s.first = (int)((long)(px - s.sb_x0) * nf / s.sb_w) - s.cap / 2; if (s.first > nf - s.cap) s.first = nf - s.cap; if (s.first < 0) s.first = 0; continue; }
                    if (rg->kind == R_SSTAGE) {
                        s.playing = 0;
                        if (s.tool == 0) { /* SELECT: a border of the picked part is dragged to resize it */
                            int m = st_edge_at(&s, &L, st_t(s.frame), px, py);
                            if (m) {
                                const Part *pt = &s.a.part[s.pick];
                                PartPose pz = parts_pose(&s.a, s.pick, st_t(s.frame));
                                st_push(&s);
                                s.rsz = m; memcpy(s.rsz_box, pt->box, sizeof s.rsz_box); s.rsz_off[0] = (int)lround(pz.x); s.rsz_off[1] = (int)lround(pz.y);
                                continue;
                            }
                        }
                        int hp = st_part_at(&s, st_t(s.frame), px, py, &L, s.tool != 2); /* (the part pressed on is the one the tool works on; ROTATE: not a sprite) */
                        if (hp < 0) { if (s.tool == 2 && st_part_at(&s, st_t(s.frame), px, py, &L, 1) >= 0) st_msg(&s, "A SPRITE MOVES, IT DOES NOT ROTATE"); continue; } /* (nothing there: nothing happens) */
                        s.pick = hp;
                        if (s.tool == 0) {}
                        else { /* MOVE / ROTATE: a drag on the stage changes the picked part at this frame */
                            st_push(&s);
                            int k2 = st_ensure_key(&s, s.pick, st_t(s.frame));
                            if (k2 >= 0) { s.drag = 1; s.px0 = px; s.py0 = py; s.dpose = s.a.part[s.pick].key[k2].pose; }
                        }
                    }
                    else if (rg->kind == R_STOOL) { if (rg->i == 3) { s.playing = 0; st_toggle_key(&s); } else if (rg->i == 2 && s.a.part[s.pick].sprite) st_msg(&s, "A SPRITE MOVES, IT DOES NOT ROTATE"); else s.tool = rg->i; } /* (KEY: a key here or none; ROTATE greyed for a sprite: not picked) */
                    else if (rg->kind == R_SKEYSHOW) s.hide_keys = !s.hide_keys;
                    else if (rg->kind == R_SPART) { s.pick = rg->i; s.focus = 1; }
                    else if (rg->kind == R_SVAL) { s.row = rg->i / 3; if (rg->i % 3) { s.playing = 0; st_change(&s, s.row, rg->i % 3 == 2 ? 1 : -1); } }
                    else if (rg->kind == R_SKEY) { s.playing = 0; st_toggle_key(&s); }
                    else if (rg->kind == R_SEASE) { s.playing = 0; st_ease(&s); }
                    else if (rg->kind == R_FRAME) { s.frame = rg->i; s.playing = 0; }
                    else if (rg->kind == R_PLAY) { s.playing = !s.playing; s.play_t0 = wall_now() - st_t(s.frame) / s.speed; }
                    else if (rg->kind == R_ONION) s.onion = !s.onion;
                    else if (rg->kind == R_SPEED) s.speed = s.speed < 0.75 ? 1 : s.speed < 1.5 ? 2 : 0.5;
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] >= 'A' && in[i + 2] <= 'D') { /* arrows: left / right the frame, up / down the value */
                    char ar = in[i + 2];
                    i += 3;
                    s.playing = 0;
                    if (s.help && (ar == 'A' || ar == 'B')) { s.hscroll += ar == 'B' ? 1 : -1; continue; } /* (the help box open: up / down scroll it) */
                    if (ar == 'C' || ar == 'D') { s.frame = (s.frame + (ar == 'C' ? 1 : nf - 1)) % nf; if (s.frame < s.first) s.first = s.frame; }
                    else { s.row = (s.row + (ar == 'B' ? 1 : ST_ROWS - 1)) % ST_ROWS; s.pfollow = 1; }
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin) { i += 3; continue; }
                s.asking = 0; s.help = 0; /* (a lone Esc: back to editing) */
                i += 1; continue;
            }
            char ch = in[i++];
            s.msg[0] = 0;
            if (ch == '?') s.help = !s.help;
            else if (s.asking) { if (ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y') quit = 1; else if (ch == 'd' || ch == 'D' || ch == 'n' || ch == 'N') { if (s.changed) st_discard(&s); quit = 1; } else s.asking = 0; }
            else if (ch == 0x03 || ch == 'q' || ch == 'Q') s.asking = 1; /* (always: keep or discard if you changed something, else just quit) */
            else if (ch == ' ') { s.playing = !s.playing; s.play_t0 = wall_now() - st_t(s.frame) / s.speed; }
            else if (ch == '.' || ch == '>') { s.playing = 0; st_change(&s, s.row, 1); s.pfollow = 1; } /* (the value, like the < > beside it) */
            else if (ch == ',' || ch == '<') { s.playing = 0; st_change(&s, s.row, -1); s.pfollow = 1; }
            else if (ch == '+' || ch == '=') st_zoom(&s, &L, 1);
            else if (ch == '-') st_zoom(&s, &L, -1);
            else if (ch == 'v' || ch == 'V') s.hide_keys = !s.hide_keys;
            else if (ch == 'o' || ch == 'O') s.onion = !s.onion;
            else if (ch == '0') { s.zoom = 0; s.vx = s.a.view_x; s.vy = s.a.view_y; s.focus = 1; } /* (the zoom as it was, the picked part in the middle) */
            else if (ch == 'w' || ch == 'W') st_pan(&s, 0, -1);
            else if (ch == 's' || ch == 'S') st_pan(&s, 0, 1);
            else if (ch == 'a' || ch == 'A') st_pan(&s, -1, 0);
            else if (ch == 'd' || ch == 'D') st_pan(&s, 1, 0);
            else if (ch == ']' || ch == '[') { /* the next / last part (ROTATE: the sprites are skipped) */
                for (int n = 0; n < s.a.npart; n++) { s.pick = (s.pick + (ch == ']' ? 1 : s.a.npart - 1)) % s.a.npart; if (!(s.tool == 2 && s.a.part[s.pick].sprite)) break; }
                s.focus = 1;
            }
            else if (ch == 'k' || ch == 'K') st_toggle_key(&s);
            else if (ch == 'x' || ch == 'X') st_del_key(&s);
            else if (ch == 'e' || ch == 'E') st_ease(&s);
            else if (ch == 0x1A || ch == 'u') { if (s.nundo) { s.nundo--; st_swap(&s, s.nundo); s.nredo++; st_save(&s); } }
            else if (ch == 0x19 || ch == 'r') { if (s.nredo) { st_swap(&s, s.nundo); s.nundo++; s.nredo--; st_save(&s); } }
        }
        memmove(in, in + i, (size_t)(nin - i)); nin -= i;
        (void)t;
        if (getenv("CLAWD_STAGE_LOG")) { FILE *lg = fopen(getenv("CLAWD_STAGE_LOG"), "a"); if (lg) { fprintf(lg, "tool=%d view=%d,%d,%d pick=%s frame=%d keyed=%d\n", s.tool, s.vx, s.vy, L.z, s.a.part[s.pick].name, s.frame, st_key_at(&s.a.part[s.pick], st_t(s.frame)) >= 0); fclose(lg); } } /* (for the tests: what the page is on after each input) */
    }
    cells_free(&prev);
    for (int f = 0; f < ST_MAXF; f++) if (s.thumb[f].p) canvas_free(&s.thumb[f]);
    free(s.orig);
    return 0;
}
static int stage_run(const char *dir, const char *name) { /* --edit anims/NAME */
    stage_dirs(dir);
    { static Stage probe; int bad = stage_open(&probe, name); free(probe.orig); if (bad) return 1; } /* (a file that does not load says so before the terminal changes) */
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) { fprintf(stderr, "clawd-room --edit needs a terminal\n"); return 1; }
    tty_on();
    int rc = stage_loop(dir, name);
    tty_off();
    return rc;
}
