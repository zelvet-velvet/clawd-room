/* The construct: a gate built from its own pieces (the gate theme `construct`, Rooms and Art "Gates appear"). The gate is drawn whole once;
 * what it painted is cut into real parts (a board, a post, a length of rail, the handle) that fly in from the air in a pale blue glow and
 * click into place, inside a blue outline of what is to come. */
#ifndef CONSTRUCT_H
#define CONSTRUCT_H
#include "canvas.h"

/* cv holds the room as it was before the gate; after is the same with the whole gate drawn (same size as cv); t 0..1 is the build.
 * Draws the build into cv. */
void construct_draw(Canvas *cv, const Rgb *after, double t);
#endif
