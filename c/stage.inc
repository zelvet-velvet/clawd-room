/* The stage page of the pixel editor (Interaction, "Part animations in the pixel editor"): clawd-room --edit anims/NAME opens an
 * animation of anims/ (parts.c). The canvas shows the room with the thing on its stage and every part outlined; the panel holds the
 * picked part and its values at this frame; the timeline shows the frames as pictures with a diamond under each keyframe of the
 * picked part. Included by editor.c (it uses the editor's drawing helpers and its terminal code); what you change is saved as you go
 * to ~/.clawd-room/anims/NAME.txt (yours over the default), and a running room reads it again.
 *   mouse: a part on the stage or in the list picks it; SELECT / MOVE / TURN / KEY tools: MOVE drags the picked part, TURN drags
 *          its angle, KEY adds a key at this frame; < > beside a value change it at this frame (a key is made if there is none)
 *   keys:  left / right frame, Space play, up / down value, + - change it, [ ] part, k add key, x delete key, e ease of the key,
 *          u / r undo / redo, q quit */
#include "fixture.h"
#include "parts.h"

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n); /* draw.c */

enum { R_SSTAGE = 64, R_SVAL, R_SPART, R_SKEY, R_SKEYDEL, R_SEASE, R_STOOL };
#define ST_UNDO 24
#define ST_FPS 16          /* the frames: 16 a second, like the room */
#define ST_MAXF 300
#define ST_TW 44           /* a frame as a picture */
#define ST_TH 26
#define ST_BG RGB(18, 18, 24)
#define ST_ROWS 5          /* the values: move x, move y, turn, show, glow */
static const char *const ST_ROW[ST_ROWS] = {"MOVE X", "MOVE Y", "TURN", "SHOW", "GLOW"};
static const char *const ST_EASE[] = {"LINEAR", "OUT", "IN OUT", "BACK", "IN", "SINE", "SINE OUT", "SINE IN"};

typedef struct {
    PartAnim a, undo[ST_UNDO];
    int nundo, nredo;
    char name[24];
    int pick, row, frame, tool, playing, first, help, vx, vy, asking, changed;
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
 * animation began, t ago), rendered once to settle the room, then at that moment */
