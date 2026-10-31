/* The room: who is in it, how they move and what they do (Room in room_tui.py), and how it is drawn. */
#ifndef ROOM_H
#define ROOM_H
#include <stddef.h>
#include "canvas.h"
#include "session.h"
#include "sprites.h"
#include "theme.h"
#include "persp.h"
#include "anim.h"

enum { SIDE_LEFT, SIDE_RIGHT, SIDE_UP, SIDE_DOWN, SIDES };
#define PICK_IN 0.25    /* the picked Clawd (Tab): seconds the corners and the dimming take to appear, */
#define PICK_OUT 0.4    /* ... to fade out again, */
#define PICK_GLIDE 0.2  /* ... to glide to the next Clawd, */
#define PICK_HOLD 1e9   /* ... and how long they stay after the last Tab: until Esc (room_unpick) */
#define PICK_DIM 0.60   /* how much the room outside the spotlight is darkened (60 in 100) */
#define PICK_R 20       /* the spotlight: a circle of this radius (room pixels; they are square on screen), soft at the edge */
extern const char *SIDE_NAME[SIDES];     /* "left" "right" "up" "down" */
int side_index(const char *name);        /* -1 if not a side */
#define OPPOSITE(side) ((side) ^ 1) /* left<->right, up<->down */

#define MAX_SESS 64
#define MAX_GUESTS 4
#define MAX_PARTS 256
#define MAX_OUT 32
#define MAX_CLAIMS 32
#define MAX_SHOWS 64   /* sessions a linked room says it shows */
#define MAX_HELD 256   /* ... all the other rooms' together */

typedef struct { double x, y, vx, vy, life; Rgb c; int heart; } Particle;
typedef struct { char name[24], side[8]; } Peer; /* side: "left" "right" "up" "down" or "" */
typedef struct { int side; char theme[12], id[24]; } Neighbor;

/* a message between windows ("walk" a Clawd over, or "recall" it) */
typedef struct {
    char peer[24];          /* sent: to, received: from */
    int walk;               /* 1 "walk", 0 "recall" */
    char sid[72], name[40]; /* name only in walk messages */
    int side;               /* the edge it enters by (walk) */
    int home;               /* walk: one of ours coming back */
    int move;               /* walk: it moves in (lives there from now on) */
    int has_y; double y;
} Msg;

/* where the workspaces are and the strip of floor where Clawds may walk, in perspective (style A) */
typedef struct {
    int w, h, banner, slot_w, floor_y, ymin, ymax, rope_x, stairs_x, rope_end;
    View view;                 /* the perspective (persp.h) */
    double gate_in[SIDES][2];  /* for each gate: where a Clawd steps on and off it (feet, room pixels) ... */
    double gate_out[SIDES][2]; /* ... and where it goes through (the door, the platform's door, the park's stair tower, down the well) */
    int gate_has_mid[SIDES]; double gate_mid[SIDES][2]; /* a point on the way between them (the top of the stairs) */
    int shown, more, has_more_pos, more_pos[2];
    int home[MAX_SESS][2];   /* by index in room->s (only the first `shown`) */
    double door_y[2];
    Spots spots;
    int has_up, fence_sides;   /* stairs up; the park's sides with a building (1 left, 2 right) */
    WallPlan wall;             /* the wall items where the placer put them */
    Box keep[48]; int keep_mask[48], nkeep; /* what must stay clear: the gates, the playground, posts, bushes (mask 1 floor, 2 wall) */
    int bush[8][4], nbush;     /* the park's bushes that fit: x, y, size, right side */
    Box play[2]; int nplay, play_dx[2], play_on[2]; /* the park's swing set and slide, and how far the placer moved them across */
} Geo;

