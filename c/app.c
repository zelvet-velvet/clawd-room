/* The terminal program: arguments, terminal setup, the start menu and the room's main loop (port of main / run / choose_scene). */
#include "app.h"
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "art.h"
#include "clawd.h"
#include "link.h"
#include "room.h"
#include "tail.h"
#include "term.h"
#include "win.h"
#include "ui.h"

#define FPS 16
#define REPLAY_SECONDS 600
#define DEFAULT_EVENTS "/.clawd-room/events.jsonl"

static const char *RATIO_CHOICES[] = {"auto", "3:1", "2:1", "16:9", "4:3", "1:1", "3:4", "large", "huge"};
static const char *const RATIO_LABELS[] = {"FIT WINDOW", "3:1  144X48", "2:1  120X60", "16:9 128X72", "4:3  128X96", "1:1  120X120", "3:4  96X128", "LARGE 192X108", "HUGE 256X144"};
#define N_RATIOS 9

typedef struct {
    char events[512];
    int has_at; double at;
    const char *room, *theme;
    char ratio[8];
    int scale, zoom, no_auto, no_detect, no_split;
    const char *dump;
    int build_art, export_sprites, force, add, list_windows;
    int font, profile_font, near, opened;
    const char *size, *left, *right, *up, *down;
} Args;

static volatile sig_atomic_t g_winch, g_quit;
static void on_winch(int s) { (void)s; g_winch = 1; }
static void on_quit(int s) { (void)s; g_quit = 1; }

static double mono(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static double wall(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }

static void put(const char *s, size_t n) {
    while (n) { ssize_t w = write(STDOUT_FILENO, s, n); if (w < 0) { if (errno == EINTR) continue; return; } s += w; n -= (size_t)w; }
}
static void puts_(const char *s) { put(s, strlen(s)); }

static void term_size(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) { *cols = ws.ws_col; *rows = ws.ws_row; }
    else { *cols = 80; *rows = 24; }
}

typedef struct { struct termios old; int on; } RawTerm;

static void raw_on(RawTerm *r) { /* like tty.setcbreak: no echo, no line buffering; Ctrl-C still works */
    tcgetattr(STDIN_FILENO, &r->old);
    struct termios t = r->old;
    t.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    r->on = 1;
}
static void raw_off(RawTerm *r) { if (r->on) tcsetattr(STDIN_FILENO, TCSADRAIN, &r->old); r->on = 0; }

static void screen_on(void) { puts_("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[0m\x1b[2J"); } /* alt screen, hide cursor, no wrap */
static void screen_off(void) { puts_("\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"); }

static void show(const Cells *cur, const Cells *prev, int sync) { /* one write per frame, wrapped in "synchronized update" so it never tears */
    size_t n;
    char *d = diff_frame(cur, prev, &n);
    char *out = malloc(n + 64);
    size_t k = 0;
    if (sync) k += (size_t)sprintf(out + k, "\x1b[?2026h");
    memcpy(out + k, d, n); k += n;
    k += (size_t)sprintf(out + k, sync ? "\x1b[0m\x1b[?2026l" : "");
    put(out, k);
    free(d); free(out);
}

/* what is waiting on the keyboard: the useful keys as characters (a lone Esc is 27, the arrows are w a s d, Enter is \r,
 * other escape sequences are skipped).
 * The terminal's answers to our size questions (CSI 4;h;w t = text area, CSI 6;h;w t = cell, in pixels) go to `rep`. */
static int read_keys(char *keys, size_t n, Reports *rep) {
    char data[256];
    ssize_t got = read(STDIN_FILENO, data, sizeof data);
    if (got <= 0) return 0;
    int len = 0;
    for (ssize_t i = 0; i < got;) { /* take out the size reports */
        if (data[i] == 27 && i + 3 < got && data[i + 1] == '[' && (data[i + 2] == '4' || data[i + 2] == '6') && data[i + 3] == ';') {
            ssize_t j = i + 4;
            while (j < got && data[j] != 't') j++;
            if (j < got) {
                int h, w;
                char tmp[64];
                snprintf(tmp, sizeof tmp, "%.*s", (int)(j - i - 4 > 60 ? 60 : j - i - 4), data + i + 4);
                if (sscanf(tmp, "%d;%d", &h, &w) == 2) {
                    if (data[i + 2] == '4') { rep->has_text = 1; rep->text[0] = w; rep->text[1] = h; }
                    else { rep->has_cell = 1; rep->cell[0] = w; rep->cell[1] = h; }
                }
                i = j + 1;
                continue;
            }
        }
        data[len++] = data[i++];
    }
    size_t k = 0;
    if (len == 1 && data[0] == 27) { keys[k++] = 27; keys[k] = 0; return 1; }
    for (int i = 0; i < len && k + 1 < n; i++) {
        char c = data[i];
        if (c == 27) { /* ESC [ A..D or ESC O A..D: an arrow; anything else starting with ESC is skipped */
            if (i + 2 < len && (data[i + 1] == '[' || data[i + 1] == 'O') && data[i + 2] >= 'A' && data[i + 2] <= 'D') { keys[k++] = "wsda"[data[i + 2] - 'A']; i += 2; continue; }
            if (i + 1 < len && data[i + 1] == '[') { i += 2; while (i < len && !(data[i] >= 0x40 && data[i] <= 0x7e)) i++; }
            else i++;
            continue;
        }
        if (c && strchr("WASD", c)) c += 32;
        if (c == '\n') c = '\r';
        if (c && strchr("qpfgr?[]wasd\r", c)) keys[k++] = c;
    }
    keys[k] = 0;
    return (int)k;
}

