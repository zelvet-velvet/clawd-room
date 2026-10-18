/* One Clawd (Session in room_tui.py). Python's None becomes a has_* flag next to the value. */
#ifndef SESSION_H
#define SESSION_H
#include "rng.h"

#define CHEER_FOR 2.5
#define SLEEP_AFTER 90.0

typedef struct {
    char sid[72];
    char name[48];         /* the folder the session runs in */
    char state[8];         /* "idle" "work" "think" "wait" "cheer" "sleep" */
    char tool[40];
    double created, last, state_t, hold_work_until;
    double left_at;        /* when its session ended; valid when has_left */
    int has_left, gone;
    Rng rng;               /* random.Random(crc32(sid)) */
    double seed;           /* 0..1, each Clawd's own rhythm */
    int has_pos;
    double px, py;         /* body centre x and body top y in room pixels */
    int has_goal; double gx, gy;
    int nroute; double rx[4], ry[4]; /* more goals after the current one (rope, stairs) */
    int moving, vclimb, leaving, returning;
    double walk_phase, walk_t, run;
    int face;              /* 1 right, -1 left */
    char stage[12];        /* "walk", "wait", "talk", "chase", "flee" or "" */
    char chat_with[72];    /* sid of the friend, "" if none */
    int chat_kind;
    char part[12];         /* "visitor" or "host" in a visit */
    double chat_until, catch_ok, flee_next, next_heart, next_plan, stay_until;
    int last_hf;
    int has_burst; double burst_t;
    double react_until, cup_until;
    int peek_on, peek_has_end; char peek_sid[72]; double peek_end; /* watching a busy friend */
    char emote[12];        /* "stretch", "dance", "swing", "slide", "jump", "wave" or "" */
    double emote_start, emote_end;
    int has_then; char then_kind[12]; double then_dur; /* an act to start when it gets where it is walking */
    int has_travel; char travel_peer[24]; int travel_edge;
    int guest;
    char origin[24], origin_sid[72]; int enter_side; /* a visitor: where it comes from and how it came in */
    int has_away; char away_peer[24]; int away_side; double away_since; int away_recalled;
    int move;              /* the trip is a move (m key): it carries its work to that room and lives there from now on */
    int pack; double pack_t0; /* moving: 1 to its desk, 2 packing it into a box, 3 carrying the box, 4 unpacking it (since pack_t0) */
} Session;

void session_init(Session *s, const char *sid, double t);
const char *session_shown(const Session *s, double now); /* the state to draw, after the little delays that keep it calm */
unsigned crc32_str(const char *s);
#endif
