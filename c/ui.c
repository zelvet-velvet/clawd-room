#include "ui.h"
#include "clawd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *MENU_LABEL[MENU_N] = {"OFFICE", "COFFEE", "PARK"};
const char *MENU_SCENE[MENU_N] = {"office", "coffee", "park"};

void overlay_text(char *out, size_t n, int paused, int help_on, const char *note, const char *flash) {
    out[0] = 0;
    const char *parts[4];
    int k = 0;
    if (paused) parts[k++] = "PAUSED";
    if (help_on) parts[k++] = "q/esc quit   p pause   r room size   [ ] zoom   z auto zoom   tab follow next   c auto follow   arrows/wasd move   f auto font+scale   g send a Clawd   ? help";
    if (note && note[0]) parts[k++] = note;
    if (flash && flash[0]) parts[k++] = flash;
    for (int i = 0; i < k; i++) {
        if (i) strncat(out, "   ", n - strlen(out) - 1);
        strncat(out, parts[i], n - strlen(out) - 1);
    }
}

void too_small(char *out, size_t n, int cols, int rows) {
    char msg[64];
    snprintf(msg, sizeof msg, "need %dx%d, have %dx%d", MIN_COLS, MIN_ROWS, cols, rows);
    if ((int)strlen(msg) > cols) msg[cols > 0 ? cols : 0] = 0;
    int r = rows / 2 + 1 > 1 ? rows / 2 + 1 : 1, c = (cols - (int)strlen(msg)) / 2 + 1 > 1 ? (cols - (int)strlen(msg)) / 2 + 1 : 1;
    snprintf(out, n, "\x1b[0m\x1b[2J\x1b[%d;%dH%s", r, c, msg);
}

const char *const SIZE_LABEL[N_SIZES] = {"AUTO", "S  120X60", "M  160X90", "L  220X120", "XL 300X160"};
const char *const SIZE_KEY[N_SIZES] = {"auto", "s", "m", "l", "xl"};
const int SIZE_MAX_W[N_SIZES] = {0, 120, 160, 220, 300}, SIZE_MAX_H[N_SIZES] = {0, 60, 90, 120, 160};

int size_scale(int pw, int ph, int size, int n) {
    if (size <= 0 || size >= N_SIZES) return auto_scale(pw, ph, n);
    int kw = (pw + SIZE_MAX_W[size] - 1) / SIZE_MAX_W[size], kh = (ph + SIZE_MAX_H[size] - 1) / SIZE_MAX_H[size];
    int k = kw > kh ? kw : kh;
    return k < 1 ? 1 : k;
}

int auto_scale(int pw, int ph, int n) {
    for (int k = SCALE_CAP; k >= 1; k--) {
        int w = pw / k, h = ph / k, ys[16], banner, per_row, slot_w, rows_shown;
        if (w < SLOT_MIN_W || h < SLOT_H) continue;
        layout_slots(w, h, n, &banner, &per_row, &slot_w, &rows_shown, ys);
        if (per_row * rows_shown >= (n > 1 ? n : 1) && w >= ROAM_W && h >= ROAM_H) return k;
    }
    return 1;
}

void draw_size_menu(Canvas *cv, const char *const *labels, int n, int pick) {
    const int LINE = 7, PAD = 3;
    int fit = (cv->h - 2 * PAD - 9) / LINE; /* the lines that fit: the list scrolls with the pick */
    int shown = n < fit ? n : fit > 1 ? fit : 1, first = pick - shown / 2;
    if (first > n - shown) first = n - shown;
    if (first < 0) first = 0;
    int w = text_width("ROOM SIZE");
    for (int i = 0; i < n; i++) if (text_width(labels[i]) + 6 > w) w = text_width(labels[i]) + 6;
    w += 2 * PAD;
    int h = 2 * PAD + 9 + shown * LINE, x = (cv->w - w) / 2, y = (cv->h - h) / 2;
    canvas_rect(cv, x - 1, y - 1, w + 2, h + 2, C_GREY);
    canvas_rect(cv, x, y, w, h, RGB(20, 20, 28));
    canvas_text(cv, x + (w - text_width("ROOM SIZE")) / 2, y + PAD, "ROOM SIZE", C_YELLOW);
    for (int i = 0; i < shown; i++) {
        int q = first + i, ly = y + PAD + 9 + i * LINE;
        if (q == pick) canvas_text(cv, x + PAD, ly, ">", C_YELLOW);
        canvas_text(cv, x + PAD + 6, ly, labels[q], q == pick ? C_WHITE : C_GREY);
    }
}

static Rgb dim(Rgb c, double f) { return RGB((int)(c.r * f), (int)(c.g * f), (int)(c.b * f)); }

