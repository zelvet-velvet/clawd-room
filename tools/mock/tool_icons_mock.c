/* Mockup (tools/mock, build.sh): the editor's PAINT / ERASE / PICK / FILL as pixel icons instead of words, three ways:
 * A icons only (the tool's name shows under the grid while the pointer is on it), B icons with their name under them,
 * C icons with the key that picks them. Drawn over the real editor (editor.c is included).
 *   tool_icons_mock OUT_DIR SPRITES_DIR */
#include "editor.c"
#include "art.h"

/* 9 x 9 icons: k dark outline, y pencil wood, w light, p pink, g grey, b blue (paint), r red, . nothing */
static const char *ICON[5][9] = {
    {"......kk.", ".....kppk", "....kyykk", "...kyyyk.", "..kyyyk..", ".kwyyk...", ".kwwk....", "kkkk.....", "k........"}, /* pencil */
    {".........", "....kkkk.", "...kwwwwk", "..kwwwwk.", ".kppppk..", "kppppk...", "kpppk....", ".kkk.....", "........."}, /* eraser */
    {"......rr.", ".....rrrr", "....grrr.", "...wgg...", "..wwg....", ".wbw.....", ".bb......", "b........", "........."}, /* eyedropper: red bulb, glass, a drop */
    {"..ggg....", ".g...g...", "g.....g..", "ggggggg..", "gwwwwwgb.", ".gwwwg.b.", ".gwwwg.bb", "..ggg..bb", "........."}, /* bucket: handle, pail, paint pouring */
    {"..ggg..y.", ".g...gyyy", "g.....gy.", "ggggggg..", "gwwwwwgb.", ".gwwwg.b.", ".gwwwg.bb", "..ggg..bb", "........."}, /* bucket, all: a sparkle */
};
static Rgb icol(char ch) {
    switch (ch) { case 'k': return RGB(70, 72, 88); case 'y': return RGB(240, 196, 90); case 'w': return RGB(236, 236, 244);
        case 'p': return RGB(240, 150, 170); case 'g': return RGB(170, 174, 190); case 'b': return RGB(110, 170, 250); case 'r': return RGB(230, 90, 80); }
    return RGB(0, 0, 0);
}
static void icon(Canvas *c, int x, int y, int which, int z) {
    for (int yy = 0; yy < 9; yy++) for (int xx = 0; xx < 9; xx++) if (ICON[which][yy][xx] != '.') canvas_rect(c, x + xx * z, y + yy * z, z, z, icol(ICON[which][yy][xx]));
}
/* the tile behind an icon: 0 flat (framed in yellow when on), 1 a rounded key cap with light on top and a shadow under
 * it (pressed in when on), 2 a soft tint of the tool's own colour (bright when on, a yellow bar under it), 3 a rounded
 * tile, on: a yellow ring with a soft glow */
