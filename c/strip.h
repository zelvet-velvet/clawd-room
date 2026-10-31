/* The room in a pane too small for it (under MIN_COLS x MIN_ROWS), picked 2026-11-01 (mockups E + I, Tab as in 36):
 * from STRIP_BAND_ROWS rows a band (wall, baseboard, floor) with a Clawd at a laptop per session, the screen in the session's
 * state colour; below that a face per session on a tile in the state's colour. Tab picks a Clawd: yellow corners, the rest
 * dimmed, its name beside it, until Esc. When the sessions do not all fit, the band shows a window of them that follows the
 * picked one, and a Clawd out of view that starts waiting is picked (and so brought into view) by itself. */
#ifndef STRIP_H
#define STRIP_H
#include "canvas.h"
#include "room.h"

#define STRIP_BAND_ROWS 4 /* terminal rows from which the band shows (8 room pixels: a whole Clawd); fewer: faces */
#define STRIP_SLOT 18     /* room pixels per Clawd in the band (the Clawd 14, its laptop over its right arm, a gap) */
#define STRIP_TILE 15     /* ... per face tile */
#define STRIP_MAX 64      /* sessions the strip keeps track of */

typedef struct {
    int first;               /* the first session in view (when they do not all fit) */
    char pick[72];           /* the picked session (Tab), or "" */
    char waited[STRIP_MAX][72]; int nwaited; /* sessions already brought into view for their wait (once per wait) */
} Strip;

int strip_wanted(int cols, int rows);                  /* the pane is too small for the room */
Canvas strip_render(Strip *st, Room *room, int cols, int rows, double t, double now);
const char *strip_step(Strip *st, Room *room, double now, int dir); /* Tab (dir 1) / back (-1): the next Clawd's name, NULL if none */
void strip_unpick(Strip *st);                          /* Esc */
#endif
