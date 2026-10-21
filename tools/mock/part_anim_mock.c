/* Mockup (tools/mock, build.sh): part animations in the pixel editor, drawn on top of the editor's own picture. It starts from
 * the real editor (editor.c is included: its title, canvas frame, tools, colour panel and timeline are drawn by draw()), then
 * puts the stage page over it: the canvas shows a thing of the room (a Clawd on the swing, the barista's espresso machine) with
 * its parts outlined, the panel keeps the tool key caps and the sections but holds the picked part and its values at this frame,
 * the timeline keeps its line (name, ONION, speed, PLAY), the real-size view at its left, and shows keyframes:
 *   A: a dope sheet (a row per part, keyframes as diamonds);  B: the frames as pictures, keyframes as diamonds under them.
 *   part_anim_mock OUT_DIR SPRITES_DIR   (writes part-anim-swing.png and part-anim-barista.png; each: the editor as it is
 *   now, then A, then B) */
#include "editor.c"
#include "art.h"
#include "fixture.h"
#include "theme.h"

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
static Ed E;

typedef struct { int x, y, w, h; } PBox;
typedef struct {
    const char *name, *file, *fixture;                  /* the title, the file the animation is saved in, the scene */
    int cols, rows, steps;                              /* the terminal of the scene, steps of 1/16 s run before the picture */
    int cx, cy;                                         /* the stage: the room pixel at the canvas's top left */
    const PBox *box; const char *const *pname; int nparts, picked; /* the parts' boxes (room pixels) and names; the picked one */
    int dots_dy, ndots;                                 /* the picked part's path, dotted up (-1) or down (+1) from its box */
    const char *group; const char *V[5];                /* the picked part's group and its values at this frame */
    const char *rows_[6]; const double keys[6][6];      /* the dope sheet: a row per part, keyframes 0..1 along the length (-1 ends) */
    int nkeys_frames, key_at[8];                        /* B: how many frame pictures, which of them are keyframes of the picked part */
    const char *len; int frames, at;                    /* the length, the number of frames, the open one */
} Case;

static Canvas shot(const Case *cs, int extra) { /* the scene after steps (+ extra) of 1/16 s, seated at its places */
    FILE *f = fopen("/tmp/clawd-mock-part-anim.fx", "w");
    fputs(cs->fixture, f);
    fclose(f);
    static Fixture fx;
    fixture_load(&fx, "/tmp/clawd-mock-part-anim.fx");
    unlink("/tmp/clawd-mock-part-anim.fx");
    for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; }
    fx.room.freeze = 0;
    char note[64];
    Canvas c = {0};
    for (int k = 0; k < cs->steps + extra; k++) { if (c.p) canvas_free(&c); c = render(fx.cols, fx.rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note); }
    return c;
}
static void blit_crop(Canvas *cv, const Canvas *st, int sx, int sy, int w, int h, int dx, int dy, int z) { /* a piece of the stage, z times */
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int X = sx + x, Y = sy + y;
        Rgb k = X >= 0 && X < st->w && Y >= 0 && Y < st->h ? st->p[Y * st->w + X] : RGB(18, 18, 24);
        canvas_rect(cv, dx + x * z, dy + y * z, z, z, k);
    }
}
static void diamond(Canvas *c, int x, int y, Rgb k) { canvas_px(c, x, y - 2, k); canvas_rect(c, x - 1, y - 1, 3, 1, k); canvas_rect(c, x - 2, y, 5, 1, k); canvas_rect(c, x - 1, y + 1, 3, 1, k); canvas_px(c, x, y + 2, k); }

