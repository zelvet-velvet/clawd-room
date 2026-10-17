/* The three rooms in perspective (style A): background, workspaces, gates (room.h: theme_gate), the room's sign, roles, accessories, playground spots. */
#ifndef THEME_H
#define THEME_H
#include "canvas.h"

typedef struct { int x, y, bar; } SwingSpot;
typedef struct { int base[2], top[2], bottom[2]; } SlideSpot;
typedef struct { int has; SwingSpot swing; SlideSpot slide; } Spots;

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
void theme_sign(const Theme *th, Canvas *cv, const char *title, int has_up, int fence_sides); /* the room's name as a sign in the room (and the coffee shop's menu when there are no stairs to hang it on) */
void theme_accessory(const Theme *th, Canvas *cv, int x, int y, const char *role);
/* roles for the ordered sessions: role[i] is "" , "barista" or "customer" */
const char *theme_role(const Theme *th, int index);
Spots theme_spots(const Theme *th, int w, int h);
#endif
