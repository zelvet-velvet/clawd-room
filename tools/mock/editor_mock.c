/* Mockup (tools/mock, build.sh): the pixel editor as an animation editor: a timeline of the animation's frames with play,
 * onion skin (the frame before in red, the one after in blue, faint, under the grid) and a FILL tool (paint bucket: the
 * area it would fill outlined under the pointer). A over the editor's own drawing: the timeline along the bottom; B: a
 * filmstrip down the left. The editor's code is included so its own draw() makes the rest. */
#include "editor.c"
#include "art.h"

#define BAND 40 /* the timeline's height (A) */
static Ed E, OTHER;
static const char *SPRITES = "sprites";
static int frame_of(const char *name, char *base, int *k) { /* "walk-2" -> "walk", 2; "window~2" -> "window", 2; "window" -> 1 */
    const char *d = strrchr(name, '-'), *t = strrchr(name, '~');
    const char *s = t ? t : d;
    if (s && s[1] >= '0' && s[1] <= '9') { snprintf(base, 64, "%.*s", (int)(s - name), name); *k = atoi(s + 1); return 1; }
    snprintf(base, 64, "%s", name); *k = 1; return 0;
}
/* the frames of the animation the open sprite is in (sprites whose name is BASE-N), as sprite numbers */
static int anim_frames(const Ed *e, int *sp, int max) {
    char base[64], b2[64]; int k, n = 0;
    frame_of(e->name, base, &k);
    for (int i = 0; i < e->f.nsprite && n < max; i++) {
        Ed *o = &OTHER; o->f = e->f; sprite_open(o, i);
        if (frame_of(o->name, b2, &k) && !strcmp(b2, base)) sp[n++] = i;
    }
    return n;
}
static void thumb(Canvas *cv, const Ed *o, int x, int y, int z, int cur) {
    canvas_rect(cv, x - 1, y - 1, o->w * z + 2, o->h * z + 2, cur ? RGB(70, 70, 92) : RGB(34, 34, 44));
    checker(cv, x, y, o->w * z, o->h * z);
    for (int yy = 0; yy < o->h; yy++) for (int xx = 0; xx < o->w; xx++) { int tr; Rgb c = colour_of(&o->f, o->g[yy][xx], &tr); if (!tr) canvas_rect(cv, x + xx * z, y + yy * z, z, z, c); }
    if (cur) frame(cv, x - 1, y - 1, o->w * z + 2, o->h * z + 2, C_YELLOW);
}
/* onion skin on the grid: where this frame is see-through, the frame before (red) and after (blue) show faintly */
static void onion(Canvas *cv, const Layout *L, const Ed *e, const Ed *prev, const Ed *next) {
    for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) {
        int tr; colour_of(&e->f, e->g[y][x], &tr);
        if (!tr) continue;
        int tp = 1, tn = 1;
        if (prev && y < prev->h && x < prev->w) colour_of(&prev->f, prev->g[y][x], &tp);
        if (next && y < next->h && x < next->w) colour_of(&next->f, next->g[y][x], &tn);
        if (tp && tn) continue;
        Rgb k = !tp && !tn ? RGB(170, 120, 200) : !tp ? RGB(230, 90, 90) : RGB(90, 150, 240);
        for (int dy = 0; dy < L->z; dy++) for (int dx = 0; dx < L->z; dx++) {
            int X = L->gx + x * L->z + dx, Y = L->gy + y * L->z + dy;
            Rgb c = cv->p[Y * cv->w + X];
            cv->p[Y * cv->w + X] = RGB((c.r * 6 + k.r * 4) / 10, (c.g * 6 + k.g * 4) / 10, (c.b * 6 + k.b * 4) / 10);
        }
    }
}
/* FILL: the area the bucket would fill from (fx, fy): the same colour, touching side by side; outlined in dashes */
static void fill_preview(Canvas *cv, const Layout *L, const Ed *e, int fx, int fy) {
    static unsigned char m[MAXH][MAXW]; memset(m, 0, sizeof m);
    char ch = e->g[fy][fx]; int qx[MAXW * MAXH], qy[MAXW * MAXH], n = 0;
    qx[n] = fx; qy[n++] = fy; m[fy][fx] = 1;
    for (int i = 0; i < n; i++) for (int d = 0; d < 4; d++) {
        int x = qx[i] + (d == 0) - (d == 1), y = qy[i] + (d == 2) - (d == 3);
        if (x < 0 || y < 0 || x >= e->w || y >= e->h || m[y][x] || e->g[y][x] != ch) continue;
        m[y][x] = 1; qx[n] = x; qy[n++] = y;
    }
    for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) if (m[y][x]) {
        int X = L->gx + x * L->z, Y = L->gy + y * L->z, z = L->z;
        Rgb c = C_WHITE;
        for (int k = 0; k < z; k++) {
            int on = ((X + Y + k) / 2) % 2;
            if (!on) continue;
            if (y == 0 || !m[y - 1][x]) canvas_px(cv, X + k, Y, c);
            if (y == e->h - 1 || !m[y + 1][x]) canvas_px(cv, X + k, Y + z - 1, c);
            if (x == 0 || !m[y][x - 1]) canvas_px(cv, X, Y + k, c);
            if (x == e->w - 1 || !m[y][x + 1]) canvas_px(cv, X + z - 1, Y + k, c);
        }
    }
    /* the bucket by the pointer */
    int bx = L->gx + fx * L->z + L->z, by = L->gy + fy * L->z - 6;
    canvas_rect(cv, bx, by + 1, 5, 4, RGB(200, 200, 215)); canvas_rect(cv, bx + 1, by + 2, 3, 2, RGB(120, 120, 140));
    canvas_rect(cv, bx + 5, by + 3, 1, 3, C_YELLOW);
}
static void tools_row(Canvas *cv, const Layout *L) { /* PAINT ERASE PICK + FILL (FILL on) */
    int x = L->px, y = L->gy;
    canvas_rect(cv, x, y - 1, 120, 11, RGB(18, 18, 24));
    button(cv, x, y, "PAINT", 0, 0, 0);
    button(cv, x + 26, y, "ERASE", 0, 0, 0);
    button(cv, x + 52, y, "PICK", 0, 0, 0);
    button(cv, x + 74, y, "FILL", 1, 0, 0);
}
static void open_sp(Ed *o, int sp) { o->f = E.f; o->file = E.file; sprite_open(o, sp); }

