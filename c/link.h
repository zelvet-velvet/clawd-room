/* The link between clawd-room windows on the same machine (roomlink.py): presence files and inbox files in
 * ~/.clawd-room/link/. Same file formats as the Python version, so C and Python rooms see each other. */
#ifndef LINK_H
#define LINK_H
#include "room.h"

#define MAX_LINK_PEERS 16
#define LINK_ALIVE_FOR 4.0
#define LINK_BEAT_EVERY 1.0
#define LINK_SCAN_EVERY 0.5

typedef struct {
    char id[40], name[32], theme[32];
    int cols, rows;
    int has_band; int band[2];
    int has_grid; double grid[7];
    char homes[MAX_CLAIMS][72]; int nhomes; /* the sessions that moved into that room */
    char shows[MAX_SHOWS][72]; int nshows;  /* the sessions it shows now */
} LinkPeer;

typedef struct {
    char name[32], id[40], dir[512], theme[32];
    LinkPeer peers[MAX_LINK_PEERS]; int npeers; /* the other rooms that are alive, sorted by id */
    long pos; char *buf; size_t buflen;         /* how much of our inbox we have read */
    double last_beat, last_scan;
    int has_band; int band[2];
    int has_grid; double grid[7];
    char homes[MAX_CLAIMS][72]; int nhomes; /* ours, said in the heartbeat */
    char shows[MAX_SHOWS][72]; int nshows;  /* ... and the sessions we show */
} Link;

typedef struct { int kind; LinkPeer peer; char type[12]; Msg msg; } LinkEvent; /* kind: 0 joined, 1 left, 2 msg (type, msg) */

void link_clean_name(const char *in, char *out, size_t n);
void link_init(Link *l, const char *name, const char *dir_or_null); /* dir: $CLAWD_LINK_DIR or ~/.clawd-room/link */
void link_start(Link *l);
void link_close(Link *l);
/* one call per frame: keeps our presence fresh, notices rooms coming and going; returns the number of events */
int link_update(Link *l, double now, int cols, int rows, LinkEvent *out, int max);
/* a "walk" or "recall" message (or hello/bye/ping/pong with no data); 0 if that room is gone */
int link_peer_file(const Link *l, const char *id); /* that room's presence file is still there (it has not closed, maybe just slow) */
int link_send_msg(Link *l, const char *to, const Msg *m);
int link_send_num(Link *l, const char *to, const char *type, const char *key, double value);
int link_send_simple(Link *l, const char *to, const char *type, const char *key, const char *value);
#endif
