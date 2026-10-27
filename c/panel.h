/* The settings panel beside the room (key `.`): a list of settings, w / s pick one, a / d change it; every change is written to
 * settings.conf at once and the room reads it again (app.c live_settings). */
#ifndef PANEL_H
#define PANEL_H
#include "canvas.h"

#define PANEL_W 86      /* canvas pixels; the room keeps the rest (a window under PANEL_MIN_COLS gets the panel over the room) */
#define PANEL_MIN_COLS 130

typedef struct { int open, sel; } Panel;

int panel_key(Panel *p, char k);              /* w / s / a / d / Esc / `.` while open: 1 when the key was taken (a change is written to the file) */
void draw_panel(Canvas *cv, int x, const Panel *p); /* the panel with its left edge at x, full height */
#endif
