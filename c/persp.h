/* The room in one-point perspective (style A): the back wall in the middle, the side walls slanting in, the floor and
 * everything on it worked out from one vanishing point (where the floor's side edges meet, above the picture).
 * World: X across (0 = the middle), Z depth (1 = the front edge of the picture), E height above the floor; the eye is at
 * E = 1, so a thing p screen pixels tall at the front is E = p / f. The ceiling and the walls' tops are drawn stylised. */
#ifndef PERSP_H
#define PERSP_H
#include "canvas.h"

#define VIEW_BL 0.172 /* the back wall, as parts of the room's width and height */
#define VIEW_BR 0.828
#define VIEW_BT 0.139
#define VIEW_BB 0.472

typedef struct {
    int w, h;
    double bl, br, bt, bb;  /* the back wall in room pixels */
    double vpx, vpy, f;     /* the vanishing point and the focal length (f = h - vpy, so the front edge is Z = 1) */
    double zb, xw;          /* the back wall's depth and the room's half-width in world units */
    int outdoor;            /* the park: its stairs go up to a terrace (theme.c stairs_geometry) */
} View;

void view_init(View *v, int w, int h);
double view_sx(const View *v, double X, double Z);
double view_sy(const View *v, double Z, double E);
void view_world(const View *v, double x, double y, double *X, double *Z); /* a point on the floor -> world */
void view_floor_span(const View *v, double y, double *xl, double *xr);     /* where the floor is on row y */
double view_e(const View *v, double px);                                   /* p pixels tall at the front -> E */
/* fill the quad of four world points (X, Z, E each) */
void view_quad(Canvas *c, const View *v, const double *p, Rgb col);
/* a box standing on the floor: x0..x1 across, z0..z1 deep, e high; the top, the front, and the side that faces us */
void view_box(Canvas *c, const View *v, double x0, double x1, double z0, double z1, double e, Rgb top, Rgb front, Rgb side);
/* fill a convex polygon given in room pixels */
void poly_fill(Canvas *c, const double *xy, int n, Rgb col);
Rgb rgb_mix(Rgb a, Rgb b, double f);
#endif