static Canvas st_shot(const PartAnim *a, double t) {
    static Fixture fx;
    char text[PARTS_SCENE_LINES * 200 + 16], sid[PARTS_SCENE_LINES][24], kind[PARTS_SCENE_LINES][16];
    int nemote = 0;
    text[0] = 0;
    for (int i = 0; i < a->nscene; i++) {
        if (!strncmp(a->scene[i], "emote ", 6)) { if (nemote < PARTS_SCENE_LINES && sscanf(a->scene[i] + 6, "%23s %15s", sid[nemote], kind[nemote]) == 2) nemote++; continue; }
        size_t n = strlen(text);
        snprintf(text + n, sizeof text - n, "%s\n", a->scene[i]);
    }
    if (fixture_load_text(&fx, text)) return canvas_new(8, 8, ST_BG);
    fx.room.freeze = 0; /* (it moves: the swing's Clawd by the animation) */
    char note[64];
    Canvas settle = render(fx.cols, fx.rows, &fx.room, fx.now, fx.t, note, sizeof note);
    canvas_free(&settle);
    for (int i = 0; i < nemote; i++) room_emote(&fx.room, sid[i], kind[i], fx.now - t, 100000.0);
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
static void st_msg_up(Stage *s, const char *m) { st_msg(s, m); for (char *q = s->msg; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32; } /* (the font has capitals only) */
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
    st_push(s);
    int k = st_ensure_key(s, s->pick, st_t(s->frame));
    if (k < 0) return;
    st_value(&s->a.part[s->pick].key[k].pose, row, delta);
    st_changed(s);
    st_save(s);
}
static void st_add_key(Stage *s) {
    Part *p = &s->a.part[s->pick];
    char m[96];
    if (st_key_at(p, st_t(s->frame)) >= 0) { snprintf(m, sizeof m, "%s ALREADY HAS A KEY AT FRAME %d", p->name, s->frame + 1); st_msg_up(s, m); return; }
    st_push(s);
    if (st_ensure_key(s, s->pick, st_t(s->frame)) >= 0) { st_changed(s); st_save(s); snprintf(m, sizeof m, "KEY ADDED: %s AT FRAME %d", p->name, s->frame + 1); st_msg_up(s, m); }
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
    {"..bbbbb..", ".b.....b.", "b.......b", "b.......b", "b......bbb", ".b......b.", "..bbbbb.b.", ".......b..", "........."},  /* TURN: an arrow round */
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
    L.z = L.cw >= 120 ? 2 : 1;
    (void)s;
    return L;
}

static void stage_draw(Stage *s, Canvas *cv, double wall) {
    g_nreg = 0;
    canvas_rect(cv, 0, 0, cv->w, cv->h, ST_BG);
    StLayout L = st_layout(s, cv->w, cv->h);
    const PartAnim *a = &s->a;
    double t = st_now_t(s, wall);
    int nf = st_nf(a), cur = (int)lround(t * ST_FPS) % nf;
    char buf[160];
    snprintf(buf, sizeof buf, "EDIT %s  ANIMS/%s.TXT  %d PARTS", a->name, s->name, a->npart);
    for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 4, 3, buf, C_YELLOW);
    if (s->msg[0]) canvas_text(cv, cv->w - 6 - text_width(s->msg), 3, s->msg, C_YELLOW);
    /* the stage: the room at this time, a crop of it, every part outlined (the picked one yellow), the picked part's path dotted */
    Canvas shot = st_shot(a, t);
    int vw = L.cw / L.z, vh = L.ah / L.z;
    int vx = a->view_x, vy = a->view_y; /* (the piece of the room shown stays inside it) */
    if (vx + vw > shot.w) vx = shot.w - vw;
    if (vy + vh > shot.h) vy = shot.h - vh;
    if (vx < 0) vx = 0;
    if (vy < 0) vy = 0;
    s->vx = vx; s->vy = vy;
    canvas_rect(cv, L.gx, L.ty, L.cw, L.ah, ST_BG);
    st_crop(cv, &shot, vx, vy, vw, vh, L.gx, L.ty, L.z);
    for (int i = 0; i < a->npart; i++) {
        const Part *p = &a->part[i];
        if (!p->has_box) continue;
        int bx, by, bw, bh;
        st_box(p, parts_pose(a, i, t), &bx, &by, &bw, &bh);
        Rgb oc = i == s->pick ? C_YELLOW : RGB(95, 105, 135);
        double turn = parts_pose(a, i, t).turn;
        if (fabs(turn) < 0.5) frame(cv, L.gx + (bx - vx) * L.z, L.ty + (by - vy) * L.z, bw * L.z, bh * L.z, oc);
        else { /* turned: the outline turned about its middle, and a mark on its top edge */
            double cxp = L.gx + (bx - vx + bw / 2.0) * L.z, cyp = L.ty + (by - vy + bh / 2.0) * L.z, w2 = bw * L.z / 2.0 + 1, h2 = bh * L.z / 2.0 + 1, ca = cos(turn * M_PI / 180), sa = sin(turn * M_PI / 180);
            double cx4[4] = {-w2, w2, w2, -w2}, cy4[4] = {-h2, -h2, h2, h2};
            int px4[4], py4[4];
            for (int k = 0; k < 4; k++) { px4[k] = (int)lround(cxp + cx4[k] * ca - cy4[k] * sa); py4[k] = (int)lround(cyp + cx4[k] * sa + cy4[k] * ca); }
            for (int k = 0; k < 4; k++) canvas_line(cv, px4[k], py4[k], px4[(k + 1) % 4], py4[(k + 1) % 4], oc);
            canvas_line(cv, (px4[0] + px4[1]) / 2, (py4[0] + py4[1]) / 2, (int)lround(cxp), (int)lround(cyp), oc); /* (top middle to the middle: which way is up) */
        }
    }
    if (s->pick < a->npart && a->part[s->pick].has_box) { /* the path of the picked part: where it is at each frame, dotted */
        const Part *p = &a->part[s->pick];
        for (int f = 0; f < nf; f++) {
            PartPose q = parts_pose(a, s->pick, st_t(f));
            canvas_px(cv, L.gx + (p->box[0] + p->box[2] / 2 + (int)lround(q.x) - vx) * L.z + L.z / 2, L.ty + (p->box[1] + (int)lround(q.y) - vy) * L.z + L.z / 2, f == cur ? C_WHITE : RGB(150, 130, 60));
        }
    }
    frame(cv, L.gx, L.ty, L.cw, L.ah, RGB(90, 90, 110));
    region(R_SSTAGE, 0, L.gx, L.ty, L.cw, L.ah, RGB(0, 0, 0));
    if (s->pick < a->npart) {
        PartPose q = parts_pose(a, s->pick, t);
        snprintf(buf, sizeof buf, "PART %s  X %d  Y %d", a->part[s->pick].name, a->part[s->pick].box[0] + (int)lround(q.x), a->part[s->pick].box[1] + (int)lround(q.y));
        for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        canvas_text(cv, L.gx, L.ty + L.ah + 4, buf, C_GREY);
    }
    /* the panel: the tools, the picked part, its values at this frame, the keys' buttons, the parts */
    int x = L.px, y = L.ty, fw = L.fw, pbot = L.ty + L.ah;
    static const char *TOOL[4] = {"SELECT", "MOVE", "TURN", "KEY"};
    int roww = 0;
    for (int i = 0; i < 4; i++) roww += text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13;
    for (int i = 0, used = 0; i < 4; i++) {
        int cw = text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13, tx = x + used + (fw - roww) * i / 3;
        used += cw;
        stage_tool(cv, tx + (cw - 13) / 2, y, i, s->tool == i);
        canvas_text(cv, tx + (cw - text_width(TOOL[i])) / 2, y + 16, TOOL[i], s->tool == i ? C_WHITE : C_GREY);
        region(R_STOOL, i, tx, y, cw, 21, RGB(0, 0, 0));
    }
    y += 27;
    const Part *pp = s->pick < a->npart ? &a->part[s->pick] : NULL;
    PartPose q = pp ? parts_pose(a, s->pick, t) : (PartPose){0, 0, 0, 0, 1};
    int kat = pp ? st_key_at(pp, st_t(cur)) : -1;
    if (pp) {
        if (y + 20 <= pbot) { canvas_text(cv, x, y, "PART", RGB(84, 84, 104)); canvas_rect(cv, x, y + 7, fw, 1, RGB(40, 40, 52)); }
        y += 12;
        canvas_rect(cv, x, y, 52, 7, RGB(34, 34, 44));
        snprintf(buf, sizeof buf, "%s", pp->name); for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        canvas_text(cv, x + 2, y + 1, buf, C_WHITE);
        snprintf(buf, sizeof buf, "%s", pp->group); for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        canvas_text(cv, x + 56, y + 1, buf, C_GREY);
        y += 11;
        if (y + 20 <= pbot) { canvas_text(cv, x, y, kat >= 0 ? "AT THIS FRAME  (A KEY)" : "AT THIS FRAME", RGB(84, 84, 104)); canvas_rect(cv, x, y + 7, fw, 1, RGB(40, 40, 52)); }
        y += 12;
        double vals[ST_ROWS] = {q.x, q.y, q.turn, q.show, q.glow};
        for (int i = 0; i < ST_ROWS && y + 8 <= pbot; i++, y += 9) {
            int on = i == s->row;
            canvas_text(cv, x, y + 1, ST_ROW[i], on ? C_WHITE : C_GREY);
            char v[16];
            if (i == 3) snprintf(v, sizeof v, "%s", q.show ? "YES" : "NO"); else snprintf(v, sizeof v, "%d", (int)lround(vals[i]));
            int vw2 = fw >= 96 ? 26 : 22, vx = x + fw - vw2 - 8;
            st_value_box(cv, vx, y, vw2, v, on);
            region(R_SVAL, i * 3, vx, y, vw2, 7, RGB(0, 0, 0));
            canvas_text(cv, vx - 6, y + 1, "<", on ? C_YELLOW : RGB(84, 84, 104)); region(R_SVAL, i * 3 + 1, vx - 7, y, 7, 7, RGB(0, 0, 0));
            canvas_text(cv, vx + vw2 + 2, y + 1, ">", on ? C_YELLOW : RGB(84, 84, 104)); region(R_SVAL, i * 3 + 2, vx + vw2 + 1, y, 7, 7, RGB(0, 0, 0));
        }
        y += 3;
        if (y + 9 <= pbot) {
            int bx = x;
            button(cv, bx, y, "+ KEY", 0, R_SKEY, 0); bx += text_width("+ KEY") + 8;
            button(cv, bx, y, "- KEY", 0, R_SKEYDEL, 0); bx += text_width("- KEY") + 8;
            snprintf(buf, sizeof buf, "%s", kat >= 0 ? ST_EASE[pp->key[kat].ease] : "EASE");
            button(cv, bx, y, buf, 0, R_SEASE, 0);
        }
        y += 13;
    }
    if (y + 20 <= pbot) { canvas_text(cv, x, y, "PARTS", RGB(84, 84, 104)); canvas_rect(cv, x, y + 7, fw, 1, RGB(40, 40, 52)); }
    y += 12;
    for (int i = 0, bx = x; i < a->npart && y + 9 <= pbot; i++) {
        snprintf(buf, sizeof buf, "%s", a->part[i].name); for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        int bw = text_width(buf) + 6;
        if (bx + bw > x + fw) { bx = x; y += 11; if (y + 9 > pbot) break; }
        button(cv, bx, y, buf, i == s->pick, R_SPART, i);
        bx += bw + 3;
    }
    /* the timeline: its line, the thing as in the room at its left, the frames as pictures with the picked part's keys under them */
    int ty0 = L.y0, ly = ty0 + 3;
    canvas_rect(cv, 0, ty0, cv->w, 1, RGB(44, 44, 56));
    const char *sh = cv->w >= 220 ? "? SHORTCUTS  Q QUIT" : "? HELP  Q QUIT";
    int lw = text_width(sh) + 16, sx0 = lw, sx1 = cv->w - 4;
    canvas_rect(cv, sx0 - 8, ty0 + 1, 1, L.th - 1, RGB(40, 40, 52));
    canvas_text(cv, 4, ly + 2, sh, RGB(150, 150, 170));
    snprintf(buf, sizeof buf, "[ %s ]  FRAME %d OF %d   %.1f S", a->name, cur + 1, nf, a->length);
    for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, sx0, ly + 2, buf, C_YELLOW);
    const char *spd = s->speed < 0.75 ? "0.5X" : s->speed > 1.5 ? "2X" : "1X", *play = s->playing ? "STOP" : "PLAY";
    int bw = text_width(spd) + 9 + text_width(play) + 6, bx = sx1 - bw;
    button(cv, bx, ly, spd, 0, R_SPEED, 0); bx += text_width(spd) + 9;
    button(cv, bx, ly, play, s->playing, R_PLAY, 0);
    if (L.th > 15) {
        int ty = ly + 12, ah = L.th - 15, aw = lw - 16;
        st_crop(cv, &shot, vx, vy, aw, ah > 40 ? 40 : ah, 4, ty, 1); /* the thing as in the room, 1x */
        frame(cv, 4, ty, aw, ah > 40 ? 40 : ah, RGB(70, 70, 92));
        int cap = (sx1 - sx0 + 6) / (ST_TW + 6);
        if (cap < 1) cap = 1;
        if (cur < s->first) s->first = cur; /* (the strip keeps the open frame in view) */
        if (cur >= s->first + cap) s->first = cur - cap + 1;
        if (s->first < 0) s->first = 0;
        int tx = sx0, shown = 0;
        region(R_STRIP, 0, sx0, ty - 1, sx1 - sx0, ST_TH + 12, RGB(0, 0, 0));
        if (s->thumb_pick != s->pick) { memset(s->thumb_ok, 0, sizeof s->thumb_ok); s->thumb_pick = s->pick; }
        for (int f = s->first; f < nf && tx + ST_TW <= sx1; f++, tx += ST_TW + 6, shown++) {
            if (!s->thumb_ok[f]) { /* (a picture of the frame around the picked part, drawn the first time it is seen) */
                if (s->thumb[f].p) canvas_free(&s->thumb[f]);
                Canvas full = st_shot(a, st_t(f));
                int cx = vx + 4, cy = vy + 6;
                if (pp && pp->has_box) { cx = pp->box[0] + pp->box[2] / 2 - ST_TW / 2; cy = pp->box[1] + pp->box[3] / 2 - ST_TH / 2; }
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
            canvas_text(cv, tx + (ST_TW - text_width(nb)) / 2, ty + ST_TH + 2, nb, on ? C_YELLOW : C_GREY);
            if (pp && st_key_at(pp, st_t(f)) >= 0) st_diamond(cv, tx + ST_TW / 2, ty + ST_TH + 11, C_YELLOW);
        }
        if (shown < nf) { /* a scroll bar: where the frames shown are among all of them */
            int by = ty + ST_TH + 16, tw = sx1 - sx0, k0 = sx0 + tw * s->first / nf, k1 = sx0 + tw * (s->first + shown) / nf;
            canvas_rect(cv, sx0, by, tw, 2, RGB(40, 40, 52));
            canvas_rect(cv, k0, by, k1 - k0 > 4 ? k1 - k0 : 4, 2, RGB(140, 140, 160));
        }
    }
    if (s->asking) { /* q with changes: keep or discard */
        static const char *const Q[] = {"KEEP YOUR CHANGES?", "", "S  KEEP THEM", "D  DISCARD: THE FILE GOES BACK", "    TO HOW IT WAS", "ESC  KEEP EDITING"};
        int n = (int)(sizeof Q / sizeof *Q), bw2 = 0;
        for (int i = 0; i < n; i++) if (text_width(Q[i]) > bw2) bw2 = text_width(Q[i]);
        int hh = n * 7 + 8, hx = L.gx + (L.cw - bw2 - 12) / 2, hy = L.ty + (L.ah - hh) / 2;
        if (hy < L.ty) hy = L.ty;
        canvas_rect(cv, hx, hy, bw2 + 12, hh, RGB(26, 26, 36)); frame(cv, hx, hy, bw2 + 12, hh, C_YELLOW);
        for (int i = 0; i < n; i++) canvas_text(cv, hx + 6, hy + 4 + i * 7, Q[i], i == 0 ? C_YELLOW : C_WHITE);
    }
    if (s->help) { /* ?: the keys and the mouse, in a box over the stage */
        static const char *const H[] = {"MOUSE", "CLICK A PART: PICK IT", "MOVE / TURN: DRAG ON THE STAGE", "KEY: CLICK TO ADD A KEY", "< > BESIDE A VALUE: CHANGE IT", "CLICK A FRAME: GO THERE",
                                        "KEYS", "LEFT RIGHT  FRAME", "UP DOWN  A VALUE", "+ -  CHANGE IT", "SPACE  PLAY", "[ ]  OTHER PART", "K  ADD A KEY", "X  DELETE THE KEY", "E  KEY EASE", "U R  UNDO REDO", "Q  QUIT"};
        int n = (int)(sizeof H / sizeof *H), bw2 = 0;
        for (int i = 0; i < n; i++) if (text_width(H[i]) > bw2) bw2 = text_width(H[i]);
        int hh = n * 7 + 8, hx = L.gx + (L.cw - bw2 - 12) / 2, hy = L.ty + (L.ah - hh) / 2;
        if (hy < L.ty) hy = L.ty;
        canvas_rect(cv, hx, hy, bw2 + 12, hh, RGB(26, 26, 36)); frame(cv, hx, hy, bw2 + 12, hh, RGB(110, 110, 140));
        for (int i = 0; i < n; i++) canvas_text(cv, hx + 6, hy + 4 + i * 7, H[i], i == 0 || i == 6 ? C_YELLOW : C_WHITE);
    }
    canvas_free(&shot);
}

