/* The placer (Engine Design, step 2): gives wall items free places on the back wall and workspaces free places on the
 * floor, away from what must stay clear (the gates, the playground, the bushes) and from each other. */
#ifndef PLACE_H
#define PLACE_H

typedef struct { int x0, y0, x1, y1; } Box; /* [x0, x1) x [y0, y1), room pixels */

static inline int box_hit(Box a, Box b) { return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1; }
static inline Box box_grow(Box a, int m) { return (Box){a.x0 - m, a.y0 - m, a.x1 + m, a.y1 + m}; }

/* a wall item, as a room describes it */
enum { BAND_TOP, BAND_CEIL, BAND_FLOOR, BAND_SHELF, BAND_POST }; /* near the top of the wall / hanging from the ceiling /
                                                                    standing on the floor against it / at shelf height /
                                                                    a post standing on the ground in front (the park) */
enum { WHEN_ALWAYS, WHEN_UP, WHEN_NO_UP };                       /* always / only with stairs up / only without */
typedef struct {
    const char *kind;  /* "sign", "window", "plant", "menu", "lamp", "shelf", "signpost" */
    int w, h;          /* size (w 0: the sign, as wide as its words) */
    int band;
    double prefer;     /* preferred middle, as a part of the back wall's width (BAND_POST: of the room's width) */
    double prefer_up;  /* ... when there are stairs */
    int priority;      /* higher is placed first; 9 is never left out */
    int when;
} WallSpec;

typedef struct { const char *kind; Box box; } Placed;
#define MAX_PLACED 16
typedef struct { Placed p[MAX_PLACED]; int n; } WallPlan;

/* places the specs (highest priority first) on the wall [wl, wr) between top wt and floor line wb, each away from `keep`
 * (n_keep boxes) and the ones placed before; post items on the floor [0, w) */
void place_wall(const WallSpec *spec, int nspec, int has_up, int w, double wl, double wr, double wt, double wb, const Box *keep, int n_keep, WallPlan *out);
#endif