typedef struct {
    Session pool[MAX_SESS]; int npool;     /* the sessions, in the order they appeared */
    Session gpool[MAX_GUESTS]; int ngp;    /* visitors from other windows (not yet placed ones included) */
    Session *s[MAX_SESS]; int n;           /* the ones this room shows, ordered by (created, sid): rebuilt each frame */
    Session *guests[MAX_GUESTS]; int ng;   /* the placed visitors */
    Particle parts[MAX_PARTS]; int np;
    const Theme *theme;
    char title[48];
    Peer peers[8]; int npeers;
    Neighbor nb[SIDES]; int nnb;     /* windows close enough to have a gate, in the order they were given */
    int has_link[SIDES]; double link_pos[SIDES];
    int has_gate[SIDES]; double gate_scale[SIDES];
    int scale;               /* TUI scale: room pixels are scale x scale terminal cells (auto shape only) */
    char ratio[8];           /* "auto" (the room follows the window) or a fixed shape (--ratio, kept for the tests) */
    int size_w, size_h;      /* size of the room in pixels as last drawn */
    int view_x, view_y, view_k; /* where the room sits in the terminal canvas: x offset, y offset, scale */
    int cam_zoom;            /* camera zoom over the laid-out room: 0 or 1 = whole room, 2..CAM_MAX_ZOOM = closer */
    int cam_set; double cam_x, cam_y; /* camera centre in room pixels (cam_set 0: centre of the room) */
    int cam_vw, cam_vh;      /* room pixels the camera shows, as last drawn (for the pan step) */
    int gate_seen[SIDES]; double gate_t0[SIDES]; /* when each gate appeared (it animates in: draw.c GATE_ANIM) */
    int drawn;               /* a frame has been drawn (gates there from the start do not animate) */
    double door_open[SIDES], door_t; /* how far each door leaf is open (0..1, opens while a Clawd passes) and when it was last moved */
    Animator anim;           /* eased values: gates appearing ("gate0".."gate3"), doors swinging ("door0", "door1") */
    int has_gate_sync; double gate_sync_t; /* when the windows stopped moving + 1 s, in draw time: both rooms start the animation then */
#define GATE_SYNC_WAIT 1.0 /* s after both rooms were ready (a heartbeat is written at once and read within 0.5 s) */
#define GATE_SYNC_ALONE 3.0 /* s after we were ready, while the other room has not said it is (an older room never does) */
    int has_gate_start[SIDES]; double gate_start[SIDES]; /* agreed with the room behind the gate (app.c), in draw time: overrides gate_sync_t */
    char pick_sid[72]; double pick_until;  /* the picked Clawd (Tab) stands out until this draw time, then fades (draw.c draw_pick) */
    int pick_seen; double pick_cx, pick_cy; /* the corners are on screen, centred here (room pixels, glide included): the next pick glides from there */
    /* movement state */
    int has_last_now; double last_now;
    int has_geo_key; int geo_w, geo_h;
    Msg incoming[MAX_OUT]; int nin;
    Msg outbox[MAX_OUT]; int nout;
    char gone_peers[4][24]; int ngone;
    int freeze;              /* tests: draw without moving anybody */
    int debug_overlay;       /* the debug overlay (key o): item boxes, anchors, gate points, routes */
    int split;               /* linked rooms share the sessions: this room shows the ones whose crc32 picks it */
    int n_owner_ids; char owner_ids[17][40], owner_me[40];
    char mine[MAX_CLAIMS][72]; int nmine; /* the sessions that moved into this room (m key): it shows them while it runs */
    char claim_sid[MAX_CLAIMS * 4][72], claim_room[MAX_CLAIMS * 4][40]; int nclaim; /* ... and the other rooms' (from their heartbeats) */
    char held_sid[MAX_HELD][72], held_room[MAX_HELD][40]; int nheld; /* the sessions the other rooms show now (from their heartbeats) */
    int has_geo;             /* geo below is from the last frame (the g key needs it) */
    Geo geo;
} Room;

#define SLOT_H 40
#define SLOT_MIN_W 24     /* a workspace on the floor, in room pixels across */
#define SLOT_MAX_W 40
#define ROW_PITCH 15       /* room pixels between the rows of workspaces */
#define ROW_MARGIN 3       /* room pixels kept free at the floor's sides of each row */
#define WALK_MIN 16        /* room pixels of floor kept in front of the last row to walk on */
#define TREE_X 0.9         /* the park's big tree, as a part of the width (at the back right corner) */
#define PARK_STAIRS_FOOT 0.58 /* the park's stone stairs: their foot and top along the back (parts of its width), */
#define PARK_STAIRS_TOP 0.68
#define PARK_TERRACE_UP 0.45  /* ... the terrace this far up from the back of the lawn towards the top of the picture, */
#define PARK_DOOR_W 10        /* ... and behind it a tall building whose double door, this big, is the way up, */
#define PARK_DOOR_H 14
#define PARK_DOOR_DX 10       /* ... this far past the top of the steps */
#define STAIRS_FOOT 0.2     /* the stairs on the back wall start this far along it ... */
#define STAIRS_TOP 0.42     /* ... and reach the platform here */
#define ARCH_W 12           /* the opening up: width, height (room pixels), and how far in from the platform's right end */
#define ARCH_H 16
#define ARCH_IN 3
#define DOOR_HALF_DEPTH 0.05 /* a side door spans this part of its depth to each side (a normal door's width) */
#define DOOR_H_PX 22         /* a side door's height on screen (room pixels) */
#define PLATFORM_UP 0.4     /* the platform (a half floor) is this part of the way up the back wall (room above it for the archway) */
#define PLAY_Y 0.62        /* the park's swing and slide stand this far down the floor */
#define PLAY_SWING_X 0.23  /* ... the swing this far across, the slide there */
#define PLAY_SLIDE_X 0.6
#define WELL_FRONT 5         /* the stair well (indoors and the park): at the front left, its front edge this far above the front edge (room pixels), */
#define WELL_SIDE 20         /* ... its middle this far in from the left-hand wall, */
#define WELL_DEEP 5         /* ... this deep on screen, */
#define WELL_HALF_W 0.07    /* ... this wide (world units each side) */
#define CAM_MAX_ZOOM 4     /* [ ] camera zoom steps over the room */
#define PER_ROW 5
#define BANNER_H 8
#define PACK_T 1.4         /* moving (m key): seconds the workspace takes to shrink into a box, */
#define UNPACK_T 1.2       /* ... and to come out of it at the new desk */
#define SEAT_DY -12 /* a Clawd sits behind its desk / table: its body top this far above the furniture's front edge */