/* the stage tools as key caps in the editor's own style (tool_button's shape), their icons 9 x 9 */
static const char *const STAGE_ICON[4][9] = {
    {"k........", "kwk......", "kwwk.....", "kwwwk....", "kwwwwk...", "kwwkkk...", "kwkwk....", "kk.kwk...", "....kk..."}, /* SELECT: a pointer */
    {"....w....", "...www...", "....w....", ".w..w..w.", "wwwwwwwww", ".w..w..w.", "....w....", "...www...", "....w...."}, /* MOVE: four arrows */
    {"..bbbbb..", ".b.....b.", "b.......b", "b.......b", "b......bbb", ".b......b.", "..bbbbb.b.", ".......b..", "........."},  /* TURN: an arrow round */
    {"....y....", "...yyy...", "..yyyyy..", ".yyyyyyy.", "yyyyyyyyy", ".yyyyyyy.", "..yyyyy..", "...yyy...", "....y...."},  /* KEY: a diamond */
};
static void stage_tool(Canvas *cv, int x, int y, int which, int on) {
    int w = 13, h = 13, dy = on ? 1 : 0;
    canvas_rect(cv, x + 1, y + 1, w - 2, h, RGB(14, 14, 20)); canvas_rect(cv, x, y + 2, w, h - 2, RGB(14, 14, 20));
    Rgb cap = on ? RGB(70, 72, 100) : RGB(52, 54, 70);
    canvas_rect(cv, x + 1, y + dy, w - 2, h - dy, cap); canvas_rect(cv, x, y + dy + 1, w, h - dy - 2, cap);
    canvas_rect(cv, x + 2, y + dy, w - 4, 1, on ? RGB(110, 112, 145) : RGB(86, 88, 110));
    for (int yy = 0; yy < 9; yy++) for (int xx = 0; xx < 9 && STAGE_ICON[which][yy][xx]; xx++)
        if (STAGE_ICON[which][yy][xx] != '.') canvas_px(cv, x + 2 + xx, y + 2 + dy + yy, tool_colour(STAGE_ICON[which][yy][xx]));
    if (on) canvas_rect(cv, x + 2, y + h + 1, w - 4, 1, C_YELLOW);
}
static void value_box(Canvas *cv, int x, int y, int w, const char *v, int on) { /* like the hex code's box */
    canvas_rect(cv, x, y, w, 7, on ? RGB(60, 60, 80) : RGB(34, 34, 44));
    if (on) frame(cv, x, y, w, 7, C_YELLOW);
    canvas_text(cv, x + w - 2 - text_width(v), y + 1, v, on ? C_YELLOW : C_WHITE);
}