static int key_ready(double wait) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {(time_t)wait, (suseconds_t)((wait - (time_t)wait) * 1e6)};
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, wait > 0 ? &tv : &(struct timeval){0, 0}) > 0;
}

static int floordiv(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
static int in_ids(char ids[][40], int n, const char *id) { for (int i = 0; i < n; i++) if (!strcmp(ids[i], id)) return 1; return 0; }

/* the windows: where they are, the font and size helpers (WSL or iTerm2) */
typedef struct { Watcher w; Tool tool; int has_watcher, has_tool; } Desktop;

static void desktop_start(Desktop *d, const Args *a) {
    memset(d, 0, sizeof *d);
    if (!a->no_detect && win_available()) {
        d->has_watcher = watcher_start(&d->w);
        d->has_tool = tool_start(&d->tool);
    }
}

/* ---- the room ---------------------------------------------------------------------------------- */
static int run(Args *a, const SpriteFile *frames, const SpriteFile *items) {
    static Room room;
    room_init(&room);
    room.theme = theme_pick(a->room, a->theme);
    snprintf(room.ratio, sizeof room.ratio, "%s", a->ratio);
    static Event ev[4096];
    Tail tail;
    tail_init(&tail, a->events);
    double start = a->has_at ? a->at : wall();
    int n = tail_history(&tail, start - REPLAY_SECONDS, ev, 4096);
    for (int i = 0; i < n; i++) room_apply(&room, ev[i].t, ev[i].sid, ev[i].ev, ev[i].cwd, ev[i].tool);
    draw_set_sprites(frames, items);

    Link link;
    int has_link = a->room != NULL;
    if (has_link) link_init(&link, a->room, NULL);
    char win_id[80], title[128];
    if (has_link) snprintf(win_id, sizeof win_id, "%s", link.id); else snprintf(win_id, sizeof win_id, "solo-%d", (int)getpid()); /* our window title is "clawd-room <win_id>" */
    snprintf(title, sizeof title, "%s%s", WIN_PREFIX, win_id);
    Desktop desk;
    desktop_start(&desk, a);
    Reports reports = {0}; /* what the terminal tells us about its pixel sizes */
    OpenSetup setup;
    int has_setup = 0;
    Fitter fitter;
    int has_fitter = 0;
    if (desk.has_tool && !a->no_auto && !a->opened) { opensetup_init(&setup, &desk.tool, title, a->font, a->size, &reports, 1, a->profile_font); has_setup = 1; }
    int font_set = desk.has_tool && !a->no_auto && a->font; /* we changed the terminal font: put it back on exit */
    int auto_ = !a->scale; /* the TUI scale follows the window until [ ] is pressed */
    room.scale = a->scale ? a->scale : 1;
    room.cam_zoom = a->zoom;
    struct { int has; double t; int has_cell, cell[2]; int prescale; } pend = {0};
    char flash[96] = "";
    double flash_until = 0;
    int last_closed[SIDES] = {0};
    static char held[MAX_LINK_PEERS][40];
    int nheld = 0;
    char still_sig[1024] = "";
    int still_has_sig = 0;
    double still_since = wall(); /* when the windows last moved (gates wait until they are still) */

    RawTerm rt = {0};
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_winch; sigaction(SIGWINCH, &sa, NULL);
    sa.sa_handler = on_quit; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL);
    g_winch = 1; g_quit = 0;
    raw_on(&rt);
    if (has_link) {
        snprintf(link.theme, sizeof link.theme, "%s", room.theme->name);
        link_start(&link);
        snprintf(room.title, sizeof room.title, "%s", link.name);
    }
    if (desk.has_watcher) { char t[160]; snprintf(t, sizeof t, "\x1b[22;0t\x1b]2;%s\x07", title); puts_(t); } /* window title = how we are found */
    screen_on();
    double t0 = mono(), frozen_now = 0, frozen_t = 0;
    int paused = 0, help_on = 0, dirty = 1, cols = 0, rows = 0, size_menu = 0, size_pick = 0;
    Cells prev = {0, 0, NULL};
    int have_prev = 0;
    while (!g_quit) {
        double frame_start = mono(), t = frame_start - t0, now = a->has_at ? a->at + t : wall();
        if (desk.has_watcher) watcher_poll(&desk.w);
        char keys[256];
        if (key_ready(0) && read_keys(keys, sizeof keys, &reports)) {
            for (char *kp = keys; *kp; kp++) {
                char k = *kp;
                if (k == 'q') { g_quit = 1; break; }
                if (size_menu) { /* the ROOM SIZE list takes the keys while it is open */
                    if (k == 'w' || k == 'a') size_pick = (size_pick + N_RATIOS - 1) % N_RATIOS;
                    else if (k == 's' || k == 'd') size_pick = (size_pick + 1) % N_RATIOS;
                    else if (k == '\r') {
                        snprintf(room.ratio, sizeof room.ratio, "%s", RATIO_CHOICES[size_pick]);
                        room.cam_zoom = 1; room.cam_set = 0;
                        snprintf(flash, sizeof flash, "ROOM SIZE %s%s", RATIO_LABELS[size_pick], size_pick ? "  (F FITS THE WINDOW)" : "");
                        flash_until = wall() + 4; have_prev = 0;
                        puts_("\x1b[0m\x1b[2J");
                        size_menu = 0;
                    } else if (k == 27 || k == 'r') size_menu = 0;
                    dirty = 1;
                    continue;
                }
                if (k == 27) { g_quit = 1; break; }
                if (k == 'p') { paused = !paused; frozen_now = now; frozen_t = t; dirty = 1; }
                else if (k == '?') { help_on = !help_on; dirty = 1; }
                else if (k == 'r') { /* open the ROOM SIZE list at the current size */
                    size_pick = 0;
                    while (size_pick < N_RATIOS - 1 && strcmp(RATIO_CHOICES[size_pick], room.ratio)) size_pick++;
                    size_menu = 1; dirty = 1;
                } else if (k == 'w' || k == 'a' || k == 's' || k == 'd') { /* move the camera a quarter of the view */
                    if (!room.cam_vw || (room.cam_vw >= room.size_w && room.cam_vh >= room.size_h)) { snprintf(flash, sizeof flash, "ALL OF THE ROOM IS SHOWN: ] ZOOMS IN"); flash_until = wall() + 3; }
                    else {
                        int sx = room.cam_vw / 4 > 1 ? room.cam_vw / 4 : 1, sy = room.cam_vh / 4 > 1 ? room.cam_vh / 4 : 1;
                        room.cam_x += k == 'a' ? -sx : k == 'd' ? sx : 0;
                        room.cam_y += k == 'w' ? -sy : k == 's' ? sy : 0;
                    }
                    dirty = 1;
                } else if (k == 'g') { /* send a Clawd next door right now (to try crossing) */
                    char buf[64];
                    room_order(&room);
                    snprintf(flash, sizeof flash, "%s", room.has_geo ? room_send_one(&room, &room.geo, now, buf, sizeof buf) : "NO WINDOW NEXT TO THIS ONE");
                    flash_until = wall() + 4; dirty = 1;
                } else if (k == '[' || k == ']') { /* camera zoom: the room keeps its layout, the view gets closer */
                    int z = (room.cam_zoom > 1 ? room.cam_zoom : 1) + (k == ']' ? 1 : -1);
                    room.cam_zoom = z < 1 ? 1 : z > CAM_MAX_ZOOM ? CAM_MAX_ZOOM : z;
                    if (room.cam_zoom > 1) snprintf(flash, sizeof flash, "ZOOM %dX  ARROWS/WASD MOVE", room.cam_zoom);
                    else snprintf(flash, sizeof flash, "ZOOM 1X");
                    flash_until = wall() + 3; have_prev = 0; dirty = 1;
                    puts_("\x1b[0m\x1b[2J");
                } else if (k == 'f') {
                    if (!desk.has_watcher) { snprintf(flash, sizeof flash, "FIT NEEDS WSL OR ITERM2"); flash_until = wall() + 3; }
                    else if (!strcmp(room.ratio, "auto")) { /* automatic font and scale again (the window is left as it is) */
                        auto_ = 1; room.cam_zoom = 1;
                        opensetup_init(&setup, &desk.tool, title, a->font, a->size, &reports, 0, a->profile_font); has_setup = 1;
                        font_set = font_set || a->font;
                        room_order(&room);
                        room.scale = auto_scale(cols, rows * 2, room.n);
                        snprintf(flash, sizeof flash, "AUTO FONT AND SCALE");
                        flash_until = wall() + 3; have_prev = 0;
                        puts_("\x1b[0m\x1b[2J");
                    } else { /* fixed shape: zoom the font, then size the window to the exact grid */
                        int sw = 0, sh = 0;
                        static const struct { const char *k; int w, h; } RS[] = {{"3:1", 144, 48}, {"2:1", 120, 60}, {"16:9", 128, 72}, {"4:3", 128, 96}, {"1:1", 120, 120}, {"3:4", 96, 128}, {"large", BIG_ROOM_W, BIG_ROOM_H}, {"huge", HUGE_ROOM_W, HUGE_ROOM_H}};
                        for (size_t q = 0; q < sizeof RS / sizeof RS[0]; q++) if (!strcmp(RS[q].k, room.ratio)) { sw = RS[q].w; sh = RS[q].h; }
                        fitter_init(&fitter, &desk.tool, title, sw, sh / 2); has_fitter = 1;
                        snprintf(flash, sizeof flash, "FITTING WINDOW...");
                        flash_until = wall() + 30;
                    }
                    dirty = 1;
                }
            }
        }
        int c, r;
        term_size(&c, &r);
        if (g_winch || c != cols || r != rows) {
            g_winch = 0;
            cols = c; rows = r;
            have_prev = 0; dirty = 1;
            int prescale = room.scale;
            if (auto_ && !strcmp(room.ratio, "auto") && !a->no_auto) { room_order(&room); room.scale = auto_scale(cols, rows * 2, room.n); } /* at once; undone below if it was the font */
            if (!pend.has) { pend.has = 1; pend.t = wall(); pend.has_cell = reports.has_cell; pend.cell[0] = reports.cell[0]; pend.cell[1] = reports.cell[1]; pend.prescale = prescale; }
            reports.has_cell = 0;
            puts_("\x1b[0m\x1b[2J\x1b[14t\x1b[16t"); /* ... and ask how big the text area and a cell are in pixels */
            if (getenv("CLAWD_SIZE_LOG")) { FILE *lf = fopen(getenv("CLAWD_SIZE_LOG"), "a"); if (lf) { fprintf(lf, "%.1f %d %d\n", wall(), cols, rows); fclose(lf); } }
        }
        n = tail_poll(&tail, ev, 4096);
        for (int i = 0; i < n; i++) room_apply(&room, ev[i].t, ev[i].sid, ev[i].ev, ev[i].cwd, ev[i].tool);
        room_prune(&room, now);
        if (has_link) {
            static LinkEvent lev[64];
            int ne = link_update(&link, wall(), cols, rows, lev, 64);
            for (int i = 0; i < ne; i++) {
                if (lev[i].kind < 2) {
                    char nm[32];
                    snprintf(nm, sizeof nm, "%s", lev[i].peer.name);
                    for (char *p = nm; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
                    snprintf(flash, sizeof flash, "%s %s", nm, lev[i].kind == 0 ? "JOINED" : "LEFT");
                    flash_until = wall() + 4; dirty = 1;
                    if (lev[i].kind == 1) room_peer_gone(&room, lev[i].peer.id);
                } else if (!strcmp(lev[i].type, "walk") || !strcmp(lev[i].type, "recall")) room_receive(&room, &lev[i].msg);
            }
            const int *mine = desk.has_watcher ? watcher_rect(&desk.w, win_id) : NULL;
            int psides[MAX_LINK_PEERS];
            Neighbor old_nb[SIDES];
            int n_old = room.nnb;
            memcpy(old_nb, room.nb, sizeof old_nb);
            room.npeers = 0; room.nnb = 0;
            char new_held[MAX_LINK_PEERS][40];
            int n_new_held = 0;
            for (int i = 0; i < link.npeers; i++) { /* which window is next to this one */
                const LinkPeer *p = &link.peers[i];
                int side = -1;
                if (a->left && !strcmp(a->left, p->name)) side = SIDE_LEFT;
                else if (a->right && !strcmp(a->right, p->name)) side = SIDE_RIGHT;
                else if (a->up && !strcmp(a->up, p->name)) side = SIDE_UP;
                else if (a->down && !strcmp(a->down, p->name)) side = SIDE_DOWN;
                if (side < 0 && desk.has_watcher) {
                    int attached = in_ids(held, nheld, p->id);
                    for (int q = 0; q < n_old; q++) if (!strcmp(old_nb[q].id, p->id)) attached = 1;
                    side = neighbor_side(mine, watcher_rect(&desk.w, p->id), a->near, attached ? HYSTERESIS_PX : 0); /* stay attached a bit longer */
                }
                psides[i] = side;
                if (side >= 0) snprintf(new_held[n_new_held++], 40, "%s", p->id);
                if (room.npeers < 8) {
                    snprintf(room.peers[room.npeers].name, sizeof room.peers[0].name, "%s", p->name);
                    snprintf(room.peers[room.npeers].side, sizeof room.peers[0].side, "%s", side >= 0 ? SIDE_NAME[side] : "");
                    room.npeers++;
                }
                if (side >= 0) {
                    int k = 0;
                    while (k < room.nnb && room.nb[k].side != side) k++;
                    if (k == room.nnb) room.nnb++; /* (a second window on the same side replaces the first, in the first one's place) */
                    room.nb[k].side = side;
                    snprintf(room.nb[k].theme, sizeof room.nb[k].theme, "%s", p->theme);
                    snprintf(room.nb[k].id, sizeof room.nb[k].id, "%s", p->id);
                }
            }
            memcpy(held, new_held, sizeof held); nheld = n_new_held; /* attached now (also counts while its gate is closed) */
            if (!a->no_split) { /* the linked rooms share the sessions: each shows its own part */
                int m = 0;
                snprintf(room.owner_ids[m++], 40, "%s", link.id);
                for (int i = 0; i < link.npeers && m < 17; i++) snprintf(room.owner_ids[m++], 40, "%s", link.peers[i].id);
                for (int i = 0; i < m; i++) for (int j = i + 1; j < m; j++) if (strcmp(room.owner_ids[j], room.owner_ids[i]) < 0) { char tt[40]; strcpy(tt, room.owner_ids[i]); strcpy(room.owner_ids[i], room.owner_ids[j]); strcpy(room.owner_ids[j], tt); }
                room.n_owner_ids = m; room.split = 1;
                snprintf(room.owner_me, sizeof room.owner_me, "%s", link.id);
            }
            const int *mine_area = desk.has_watcher ? watcher_area(&desk.w, win_id) : NULL;
            Grid my_grid = {0};
            int has_grid = 0;
            if (mine_area) { my_grid = grid_params(&room, mine_area, watcher_dpi(&desk.w, win_id), &reports, cols, rows); has_grid = 1; }
            link.has_grid = has_grid;
            if (has_grid) { double g[7] = {my_grid.ox, my_grid.oy, my_grid.cw, my_grid.ch, my_grid.k, my_grid.x0, my_grid.y0}; memcpy(link.grid, g, sizeof g); }
            link.has_band = has_grid && my_band(&room, &my_grid, link.band);
            char sig[1024] = "";
            int has_sig = 0;
            if (mine_area) { /* where everything is, to about 4 pixels: if it changes the windows are being moved */
                has_sig = 1;
                int o = snprintf(sig, sizeof sig, "%d,%d,%d,%d|", floordiv(mine_area[0], 4), floordiv(mine_area[1], 4), floordiv(mine_area[2], 4), floordiv(mine_area[3], 4));
                if (has_grid) o += snprintf(sig + o, sizeof sig - (size_t)o, "%d,%d,%d|", (int)nearbyint(my_grid.cw), (int)nearbyint(my_grid.ch), (int)nearbyint(my_grid.k));
                const char *ids[SIDES];
                for (int q = 0; q < room.nnb; q++) ids[q] = room.nb[q].id;
                for (int i = 0; i < room.nnb; i++) for (int j = i + 1; j < room.nnb; j++) if (strcmp(ids[j], ids[i]) < 0) { const char *tt = ids[i]; ids[i] = ids[j]; ids[j] = tt; }
                for (int q = 0; q < room.nnb; q++) {
                    const int *pa = watcher_area(&desk.w, ids[q]);
                    o += snprintf(sig + o, sizeof sig - (size_t)o, "%s:", ids[q]);
                    if (pa) o += snprintf(sig + o, sizeof sig - (size_t)o, "%d,%d,%d,%d", floordiv(pa[0], 4), floordiv(pa[1], 4), floordiv(pa[2], 4), floordiv(pa[3], 4));
                    for (int p = 0; p < link.npeers; p++) if (!strcmp(link.peers[p].id, ids[q]) && link.peers[p].has_grid)
                        o += snprintf(sig + o, sizeof sig - (size_t)o, ":%d,%d,%d", (int)nearbyint(link.peers[p].grid[2]), (int)nearbyint(link.peers[p].grid[3]), (int)nearbyint(link.peers[p].grid[4]));
                    o += snprintf(sig + o, sizeof sig - (size_t)o, ";");
                }
            }
            if (has_sig != still_has_sig || strcmp(sig, still_sig)) { still_has_sig = has_sig; snprintf(still_sig, sizeof still_sig, "%s", sig); still_since = wall(); }
            Passages ps;
            memset(&ps, 0, sizeof ps);
            if (wall() - still_since < 1.0) { /* pending: no gates until the windows have been still */
                room.nnb = 0;
                for (int i = 0; i < room.npeers; i++) room.peers[i].side[0] = 0;
                for (int i = 0; i < SIDES; i++) { room.has_link[i] = 0; room.has_gate[i] = 0; }
            } else {
                plan_passages(&room, desk.has_watcher ? &desk.w : NULL, win_id, &my_grid, has_grid, &link, &ps);
                for (int i = 0; i < SIDES; i++) {
                    room.has_link[i] = ps.has_pos[i]; room.link_pos[i] = ps.pos[i];
                    room.has_gate[i] = ps.has_scale[i]; room.gate_scale[i] = ps.scale[i];
                }
            }
            { /* no gate where it does not fit */
                int w = 0;
                for (int q = 0; q < room.nnb; q++) if (!ps.closed[room.nb[q].side]) room.nb[w++] = room.nb[q];
                room.nnb = w;
                for (int i = 0; i < room.npeers; i++) { int sd = side_index(room.peers[i].side); if (sd >= 0 && ps.closed[sd]) room.peers[i].side[0] = 0; }
            }
            if (getenv("CLAWD_SIZE_LOG")) { /* test hook: note where the gates go whenever that changes */
                static char logged[256];
                char cur[256] = "{";
                int o = 1, first = 1;
                for (int q = 0; q < room.nnb; q++) {
                    int sd = room.nb[q].side;
                    if (!room.has_link[sd]) continue;
                    o += snprintf(cur + o, sizeof cur - (size_t)o, "%s'%s': %.1f", first ? "" : ", ", SIDE_NAME[sd], room.link_pos[sd]);
                    first = 0;
                }
                snprintf(cur + o, sizeof cur - (size_t)o, "}");
                if (strcmp(cur, logged)) { snprintf(logged, sizeof logged, "%s", cur); FILE *lf = fopen(getenv("CLAWD_SIZE_LOG"), "a"); if (lf) { fprintf(lf, "%.1f link %s\n", wall(), cur); fclose(lf); } }
            }
            int changed = 0, any = 0;
            for (int i = 0; i < SIDES; i++) { if (ps.closed[i] != last_closed[i]) changed = 1; if (ps.closed[i]) any = 1; }
            if (changed) { /* say why a window next to us has no gate */
                if (any) { snprintf(flash, sizeof flash, "NO GATE: NOT ENOUGH FLOOR / OVERLAP"); flash_until = wall() + 4; }
                for (int i = 0; i < SIDES; i++) last_closed[i] = ps.closed[i];
                dirty = 1;
            }
            (void)psides;
            for (int i = 0; i < room.nout; i++) if (!link_send_msg(&link, room.outbox[i].peer, &room.outbox[i])) room_peer_gone(&room, room.outbox[i].peer);
            room.nout = 0;
        }
        if (pend.has) { /* did the window change, or the font (the cell size changed, the window did not)? */
            if (reports.has_cell || wall() - pend.t > 0.4) {
                if (reports.has_cell && pend.has_cell && (reports.cell[0] != pend.cell[0] || reports.cell[1] != pend.cell[1]) && !has_setup && !has_fitter && auto_) {
                    auto_ = 0; room.scale = pend.prescale; /* Ctrl +/- : the user took over, keep the scale */
                    snprintf(flash, sizeof flash, "FONT CHANGED: F = AUTO FONT AND SCALE");
                    flash_until = wall() + 5;
                } else if (!reports.has_cell && pend.has_cell) { reports.has_cell = 1; reports.cell[0] = pend.cell[0]; reports.cell[1] = pend.cell[1]; }
                pend.has = 0; have_prev = 0; dirty = 1;
            }
        }
        if (has_setup) {
            if (!opensetup_update(&setup, wall(), cols, rows, desk.has_watcher ? watcher_rect(&desk.w, win_id) : NULL, desk.has_watcher ? watcher_area(&desk.w, win_id) : NULL)) { has_setup = 0; dirty = 1; }
            else if (setup.ask_pending) puts_("\x1b[14t\x1b[16t");
        }
        if (has_fitter) {
            fitter_update(&fitter, wall(), cols, rows, desk.has_watcher ? watcher_rect(&desk.w, win_id) : NULL);
            if (fitter.done) { snprintf(flash, sizeof flash, "%s", fitter.message); flash_until = wall() + 5; has_fitter = 0; dirty = 1; }
        }
        if (dirty || !paused) {
            if (cols < MIN_COLS || rows < MIN_ROWS) {
                char buf[128];
                too_small(buf, sizeof buf, cols, rows);
                puts_(buf);
                have_prev = 0;
            } else {
                double shown_now = paused ? frozen_now : now, shown_t = paused ? frozen_t : t;
                char note[32], over[512];
                Canvas cv = render(cols, rows, &room, shown_now, shown_t, note, sizeof note);
                if (size_menu) draw_size_menu(&cv, RATIO_LABELS, N_RATIOS, size_pick);
                overlay_text(over, sizeof over, paused, help_on, note, wall() < flash_until ? flash : "");
                Cells cells = cells_from_canvas(&cv, over);
                show(&cells, have_prev ? &prev : NULL, 1);
                if (have_prev) cells_free(&prev);
                prev = cells; have_prev = 1;
                canvas_free(&cv);
            }
            dirty = 0;
        }
        double wait = frame_start + 1.0 / FPS - mono();
        if (wait > 0) key_ready(wait); /* sleeps, but wakes on a key press */
    }
    if (have_prev) cells_free(&prev);
    tail_free(&tail);
    if (desk.has_tool) {
        if (font_set) { tool_reset_zoom(&desk.tool, title); usleep(800000); } /* put the terminal font back to the profile's */
        tool_stop(&desk.tool);
    }
    if (desk.has_watcher) { watcher_stop(&desk.w); puts_("\x1b[23;0t"); } /* give the window title back (if the terminal supports it) */
    if (has_link) link_close(&link);
    raw_off(&rt);
    screen_off();
    return 0;
}

/* ---- the start menu ---------------------------------------------------------------------------- */
/* returns 1 and the chosen scene / room name (room "" for solo), or 0 to quit */
static int choose_scene(Args *a, const char **scene, const char **room_name) {
    RawTerm rt = {0};
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_winch; sigaction(SIGWINCH, &sa, NULL);
    sa.sa_handler = on_quit; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL);
    g_quit = 0;
    Desktop desk;
    desktop_start(&desk, a);
    Reports reports = {0};
    char title[128];
    snprintf(title, sizeof title, "%smenu-%d", WIN_PREFIX, (int)getpid());
    OpenSetup setup;
    int has_setup = 0, chosen = 0;
    if (desk.has_tool && !a->no_auto) { opensetup_init(&setup, &desk.tool, title, a->font, a->size, &reports, 1, a->profile_font); has_setup = 1; }
    raw_on(&rt);
    if (desk.has_watcher) { char t[160]; snprintf(t, sizeof t, "\x1b]2;%s\x07", title); puts_(t); } /* window title = how we are found */
    screen_on();
    int pick = 0, solo = 0, result = 0, cols = 0, rows = 0, have_prev = 0;
    double t0 = mono();
    Cells prev = {0, 0, NULL};
    while (!g_quit) {
        if (desk.has_watcher) watcher_poll(&desk.w);
        if (key_ready(0.08)) {
            char raw[256];
            ssize_t got = read(STDIN_FILENO, raw, sizeof raw - 1);
            if (got > 0) {
                raw[got] = 0;
                char data[256];
                int len = 0;
                for (ssize_t i = 0; i < got;) { /* the terminal's answers about its pixel sizes may arrive in the same read */
                    if (raw[i] == 27 && i + 3 < got && raw[i + 1] == '[' && (raw[i + 2] == '4' || raw[i + 2] == '6') && raw[i + 3] == ';') {
                        ssize_t j = i + 4;
                        while (j < got && raw[j] != 't') j++;
                        if (j < got) {
                            int h, w;
                            if (sscanf(raw + i + 4, "%d;%d", &h, &w) == 2) {
                                if (raw[i + 2] == '4') { reports.has_text = 1; reports.text[0] = w; reports.text[1] = h; }
                                else { reports.has_cell = 1; reports.cell[0] = w; reports.cell[1] = h; }
                            }
                            i = j + 1;
                            continue;
                        }
                    }
                    data[len++] = raw[i++];
                }
                data[len] = 0;
                if (!strcmp(data, "\x1b[D") || !strcmp(data, "\x1bOD") || !strcmp(data, "h") || !strcmp(data, "k")) pick = (pick + MENU_N - 1) % MENU_N;
                else if (!strcmp(data, "\x1b[C") || !strcmp(data, "\x1bOC") || !strcmp(data, "l") || !strcmp(data, "j") || !strcmp(data, "\t")) pick = (pick + 1) % MENU_N;
                else if (!strcmp(data, "s") || !strcmp(data, "S")) solo = !solo;
                else if (!strcmp(data, "q") || !strcmp(data, "Q") || !strcmp(data, "\x03") || !strcmp(data, "\x1b")) break;
                else if (!strcmp(data, "\r") || !strcmp(data, "\n") || !strcmp(data, " ")) { *scene = MENU_SCENE[pick]; *room_name = solo ? "" : MENU_SCENE[pick]; result = 1; chosen = 1; break; }
                else if (len == 1 && data[0] >= '1' && data[0] < '1' + MENU_N) pick = data[0] - '1';
            }
        }
        int c, r;
        term_size(&c, &r);
        if (c != cols || r != rows || g_winch) { g_winch = 0; cols = c; rows = r; have_prev = 0; puts_("\x1b[0m\x1b[2J"); }
        if (has_setup) {
            if (!opensetup_update(&setup, wall(), cols, rows, desk.has_watcher ? watcher_rect(&desk.w, title + strlen(WIN_PREFIX)) : NULL, desk.has_watcher ? watcher_area(&desk.w, title + strlen(WIN_PREFIX)) : NULL)) { has_setup = 0; a->opened = 1; } /* the room need not do it again */
            else if (setup.ask_pending) puts_("\x1b[14t\x1b[16t");
        }
        Canvas cv = render_menu(cols, rows, pick, solo, mono() - t0);
        Cells cells = cells_from_canvas(&cv, "");
        show(&cells, have_prev ? &prev : NULL, 0);
        if (have_prev) cells_free(&prev);
        prev = cells; have_prev = 1;
        canvas_free(&cv);
    }
    if (have_prev) cells_free(&prev);
    if (desk.has_tool) {
        if (!chosen && a->font && !a->no_auto) { tool_reset_zoom(&desk.tool, title); usleep(800000); } /* leaving: put the terminal font back */
        tool_stop(&desk.tool);
    }
    if (desk.has_watcher) watcher_stop(&desk.w);
    raw_off(&rt);
    screen_off();
    return result;
}

