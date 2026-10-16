/* Canvas -> terminal: half-block cells, and the ANSI string that updates the screen from the previous frame
 * (port of cells_from_canvas / diff_frame in room_tui.py). */
#ifndef TERM_H
#define TERM_H
#include <stddef.h>
#include <stdint.h>
#include "canvas.h"

typedef struct { uint32_t ch; Rgb fg, bg; } Cell;
typedef struct { int cols, rows; Cell *c; } Cells;

Cells cells_from_canvas(const Canvas *cv, const char *overlay); /* overlay: plain text for the last row, or NULL/"" */
void cells_free(Cells *c);
/* appends the update from prev (NULL = draw everything) to a malloc'd string; the caller frees it */
char *diff_frame(const Cells *cur, const Cells *prev, size_t *len);
#endif