/* ---- the window --------------------------------------------------------------------------------------------------------- */
static int stage_open(Stage *s, const char *name) {
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    s->speed = 1; s->row = 1; s->thumb_pick = -1;
    char path[700];
    snprintf(path, sizeof path, "%s/%s.txt", parts_mine_dir(), name);
    s->orig = parts_mine_dir()[0] ? slurp(path) : NULL;
    if (!parts_mine_dir()[0] || access(path, R_OK)) snprintf(path, sizeof path, "%s/%s.txt", parts_default_dir(), name);
    if (parts_load(&s->a, path)) return 1;
    if (!s->a.npart) { fprintf(stderr, "%s: no parts\n", path); return 1; }
    if (!s->a.nscene) { fprintf(stderr, "%s: no scene lines: the editor's stage has nothing to show\n", path); return 1; }
    return 0;
}
static void st_pick_at(Stage *s, double t, int mx, int my, const StLayout *L) { /* the part under the mouse, the smallest first */
    const PartAnim *a = &s->a;
    int best = -1, barea = 1 << 30;
    for (int i = 0; i < a->npart; i++) {
        const Part *p = &a->part[i];
        if (!p->has_box) continue;
        int bx, by, bw, bh;
        st_box(p, parts_pose(a, i, t), &bx, &by, &bw, &bh);
        int X = (mx - L->gx) / L->z + s->vx, Y = (my - L->ty) / L->z + s->vy;
        if (X >= bx - 1 && X <= bx + bw && Y >= by - 1 && Y <= by + bh && bw * bh < barea) { best = i; barea = bw * bh; }
    }
    if (best >= 0) s->pick = best;
}
static int stage_run(const char *dir, const char *name) {
    static Stage s;
    if (!parts_default_dir()[0]) { char rd[700]; snprintf(rd, sizeof rd, "%s", dir); char *sl = strrchr(rd, '/'); strcpy(sl ? sl : rd, "/anims"); parts_set_dir(rd); }
    if (!parts_mine_dir()[0]) { char md[700]; snprintf(md, sizeof md, "%s", getenv("CLAWD_MY_ANIMS") ? getenv("CLAWD_MY_ANIMS") : sprites_mine_dir()); if (!getenv("CLAWD_MY_ANIMS")) { char *sl = strrchr(md, '/'); if (sl) strcpy(sl, "/anims"); } parts_set_mine_dir(md); }
    if (stage_open(&s, name)) return 1;
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) { fprintf(stderr, "clawd-room --edit needs a terminal\n"); return 1; }
    tty_on();
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
        if (select(STDIN_FILENO + 1, &fs, NULL, NULL, &tv) <= 0) { if (nin == 1 && in[0] == 0x1b) { nin = 0; s.asking = 0; } continue; }
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
                    if (b & 64) { if (rg && (rg->kind == R_STRIP || rg->kind == R_FRAME)) s.first += (b & 3) == 0 || (b & 3) == 2 ? -1 : 1; if (s.first < 0) s.first = 0; if (s.first > nf - 1) s.first = nf - 1; continue; }
                    if (s.drag && (motion || release)) { /* dragging the picked part: MOVE moves it, TURN turns it */
                        if (release) { s.drag = 0; st_save(&s); continue; }
                        int k2 = st_ensure_key(&s, s.pick, st_t(s.frame));
                        if (k2 >= 0) {
                            PartPose *q = &s.a.part[s.pick].key[k2].pose;
                            if (s.tool == 1) { q->x = s.dpose.x + (px - s.px0) / L.z; q->y = s.dpose.y + (py - s.py0) / L.z; }
                            else q->turn = s.dpose.turn + (px - s.px0) * 2;
                            st_changed(&s);
                        }
                        continue;
                    }
                    if (release || motion || btn != 0 || !rg) continue;
                    s.msg[0] = 0;
                    if (rg->kind == R_SSTAGE) {
                        s.playing = 0;
                        st_pick_at(&s, st_t(s.frame), px, py, &L); /* (the part pressed on is the one the tool works on) */
                        if (s.tool == 0) {}
                        else if (s.tool == 3) st_add_key(&s);
                        else { /* MOVE / TURN: a drag on the stage changes the picked part at this frame */
                            st_push(&s);
                            int k2 = st_ensure_key(&s, s.pick, st_t(s.frame));
                            if (k2 >= 0) { s.drag = 1; s.px0 = px; s.py0 = py; s.dpose = s.a.part[s.pick].key[k2].pose; }
                        }
                    }
                    else if (rg->kind == R_STOOL) s.tool = rg->i;
                    else if (rg->kind == R_SPART) s.pick = rg->i;
                    else if (rg->kind == R_SVAL) { s.row = rg->i / 3; if (rg->i % 3) { s.playing = 0; st_change(&s, s.row, rg->i % 3 == 2 ? 1 : -1); } }
                    else if (rg->kind == R_SKEY) { s.playing = 0; st_add_key(&s); }
                    else if (rg->kind == R_SKEYDEL) { s.playing = 0; st_del_key(&s); }
                    else if (rg->kind == R_SEASE) { s.playing = 0; st_ease(&s); }
                    else if (rg->kind == R_FRAME) { s.frame = rg->i; s.playing = 0; }
                    else if (rg->kind == R_PLAY) { s.playing = !s.playing; s.play_t0 = wall_now() - st_t(s.frame) / s.speed; }
                    else if (rg->kind == R_SPEED) s.speed = s.speed < 0.75 ? 1 : s.speed < 1.5 ? 2 : 0.5;
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] >= 'A' && in[i + 2] <= 'D') { /* arrows: left / right the frame, up / down the value */
                    char ar = in[i + 2];
                    i += 3;
                    s.playing = 0;
                    if (ar == 'C' || ar == 'D') { s.frame = (s.frame + (ar == 'C' ? 1 : nf - 1)) % nf; if (s.frame < s.first) s.first = s.frame; }
                    else s.row = (s.row + (ar == 'B' ? 1 : ST_ROWS - 1)) % ST_ROWS;
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin) { i += 3; continue; }
                s.asking = 0; /* (a lone Esc: back to editing) */
                i += 1; continue;
            }
            char ch = in[i++];
            s.msg[0] = 0;
            if (ch == '?') s.help = !s.help;
            else if (s.asking) { if (ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y') quit = 1; else if (ch == 'd' || ch == 'D' || ch == 'n' || ch == 'N') { st_discard(&s); quit = 1; } else s.asking = 0; }
            else if (ch == 0x03 || ch == 'q' || ch == 'Q') { if (s.changed) s.asking = 1; else quit = 1; }
            else if (ch == ' ') { s.playing = !s.playing; s.play_t0 = wall_now() - st_t(s.frame) / s.speed; }
            else if (ch == '.' || ch == '>') { s.playing = 0; s.frame = (s.frame + 1) % nf; }
            else if (ch == ',' || ch == '<') { s.playing = 0; s.frame = (s.frame + nf - 1) % nf; }
            else if (ch == '+' || ch == '=') st_change(&s, s.row, 1);
            else if (ch == '-') st_change(&s, s.row, -1);
            else if (ch == ']') s.pick = (s.pick + 1) % s.a.npart;
            else if (ch == '[') s.pick = (s.pick + s.a.npart - 1) % s.a.npart;
            else if (ch == 'k' || ch == 'K') st_add_key(&s);
            else if (ch == 'x' || ch == 'X') st_del_key(&s);
            else if (ch == 'e' || ch == 'E') st_ease(&s);
            else if (ch == 0x1A || ch == 'u') { if (s.nundo) { s.nundo--; st_swap(&s, s.nundo); s.nredo++; st_save(&s); } }
            else if (ch == 0x19 || ch == 'r') { if (s.nredo) { st_swap(&s, s.nundo); s.nundo++; s.nredo--; st_save(&s); } }
        }
        memmove(in, in + i, (size_t)(nin - i)); nin -= i;
        (void)t;
    }
    tty_off();
    cells_free(&prev);
    for (int f = 0; f < ST_MAXF; f++) if (s.thumb[f].p) canvas_free(&s.thumb[f]);
    free(s.orig);
    return 0;
}
