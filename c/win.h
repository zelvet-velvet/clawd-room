/* Desktop helpers (WSL + Windows Terminal through PowerShell, or macOS + iTerm2 through osascript): where the clawd-room windows are,
 * changing the font size and the window size, and where the gates between neighbouring windows go. The two PowerShell
 * scripts are in ps_scripts.h. */
#ifndef WIN_H
#define WIN_H
#include <sys/types.h>
#include "room.h"
#include "link.h"

#define WIN_PREFIX "clawd-room "
#define NEAR_PX 30
#define MAX_OVERLAP_PX 30
#define HYSTERESIS_PX 15
#define DOOR_H 14
#define HATCH_W 21
#define DOOR_CLEAR 10 /* room pixels a ladder keeps from the windows' side edges (a door is 8 wide), when the overlap allows it */

int win_available(void); /* powershell.exe on the PATH, or running inside iTerm2 on macOS */
const char *win_platform(void); /* "Windows Terminal" or "iTerm2" */

/* ---- where the windows are -------------------------------------------------------------------- */
/* a window, or (pane) one pane of a Windows Terminal window split into panes: rid is its UI Automation runtime id, owner
 * the index of its window; a split window's title is "-" unless a clawd-room pane has the focus */
typedef struct { char title[96]; int rect[4], area[4], dpi; int pane, npanes, owner, focus, clawd; char rid[40]; } WinInfo;
#define WATCH_MAX 48
#define WATCH_PEERS 16
typedef struct {
    pid_t pid; int fd;
    char line[2048]; size_t n;
    WinInfo cur[WATCH_MAX]; int ncur;
    WinInfo pend[WATCH_MAX]; int npend;
    double updated;
    /* which pane is whose (watcher_resolve): ours learned from the focus, the other rooms' from their heartbeats */
    char my_id[80], my_rid[40], cand_rid[40]; int cand_n; double resolved;
    char peer_id[WATCH_PEERS][40], peer_rid[WATCH_PEERS][40]; int npeers;
} Watcher;

int watcher_start(Watcher *w);
void watcher_poll(Watcher *w); /* reads what the helper has printed (non-blocking) */
void watcher_stop(Watcher *w);
const int *watcher_rect(const Watcher *w, const char *room_id); /* outer rectangle l,t,r,b or NULL (not found, stale) */
const int *watcher_area(const Watcher *w, const char *room_id); /* the terminal part of the window or NULL */
int watcher_dpi(const Watcher *w, const char *room_id);        /* 96 if unknown */
/* feed one line of the helper's output (for tests) */
void watcher_feed(Watcher *w, const char *line, double now);
/* after each poll: which pane of a split window is ours (the one with the focus while the window has our title, seen in
 * two lists in a row; else the only Clawd Room pane in any split window nobody else claims), and the other rooms' panes from
 * their heartbeats */
void watcher_resolve(Watcher *w, const char *my_id, const Link *link_or_null);
int watcher_in_pane(const Watcher *w, const char *room_id); /* 1 in a pane of a split window, 0 a whole window, -1 unknown */

/* "left" "right" "up" "down" side index if `other` is next to `mine` and close enough, else -1 */
int neighbor_side(const int *mine, const int *other, int near_px, int slack);

/* ---- changing the font and the window --------------------------------------------------------- */
typedef struct { pid_t pid; int fd; } Tool;
int tool_start(Tool *t);
void tool_resize(Tool *t, const char *title, int w, int h);
void tool_zoom(Tool *t, const char *title, int step, int only_if_focused, int live);
void tool_reset_zoom(Tool *t, const char *title);
#define SCREEN_W_PERCENT 50 /* --size screen: a macOS quarter tile, half the screen's width ... */
#define SCREEN_H_PERCENT 50 /* ... and half its height, snapped into the quarter its centre is in (iTerm2) */
void tool_screen(Tool *t, const char *title, int w_percent, int h_percent);
void tool_stop(Tool *t);

typedef struct { int has_text, has_cell; int text[2], cell[2]; } Reports; /* what the terminal says about its pixel sizes: (w, h) */

/* brings a window to exactly cols x rows cells (the f key for fixed room shapes) */
typedef struct {
    Tool *tool; char title[100];
    int tc, tr; int stage; /* 0 zoom, 1 resize */
    int last; /* 0 none, 1 in, 2 out */
    int has_prev, prev_cols, prev_rows;
    int steps; double cw, ch; int has_cw;
    double next_at; int has_sent, sent[4];
    int done; char message[48];
} Fitter;
void fitter_init(Fitter *f, Tool *tool, const char *title, int cols, int rows);
void fitter_update(Fitter *f, double now, int cols, int rows, const int *rect);

/* when a room opens: font to `font` points and the window to `size`, in one go */
typedef struct {
    Tool *tool; char title[100];
    int font, profile_font; char size[16]; int size_off, size_screen;
    Reports *reports;
    int stage; /* 0 reset, 1 probe, 2 fit, 3 font, 4 wait, 5 done */
    double wait_until, started, asked; int has_started;
    Fitter fit; int has_fit;
    int ask_pending; /* the caller should send CSI 14 t / 16 t */
    int pane;    /* set by the caller before each update: watcher_in_pane (-1 unknown: wait up to OPEN_PANE_WAIT, then as in a pane) */
    int skipped; /* the room is in a split pane: neither the window nor the font was touched */
} OpenSetup;
#define OPEN_PANE_WAIT 15.0 /* seconds to wait for the watcher to say whether the room is in a split pane (PowerShell can take several
                             * to start); still unknown then: leave the window and the font alone */
void opensetup_init(OpenSetup *o, Tool *tool, const char *title, int font, const char *size, Reports *reports, int window, int profile_font);
/* returns 1 while still working; sets o->ask_pending when the terminal should be asked for its sizes */
int opensetup_update(OpenSetup *o, double now, int cols, int rows, const int *rect, const int *area);
double opensetup_window_px(const OpenSetup *o, const int *rect, const int *area, int cols, int rows, double out[2]); /* 1 if known */

/* ---- where the gates go ------------------------------------------------------------------------ */
typedef struct { double ox, oy, cw, ch, k, x0, y0; } Grid;
void cell_px(const int *area, int dpi, const Reports *rep, int cols, int rows, double *cw, double *ch);
Grid grid_params(const Room *room, const int *area, int dpi, const Reports *rep, int cols, int rows);
int my_band(const Room *room, const Grid *g, int band[2]); /* 1 if known */
void snap_both(double lo, double hi, const double (*grids)[2], int n, int *index_out);
/* the gate positions for the windows next to this one: link_pos[side] (has_pos), closed sides, scales */
typedef struct { int has_pos[SIDES]; double pos[SIDES]; int closed[SIDES]; int has_scale[SIDES]; double scale[SIDES]; } Passages;
void plan_passages(const Room *room, const Watcher *w, const char *win_id, const Grid *my_grid, int has_grid, const Link *link, Passages *out);
#endif