static int g_bg;
static Rgb tint(int which) { static const Rgb T[5] = {{240, 196, 90}, {240, 150, 170}, {230, 90, 80}, {110, 170, 250}, {110, 170, 250}}; return T[which]; }
static void round_rect(Canvas *c, int x, int y, int w, int h, Rgb k) { canvas_rect(c, x + 1, y, w - 2, h, k); canvas_rect(c, x, y + 1, w, h - 2, k); }
static void tile(Canvas *c, int x, int y, int w, int h, int which, int on) {
    if (g_bg == 0) { canvas_rect(c, x, y, w, h, on ? RGB(90, 90, 120) : RGB(34, 34, 44)); if (on) frame(c, x, y, w, h, C_YELLOW); return; }
    if (g_bg == 1) {
        int dy = on ? 1 : 0;
        round_rect(c, x, y + 1, w, h, RGB(14, 14, 20));                                   /* (its shadow) */
        round_rect(c, x, y + dy, w, h - dy, on ? RGB(70, 72, 100) : RGB(52, 54, 70));
        canvas_rect(c, x + 2, y + dy, w - 4, 1, on ? RGB(110, 112, 145) : RGB(86, 88, 110)); /* (light on its top edge) */
        if (on) { canvas_rect(c, x + 2, y + h, w - 4, 1, C_YELLOW); }
        return;
    }
    if (g_bg == 2) {
        Rgb t = tint(which), k = on ? RGB((t.r + 2 * 40) / 3, (t.g + 2 * 40) / 3, (t.b + 2 * 52) / 3) : RGB((t.r + 6 * 30) / 7, (t.g + 6 * 30) / 7, (t.b + 6 * 40) / 7);
        round_rect(c, x, y, w, h, k);
        if (on) canvas_rect(c, x + 2, y + h + 1, w - 4, 1, C_YELLOW);
        return;
    }
    if (on) { round_rect(c, x - 2, y - 2, w + 4, h + 4, RGB(60, 54, 30)); round_rect(c, x - 1, y - 1, w + 2, h + 2, C_YELLOW); } /* (a soft glow, a ring) */
    round_rect(c, x, y, w, h, on ? RGB(64, 64, 88) : RGB(40, 40, 54));
}
static int ibutton(Canvas *c, int x, int y, int which, int on, const char *label, const char *key) {
    int w = 13, h = 13;
    tile(c, x, y, w, h, which, on);
    icon(c, x + 2, y + 2, which, 1);
    if (key) { canvas_rect(c, x + w - 5, y + h - 6, 5, 6, RGB(200, 200, 215)); canvas_text(c, x + w - 4, y + h - 6, key, RGB(20, 20, 28)); }
    if (label) canvas_text(c, x + (w - text_width(label)) / 2, y + h + 3, label, on ? C_WHITE : C_GREY);
    return w + (label ? 7 : 3);
}
static Ed E;
static Canvas editor_with(const char *dir, int style, int fill_all, int W, int H) {
    memset(&E, 0, sizeof E);
    E.pk = 1; E.pi = 0; E.hx = 7; E.hy = 3; E.drag = -1; E.speed = 1; E.onion = 1; E.del_ask = -1; E.filling = fill_all ? 2 : 1;
    file_open(&E, dir, 2, "walk-2");
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    draw(&E, &cv);
    Layout L = layout(&E, W, H);
    int x = L.px, y = L.gy - 1;
    canvas_rect(&cv, x - 1, y - 1, 120, style == 1 ? 9 : 12, RGB(18, 18, 24)); /* (the words out) */
    if (style == 1) { /* B: with names, the panel moves down a little: drawn over, so shift the colour field down by 8 */
        Canvas cp = canvas_new(W, H, RGB(18, 18, 24));
        memcpy(cp.p, cv.p, sizeof(Rgb) * (size_t)(W * H));
        for (int yy = H - 1; yy >= y + 13; yy--) for (int xx = x; xx < W; xx++) if (yy - 8 >= y + 13) cv.p[yy * W + xx] = cp.p[(yy - 8) * W + xx];
        canvas_rect(&cv, x - 1, y - 1, W - x + 1, 22, RGB(18, 18, 24));
        canvas_free(&cp);
    }
    static const char *NAME[4] = {"PAINT", "ERASE", "PICK", "FILL"}, *KEY[4] = {"P", "E", "I", "B"};
    for (int i = 0; i < 4; i++) {
        int on = i == 0 || i == 3, which = i == 3 && fill_all ? 4 : i;
        x += ibutton(&cv, x, y, which, on, style == 1 ? (i == 3 && fill_all ? "ALL" : NAME[i]) : NULL, style == 2 ? KEY[i] : NULL);
        if (style == 1) x += 6;
    }
    if (style == 0) { /* A: the tool's name and what it does, under the grid, while the pointer is on its button */
        canvas_rect(&cv, L.gx, L.gy + E.h * L.z + 3, L.px - L.gx - 4, 7, RGB(18, 18, 24));
        canvas_text(&cv, L.gx, L.gy + E.h * L.z + 4, fill_all ? "FILL ALL: EVERY PIXEL OF THAT COLOUR" : "FILL: THE TOUCHING PIXELS OF ONE COLOUR", C_GREY);
    }
    return cv;
}
int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/11-tool-icons", *dir = argc > 2 ? argv[2] : "sprites";
    int W = 300, H = 140;
    static const char *T[3] = {"A  ICONS ONLY (NAME UNDER THE GRID ON HOVER)", "B  ICONS WITH NAMES", "C  ICONS WITH THEIR KEY"};
    if (argc > 3) { /* the backgrounds for B, picked: four tiles */
        static const char *BG[4] = {"B0  FLAT (AS BEFORE)", "B1  KEY CAPS", "B2  THE TOOL'S OWN TINT", "B3  ROUNDED, A GLOWING RING"};
        Canvas sheet = canvas_new(2 * (130 + 8) + 8, 2 * 52 + 8, RGB(24, 24, 30));
        for (int b = 0; b < 4; b++) {
            g_bg = b;
            Canvas cv = editor_with(dir, 1, 0, W, H);
            Layout L = layout(&E, W, H);
            int sx = 8 + (b % 2) * 138, sy = 6 + (b / 2) * 52;
            char t[48]; snprintf(t, sizeof t, "%s", BG[b]); for (char *q = t; *q; q++) if (*q == 39) *q = ' ';
            canvas_text(&sheet, sx, sy, t, RGB(220, 220, 235));
            for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 128; xx++) canvas_px(&sheet, sx + xx, sy + 9 + yy, cv.p[(L.gy - 3 + yy) * W + L.px - 3 + xx]);
            canvas_free(&cv);
        }
        char p[400]; snprintf(p, sizeof p, "%s/tool-icons-b-backgrounds.png", o);
        png_write(p, &sheet, 5); fprintf(stderr, "wrote %s\n", p);
        return 0;
    }
    Canvas sheet = canvas_new(W + 12, 3 * (H + 12) + 70, RGB(24, 24, 30));
    for (int st = 0; st < 3; st++) {
        Canvas cv = editor_with(dir, st, 0, W, H);
        canvas_text(&sheet, 6, 4 + st * (H + 12), T[st], RGB(220, 220, 235));
        canvas_blit(&sheet, &cv, 6, 12 + st * (H + 12));
        canvas_free(&cv);
    }
    /* the icons four times bigger, and FILL ALL's bucket */
    int y = 3 * (H + 12) + 8, x = 6;
    canvas_text(&sheet, x, y, "THE ICONS (X4): PAINT  ERASE  PICK  FILL  FILL ALL", RGB(220, 220, 235));
    for (int i = 0; i < 5; i++) { canvas_rect(&sheet, x + i * 44, y + 10, 40, 40, RGB(34, 34, 44)); icon(&sheet, x + i * 44 + 2, y + 12, i, 4); }
    char p[400];
    snprintf(p, sizeof p, "%s/tool-icons.png", o);
    png_write(p, &sheet, 3);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
