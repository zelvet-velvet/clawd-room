#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "construct.h"

#define MAXP 1500
#define FLY 0.24 /* the part of the build one piece takes to fly in */
typedef struct { int n, first, x0, y0, x1, y1; double start, ox, oy, spin; } Piece;

static double hsh(int k, int s) { unsigned h = (unsigned)k * 2654435761u ^ (unsigned)s * 40503u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return (h & 0xffff) / 65536.0; }
static double seg(double t, double a, double b) { double u = (t - a) / (b - a); return u < 0 ? 0 : u > 1 ? 1 : u; }
static double ease_out(double q) { q = q < 0 ? 0 : q > 1 ? 1 : q; return 1 - (1 - q) * (1 - q) * (1 - q); }
static Rgb mixc(Rgb a, Rgb b, double f) { return RGB((uint8_t)(a.r + (b.r - a.r) * f), (uint8_t)(a.g + (b.g - a.g) * f), (uint8_t)(a.b + (b.b - a.b) * f)); }

/* the pieces: runs of one colour, cut into a real size (long runs into lengths, big blocks into boards 3 high and about 13 long) */
static int *g_lab, *g_ord, *g_q;
static unsigned char *g_aura;
static size_t g_cap;
static Piece g_p[MAXP];

static int cut(const Rgb *after, const Rgb *before, int w, int h, int *np) {
    size_t n = (size_t)w * (size_t)h;
    int count = 0;
    for (size_t i = 0; i < n; i++) g_lab[i] = -2;
    for (size_t i = 0; i < n; i++) if (!rgb_eq(after[i], before[i])) g_lab[i] = -1;
    int comp = 0;
    for (size_t s = 0; s < n; s++) if (g_lab[s] == -1) {
        Rgb seed = after[s];
        int qn = 0, bx0 = (int)(s % (size_t)w), bx1 = bx0, by0 = (int)(s / (size_t)w), by1 = by0;
        g_q[qn++] = (int)s; g_lab[s] = -3;
        for (int k = 0; k < qn; k++) {
            int ix = g_q[k] % w, iy = g_q[k] / w;
            bx0 = ix < bx0 ? ix : bx0; bx1 = ix > bx1 ? ix : bx1; by0 = iy < by0 ? iy : by0; by1 = iy > by1 ? iy : by1;
            for (int d = 0; d < 4; d++) {
                int xx = ix + (d == 0) - (d == 1), yy = iy + (d == 2) - (d == 3);
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                int j = yy * w + xx;
                if (g_lab[j] == -1 && rgb_eq(after[j], seed)) { g_lab[j] = -3; g_q[qn++] = j; }
            }
        }
        int bw = bx1 - bx0 + 1, bh = by1 - by0 + 1, off = (int)(hsh(comp, 23) * 12);
        static int id[4096];
        for (int k = 0; k < 4096; k++) id[k] = -1;
        for (int k = 0; k < qn; k++) {
            int ix = g_q[k] % w, iy = g_q[k] / w, sub;
            if (bh >= 4 && bw >= 6) { int band = (iy - by0) / 3; sub = band * 64 + (ix - bx0 + off + band * 5) / 13; }
            else if (bw > bh) sub = (ix - bx0 + off) / 11;
            else sub = (iy - by0) / 12;
            sub &= 4095;
            if (id[sub] < 0) id[sub] = count < MAXP - 1 ? count++ : count - 1;
            g_lab[g_q[k]] = id[sub];
        }
        comp++;
    }
    *np = count;
    return count;
}

