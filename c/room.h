/* What the drawing code needs to know about a room (the Room fields of room_tui.py that draw_room reads). */
#ifndef ROOM_H
#define ROOM_H
#include <stddef.h>
#include "canvas.h"
#include "session.h"
#include "sprites.h"
#include "theme.h"

enum { SIDE_LEFT, SIDE_RIGHT, SIDE_UP, SIDE_DOWN, SIDES };

typedef struct { double x, y, vx, vy, life; Rgb c; int heart; } Particle;
typedef struct { char name[24], side[8]; } Peer; /* side: "left" "right" "up" "down" or "" */
typedef struct { int side; char theme[12]; } Neighbor;

typedef struct {
    Session *s; int n;       /* the ones this room owns, in drawing order (created, then sid) */
    Session *guests; int ng; /* visitors from other windows that are placed */
    Particle *parts; int np;
    const Theme *theme;
    char title[48];
    Peer peers[8]; int npeers;
    Neighbor nb[SIDES]; int nnb;     /* windows close enough to have a gate, in the order they were given */
    int has_link[SIDES]; double link_pos[SIDES];
    int has_gate[SIDES]; double gate_scale[SIDES];
    int scale;               /* TUI scale: room pixels are scale x scale terminal cells (auto shape only) */
    char ratio[8];           /* "auto" or a key of RATIOS */
    int size_w, size_h;      /* size of the room in pixels as last drawn */
} Room;

#define SLOT_H 40
#define SLOT_MIN_W 30
#define SLOT_MAX_W 62
#define PER_ROW 5
#define BANNER_H 8
#define SEAT_DY 7

void draw_set_sprites(const SpriteFile *frames, const SpriteFile *items); /* the editable files the drawing prefers */
/* draws the room on cv; returns the "+N MORE" note in `note` (empty when everything fit) */
void draw_room(Canvas *cv, Room *room, double now, double t, char *note, size_t note_n);
/* the canvas for the whole terminal (cols x rows cells) with the room in it */
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
#endif