void draw_menu(Canvas *cv, int pick, int solo, double t) {
    const Rgb MARGIN = RGB(14, 14, 18);
    canvas_rect(cv, 0, 0, cv->w, cv->h, MARGIN);
    canvas_text(cv, cv->w / 2 - text_width("CLAWD ROOM") / 2, 3, "CLAWD ROOM", C_YELLOW);
    if (cv->w < 46 || cv->h < 36) { /* no room for the cards */
        canvas_text(cv, cv->w / 2 - text_width("TOO SMALL") / 2, cv->h / 2, "TOO SMALL", C_GREY);
        return;
    }
    int tall = cv->h >= 56;
    if (tall) canvas_text(cv, cv->w / 2 - text_width("PICK A ROOM") / 2, 11, "PICK A ROOM", C_GREY);
    int top = tall ? 19 : 11, bottom = cv->h - 22; /* the link line and the key hints */
    int ch = bottom - top - 9;
    ch = ch > 90 ? 90 : ch;
    ch = ch < 10 ? 10 : ch;
    int cw = ch * 4 / 3 < (cv->w - 16) / 3 ? ch * 4 / 3 : (cv->w - 16) / 3;
    cw = cw < 14 ? 14 : cw;
    ch = ch < cw * 3 / 4 + 2 ? ch : cw * 3 / 4 + 2;
    int gap = (cv->w - 3 * cw) / 4;
    int spare = bottom - top - ch - 9, y = top + (spare > 0 ? spare / 2 : 0);
    for (int i = 0; i < MENU_N; i++) {
        int x = gap + i * (cw + gap);
        const Theme *theme = theme_get(MENU_SCENE[i]);
        Canvas card = canvas_new(cw, ch, RGB(34, 36, 46));
        theme_background(theme, &card, 0);
        int chosen = i == pick;
        if (!chosen) for (int q = 0; q < card.w * card.h; q++) card.p[q] = dim(card.p[q], 0.55);
        int bob = chosen && (int)(t * 3) % 2 ? -1 : 0;
        Pose pose = {bob, 0, {chosen ? 1 : 0, chosen ? 1 : 0}, 0, chosen ? "happy" : "open", {0, 0}};
        int cy = (int)(ch * theme->horizon) + 3;
        draw_clawd(&card, cw / 2, cy < ch - 9 ? cy : ch - 9, &pose, draw_frames(), dim(C_CLAWD, chosen ? 1.0 : 0.6), NULL, "");
        if (chosen) canvas_rect(cv, x - 2, y - 2, cw + 4, ch + 4, C_YELLOW);
        canvas_blit(cv, &card, x, y);
        canvas_text(cv, x + cw / 2 - text_width(MENU_LABEL[i]) / 2, y + ch + 4, MENU_LABEL[i], chosen ? C_YELLOW : C_GREY);
        canvas_free(&card);
    }
    const char *mode = solo ? "S  SOLO" : "S  LINKED";
    canvas_text(cv, cv->w / 2 - text_width(mode) / 2, cv->h - 15, mode, C_WHITE);
    static const char *HINTS[3] = {"< > CHOOSE   ENTER OPEN   Q QUIT", "< >   ENTER   Q QUIT", "ENTER  Q"};
    const char *hint = "";
    for (int i = 0; i < 3; i++) if (text_width(HINTS[i]) <= cv->w - 4) { hint = HINTS[i]; break; }
    canvas_text(cv, cv->w / 2 - text_width(hint) / 2, cv->h - 8, hint, C_GREY);
}

Canvas render_menu(int cols, int rows, int pick, int solo, double t) {
    const Rgb MARGIN = RGB(14, 14, 18);
    int pw = cols, ph = rows * 2;
    int k = pw / (MENU_W * 5 / 4) < ph / (MENU_H * 5 / 4) ? pw / (MENU_W * 5 / 4) : ph / (MENU_H * 5 / 4);
    if (k < 1) k = 1; /* chunkier pixels in a big window */
    Canvas small = canvas_new(pw / k, ph / k / 2 * 2, MARGIN);
    draw_menu(&small, pick, solo, t);
    Canvas big = canvas_scaled(&small, k), full = canvas_new(pw, ph, MARGIN);
    canvas_blit(&full, &big, (pw - big.w) / 2, (ph - big.h) / 4 * 2); /* whole terminal rows */
    canvas_free(&small); canvas_free(&big);
    return full;
}

char *dump_text(int cols, int rows, Room *room, double now, double t) {
    char note[32];
    Canvas cv = render(cols, rows, room, now, t, note, sizeof note);
    char *out = malloc((size_t)rows * (size_t)(cols * 3 + 1) + 1), *p = out;
    static const char *CH[4] = {" ", "\xe2\x96\x80", "\xe2\x96\x84", "\xe2\x96\x88"}; /* " " upper-half lower-half full */
    for (int r = 0; r < rows; r++) {
        char *line = p;
        char *last = p;
        for (int x = 0; x < cols; x++) {
            Rgb a = cv.p[2 * r * cv.w + x], b = cv.p[(2 * r + 1) * cv.w + x];
            int la = (a.r + a.g + a.b) > 330, lb = (b.r + b.g + b.b) > 330;
            const char *s = CH[la + 2 * lb];
            size_t l = strlen(s);
            memcpy(p, s, l);
            p += l;
            if (s[0] != ' ') last = p; /* trailing spaces are dropped (Python's rstrip) */
        }
        p = last;
        (void)line;
        *p++ = '\n';
    }
    if (p > out) p--; /* the last newline: print() adds its own */
    *p = 0;
    canvas_free(&cv);
    return out;
}
