/* Mockup (tools/mock, build.sh): a sprite picker in the editor (./clawd-room.sh --edit with no name, l in the editor, or
 * a name that is not there: the picker with it typed in the search). A: cards in a grid, a tab per file; B: a list on the
 * left, the picked one big on the right with its frames. Drawn with the editor's own helpers (editor.c is included).
 *   picker_mock OUT_DIR SPRITES_DIR */
#include "editor.c"
#include "art.h"

#define BG RGB(18, 18, 24)
#define DIM RGB(110, 110, 130)
static File F[NFILES];
typedef struct { int file, sp, n; char name[64]; } Item;
static Item IT[256];
static int NI;
static void gather(const char *dir) {
    NI = 0;
    for (int f = 0; f < NFILES; f++) {
        char path[700];
        snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
        memset(&F[f], 0, sizeof F[f]);
        if (file_load(&F[f], path)) continue;
        static unsigned char seen[512];
        memset(seen, 0, sizeof seen);
        for (int i = 0; i < F[f].nsprite && NI < 256; i++) {
            if (seen[i]) continue;
            int fr[128], n = anim_frames(&F[f], i, fr, 128), num;
            for (int k = 0; k < n; k++) seen[fr[k]] = 1;
            char name[64], base[64];
            sprite_name(&F[f], fr[0], name, sizeof name);
            int numbered = split_name(name, base, sizeof base, &num) == 1;
            IT[NI] = (Item){f, fr[0], n, ""};
            snprintf(IT[NI].name, sizeof IT[NI].name, "%s", n > 1 || numbered ? base : name);
            for (char *q = IT[NI].name; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            NI++;
        }
    }
}
static void sprite_at(Canvas *cv, int f, int sp, int x, int y, int z, int maxw, int maxh) {
    static char g[MAXH][MAXW + 1];
    int w, h;
    grid_of(&F[f], sp, g, &w, &h);
    while (z > 1 && (w * z > maxw || h * z > maxh)) z--;
    int ox = x + (maxw - w * z) / 2, oy = y + (maxh - h * z) / 2;
    for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) { int tr; Rgb c = colour_of(&F[f], g[yy][xx], &tr); if (!tr) canvas_rect(cv, ox + xx * z, oy + yy * z, z, z, c); }
}
static void hint(Canvas *cv, const char *t) { canvas_rect(cv, 0, cv->h - 10, cv->w, 1, RGB(40, 40, 52)); canvas_text(cv, 4, cv->h - 7, t, RGB(150, 150, 170)); }
static void search(Canvas *cv, int x, int y, int w, const char *typed) {
    canvas_rect(cv, x, y, w, 9, RGB(30, 30, 40));
    canvas_text(cv, x + 3, y + 2, typed[0] ? typed : "TYPE TO FIND", typed[0] ? C_WHITE : RGB(90, 90, 110));
    if (typed[0]) canvas_rect(cv, x + 3 + text_width(typed) + 1, y + 2, 3, 5, C_YELLOW);
}

/* A: cards. Tabs for the files along the top, a search box, the cards in a grid (the sprite on a checker, its name, the
 * frames it has); the picked card framed in yellow */
static Canvas option_a(int W, int H, int tab, const char *typed, int pick) {
    Canvas cv = canvas_new(W, H, BG);
    canvas_text(&cv, 4, 3, "OPEN A SPRITE", C_YELLOW);
    static const char *TABS[NFILES] = {"PROPS", "ITEMS", "CLAWD", "UI"};
    int tx = 4, ty = 12;
    for (int f = 0; f < NFILES; f++) {
        int tw = text_width(TABS[f]) + 8;
        canvas_rect(&cv, tx, ty, tw, 10, f == tab ? RGB(52, 54, 70) : RGB(28, 28, 36));
        if (f == tab) canvas_rect(&cv, tx + 2, ty + 9, tw - 4, 1, C_YELLOW);
        canvas_text(&cv, tx + 4, ty + 3, TABS[f], f == tab ? C_WHITE : C_GREY);
        tx += tw + 2;
    }
    search(&cv, W - 4 - 90, ty, 90, typed);
    int cw = 46, chh = 42, gap = 4, cols = (W - 8 + gap) / (cw + gap), x0 = 4, y0 = ty + 16, k = 0;
    for (int i = 0; i < NI; i++) {
        if (IT[i].file != tab) continue;
        int cx = x0 + (k % cols) * (cw + gap), cy = y0 + (k / cols) * (chh + gap);
        if (cy + chh > H - 12) break;
        int on = k == pick;
        canvas_rect(&cv, cx, cy, cw, chh, on ? RGB(44, 44, 60) : RGB(28, 28, 36));
        checker(&cv, cx + 3, cy + 3, cw - 6, 26);
        sprite_at(&cv, IT[i].file, IT[i].sp, cx + 3, cy + 3, 3, cw - 6, 26);
        char nm[16]; snprintf(nm, sizeof nm, "%.11s", IT[i].name);
        canvas_text(&cv, cx + 3, cy + 31, nm, on ? C_WHITE : RGB(190, 190, 205));
        if (IT[i].n > 1) { char fn[8]; snprintf(fn, sizeof fn, "%d", IT[i].n); canvas_rect(&cv, cx + cw - text_width(fn) - 6, cy + 4, text_width(fn) + 4, 7, RGB(18, 18, 24)); canvas_text(&cv, cx + cw - text_width(fn) - 4, cy + 5, fn, C_YELLOW); }
        if (on) frame(&cv, cx, cy, cw, chh, C_YELLOW);
        k++;
    }
    hint(&cv, "ARROWS PICK  ENTER OPEN  TAB FILE  TYPE TO FIND  ESC BACK");
    return cv;
}