/* ---- arguments --------------------------------------------------------------------------------- */
static void usage(void) {
    fprintf(stderr,
        "usage: clawd-room [--events FILE] [--at TS] [--room NAME] [--theme office|coffee|park] [--ratio auto|3:1|2:1|16:9|4:3|1:1|3:4|large|huge]\n"
        "                  [--scale N] [--zoom 1-4] [--no-auto] [--no-detect] [--dump COLSxROWS]\n"
        "  (accepted, not used yet by the C version: --left/--right/--up/--down NAME, --no-split, --size, --font, --profile-font, --near)\n");
}

int app_main(int argc, char **argv) {
    Args a;
    memset(&a, 0, sizeof a);
    const char *home = getenv("HOME");
    const char *env = getenv("CLAWD_EVENTS");
    if (env) snprintf(a.events, sizeof a.events, "%s", env);
    else snprintf(a.events, sizeof a.events, "%s%s", home ? home : "", DEFAULT_EVENTS);
    snprintf(a.ratio, sizeof a.ratio, "auto");
    a.font = 8; a.profile_font = 14; a.near = NEAR_PX; a.size = "110x32";
    static const struct option opts[] = {
        {"events", 1, 0, 'e'}, {"at", 1, 0, 'a'}, {"room", 1, 0, 'r'}, {"theme", 1, 0, 't'}, {"left", 1, 0, 'L'}, {"right", 1, 0, 'R'},
        {"up", 1, 0, 'U'}, {"down", 1, 0, 'D'}, {"no-split", 0, 0, 'S'}, {"ratio", 1, 0, 'x'}, {"size", 1, 0, 'z'}, {"font", 1, 0, 'f'},
        {"profile-font", 1, 0, 'p'}, {"scale", 1, 0, 'c'}, {"no-auto", 0, 0, 'A'}, {"no-detect", 0, 0, 'N'}, {"near", 1, 0, 'n'},
        {"dump", 1, 0, 'd'}, {"build-art", 0, 0, 'B'}, {"export-sprites", 0, 0, 'X'}, {"force", 0, 0, 'F'}, {"add", 0, 0, 'P'},
        {"list-windows", 0, 0, 'W'}, {"zoom", 1, 0, 'Z'}, {"help", 0, 0, 'h'}, {0, 0, 0, 0}};
    int o, font_given = 0;
    while ((o = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        switch (o) {
        case 'e': snprintf(a.events, sizeof a.events, "%s", optarg); break;
        case 'a': a.has_at = 1; a.at = atof(optarg); break;
        case 'r': a.room = optarg; break;
        case 't': a.theme = optarg; break;
        case 'L': a.left = optarg; break; case 'R': a.right = optarg; break; case 'U': a.up = optarg; break; case 'D': a.down = optarg; break;
        case 'S': a.no_split = 1; break;
        case 'x': snprintf(a.ratio, sizeof a.ratio, "%s", optarg); break;
        case 'z': a.size = optarg; break;
        case 'f': a.font = atoi(optarg); font_given = 1; break;
        case 'p': a.profile_font = atoi(optarg); break;
        case 'c': a.scale = atoi(optarg); break;
        case 'Z': a.zoom = atoi(optarg); break;
        case 'A': a.no_auto = 1; break;
        case 'N': a.no_detect = 1; break;
        case 'n': a.near = atoi(optarg); break;
        case 'd': a.dump = optarg; break;
        case 'B': a.build_art = 1; break; case 'X': a.export_sprites = 1; break; case 'F': a.force = 1; break; case 'P': a.add = 1; break; case 'W': a.list_windows = 1; break;
        default: usage(); return o == 'h' ? 0 : 2;
        }
    }
    if (!font_given && !strcmp(win_platform(), "iTerm2")) a.font = 0; /* iTerm2: keep the font unless asked (the font keys need Accessibility) */
    int ok = 0;
    for (int i = 0; i < N_RATIOS; i++) if (!strcmp(a.ratio, RATIO_CHOICES[i])) ok = 1;
    if (!ok) { fprintf(stderr, "clawd-room: bad --ratio %s\n", a.ratio); return 2; }
    if (a.theme && strcmp(a.theme, "office") && strcmp(a.theme, "coffee") && strcmp(a.theme, "park")) { fprintf(stderr, "clawd-room: bad --theme %s\n", a.theme); return 2; }

    const char *dir = getenv("CLAWD_SPRITES");
    char path[600];
    static char dirbuf[512];
    if (!dir) { /* the sprites folder next to the program's folder (c/ and sprites/ are siblings) */
        ssize_t n = readlink("/proc/self/exe", dirbuf, sizeof dirbuf - 20);
        if (n > 0) { dirbuf[n] = 0; char *s = strrchr(dirbuf, '/'); if (s) { *s = 0; s = strrchr(dirbuf, '/'); if (s) { strcpy(s, "/sprites"); dir = dirbuf; } } }
        if (!dir) dir = "../sprites";
    }
    snprintf(path, sizeof path, "%s/clawd.txt", dir);
    SpriteFile frames = sprites_load(path), items;
    snprintf(path, sizeof path, "%s/items.txt", dir);
    items = sprites_load(path);
    int rc = 0;
    if (a.build_art || a.export_sprites) { /* the sprite files and the pictures made from them */
        char root[600], art[700];
        snprintf(root, sizeof root, "%s", dir);
        char *sl = strrchr(root, '/');
        if (sl) *sl = 0; else strcpy(root, ".");
        snprintf(art, sizeof art, "%s/Clawd Room Note/Animations", root);
        rc = a.export_sprites ? art_export(dir, a.force, a.add) : art_build(dir, art);
        sprites_free(&frames); sprites_free(&items);
        return rc;
    }
    if (a.list_windows) { /* the clawd-room windows on the desktop (Windows Terminal or iTerm2) */
        if (!win_available()) { fprintf(stderr, "needs WSL (powershell.exe) or iTerm2 on macOS\n"); return 1; }
        Watcher w;
        if (!watcher_start(&w)) return 1;
        for (int i = 0; i < 100 && !w.updated; i++) { watcher_poll(&w); usleep(100000); }
        usleep(500000);
        watcher_poll(&w);
        if (!w.ncur) puts("no windows with that title prefix");
        for (int i = 0; i < w.ncur; i++) printf("%s: %d %d %d %d (terminal area %d %d %d %d, dpi %d)\n", w.cur[i].title, w.cur[i].rect[0], w.cur[i].rect[1], w.cur[i].rect[2], w.cur[i].rect[3], w.cur[i].area[0], w.cur[i].area[1], w.cur[i].area[2], w.cur[i].area[3], w.cur[i].dpi);
        watcher_stop(&w);
        return 0;
    }
    if (a.dump) {
        int cols, rows;
        if (sscanf(a.dump, "%dx%d", &cols, &rows) != 2) { fprintf(stderr, "clawd-room: --dump wants COLSxROWS, for example 100x30\n"); return 2; }
        static Room room;
        room_init(&room);
        room.theme = theme_pick(a.room, a.theme);
        snprintf(room.ratio, sizeof room.ratio, "%s", a.ratio);
        room.cam_zoom = a.zoom;
        double now = a.has_at ? a.at : wall();
        static Event ev[4096];
        Tail tail;
        tail_init(&tail, a.events);
        int n = tail_history(&tail, now - REPLAY_SECONDS, ev, 4096);
        for (int i = 0; i < n; i++) room_apply(&room, ev[i].t, ev[i].sid, ev[i].ev, ev[i].cwd, ev[i].tool);
        room_prune(&room, now);
        draw_set_sprites(&frames, &items);
        room.freeze = 0;
        char *text = dump_text(cols, rows, &room, now, 0.0);
        puts(text);
        free(text);
        tail_free(&tail);
    } else if (!(isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))) {
        fprintf(stderr, "clawd-room needs a terminal (stdin and stdout must be a tty)\n");
        rc = 1;
    } else {
        if (!(a.room || a.theme || a.has_at)) { /* started bare (for example from a tab): ask */
            const char *scene = NULL, *rn = NULL;
            if (!choose_scene(&a, &scene, &rn)) goto done;
            a.theme = scene;
            a.room = rn[0] ? rn : NULL;
        }
        rc = run(&a, &frames, &items);
    }
done:
    sprites_free(&frames);
    sprites_free(&items);
    return rc;
}
