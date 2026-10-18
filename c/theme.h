/* The three rooms in perspective (style A): background, workspaces, gates (room.h: theme_gate), the room's sign, roles, accessories, playground spots. */
#ifndef THEME_H
#define THEME_H
#include "canvas.h"
#include "persp.h"

typedef struct { int x, y, bar; } SwingSpot;
typedef struct { int base[2], top[2], bottom[2]; } SlideSpot;
typedef struct { int has, no_swing, no_slide; SwingSpot swing; SlideSpot slide; } Spots; /* (no_*: left out, no room for it) */

typedef struct { const char *kind; double dur; } Emote;

typedef struct {
    const char *name;
    double horizon;
    int nkinds; int kinds[8];   /* visit kinds: 0 chat 1 wave 2 hop 3 high-five 4 dance 5 tag 6 order coffee */
    int nemotes; Emote emotes[8]; /* solo acts and how long they last */
    Rgb wall, wall2, floor, floor2, base, desk, desk_top, counter, counter_top;
} Theme;

const Theme *theme_get(const char *name); /* unknown name = office */
const Theme *theme_pick(const char *room_name, const char *theme_name); /* the theme named, or guessed from the room name */
/* paints the background (cached per theme and size, copied after the first time) */
void theme_background(const Theme *th, Canvas *cv, int banner);
/* a workspace whose front edge is (cx, by); variant (a hash of the session) picks the device / drink / park thing */
void theme_furniture(const Theme *th, Canvas *cv, int cx, int by, int on, Rgb color, double t, const char *role, unsigned variant);
#include "place.h"
#include "sprites.h"
void theme_set_props(const SpriteFile *props); /* sprites/props.txt: wall items drawn from it when it has them */
void theme_set_time(double t);                 /* the drawing time (animated props) */
void theme_set_rooms_dir(const char *dir);     /* rooms/: each room's wall items as a text file (read again when changed) */
int theme_wall_specs(const Theme *th, const char *title, WallSpec *out, int max); /* the room's wall items, as data */
void theme_wall_item(const Theme *th, Canvas *c, const Placed *pl, const char *title, int has_up); /* one, drawn where the placer put it */
int theme_bushes(const Theme *th, const View *v, int fence_sides, int (*out)[4], int max); /* the park's bushes: x, y, size, right side */
Box theme_bush_box(const int *bush);
void theme_bush(Canvas *c, const int *bush);
void theme_play(const Theme *th, Canvas *c, int which, int dx);   /* the park's swing set (0) and slide (1), moved dx across */
double theme_play_ground(const View *v, int which);                /* ... how far forward they stand */
Box theme_play_box(const Theme *th, int w, int h, int which);      /* ... what they cover */
Box theme_footprint(const Theme *th, int w, int h, const char *role); /* a workspace and its seated Clawd, relative to (cx, by) */
void theme_accessory(const Theme *th, Canvas *cv, int x, int y, const char *role);
/* roles for the ordered sessions: role[i] is "" , "barista" or "customer" */
const char *theme_role(const Theme *th, int index);
Spots theme_spots(const Theme *th, int w, int h, const int *dx);
#endif
