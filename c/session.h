/* One Clawd (the fields of Session in room_tui.py; more arrive with the state machine). */
#ifndef SESSION_H
#define SESSION_H

typedef struct {
    char sid[40];
    double seed;           /* 0..1, each Clawd's own rhythm */
    double state_t;        /* when the current state began */
    int moving, vclimb;
    double walk_phase;
    int face;              /* 1 right, -1 left */
    char stage[12];        /* "walk", "wait", "talk", "chase", "flee" or "" */
    char chat_with[40];    /* sid of the friend, "" if none */
    int chat_kind;
    char part[12];         /* "visitor" or "host" in a visit */
    double react_until;
    int peeking;           /* watching a busy friend */
    char emote[12];        /* "stretch", "dance", "swing", "slide", "jump", "wave" or "" */
    double emote_start;
} Session;

void session_init(Session *s, const char *sid);
#endif
