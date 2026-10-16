/* One Clawd (the fields of Session in room_tui.py; more arrive with the state machine). */
#ifndef SESSION_H
#define SESSION_H

#define CHEER_FOR 2.5
#define SLEEP_AFTER 90.0

typedef struct {
    char sid[40];
    char name[48];         /* the folder the session runs in */
    char state[8];         /* "idle" "work" "think" "wait" "cheer" "sleep" */
    char tool[40];
    double created, last, state_t, hold_work_until;
    double left_at;        /* when its session ended; valid when has_left */
    int has_left;
    double seed;           /* 0..1, each Clawd's own rhythm */
    double px, py;         /* body centre x and body top y in room pixels */
    int moving, vclimb;
    double walk_phase;
    int face;              /* 1 right, -1 left */
    char stage[12];        /* "walk", "wait", "talk", "chase", "flee" or "" */
    char chat_with[40];    /* sid of the friend, "" if none */
    int chat_kind;
    char part[12];         /* "visitor" or "host" in a visit */
    double react_until, cup_until;
    int peeking;           /* watching a busy friend */
    char emote[12];        /* "stretch", "dance", "swing", "slide", "jump", "wave" or "" */
    double emote_start;
    int guest, away;
} Session;

void session_init(Session *s, const char *sid);
const char *session_shown(const Session *s, double now); /* the state to draw, after the little delays that keep it calm */
unsigned crc32_str(const char *s);
#endif