int room_auto_scale(int pw, int ph, const Room *room); /* AUTO room size: the biggest scale at which the placer seats everyone */
void layout_slots(int w, int h, int n, int *banner, int *per_row, int *slot_w, int *rows_shown, int *ys);
void geo_init(Geo *g, const Room *room, int w, int h);
/* a gate in this theme (a door, the park's building with a door; stairs; a stair well); p 0..1 how far it has appeared */
void theme_gate(const Theme *th, Canvas *cv, const Geo *g, int side, double p, double open, int part); /* part -1: all of it */
int theme_keep_clear(const Theme *th, const Geo *g, int side, Box *out, int *mask, int max); /* what must stay clear around a gate (mask 1 floor, 2 wall) */
void theme_stair_rail(const Theme *th, Canvas *c, const Geo *g, double p); /* the stairs' railing (drawn in among the Clawds) */
void stairs_geometry(const View *v, double *x0, double *x1, double *yl); /* the stairs on the back wall: foot x, top x, landing y */
int stairs_feet_y(const View *v, double x); /* the row a Clawd's feet are on at column x on the stairs (-1 off them) */
int park_door_cx(int x1); /* the park building's door, from the top of the stairs */
void well_place(const View *v, double *fy, double *wx, double *x0, double *x1); /* the stair well: front edge y, middle x, left and right x (rim included) */
int geo_home(const Geo *g, const Room *room, const Session *s, int *cx, int *by); /* 1 if it has a desk */
const char *geo_role(const Geo *g, const Room *room, const Session *s);

void room_init(Room *r);
void room_order(Room *r);                            /* fills r->s (ordered) and r->guests (placed visitors) */
void room_apply(Room *r, double t, const char *sid, const char *ev, const char *cwd, const char *tool);
void room_emote(Room *r, const char *sid, const char *kind, double now, double dur);
void room_prune(Room *r, double now);
void room_unpick(Room *r, double t);                /* Esc: the picked Clawd stops standing out (fades out now, not after the hold) */
void room_pick(Room *r, const char *sid, double t); /* Tab: that Clawd stands out (yellow corners close in, the rest of the room dims); t is draw time */
void room_step(Room *r, const Geo *geo, double now); /* move everybody a little (once per frame) */
Session *room_add_guest(Room *r, const char *key, double now);
void room_receive(Room *r, const Msg *m);
void room_peer_gone(Room *r, const char *peer);
const char *room_send_one(Room *r, const Geo *geo, double now, char *buf, size_t n); /* send an idle Clawd through the first gate */
const char *room_send_to(Room *r, const Geo *geo, double now, int side, char *buf, size_t n); /* the g key: ... through the gate on that side */
int room_owner_of(const Room *r, const char *sid); /* linked rooms: which of owner_ids shows the session (no claim) */
const char *room_move_to(Room *r, const Geo *geo, double now, const char *sid, int side, char *buf, size_t n); /* the m key: that Clawd moves to the room on that side */
const char *room_recall(Room *r, double now, char *buf, size_t n); /* the b key: call our Clawds home, or send visitors home */

const SpriteFile *draw_frames(void);
void draw_set_sprites(const SpriteFile *frames, const SpriteFile *items); /* the editable files the drawing prefers */
/* draws the room on cv (moving everybody first unless room->freeze); returns the "+N MORE" note in `note` */
void draw_room(Canvas *cv, Room *room, double now, double t, char *note, size_t note_n);
/* the canvas for the whole terminal (cols x rows cells) with the room in it */
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
#endif
