#include "term.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cells cells_from_canvas(const Canvas *cv, const char *overlay) {
    Cells out = {cv->w, cv->h / 2, NULL};
    out.c = malloc(sizeof(Cell) * (size_t)out.cols * (size_t)(out.rows ? out.rows : 1));
    for (int r = 0; r < out.rows; r++)
        for (int x = 0; x < out.cols; x++)
            out.c[r * out.cols + x] = (Cell){0x2580, cv->p[2 * r * cv->w + x], cv->p[(2 * r + 1) * cv->w + x]}; /* upper half block */
    if (overlay && *overlay && out.rows) {
        char text[1024];
        snprintf(text, sizeof text, " %s", overlay);
        size_t n = strlen(text);
        for (int x = 0; x < out.cols; x++)
            out.c[(out.rows - 1) * out.cols + x] = (Cell){(size_t)x < n ? (unsigned char)text[x] : ' ', RGB(230, 230, 240), RGB(20, 20, 30)};
    }
    return out;
}

void cells_free(Cells *c) { free(c->c); c->c = NULL; }

static int cell_eq(const Cell *a, const Cell *b) { return a->ch == b->ch && rgb_eq(a->fg, b->fg) && rgb_eq(a->bg, b->bg); }

typedef struct { char *s; size_t n, cap; } Buf;

static void put(Buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, s, n);
    b->n += n;
}

static void putf(Buf *b, const char *fmt, int a, int bb, int c) {
    char tmp[64];
    int n = snprintf(tmp, sizeof tmp, fmt, a, bb, c);
    put(b, tmp, (size_t)n);
}

static void put_utf8(Buf *b, uint32_t cp) {
    char t[4];
    int n = 0;
    if (cp < 0x80) t[n++] = (char)cp;
    else if (cp < 0x800) { t[n++] = (char)(0xC0 | (cp >> 6)); t[n++] = (char)(0x80 | (cp & 0x3F)); }
    else { t[n++] = (char)(0xE0 | (cp >> 12)); t[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[n++] = (char)(0x80 | (cp & 0x3F)); }
    put(b, t, (size_t)n);
}

char *diff_frame(const Cells *cur, const Cells *prev, size_t *len) {
    Buf b = {NULL, 0, 0};
    Rgb fg = {0}, bg = {0};
    int have_fg = 0, have_bg = 0;
    int same_shape = prev && prev->cols == cur->cols && prev->rows == cur->rows;
    for (int r = 0; r < cur->rows; r++) {
        const Cell *row = cur->c + r * cur->cols, *old = same_shape ? prev->c + r * cur->cols : NULL;
        int x = 0, n = cur->cols;
        while (x < n) {
            if (old && cell_eq(&old[x], &row[x])) { x++; continue; }
            { char t[32]; int k = snprintf(t, sizeof t, "\x1b[%d;%dH", r + 1, x + 1); put(&b, t, (size_t)k); }
            while (x < n && !(old && cell_eq(&old[x], &row[x]))) { /* one run of changed cells */
                if (!have_fg || !rgb_eq(row[x].fg, fg)) { putf(&b, "\x1b[38;2;%d;%d;%dm", row[x].fg.r, row[x].fg.g, row[x].fg.b); fg = row[x].fg; have_fg = 1; }
                if (!have_bg || !rgb_eq(row[x].bg, bg)) { putf(&b, "\x1b[48;2;%d;%d;%dm", row[x].bg.r, row[x].bg.g, row[x].bg.b); bg = row[x].bg; have_bg = 1; }
                put_utf8(&b, row[x].ch);
                x++;
            }
        }
    }
    if (!b.s) b.s = calloc(1, 1);
    b.s[b.n] = 0;
    if (len) *len = b.n;
    return b.s;
}
