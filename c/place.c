/* The placer: wall items on the back wall, away from what must stay clear and from each other. */
#include "place.h"

static Box at(const WallSpec *s, int cx, double wt, double wb) {
    int x0 = cx - s->w / 2, y0;
    switch (s->band) {
    case BAND_CEIL: y0 = (int)wt; break;
    case BAND_FLOOR: y0 = (int)wb - 1 - s->h; break;
    case BAND_SHELF: y0 = (int)wb - 12; break;
    case BAND_POST: y0 = (int)wb + 4 - s->h; break;
    default: y0 = (int)wt + 2; break;
    }
    return (Box){x0, y0, x0 + s->w, y0 + s->h};
}

void place_wall(const WallSpec *spec, int nspec, int has_up, int w, double wl, double wr, double wt, double wb, const Box *keep, int n_keep, WallPlan *out) {
    out->n = 0;
    int done[MAX_PLACED] = {0};
    for (int round = 0; round < nspec && round < MAX_PLACED; round++) {
        int best = -1; /* the highest priority not placed yet (ties: the order given) */
        for (int i = 0; i < nspec && i < MAX_PLACED; i++)
            if (!done[i] && (best < 0 || spec[i].priority > spec[best].priority)) best = i;
        if (best < 0) break;
        done[best] = 1;
        const WallSpec *s = &spec[best];
        if ((s->when == WHEN_UP && !has_up) || (s->when == WHEN_NO_UP && has_up) || s->w <= 0) continue;
        double lo = s->band == BAND_POST ? 0 : wl, hi = s->band == BAND_POST ? w : wr;
        int c0 = (int)(lo + (hi - lo) * (has_up ? s->prefer_up : s->prefer)), found = 0;
        Box b = at(s, c0, wt, wb);
        for (int d = 0; d <= (int)(hi - lo) && !found; d++) /* the nearest free place to the preferred one */
            for (int sg = 0; sg < (d ? 2 : 1) && !found; sg++) {
                Box t = at(s, c0 + (sg ? -d : d), wt, wb);
                if (t.x0 < (int)lo + 1 || t.x1 > (int)hi - 1) continue;
                int clear = 1;
                for (int k = 0; k < n_keep && clear; k++) if (box_hit(box_grow(t, 2), keep[k])) clear = 0;
                for (int k = 0; k < out->n && clear; k++) if (box_hit(box_grow(t, 2), out->p[k].box)) clear = 0;
                if (clear) { b = t; found = 1; }
            }
        for (int up = 1; !found && s->priority >= 9 && up < b.y0; up++) /* a must-show item: hanging higher, in front of the ceiling */
            for (int d = 0; d <= (int)(hi - lo) && !found; d++)
                for (int sg = 0; sg < (d ? 2 : 1) && !found; sg++) {
                    Box t = at(s, c0 + (sg ? -d : d), wt, wb);
                    t.y0 -= up; t.y1 -= up;
                    if (t.x0 < (int)lo + 1 || t.x1 > (int)hi - 1) continue;
                    int clear = 1;
                    for (int k = 0; k < n_keep && clear; k++) if (box_hit(box_grow(t, 2), keep[k])) clear = 0;
                    for (int k = 0; k < out->n && clear; k++) if (box_hit(box_grow(t, 2), out->p[k].box)) clear = 0;
                    if (clear) { b = t; found = 1; }
                }
        if (!found && s->priority < 9) continue; /* no room for it: left out */
        if (out->n < MAX_PLACED) out->p[out->n++] = (Placed){s->kind, b};
    }
}
