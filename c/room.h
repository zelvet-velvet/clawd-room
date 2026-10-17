/* The room: who is in it, how they move and what they do (Room in room_tui.py), and how it is drawn. */
#ifndef ROOM_H
#define ROOM_H
#include <stddef.h>
#include "canvas.h"
#include "session.h"
#include "sprites.h"
#include "theme.h"
#include "persp.h"

enum { SIDE_LEFT, SIDE_RIGHT, SIDE_UP, SIDE_DOWN, SIDES };
extern const char *SIDE_NAME[SIDES];     /* "left" "right" "up" "down" */
int side_index(const char *name);        /* -1 if not a side */
#define OPPOSITE(side) ((side) ^ 1) /* left<->right, up<->down */

#define MAX_SESS 64
#define MAX_GUESTS 4
#define MAX_PARTS 256
#define MAX_OUT 32

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
    int has_y; double y;
} Msg;

/* where the workspaces are and the strip of floor where Clawds may walk, in perspective (style A) */
typedef struct {
    int w, h, banner, slot_w, floor_y, ymin, ymax, rope_x, stairs_x, rope_end;
    View view;                 /* the perspective (persp.h) */
    double gate_in[SIDES][2];  /* for each gate: where a Clawd steps on and off it (feet, room pixels) ... */
    double gate_out[SIDES][2]; /* ... and where it goes through (the door, the top of the stairs, the treehouse, down the well) */
    int shown, more, has_more_pos, more_pos[2];
    int home[MAX_SESS][2];   /* by index in room->s (only the first `shown`) */
    double door_y[2];
    Spots spots;
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
    int has_gate_sync; double gate_sync_t; /* when the windows stopped moving + 1 s, in draw time: both rooms start the animation then */
    /* movement state */
    int has_last_now; double last_now;
    int has_geo_key; int geo_w, geo_h;
    Msg incoming[MAX_OUT]; int nin;
    Msg outbox[MAX_OUT]; int nout;
    char gone_peers[4][24]; int ngone;
    int freeze;              /* tests: draw without moving anybody */
    int split;               /* linked rooms share the sessions: this room shows the ones whose crc32 picks it */
    int n_owner_ids; char owner_ids[17][40], owner_me[40];
    int has_geo;             /* geo below is from the last frame (the g key needs it) */
    Geo geo;
} Room;

#define SLOT_H 40
#define SLOT_MIN_W 24     /* a workspace on the floor, in room pixels across */
#define SLOT_MAX_W 40
#define ROW_PITCH 15       /* room pixels between the rows of workspaces */
#define ROW_MARGIN 3       /* room pixels kept free at the floor's sides of each row */
#define WALK_MIN 16        /* room pixels of floor kept in front of the last row to walk on */
#define TREE_X 0.76        /* the park's treehouse tree, as a part of the width */
#define TREEHOUSE_UP 18    /* how far above the back of the floor the treehouse door is */
#define STAIRS_FOOT 0.2     /* the stairs on the back wall start this far along it ... */
#define STAIRS_TOP 0.45     /* ... and reach the landing under the ceiling here */
#define WELL_X 0.24        /* the stair well (indoors), as a part of the width */
#define WELL_H 0.3         /* its depth on screen, as a part of the floor's height */
#define CAM_MAX_ZOOM 4     /* [ ] camera zoom steps over the room */
#define PER_ROW 5
#define BANNER_H 8
#define SEAT_DY -12 /* a Clawd sits behind its desk / table: its body top this far above the furniture's front edge */


void layout_slots(int w, int h, int n, int *banner, int *per_row, int *slot_w, int *rows_shown, int *ys);
void geo_init(Geo *g, const Room *room, int w, int h);
/* a gate in this theme (door / fence gate, stairs / treehouse, stair well / stone steps); p 0..1 how far it has appeared */
void theme_gate(const Theme *th, Canvas *cv, const Geo *g, int side, double p, double open);
void stairs_geometry(const View *v, double *x0, double *x1, double *yl); /* the stairs on the back wall: foot x, top x, landing y */
int geo_home(const Geo *g, const Room *room, const Session *s, int *cx, int *by); /* 1 if it has a desk */
const char *geo_role(const Geo *g, const Room *room, const Session *s);

void room_init(Room *r);
void room_order(Room *r);                            /* fills r->s (ordered) and r->guests (placed visitors) */
void room_apply(Room *r, double t, const char *sid, const char *ev, const char *cwd, const char *tool);
void room_prune(Room *r, double now);
void room_step(Room *r, const Geo *geo, double now); /* move everybody a little (once per frame) */
Session *room_add_guest(Room *r, const char *key, double now);
void room_receive(Room *r, const Msg *m);
void room_peer_gone(Room *r, const char *peer);
const char *room_send_one(Room *r, const Geo *geo, double now, char *buf, size_t n); /* the g key */
const char *room_recall(Room *r, double now, char *buf, size_t n); /* the b key: call our Clawds home, or send visitors home */

const SpriteFile *draw_frames(void);
void draw_set_sprites(const SpriteFile *frames, const SpriteFile *items); /* the editable files the drawing prefers */
/* draws the room on cv (moving everybody first unless room->freeze); returns the "+N MORE" note in `note` */
void draw_room(Canvas *cv, Room *room, double now, double t, char *note, size_t note_n);
/* the canvas for the whole terminal (cols x rows cells) with the room in it */
Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
#endif
