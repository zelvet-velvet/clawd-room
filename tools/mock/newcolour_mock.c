/* Mockup (tools/mock, build.sh): the file's colours are only colours to paint with; changing the colour (the field, the
 * hue bar, typing) never changes one of them but makes a new colour, held in a "+" slot (a dark square with a plus while
 * empty). Painting with it puts it in the file. A: the slot at the upper left of IN THIS FILE; B: after the last colour.
 * Each in three steps: holding a file colour, the field dragged (a new colour in the slot), painted (it joined the file).
 * Drawn over the real editor (editor.c is included).
 *   newcolour_mock OUT_DIR SPRITES_DIR */
#include "editor.c"
#include "art.h"

static Ed E;
static const Rgb NEWC = {214, 112, 160};
static void plus_slot(Canvas *cv, int sx, int sy, int held) {
    if (held) { canvas_rect(cv, sx, sy, 8, 8, NEWC); return; }
    canvas_rect(cv, sx, sy, 8, 8, RGB(30, 30, 40));
    frame(cv, sx + 1, sy + 1, 6, 6, RGB(30, 30, 40));
    canvas_rect(cv, sx + 2, sy + 4, 5, 1, RGB(150, 150, 170)); canvas_rect(cv, sx + 4, sy + 2, 1, 5, RGB(150, 150, 170));
}
/* step 0: holding the light blue; 1: the field dragged, a new colour in the slot; 2: painted, it joined the file */
static Canvas shot(const char *dir, int W, int H, int plus_first, int step, int *cx, int *cw, int *ch) {
    memset(&E, 0, sizeof E);
    E.pk = 1; E.pi = 0; E.hx = E.hy = -1; E.drag = -1; E.speed = 1; E.onion = 1; E.del_ask = -1;
    g_panel_folded = 0;
    file_open(&E, dir, 0, "window");
    ui_watch(dir);
    int light = 0;
    for (int q = 0; q < E.f.npal; q++) if (E.f.pal_ch[q] == 's') light = q;
    if (step == 0) E.pi = light; else { E.pk = 2; E.prgb = NEWC; }
    if (step == 2) for (int y = 2; y < 5; y++) for (int x = 2; x < 4; x++) E.g[y][x] = '#';
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    draw(&E, &cv);
    int x0 = 9999, y0 = 9999;
    for (int r = 0; r < g_nreg; r++) if (g_reg[r].kind == R_FILE) { if (g_reg[r].x0 < x0) x0 = g_reg[r].x0; if (g_reg[r].y0 < y0) y0 = g_reg[r].y0; }
    Rgb bg = cv.p[(y0 - 2) * cv.w + x0 + FIELD_W + 1];
    canvas_rect(&cv, x0 - 2, y0 - 2, FIELD_W + 4, 2 * SW_ROW, bg);
    canvas_rect(&cv, x0 + FIELD_W - 30, y0 - SECTION, 30, 6, bg); /* (no "+ ADD" by the title: the slot is it) */
    int n = E.f.npal + (step == 2), slot = plus_first ? 0 : n, k = 0;
    for (int s = 0; s <= n; s++) {
        int sx = x0 + (s % 10) * 10, sy = y0 + (s / 10) * SW_ROW;
        if (s == slot) { plus_slot(&cv, sx, sy, step == 1); if (step == 1) frame(&cv, sx - 1, sy - 1, 10, 10, RGB(150, 150, 170)); continue; }
        int i = k++;
        Rgb c = i < E.f.npal ? E.f.pal_c[i] : NEWC;
        canvas_rect(&cv, sx, sy, 8, 8, c);
        int used = i >= E.f.npal;
        for (int yy = 0; yy < E.h && !used; yy++) for (int xx = 0; xx < E.w; xx++) if (E.g[yy][xx] == E.f.pal_ch[i]) { used = 1; break; }
        if (used) canvas_rect(&cv, sx + 3, sy + 9, 2, 1, RGB(200, 200, 215));
        if ((step == 0 && i == light) || (step == 2 && i >= E.f.npal)) frame(&cv, sx - 1, sy - 1, 10, 10, RGB(150, 150, 170));
    }
    if (step == 2) for (int y = 2; y < 5; y++) for (int x = 2; x < 4; x++) { /* (the painted pixels on the canvas, in the new colour) */
        for (int r = 0; r < g_nreg; r++) if (g_reg[r].kind == R_GRID) {
            int z = (g_reg[r].x1 - g_reg[r].x0) / E.w;
            canvas_rect(&cv, g_reg[r].x0 + x * z, g_reg[r].y0 + y * z, z, z, NEWC);
        }
    }
    *cx = x0 - 4; *cw = FIELD_W + 8; *ch = y0 + 2 * SW_ROW + 26;
    return cv;
}
int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/16-new-colour", *dir = argc > 2 ? argv[2] : "sprites";
    int W = 300, H = 190, cx, cw, chh;
    static const char *STEP[3] = {"1 HOLDING A FILE COLOUR", "2 FIELD DRAGGED: A NEW ONE", "3 PAINTED: IT JOINED"};
    Canvas tmp = shot(dir, W, H, 1, 0, &cx, &cw, &chh); canvas_free(&tmp);
    int gap = 8;
    Canvas sheet = canvas_new(3 * (cw + gap) + 4, 2 * (chh + 22) + 4, RGB(24, 24, 30));
    for (int opt = 0; opt < 2; opt++) {
        int oy = 4 + opt * (chh + 22);
        canvas_text(&sheet, 6, oy, opt ? "B  THE + SLOT AFTER THE LAST COLOUR" : "A  THE + SLOT AT THE UPPER LEFT", RGB(220, 220, 235));
        for (int s = 0; s < 3; s++) {
            Canvas c = shot(dir, W, H, !opt, s, &cx, &cw, &chh);
            int dx = 6 + s * (cw + gap);
            canvas_text(&sheet, dx, oy + 7, STEP[s], RGB(150, 150, 170));
            for (int y = 0; y < chh; y++) for (int x = 0; x < cw; x++) canvas_px(&sheet, dx + x, oy + 14 + y, c.p[y * c.w + cx + x]);
            canvas_free(&c);
        }
    }
    char p[400];
    snprintf(p, sizeof p, "%s/new-colour.png", o);
    png_write(p, &sheet, 4);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