static Canvas option_a(const char *name, int W, int H) {
    memset(&E, 0, sizeof E); file_open(&E, SPRITES, 2, name);
    E.pk = 1; E.pi = 0; E.hx = E.w / 2; E.hy = 2;
    Canvas ed = canvas_new(W, H - BAND, RGB(18, 18, 24));
    draw(&E, &ed);
    Layout L = layout(&E, W, H - BAND);
    int sp[16], n = anim_frames(&E, sp, 16), at = 0;
    for (int i = 0; i < n; i++) if (sp[i] == E.sp) at = i;
    static Ed P, N;
    if (n > 1) { open_sp(&P, sp[(at + n - 1) % n]); open_sp(&N, sp[(at + 1) % n]); onion(&ed, &L, &E, &P, &N); }
    tools_row(&ed, &L);
    fill_preview(&ed, &L, &E, E.w / 2, 2);
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    canvas_blit(&cv, &ed, 0, 0);
    /* the timeline: the frames, the one open framed; play, speed, onion, add / copy / delete */
    int y = H - BAND + 2;
    canvas_rect(&cv, 0, y - 2, W, 1, RGB(44, 44, 56));
    char base[64], t[96]; int k; frame_of(E.name, base, &k);
    snprintf(t, sizeof t, "%s  FRAME %d OF %d", base, at + 1, n);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(&cv, 4, y + 1, t, C_YELLOW);
    int bx = 4 + text_width(t) + 8;
    button(&cv, bx, y, "PLAY", 0, 0, 0); bx += text_width("PLAY") + 9;
    button(&cv, bx, y, "6 FPS", 0, 0, 0); bx += text_width("6 FPS") + 9;
    button(&cv, bx, y, "ONION", 1, 0, 0); bx += text_width("ONION") + 9;
    canvas_text(&cv, bx + 2, y + 2, "< > FRAME  F NEW  D COPY  X DEL", RGB(150, 150, 170));
    int tx = 4, ty = y + 12;
    for (int i = 0; i < n; i++) {
        static Ed O; open_sp(&O, sp[i]);
        thumb(&cv, &O, tx, ty, 2, i == at);
        char num[4]; snprintf(num, sizeof num, "%d", i + 1);
        canvas_text(&cv, tx + O.w - 1, ty + O.h * 2 + 2, num, i == at ? C_YELLOW : C_GREY);
        tx += O.w * 2 + 6;
    }
    canvas_rect(&cv, tx, ty, 14, 16, RGB(34, 34, 44)); canvas_text(&cv, tx + 5, ty + 6, "+", C_GREY);
    canvas_free(&ed);
    return cv;
}
static Canvas option_b(const char *name, int W, int H) { /* the filmstrip down the left, the editor to its right */
    int SW = 44;
    memset(&E, 0, sizeof E); file_open(&E, SPRITES, 2, name);
    E.pk = 1; E.pi = 0; E.hx = E.w / 2; E.hy = 2;
    Canvas ed = canvas_new(W - SW, H, RGB(18, 18, 24));
    draw(&E, &ed);
    Layout L = layout(&E, W - SW, H);
    int sp[16], n = anim_frames(&E, sp, 16), at = 0;
    for (int i = 0; i < n; i++) if (sp[i] == E.sp) at = i;
    static Ed P, N;
    if (n > 1) { open_sp(&P, sp[(at + n - 1) % n]); open_sp(&N, sp[(at + 1) % n]); onion(&ed, &L, &E, &P, &N); }
    tools_row(&ed, &L);
    fill_preview(&ed, &L, &E, E.w / 2, 2);
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    canvas_blit(&cv, &ed, SW, 0);
    canvas_rect(&cv, SW - 2, 0, 1, H, RGB(44, 44, 56));
    canvas_text(&cv, 4, 3, "FRAMES", RGB(110, 110, 130));
    int ty = 12;
    for (int i = 0; i < n; i++) {
        static Ed O; open_sp(&O, sp[i]);
        char num[4]; snprintf(num, sizeof num, "%d", i + 1);
        canvas_text(&cv, 4, ty + O.h, num, i == at ? C_YELLOW : C_GREY);
        thumb(&cv, &O, 10, ty, 2, i == at);
        ty += O.h * 2 + 5;
    }
    canvas_rect(&cv, 10, ty, 28, 10, RGB(34, 34, 44)); canvas_text(&cv, 22, ty + 3, "+", C_GREY);
    ty += 15;
    button(&cv, 4, ty, "PLAY", 0, 0, 0); ty += 12;
    button(&cv, 4, ty, "ONION", 1, 0, 0); ty += 12;
    canvas_text(&cv, 4, ty + 1, "6 FPS", C_GREY);
    canvas_free(&ed);
    return cv;
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "out/09-editor-animation";
    if (argc > 2) SPRITES = argv[2];
    int W = 300, H = 190;
    Canvas a = option_a("walk-2", W, H), b = option_b("walk-2", W, H);
    Canvas sheet = canvas_new(2 * W + 18, H + 18, RGB(24, 24, 30));
    canvas_text(&sheet, 6, 3, "A  TIMELINE ALONG THE BOTTOM", RGB(220, 220, 235));
    canvas_text(&sheet, W + 12, 3, "B  FILMSTRIP DOWN THE LEFT", RGB(220, 220, 235));
    canvas_blit(&sheet, &a, 6, 12); canvas_blit(&sheet, &b, W + 12, 12);
    char p[300]; snprintf(p, sizeof p, "%s/editor-animation.png", dir);
    png_write(p, &sheet, 3);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