void construct_draw(Canvas *cv, const Rgb *after, double t) {
    int w = cv->w, h = cv->h, np = 0;
    size_t n = (size_t)w * (size_t)h;
    if (n > g_cap) {
        free(g_lab); free(g_ord); free(g_q); free(g_aura);
        g_lab = malloc(n * sizeof(int)); g_ord = malloc(n * sizeof(int)); g_q = malloc(n * sizeof(int)); g_aura = malloc(n);
        g_cap = g_lab && g_ord && g_q && g_aura ? n : 0;
    }
    if (!g_cap) return;
    if (!cut(after, cv->p, w, h, &np)) return;
    for (int k = 0; k < np; k++) { g_p[k].n = 0; g_p[k].x0 = w; g_p[k].y0 = h; g_p[k].x1 = -1; g_p[k].y1 = -1; }
    int y0 = h, y1 = -1;
    for (size_t i = 0; i < n; i++) if (g_lab[i] >= 0) {
        Piece *p = &g_p[g_lab[i]];
        int x = (int)(i % (size_t)w), y = (int)(i / (size_t)w);
        p->n++;
        p->x0 = x < p->x0 ? x : p->x0; p->x1 = x > p->x1 ? x : p->x1; p->y0 = y < p->y0 ? y : p->y0; p->y1 = y > p->y1 ? y : p->y1;
        y0 = y < y0 ? y : y0; y1 = y > y1 ? y : y1;
    }
    int at = 0; /* the pixels of each piece, together (a counting sort) */
    for (int k = 0; k < np; k++) { g_p[k].first = at; at += g_p[k].n; g_p[k].n = 0; }
    for (size_t i = 0; i < n; i++) if (g_lab[i] >= 0) { Piece *p = &g_p[g_lab[i]]; g_ord[p->first + p->n++] = (int)i; }
    for (int k = 0; k < np; k++) if (g_p[k].n) { /* the order: big parts from the floor up, then the small things (the handle) */
        double up = (double)(y1 - g_p[k].y1) / (y1 - y0 + 1), small = 1 - fmin(1, g_p[k].n / 30.0);
        double a = M_PI * (1.05 + 0.9 * hsh(k, 13)), dist = 30 + 24 * hsh(k, 17);
        g_p[k].start = 0.08 + 0.52 * fmin(1, up * 0.6 + small * 0.3 + hsh(k, 11) * 0.15);
        g_p[k].ox = cos(a) * dist; g_p[k].oy = sin(a) * dist * 0.7 - 8; g_p[k].spin = (hsh(k, 19) - 0.5) * 3.0;
    }
    Rgb BLUE = RGB(80, 170, 255), PALE = RGB(170, 225, 255);
    double wire = seg(t, 0, 0.12) * (1 - seg(t, 0.82, 1.0)); /* the blue outline of what is to come */
    if (wire > 0) for (size_t i = 0; i < n; i++) if (g_lab[i] >= 0) {
        int x = (int)(i % (size_t)w), y = (int)(i / (size_t)w), k = g_lab[i], edge = 0;
        for (int d = 0; d < 4; d++) {
            int xx = x + (d == 0) - (d == 1), yy = y + (d == 2) - (d == 3);
            if (xx < 0 || yy < 0 || xx >= w || yy >= h || g_lab[yy * w + xx] < 0) edge = 2; else if (g_lab[yy * w + xx] != k && !edge) edge = 1;
        }
        cv->p[i] = mixc(cv->p[i], edge ? PALE : BLUE, (edge == 2 ? 0.75 : edge == 1 ? 0.35 : 0.1) * wire);
    }
    memset(g_aura, 0, n);
    for (int k = 0; k < np; k++) {
        const Piece *p = &g_p[k];
        double q = (t - p->start) / FLY;
        if (!p->n || q <= 0) continue;
        double pcx = (p->x0 + p->x1) / 2.0, pcy = (p->y0 + p->y1) / 2.0;
        for (int trail = 2; trail >= 0; trail--) {
            double qq = q - trail * 0.1;
            if (trail && (q >= 1 || qq <= 0)) continue;
            double e = ease_out(qq), ang = p->spin * (1 - e) * (1 - e), dx = p->ox * (1 - e), dy = p->oy * (1 - e);
            for (int i = 0; i < p->n; i++) {
                int src = g_ord[p->first + i], x = src % w, y = src / w;
                double rx = x - pcx, ry = y - pcy;
                int X = (int)lround(pcx + dx + rx * cos(ang) - ry * sin(ang)), Y = (int)lround(pcy + dy + rx * sin(ang) + ry * cos(ang));
                if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
                size_t di = (size_t)Y * (size_t)w + (size_t)X;
                if (trail) { cv->p[di] = mixc(cv->p[di], BLUE, trail == 1 ? 0.3 : 0.15); continue; }
                Rgb col = after[src];
                if (q >= 1 && q < 1.25) col = mixc(col, RGB(255, 255, 255), 0.8 * (1 - (q - 1) / 0.25)); /* the click */
                cv->p[di] = col;
                if (q < 1.6) g_aura[di] = (unsigned char)(q < 1 ? 255 : 255 * (1.6 - q) / 0.6);
            }
        }
    }
    /* the glow that holds each part while it flies, fading once it is in (read from a copy: it does not spread by itself) */
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    for (int y = 1; y < h - 1; y++) for (int x = 1; x < w - 1; x++) if (!g_aura[y * w + x]) {
        int a = 0;
        for (int d = 0; d < 8; d++) { int v = g_aura[(y + DY[d]) * w + x + DX[d]]; if (v > a) a = v; }
        if (a) cv->p[y * w + x] = mixc(cv->p[y * w + x], PALE, 0.7 * (a & 255) / 255.0);
    }
}