/* B: a list on the left (grouped by file), the picked one big on the right with its file, size and frames */
static Canvas option_b(int W, int H, const char *typed, int pick) {
    Canvas cv = canvas_new(W, H, BG);
    canvas_text(&cv, 4, 3, "OPEN A SPRITE", C_YELLOW);
    int lw = 92;
    search(&cv, 4, 12, lw - 4, typed);
    int y = 26, k = 0, picked = -1;
    static const char *FN[NFILES] = {"PROPS", "ITEMS", "CLAWD", "UI"};
    for (int f = 0; f < NFILES && y < H - 20; f++) {
        canvas_text(&cv, 4, y, FN[f], RGB(84, 84, 104)); canvas_rect(&cv, 4, y + 7, lw - 4, 1, RGB(40, 40, 52)); y += 10;
        for (int i = 0; i < NI && y < H - 20; i++) {
            if (IT[i].file != f) continue;
            int on = k == pick;
            if (on) { canvas_rect(&cv, 2, y - 1, lw, 7, RGB(44, 44, 60)); picked = i; }
            canvas_text(&cv, 7, y, IT[i].name, on ? C_WHITE : RGB(190, 190, 205));
            if (IT[i].n > 1) { char fn[8]; snprintf(fn, sizeof fn, "%d", IT[i].n); canvas_text(&cv, lw - text_width(fn) - 4, y, fn, on ? C_YELLOW : RGB(110, 110, 130)); }
            y += 7; k++;
        }
        y += 3;
    }
    canvas_rect(&cv, lw + 4, 12, 1, H - 24, RGB(40, 40, 52));
    if (picked >= 0) { /* the picked one: big, on a checker; its file, size, frames under it */
        int px = lw + 12, pw = W - px - 6, ph = H - 12 - 60;
        checker(&cv, px, 14, pw, ph);
        sprite_at(&cv, IT[picked].file, IT[picked].sp, px, 14, 8, pw, ph);
        static char g[MAXH][MAXW + 1]; int w, h;
        grid_of(&F[IT[picked].file], IT[picked].sp, g, &w, &h);
        char info[96];
        snprintf(info, sizeof info, "%s   %s   %dX%d   %d FRAME%s", IT[picked].name, FILES[IT[picked].file], w, h, IT[picked].n, IT[picked].n == 1 ? "" : "S");
        for (char *q = info; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        canvas_text(&cv, px, 14 + ph + 4, info, C_YELLOW);
        int fr[128], n = anim_frames(&F[IT[picked].file], IT[picked].sp, fr, 128), fx = px;
        for (int i = 0; i < n && fx + 34 <= W; i++) { checker(&cv, fx, 14 + ph + 13, 32, 22); sprite_at(&cv, IT[picked].file, fr[i], fx, 14 + ph + 13, 2, 32, 22); fx += 35; }
    }
    hint(&cv, "UP DOWN PICK  ENTER OPEN  TYPE TO FIND  ESC BACK");
    return cv;
}

int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/14-sprite-picker", *dir = argc > 2 ? argv[2] : "sprites";
    gather(dir);
    int W = 300, H = 190;
    Canvas a = option_a(W, H, 2, "", 5), b = option_b(W, H, "", 12);
    Canvas sheet = canvas_new(2 * W + 18, H + 18, RGB(24, 24, 30));
    canvas_text(&sheet, 6, 3, "A  CARDS, A TAB PER FILE", RGB(220, 220, 235));
    canvas_text(&sheet, W + 12, 3, "B  A LIST, THE PICKED ONE BIG", RGB(220, 220, 235));
    canvas_blit(&sheet, &a, 6, 12); canvas_blit(&sheet, &b, W + 12, 12);
    char p[400];
    snprintf(p, sizeof p, "%s/sprite-picker.png", o);
    png_write(p, &sheet, 3);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
