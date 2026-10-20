/* Mockup (tools/mock, build.sh): your own sprites over the defaults. The picker marks the sprites you changed (a small
 * dot) and gets RESET (the picked one back to the default), RESET ALL (asks first) and, when the editor runs in the repo,
 * MAKE DEFAULT (yours becomes the repo's art). A: the buttons in the top bar; B: in a bar along the bottom with a legend;
 * C: RESET ALL asking. Drawn over the real picker (editor.c is included).
 *   layer_mock OUT_DIR SPRITES_DIR */
#include "editor.c"
#include "art.h"

static Ed E;
static const char *const MINE[] = {"WORK", "CHEER", "CHASE"}; /* (made up: the sprites that would be yours) */
static void mark_mine(Canvas *cv) { /* a dot at the top left of the cards that are yours */
    for (int r = 0; r < g_nreg; r++) if (g_reg[r].kind == R_PITEM)
        for (int m = 0; m < 3; m++) if (!strcmp(g_pi[g_reg[r].i].name, MINE[m])) {
            int x = g_reg[r].x0 + 4, y = g_reg[r].y0 + 4;
            canvas_rect(cv, x, y, 4, 4, RGB(18, 18, 24)); canvas_rect(cv, x + 1, y + 1, 2, 2, RGB(120, 200, 255));
        }
}
static int small_button(Canvas *cv, int x, int y, const char *t, int strong) {
    int w = text_width(t) + 6;
    canvas_rect(cv, x, y, w, 9, strong ? RGB(52, 54, 70) : RGB(30, 30, 40));
    canvas_text(cv, x + 3, y + 2, t, strong ? C_WHITE : C_GREY);
    return w;
}
static Canvas picker_shot(const char *dir, int W, int H) {
    memset(&E, 0, sizeof E);
    E.pk = 1; E.pi = 0; E.hx = E.hy = -1; E.drag = -1; E.speed = 1; E.onion = 1; E.del_ask = -1;
    file_open(&E, dir, 2, "walk-1");
    picker_open(&E, dir, 1);
    ui_watch(dir);
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    draw(&E, &cv);
    return cv;
}
int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/15-your-sprites", *dir = argc > 2 ? argv[2] : "sprites";
    int W = 300, H = 190;
    /* A: in the top bar, left of the view buttons */
    Canvas a = picker_shot(dir, W, H);
    mark_mine(&a);
    { int x = W - 4 - 26 - 6; const char *b[3] = {"MAKE DEFAULT", "RESET ALL", "RESET"}; for (int k = 0; k < 3; k++) { x -= text_width(b[k]) + 6; small_button(&a, x, 2, b[k], k == 2); x -= 3; } }
    /* B: a bar along the bottom, a legend */
    Canvas b = picker_shot(dir, W, H);
    mark_mine(&b);
    canvas_rect(&b, 0, H - 22, W, 12, RGB(18, 18, 24)); canvas_rect(&b, 0, H - 22, W, 1, RGB(40, 40, 52));
    { int x = 4; canvas_rect(&b, x + 1, H - 18, 2, 2, RGB(120, 200, 255)); canvas_text(&b, x + 6, H - 19, "YOURS: 3", RGB(150, 150, 170)); x += 48;
      x += small_button(&b, x, H - 20, "RESET", 1) + 3; x += small_button(&b, x, H - 20, "RESET ALL", 0) + 3; small_button(&b, x, H - 20, "MAKE DEFAULT", 0); }
    /* C: RESET ALL asking */
    Canvas c = picker_shot(dir, W, H);
    mark_mine(&c);
    { const char *q = "RESET 3 SPRITES TO THE DEFAULT?", *k = "Y RESET    N KEEP THEM";
      int w = text_width(q) + 12, h = 24, bx = (W - w) / 2, by = (H - h) / 2;
      canvas_rect(&c, bx - 1, by - 1, w + 2, h + 2, C_GREY); canvas_rect(&c, bx, by, w, h, RGB(20, 20, 28));
      canvas_text(&c, bx + (w - text_width(q)) / 2, by + 4, q, C_YELLOW); canvas_text(&c, bx + (w - text_width(k)) / 2, by + 14, k, C_WHITE); }
    Canvas sheet = canvas_new(3 * (W + 6) + 6, H + 18, RGB(24, 24, 30));
    canvas_text(&sheet, 6, 3, "A  BUTTONS IN THE TOP BAR", RGB(220, 220, 235));
    canvas_text(&sheet, W + 12, 3, "B  A BAR ALONG THE BOTTOM", RGB(220, 220, 235));
    canvas_text(&sheet, 2 * W + 18, 3, "C  RESET ALL ASKS", RGB(220, 220, 235));
    canvas_blit(&sheet, &a, 6, 12); canvas_blit(&sheet, &b, W + 12, 12); canvas_blit(&sheet, &c, 2 * W + 18, 12);
    char p[400];
    snprintf(p, sizeof p, "%s/your-sprites.png", o);
    png_write(p, &sheet, 3);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
