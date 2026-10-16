#include "canvas.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "font_data.h"

Canvas canvas_new(int w, int h, Rgb bg) {
    Canvas c = {w, h, malloc(sizeof(Rgb) * (size_t)w * (size_t)h)};
    for (int i = 0; i < w * h; i++) c.p[i] = bg;
    return c;
}

void canvas_free(Canvas *c) { free(c->p); c->p = NULL; }

void canvas_px(Canvas *c, int x, int y, Rgb col) {
    if (x >= 0 && x < c->w && y >= 0 && y < c->h) c->p[y * c->w + x] = col;
}

void canvas_rect(Canvas *c, int x, int y, int w, int h, Rgb col) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > c->w ? c->w : x + w;
    int y0 = y < 0 ? 0 : y, y1 = y + h > c->h ? c->h : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) c->p[yy * c->w + xx] = col;
}

void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, Rgb col) {
    int n = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    if (n < 1) n = 1;
    for (int i = 0; i <= n; i++) /* Python: int() of a float, truncating toward zero */
        canvas_px(c, (int)(x0 + (double)(x1 - x0) * i / n), (int)(y0 + (double)(y1 - y0) * i / n), col);
}

static const char *glyph(char ch) {
    size_t n = sizeof FONT_GLYPHS / sizeof FONT_GLYPHS[0];
    for (size_t i = 0; i < n; i++)
        if (FONT_GLYPHS[i][0][0] == ch && FONT_GLYPHS[i][0][1] == 0) return FONT_GLYPHS[i][1];
    for (size_t i = 0; i < n; i++)
        if (FONT_GLYPHS[i][0][0] == '?') return FONT_GLYPHS[i][1];
    return "000/000/000/000/000";
}

int text_width(const char *s) { size_t n = strlen(s); return n ? (int)n * 4 - 1 : 0; }

void canvas_text(Canvas *c, int x, int y, const char *s, Rgb col) {
    for (; *s; s++, x += 4) {
        const char *g = glyph((char)toupper((unsigned char)*s));
        for (int ry = 0; ry < 5; ry++)
            for (int rx = 0; rx < 3; rx++)
                if (g[ry * 4 + rx] == '1') canvas_px(c, x + rx, y + ry, col);
    }
}

uint64_t canvas_hash(const Canvas *c) {
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < c->w * c->h; i++) {
        h = (h ^ c->p[i].r) * 1099511628211ULL;
        h = (h ^ c->p[i].g) * 1099511628211ULL;
        h = (h ^ c->p[i].b) * 1099511628211ULL;
    }
    return h;
}
