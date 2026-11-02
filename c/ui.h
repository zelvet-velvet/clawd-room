/* Bits around the room: the status line, the start menu, auto scale, a plain-text frame (port of the UI helpers in room_tui.py). */
#ifndef UI_H
#define UI_H
#include <stddef.h>
#include "room.h"

#define MIN_COLS 44
#define MIN_ROWS (SLOT_H / 2)
#define ROOM_MAX_TALL 0.75 /* the room is at most this tall for its width (picked 2026-11-01 for panes, then windows too): a tall
                            * window or pane shows it in its middle, dark above and below, instead of a long floor */
#define ROOM_DARK RGB(12, 12, 16)
int room_rows(int cols, int rows); /* the terminal rows the room uses (the rest stays dark) */
#define ROAM_W 80
#define ROAM_H 62
#define MENU_W 144
#define MENU_H 84
#define MENU_N 3

extern const char *MENU_LABEL[MENU_N], *MENU_SCENE[MENU_N];

void overlay_text(char *out, size_t n, int paused, int help_on, const char *note, const char *flash);
/* the bar along the bottom of the room, in its pixel font (the terminal's own text is tiny at the fixed font): pieces of
 * text, each in its colour, wrapped at spaces; a piece with `line` set starts a new line; centred, on a dark band */
typedef struct { const char *text; Rgb color; int line; } BarPiece;
void draw_bar(Canvas *cv, const BarPiece *pieces, int n);
void draw_menu(Canvas *cv, int pick, int solo, double t);
void draw_list_menu(Canvas *cv, const char *title, const char *const *labels, int n, int pick); /* a list over the room (ROOM SIZE, SEND A CLAWD) */
Canvas render_menu(int cols, int rows, int pick, int solo, double t); /* the menu centred in the terminal canvas */
/* plain-text frame for tests: lit pixels as block characters, no colour codes; the caller frees the result */
char *dump_text(int cols, int rows, Room *room, double now, double t);
#endif
