/* Bits around the room: the status line, the start menu, auto scale, a plain-text frame (port of the UI helpers in room_tui.py). */
#ifndef UI_H
#define UI_H
#include <stddef.h>
#include "room.h"

#define MIN_COLS 44
#define MIN_ROWS (SLOT_H / 2)
#define SCALE_CAP 3
#define ROAM_W 80
#define ROAM_H 62
#define MENU_W 144
#define MENU_H 84
#define MENU_N 3

extern const char *MENU_LABEL[MENU_N], *MENU_SCENE[MENU_N];

void overlay_text(char *out, size_t n, int paused, int help_on, const char *note, const char *flash);
void too_small(char *out, size_t n, int cols, int rows);
int auto_scale(int pw, int ph, int n); /* the biggest TUI scale (1..SCALE_CAP) that seats n Clawds and leaves floor to walk on */
/* room sizes (r list): 0 AUTO (auto_scale), then S M L XL = the biggest the room may be, in room pixels */
#define N_SIZES 5
extern const char *const SIZE_LABEL[N_SIZES], *const SIZE_KEY[N_SIZES];
extern const int SIZE_MAX_W[N_SIZES], SIZE_MAX_H[N_SIZES];
int size_scale(int pw, int ph, int size, int n, const Room *room); /* (room: AUTO asks the placer; NULL: the old estimate) */ /* grow, then scale: the smallest scale that keeps the room within the size */
void draw_menu(Canvas *cv, int pick, int solo, double t);
void draw_size_menu(Canvas *cv, const char *const *labels, int n, int pick); /* the ROOM SIZE list (r key) over the room */
Canvas render_menu(int cols, int rows, int pick, int solo, double t); /* the menu centred in the terminal canvas */
/* plain-text frame for tests: lit pixels as block characters, no colour codes; the caller frees the result */
char *dump_text(int cols, int rows, Room *room, double now, double t);
#endif