static void panel(Canvas *cv, const Case *cs, const Layout *L) { /* where the colour panel is: the same tools row and sections */
    int x = L->px, y = L->ty, pbot = L->ty + L->ah;
    canvas_rect(cv, x - 2, L->ty - 2, cv->w - x, pbot - L->ty + 4, RGB(18, 18, 24));
    static const char *TOOL[4] = {"SELECT", "MOVE", "TURN", "KEY"};
    int roww = 0;
    for (int i = 0; i < 4; i++) roww += text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13;
    for (int i = 0, used = 0; i < 4; i++) {
        int cw = text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13, tx = x + used + (FIELD_W - roww) * i / 3;
        used += cw;
        stage_tool(cv, tx + (cw - 13) / 2, y, i, i == 0);
        canvas_text(cv, tx + (cw - text_width(TOOL[i])) / 2, y + 16, TOOL[i], i == 0 ? C_WHITE : C_GREY);
    }
    y += TOOL_ROW + 4;
    if (y + SECTION + 8 <= pbot) section(cv, x, y, "PART");
    y += SECTION;
    canvas_rect(cv, x, y, 9, 7, RGB(180, 60, 50)); frame(cv, x, y, 9, 7, RGB(200, 200, 215)); /* (the part's colour, like the colour's swatch) */
    int nw = text_width("CHAIN R") + 4;
    canvas_rect(cv, x + 12, y, nw, 7, RGB(34, 34, 44)); canvas_text(cv, x + 14, y + 1, cs->pname[cs->picked], C_WHITE);
    canvas_text(cv, x + 12 + nw + 4, y + 1, cs->group, C_GREY);
    y += 11;
    if (y + SECTION + 8 <= pbot) section(cv, x, y, "AT THIS FRAME");
    y += SECTION;
    static const char *K[5] = {"MOVE X", "MOVE Y", "TURN", "SHOW", "GLOW"};
    for (int i = 0; i < 5 && y + 8 <= pbot; i++, y += 9) {
        canvas_text(cv, x, y + 1, K[i], C_GREY);
        int vw = 26, vx = x + FIELD_W - vw - 8;
        if (i == 1) { canvas_text(cv, vx - 6, y + 1, "<", C_YELLOW); canvas_text(cv, vx + vw + 2, y + 1, ">", C_YELLOW); }
        value_box(cv, vx, y, vw, cs->V[i], i == 1);
    }
    y += 3;
    if (y + 9 <= pbot) { int bx = x; button(cv, bx, y, "+ KEY", 0, 0, 0); bx += text_width("+ KEY") + 8; button(cv, bx, y, "- KEY", 0, 0, 0); bx += text_width("- KEY") + 8; button(cv, bx, y, "EASE", 0, 0, 0); }
    y += 13;
    if (y + SECTION + 8 <= pbot) section(cv, x, y, "PARTS");
    y += SECTION;
    int bx = x;
    for (int i = 0; i < cs->nparts && y + 9 <= pbot; i++) {
        int bw = text_width(cs->pname[i]) + 6;
        if (bx + bw > x + FIELD_W) { bx = x; y += 11; if (y + 9 > pbot) break; }
        button(cv, bx, y, cs->pname[i], i == cs->picked, 0, 0);
        bx += bw + 3;
    }
}
static void ruler(Canvas *c, int x, int y, int w, int frames, int at) {
    canvas_rect(c, x, y + 6, w, 1, RGB(60, 60, 76));
    for (int f = 0; f <= frames; f++) {
        int fx = x + f * (w - 1) / frames;
        canvas_rect(c, fx, y + (f % 4 == 0 ? 3 : 5), 1, f % 4 == 0 ? 3 : 1, RGB(90, 90, 110));
        if (f % 8 == 0) { char t[8]; snprintf(t, sizeof t, "%.1f", f / 16.0); canvas_text(c, fx - text_width(t) / 2, y - 2, t, RGB(110, 110, 130)); }
    }
    canvas_rect(c, x + at * (w - 1) / frames, y + 2, 1, 6, C_YELLOW);
}
static void timeline(Canvas *cv, const Case *cs, const Canvas *st, int variant) { /* over the editor's own timeline: its line, then keyframes */
    int th = timeline_h(&E, cv->h), y0 = cv->h - th, y = y0 + 3;
    canvas_rect(cv, 0, y0 + 1, cv->w, th, RGB(18, 18, 24));
    const char *sh = cv->w >= 220 ? "? SHORTCUTS  Q QUIT" : "? HELP  Q QUIT";
    int lw = text_width(sh) + 16, sx0 = lw, sx1 = cv->w - 4;
    canvas_rect(cv, sx0 - 8, y0 + 1, 1, th - 1, RGB(40, 40, 52));
    canvas_text(cv, 4, y + 2, sh, RGB(150, 150, 170));
    char t[96];
    snprintf(t, sizeof t, "[ %s ]  FRAME %d OF %d   %s", cs->name, cs->at, cs->frames, cs->len);
    canvas_text(cv, sx0, y + 2, t, C_YELLOW);
    int bw = text_width("ONION") + 9 + text_width("1X") + 9 + text_width("PLAY") + 6, bx = sx1 - bw;
    button(cv, bx, y, "ONION", 1, R_ONION, 0); bx += text_width("ONION") + 9;
    button(cv, bx, y, "1X", 0, R_SPEED, 0); bx += text_width("1X") + 9;
    button(cv, bx, y, "PLAY", 0, R_PLAY, 0);
    int ty = y + 12, bottom = cv->h - 2;
    { /* the thing as in the room, at its left (like the sprite there) */
        int aw = lw - 16, ah = bottom - ty + 12;
        blit_crop(cv, st, cs->cx + 2, cs->cy + 2, aw, ah > 40 ? 40 : ah, 4, ty - 12 + 12, 1);
        frame(cv, 4, ty, aw, ah > 40 ? 40 : ah, RGB(70, 70, 92));
    }
    if (variant == 0) { /* A: a dope sheet */
        int lab = 44, tx = sx0 + lab, tw = sx1 - tx;
        ruler(cv, tx, ty, tw, cs->frames, cs->at);
        int ry = ty + 11;
        for (int r = 0; r < cs->nparts && ry + 7 <= bottom + 1; r++, ry += 8) {
            int on = r == cs->picked;
            if (on) canvas_rect(cv, sx0 - 4, ry - 1, sx1 - sx0 + 4, 8, RGB(32, 32, 44));
            canvas_text(cv, sx0, ry, cs->rows_[r], on ? C_YELLOW : C_GREY);
            canvas_rect(cv, tx, ry + 3, tw, 1, RGB(40, 40, 52));
            double k0 = cs->keys[r][0];
            for (int k = 0; k < 6 && cs->keys[r][k] >= 0; k++) {
                int kx = tx + (int)lround(cs->keys[r][k] * (tw - 1));
                if (k > 0) canvas_rect(cv, tx + (int)lround(k0 * (tw - 1)), ry + 3, kx - (tx + (int)lround(k0 * (tw - 1))), 1, on ? RGB(150, 130, 60) : RGB(80, 80, 100));
                diamond(cv, kx, ry + 3, on ? C_YELLOW : RGB(200, 200, 215));
                k0 = cs->keys[r][k];
            }
        }
        canvas_rect(cv, tx + cs->at * (tw - 1) / cs->frames, ty + 10, 1, 8 * cs->nparts, C_YELLOW);
    } else { /* B: the frames as pictures, like the editor's, with a diamond under each keyframe of the picked part */
        int n = cs->nkeys_frames, fw = 44, fh = 26, gap = 6, x = sx0;
        for (int i = 0; i < n && x + fw <= sx1; i++, x += fw + gap) {
            Canvas f = shot(cs, i * 4);
            int open = i == 2;
            canvas_rect(cv, x - 1, ty - 1, fw + 2, fh + 2, open ? RGB(70, 70, 92) : RGB(34, 34, 44));
            blit_crop(cv, &f, cs->cx + 4, cs->cy + 6, fw, fh, x, ty, 1);
            if (open) frame(cv, x - 1, ty - 1, fw + 2, fh + 2, C_YELLOW);
            char nb[8]; snprintf(nb, sizeof nb, "%d", i * 4 + 1);
            canvas_text(cv, x + (fw - text_width(nb)) / 2, ty + fh + 2, nb, open ? C_YELLOW : C_GREY);
            for (int k = 0; k < 8; k++) if (cs->key_at[k] == i) diamond(cv, x + fw / 2, ty + fh + 11, C_YELLOW);
            canvas_free(&f);
        }
        int tw = sx1 - sx0; canvas_rect(cv, sx0, ty + fh + 17, tw, 2, RGB(40, 40, 52)); canvas_rect(cv, sx0, ty + fh + 17, tw * 3 / 5, 2, RGB(140, 140, 160)); /* (the scroll bar) */
    }
}
static Canvas page(const char *sprites, const Case *cs, int variant, int W, int H) {
    memset(&E, 0, sizeof E);
    E.pk = 1; E.pi = 0; E.hx = E.hy = -1; E.drag = -1; E.speed = 1; E.onion = 1; E.del_ask = -1;
    file_open(&E, sprites, 2, variant < 0 ? "walk-2" : "walk-1"); ui_watch(sprites);
    g_panel_folded = 0; g_tl_user = 0;
    if (variant >= 0) { E.w = 76; E.h = 50; E.zoom = 2; } /* (the stage page: a canvas as wide as 76 room pixels at 2x) */
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    draw(&E, &cv);
    if (variant < 0) return cv; /* (the editor as it is now) */
    Layout L = layout(&E, W, H);
    char t[120];
    snprintf(t, sizeof t, "EDIT %s  %s  %d PARTS", cs->name, cs->file, cs->nparts);
    canvas_rect(&cv, 0, 0, W - 18, 11, RGB(18, 18, 24));
    canvas_text(&cv, 4, 3, t, C_YELLOW);
    Canvas st = shot(cs, 0);
    int cw = E.w * L.z, chh = L.ah;
    canvas_rect(&cv, L.gx, L.ty, cw, chh, RGB(18, 18, 24));
    blit_crop(&cv, &st, cs->cx, cs->cy, cw / L.z, chh / L.z, L.gx, L.ty, L.z);
    for (int i = 0; i < cs->nparts; i++) { /* the parts outlined, the picked one yellow */
        const PBox *b = &cs->box[i];
        frame(&cv, L.gx + (b->x - cs->cx) * L.z, L.ty + (b->y - cs->cy) * L.z, b->w * L.z, b->h * L.z, i == cs->picked ? C_YELLOW : RGB(95, 105, 135));
    }
    { const PBox *b = &cs->box[cs->picked]; /* its path, dotted from its box */
      for (int s = 1; s <= cs->ndots; s++) canvas_px(&cv, L.gx + (b->x - cs->cx + b->w / 2) * L.z + L.z / 2, L.ty + (b->y - cs->cy + (cs->dots_dy < 0 ? 0 : b->h) + s * cs->dots_dy * 2) * L.z, C_YELLOW); }
    frame(&cv, L.gx, L.ty, cw, chh, RGB(90, 90, 110));
    canvas_rect(&cv, L.gx, L.ty + chh + 2, cw, 9, RGB(18, 18, 24));
    snprintf(t, sizeof t, "PART %s  X %d  Y %d", cs->pname[cs->picked], cs->box[cs->picked].x, cs->box[cs->picked].y);
    canvas_text(&cv, L.gx, L.ty + chh + 4, t, C_GREY);
    panel(&cv, cs, &L);
    timeline(&cv, cs, &st, variant);
    fprintf(stderr, "canvas %d x %d, panel to %d, timeline %d\n", cw, chh, L.ty + L.ah, timeline_h(&E, H));
    canvas_free(&st);
    return cv;
}
static void sheet(const char *path, const char *sprites, const Case *cs) { /* the editor now, then A, then B */
    int W = 300, H = 215;
    Canvas sh = canvas_new(3 * W + 12, H, RGB(60, 60, 76));
    for (int v = -1; v <= 1; v++) { Canvas c = page(sprites, cs, v, W, H); { const char *lab = v < 0 ? "THE EDITOR NOW" : v == 0 ? "A  DOPE SHEET" : "B  FRAMES"; canvas_rect(&c, W - 20 - text_width(lab) - 2, 2, text_width(lab) + 4, 8, RGB(18, 18, 24)); canvas_text(&c, W - 20 - text_width(lab), 3, lab, RGB(150, 150, 170)); } canvas_blit(&sh, &c, (v + 1) * (W + 6), 0); canvas_free(&c); }
    png_write(path, &sh, 2);
    fprintf(stderr, "wrote %s\n", path);
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "out/11-part-animations", *sprites = argc > 2 ? argv[2] : "sprites";
    char p[400];
    /* the park's swing: a Clawd on it swinging towards us and back (the room is 160 x 100 room pixels) */
    static const PBox swing_box[] = {{27, 60, 31, 3}, {36, 63, 2, 13}, {48, 63, 2, 13}, {35, 75, 16, 3}, {51, 61, 10, 17}};
    static const char *const swing_names[] = {"BAR", "CHAIN L", "CHAIN R", "SEAT", "CLAWD"};
    static const Case swing = {
        "SWING", "ANIMS/SWING.TXT", "theme park\ntitle swing\nterminal 160 50\ntime 1000.0 3.4\nscale 1\nsession s00 name=api created=900 state=idle tool=Bash px=10 py=39 emote=swing@990\n",
        160, 50, 200, 14, 46,
        swing_box, swing_names, 5, 3, 1, 3,
        "IN SWING", {"0", "2", "-8", "YES", "0"},
        {"BAR", "CHAIN L", "CHAIN R", "SEAT", "CLAWD"}, {{0, 1, -1}, {0, 0.5, 1, -1}, {0, 0.5, 1, -1}, {0, 0.25, 0.5, 0.75, 1, -1}, {0, 0.25, 0.5, 0.75, 1, -1}},
        5, {0, 1, 2, 3, 4, -1, -1, -1},
        "1.6 S", 26, 14};
    snprintf(p, sizeof p, "%s/part-anim-swing.png", dir); sheet(p, sprites, &swing);
    /* the coffee shop: the barista at the espresso machine, the cup rising, steam */
    static const PBox brew_box[] = {{42, 36, 10, 8}, {44, 40, 4, 3}, {44, 31, 6, 4}, {27, 32, 11, 11}};
    static const char *const brew_names[] = {"MACHINE", "CUP", "STEAM", "CLAWD"};
    static const Case brew = {
        "BARISTA", "ANIMS/BREW.TXT", "theme coffee\ntitle brew\nterminal 120 40\ntime 1000.0 3.4\nscale 1\nsession s00 name=api created=900 state=work tool=Bash hold=1000.4\nsession s01 name=web created=910 state=think tool=Edit hold=1000.4\n",
        120, 40, 40, 14, 22,
        brew_box, brew_names, 4, 1, -1, 4,
        "IN BREWING", {"0", "-5", "0", "YES", "20"},
        {"MACHINE", "CUP", "STEAM", "CLAWD"}, {{0, 0.4, 0.8, -1}, {0, 0.3, 0.6, 0.85, -1}, {0.25, 0.5, 0.75, 1, -1}, {0, 0.2, 0.55, 0.85, -1}},
        5, {0, 1, 3, 4, -1, -1, -1, -1},
        "1.7 S", 27, 9};
    snprintf(p, sizeof p, "%s/part-anim-barista.png", dir); sheet(p, sprites, &brew);
    return 0;
}
