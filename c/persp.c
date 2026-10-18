#include "persp.h"
#include <math.h>

void view_init(View *v, int w, int h) {
    v->w = w; v->h = h;
    v->bl = w * VIEW_BL; v->br = w * VIEW_BR; v->bt = h * VIEW_BT; v->bb = h * VIEW_BB;
    v->vpx = w / 2.0;
    v->vpy = h - (h - v->bb) * (w / 2.0) / (v->bl > 1 ? v->bl : 1); /* the floor's left edge, from (0, h) to (bl, bb), carried on */
    v->f = h - v->vpy;
    v->zb = v->f / (v->bb - v->vpy);
    v->xw = (v->vpx - v->bl) * v->zb / v->f;
    v->outdoor = 0;
}

double view_sx(const View *v, double X, double Z) { return v->vpx + X * v->f / Z; }
double view_sy(const View *v, double Z, double E) { return v->vpy + v->f * (1 - E) / Z; }
double view_e(const View *v, double px) { return px / v->f; }

void view_world(const View *v, double x, double y, double *X, double *Z) {
    double d = y - v->vpy;
    *Z = v->f / (d > 1e-6 ? d : 1e-6);
    *X = (x - v->vpx) * *Z / v->f;
}

void view_floor_span(const View *v, double y, double *xl, double *xr) {
    double t = (y - v->bb) / (v->h - v->bb);
    if (t < 0) t = 0;
    *xl = v->bl * (1 - t);
    *xr = v->br + (v->w - v->br) * t;
}

Rgb rgb_mix(Rgb a, Rgb b, double f) { return RGB((int)(a.r + (b.r - a.r) * f), (int)(a.g + (b.g - a.g) * f), (int)(a.b + (b.b - a.b) * f)); }

void poly_fill(Canvas *c, const double *xy, int n, Rgb col) {
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (int i = 0; i < n; i++) { x0 = fmin(x0, xy[2 * i]); x1 = fmax(x1, xy[2 * i]); y0 = fmin(y0, xy[2 * i + 1]); y1 = fmax(y1, xy[2 * i + 1]); }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->w - 1) x1 = c->w - 1;
    if (y1 > c->h - 1) y1 = c->h - 1;
    for (int y = (int)floor(y0); y <= (int)y1; y++)
        for (int x = (int)floor(x0); x <= (int)x1; x++) { /* the pixel centre is inside when it is on one side of every edge */
            double px = x + 0.5, py = y + 0.5;
            int pos = 0, neg = 0;
            for (int i = 0; i < n; i++) {
                int j = (i + 1) % n;
                double cr = (xy[2 * j] - xy[2 * i]) * (py - xy[2 * i + 1]) - (xy[2 * j + 1] - xy[2 * i + 1]) * (px - xy[2 * i]);
                if (cr > 1e-9) pos = 1;
                if (cr < -1e-9) neg = 1;
            }
            if (!(pos && neg)) canvas_px(c, x, y, col);
        }
}

void view_quad(Canvas *c, const View *v, const double *p, Rgb col) {
    double xy[8];
    for (int i = 0; i < 4; i++) { xy[2 * i] = view_sx(v, p[3 * i], p[3 * i + 1]); xy[2 * i + 1] = view_sy(v, p[3 * i + 1], p[3 * i + 2]); }
    poly_fill(c, xy, 4, col);
}

void view_box(Canvas *c, const View *v, double x0, double x1, double z0, double z1, double e, Rgb top, Rgb front, Rgb side) {
    if (x1 < 0) { double q[12] = {x1, z0, 0, x1, z1, 0, x1, z1, e, x1, z0, e}; view_quad(c, v, q, side); } /* left of the middle: its right side shows */
    if (x0 > 0) { double q[12] = {x0, z0, 0, x0, z1, 0, x0, z1, e, x0, z0, e}; view_quad(c, v, q, side); }
    double t[12] = {x0, z0, e, x1, z0, e, x1, z1, e, x0, z1, e};
    view_quad(c, v, t, top);
    double fr[12] = {x0, z0, 0, x1, z0, 0, x1, z0, e, x0, z0, e};
    view_quad(c, v, fr, front);
}
