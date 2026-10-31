#include "win.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "ps_scripts.h"
#include "mac_scripts.h"

static double wall(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static int pyround(double v) { return (int)nearbyint(v); }

static int on_path(const char *prog) {
    const char *path = getenv("PATH");
    if (!path) return 0;
    char *copy = strdup(path), *save = NULL;
    int found = 0;
    for (char *d = strtok_r(copy, ":", &save); d && !found; d = strtok_r(NULL, ":", &save)) {
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", d, prog);
        if (access(p, X_OK) == 0) found = 1;
    }
    free(copy);
    return found;
}

/* macOS: the helpers are osascript talking to iTerm2, and only when the room runs inside iTerm2 */
static int use_mac(void) {
    const char *tp = getenv("TERM_PROGRAM");
    return tp && !strcmp(tp, "iTerm.app") && !getenv("TMUX") && access("/usr/bin/osascript", X_OK) == 0;
}

int win_available(void) { return use_mac() || on_path("powershell.exe"); }
const char *win_platform(void) { return use_mac() ? "iTerm2" : "Windows Terminal"; }

/* ---- starting powershell.exe with an encoded script ------------------------------------------- */
static char *encode_script(const char *script) { /* base64 of the UTF-16LE script (the scripts are ASCII) */
    static const char B[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = strlen(script), bytes = n * 2;
    unsigned char *u = malloc(bytes);
    for (size_t i = 0; i < n; i++) { u[2 * i] = (unsigned char)script[i]; u[2 * i + 1] = 0; }
    char *out = malloc((bytes + 2) / 3 * 4 + 1), *o = out;
    for (size_t i = 0; i < bytes; i += 3) {
        unsigned v = (unsigned)u[i] << 16 | (i + 1 < bytes ? (unsigned)u[i + 1] << 8 : 0) | (i + 2 < bytes ? u[i + 2] : 0u);
        *o++ = B[(v >> 18) & 63]; *o++ = B[(v >> 12) & 63];
        *o++ = i + 1 < bytes ? B[(v >> 6) & 63] : '='; *o++ = i + 2 < bytes ? B[v & 63] : '=';
    }
    *o = 0;
    free(u);
    return out;
}

/* returns the pid; the child dies with us (PDEATHSIG). in_fd / out_fd: -1 = /dev/null, else a pipe end is returned */
static pid_t spawn_ps(const char *script, int want_in, int want_out, int *in_fd, int *out_fd) {
    int pin[2] = {-1, -1}, pout[2] = {-1, -1};
    if (want_in && pipe(pin)) return -1;
    if (want_out && pipe(pout)) return -1;
    char *enc = encode_script(script);
    pid_t pid = fork();
    if (pid == 0) {
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
        int nul = open("/dev/null", O_RDWR);
        dup2(want_in ? pin[0] : nul, 0);
        dup2(want_out ? pout[1] : nul, 1);
        dup2(nul, 2);
        for (int i = 3; i < 64; i++) close(i);
        execlp("powershell.exe", "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", enc, (char *)NULL);
        _exit(127);
    }
    free(enc);
    if (want_in) { close(pin[0]); *in_fd = pin[1]; }
    if (want_out) { close(pout[1]); *out_fd = pout[0]; fcntl(pout[0], F_SETFL, O_NONBLOCK); }
    return pid;
}

/* osascript running one of the JavaScript helpers with its arguments; its output (if wanted) comes back on a pipe.
 * There is no PDEATHSIG on macOS: the watcher stops by itself when its parent is gone. */
static pid_t spawn_osa(const char *script, const char *const *args, int nargs, int want_out, int *out_fd) {
    int pout[2] = {-1, -1};
    if (want_out && pipe(pout)) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        int nul = open("/dev/null", O_RDWR);
        dup2(nul, 0);
        dup2(want_out ? pout[1] : nul, 1);
        dup2(nul, 2);
        for (int i = 3; i < 64; i++) close(i);
        const char *argv[12] = {"osascript", "-l", "JavaScript", "-e", script};
        int n = 5;
        for (int i = 0; i < nargs && n < 11; i++) argv[n++] = args[i];
        argv[n] = NULL;
        execv("/usr/bin/osascript", (char *const *)argv);
        _exit(127);
    }
    if (want_out) { close(pout[1]); *out_fd = pout[0]; fcntl(pout[0], F_SETFL, O_NONBLOCK); }
    return pid;
}

/* a one-shot helper nobody waits for: the middle child exits at once, so no zombie is left */
static void spawn_osa_detached(const char *script, const char *const *args, int nargs) {
    pid_t mid = fork();
    if (mid == 0) { spawn_osa(script, args, nargs, 0, NULL); _exit(0); }
    if (mid > 0) waitpid(mid, NULL, 0);
}

static void reap(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    for (int i = 0; i < 40; i++) {
        int st;
        if (waitpid(pid, &st, WNOHANG) == pid) return;
        usleep(50000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

/* ---- the watcher ------------------------------------------------------------------------------- */
int watcher_start(Watcher *w) {
    memset(w, 0, sizeof *w);
    w->fd = -1;
    if (use_mac()) {
        const char *args[1] = {WIN_PREFIX};
        w->pid = spawn_osa(OSA_WATCH, args, 1, 1, &w->fd);
        return w->pid > 0;
    }
    /* $prefix = "clawd-room " in place of the parameter line */
    const char *marker = "param([string]$prefix)";
    const char *at = strstr(PS_WATCH, marker);
    if (!at) return 0;
    size_t head = (size_t)(at - PS_WATCH);
    char *script = malloc(strlen(PS_WATCH) + 64);
    snprintf(script, strlen(PS_WATCH) + 64, "%.*s$prefix = \"%s\"%s", (int)head, PS_WATCH, WIN_PREFIX, at + strlen(marker));
    w->pid = spawn_ps(script, 0, 1, NULL, &w->fd);
    free(script);
    return w->pid > 0;
}

void watcher_feed(Watcher *w, const char *line, double now) {
    if (!strcmp(line, "--")) { /* one complete list */
        memcpy(w->cur, w->pend, sizeof(WinInfo) * (size_t)w->npend);
        w->ncur = w->npend; w->npend = 0; w->updated = now;
        return;
    }
    if (line[0] == '@') { /* @rid|focus|clawd|l|t|r|b: a pane of the window just before it */
        int owner = w->npend - 1;
        while (owner >= 0 && w->pend[owner].pane) owner--;
        WinInfo pi;
        memset(&pi, 0, sizeof pi);
        char end;
        if (owner < 0 || w->npend >= WATCH_MAX ||
            sscanf(line, "@%39[^|]|%d|%d|%d|%d|%d|%d%c", pi.rid, &pi.focus, &pi.clawd, &pi.rect[0], &pi.rect[1], &pi.rect[2], &pi.rect[3], &end) != 7) return;
        memcpy(pi.area, pi.rect, sizeof pi.area);
        pi.pane = 1; pi.owner = owner; pi.dpi = w->pend[owner].dpi;
        w->pend[owner].npanes++;
        w->pend[w->npend++] = pi;
        return;
    }
    /* title|l|t|r|b|al|at|ar|ab|dpi|bar: the title is everything before the last ten '|' */
    const char *p = line + strlen(line);
    int bars = 0;
    while (p > line && bars < 10) if (*--p == '|') bars++;
    if (bars < 10 || w->npend >= WATCH_MAX) return;
    WinInfo *wi = &w->pend[w->npend];
    int nums[10];
    const char *q = p + 1;
    for (int i = 0; i < 10; i++) {
        char *end;
        long v = strtol(q, &end, 10);
        if (end == q || (i < 9 && *end != '|') || (i == 9 && *end)) return; /* not numbers: drop the line */
        nums[i] = (int)v;
        q = end + 1;
    }
    memset(wi, 0, sizeof *wi);
    snprintf(wi->title, sizeof wi->title, "%.*s", (int)(p - line), line);
    for (int i = 0; i < 4; i++) { wi->rect[i] = nums[i]; wi->area[i] = nums[4 + i]; }
    wi->dpi = nums[8];
    w->npend++;
}

void watcher_poll(Watcher *w) {
    if (w->fd < 0) return;
    char buf[2048];
    for (;;) {
        ssize_t n = read(w->fd, buf, sizeof buf);
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                w->line[w->n] = 0;
                size_t len = w->n;
                while (len && (w->line[len - 1] == '\r' || w->line[len - 1] == ' ')) w->line[--len] = 0;
                if (len) watcher_feed(w, w->line, wall());
                w->n = 0;
            } else if (w->n + 1 < sizeof w->line) w->line[w->n++] = c;
        }
    }
}

void watcher_stop(Watcher *w) {
    if (w->fd >= 0) close(w->fd);
    w->fd = -1;
    reap(w->pid);
    w->pid = 0;
}

static const WinInfo *find(const Watcher *w, const char *room_id) {
    if (wall() - w->updated > 10.0) return NULL; /* (the helper can be slow with several rooms open: keep the last list a while, windows rarely move) */
    const char *rid = NULL; /* a room in a split window is its pane */
    if (w->my_rid[0] && !strcmp(room_id, w->my_id)) rid = w->my_rid;
    for (int i = 0; !rid && i < w->npeers; i++) if (!strcmp(w->peer_id[i], room_id)) rid = w->peer_rid[i];
    if (rid) for (int i = 0; i < w->ncur; i++) if (w->cur[i].pane && !strcmp(w->cur[i].rid, rid)) return &w->cur[i];
    char title[128];
    snprintf(title, sizeof title, "%s%s", WIN_PREFIX, room_id);
    for (int i = 0; i < w->ncur; i++) if (!w->cur[i].pane && !w->cur[i].npanes && !strcmp(w->cur[i].title, title)) return &w->cur[i]; /* (a split window's title is only its focused pane's) */
    return NULL;
}

int watcher_in_pane(const Watcher *w, const char *room_id) {
    const WinInfo *f = find(w, room_id);
    if (f) return f->pane;
    if (wall() - w->updated > 10.0) return -1;
    char title[128];
    snprintf(title, sizeof title, "%s%s", WIN_PREFIX, room_id);
    for (int i = 0; i < w->ncur; i++) if (w->cur[i].npanes && !strcmp(w->cur[i].title, title)) return 1; /* our split window, the pane not known yet */
    return -1;
}

int watcher_same_window(const Watcher *w, const char *rid_a, const char *rid_b) {
    if (!rid_a[0] || !rid_b[0]) return 0;
    int oa = -1, ob = -1;
    for (int i = 0; i < w->ncur; i++) if (w->cur[i].pane) {
        if (!strcmp(w->cur[i].rid, rid_a)) oa = w->cur[i].owner;
        if (!strcmp(w->cur[i].rid, rid_b)) ob = w->cur[i].owner;
    }
    return oa >= 0 && oa == ob;
}

void watcher_resolve(Watcher *w, const char *my_id, const Link *link) {
    snprintf(w->my_id, sizeof w->my_id, "%s", my_id);
    w->npeers = 0;
    for (int i = 0; link && i < link->npeers && w->npeers < WATCH_PEERS; i++) {
        if (!link->peers[i].pane[0]) continue;
        snprintf(w->peer_id[w->npeers], 40, "%s", link->peers[i].id);
        snprintf(w->peer_rid[w->npeers], 40, "%s", link->peers[i].pane);
        w->npeers++;
    }
    if (w->updated == w->resolved) return; /* (once per list) */
    w->resolved = w->updated;
    char title[128];
    snprintf(title, sizeof title, "%s%s", WIN_PREFIX, my_id);
    const char *focused = NULL, *only = NULL;
    int nfree = 0;
    for (int i = 0; i < w->ncur; i++) {
        if (!w->cur[i].npanes) continue;
        int ours = !strcmp(w->cur[i].title, title); /* (the focus says something only in the window showing our title) */
        for (int j = i + 1; j < w->ncur && w->cur[j].pane && w->cur[j].owner == i; j++) {
            const WinInfo *p = &w->cur[j];
            if (ours && p->focus) focused = p->rid;
            int claimed = 0;
            for (int q = 0; q < w->npeers; q++) if (!strcmp(w->peer_rid[q], p->rid)) claimed = 1;
            if (p->clawd && !claimed) { only = p->rid; nfree++; }
        }
    }
    /* the window shows our title while our pane is its active one; with the focus that is the focused pane (seen twice, as
     * the title and the focus are read a moment apart) */
    if (focused) {
        if (!strcmp(focused, w->cand_rid)) w->cand_n++; else { snprintf(w->cand_rid, sizeof w->cand_rid, "%s", focused); w->cand_n = 1; }
        if (w->cand_n >= 2) snprintf(w->my_rid, sizeof w->my_rid, "%s", focused);
    } else {
        w->cand_n = 0;
        /* else the only Clawd Room pane, in any split window, that no other room claims: e.g. ours lost the focus to the
         * next room's pane before it was seen twice, and that room has said which pane is its own */
        if (!w->my_rid[0] && nfree == 1) snprintf(w->my_rid, sizeof w->my_rid, "%s", only);
    }
}
const int *watcher_rect(const Watcher *w, const char *id) { const WinInfo *i = find(w, id); return i ? i->rect : NULL; }
const int *watcher_area(const Watcher *w, const char *id) { const WinInfo *i = find(w, id); return i ? i->area : NULL; }
int watcher_dpi(const Watcher *w, const char *id) { const WinInfo *i = find(w, id); return i ? i->dpi : 96; }

int neighbor_side(const int *mine, const int *other, int near_px, int slack) {
    if (!mine || !other) return -1;
    int top = mine[1] > other[1] ? mine[1] : other[1], bottom = mine[3] < other[3] ? mine[3] : other[3];
    int h1 = mine[3] - mine[1], h2 = other[3] - other[1], smaller = h1 < h2 ? h1 : h2;
    if (smaller > 0 && bottom - top >= 0.3 * smaller) { /* side by side: they overlap vertically */
        int gap_right = other[0] - mine[2], gap_left = mine[0] - other[2];
        if (-MAX_OVERLAP_PX - slack <= gap_right && gap_right <= near_px + slack) return SIDE_RIGHT;
        if (-MAX_OVERLAP_PX - slack <= gap_left && gap_left <= near_px + slack) return SIDE_LEFT;
    }
    int left = mine[0] > other[0] ? mine[0] : other[0], right = mine[2] < other[2] ? mine[2] : other[2];
    int w1 = mine[2] - mine[0], w2 = other[2] - other[0], narrower = w1 < w2 ? w1 : w2;
    if (narrower > 0 && right - left >= 0.3 * narrower) { /* stacked: they overlap horizontally */
        int gap_down = other[1] - mine[3], gap_up = mine[1] - other[3];
        if (-MAX_OVERLAP_PX - slack <= gap_down && gap_down <= near_px + slack) return SIDE_DOWN;
        if (-MAX_OVERLAP_PX - slack <= gap_up && gap_up <= near_px + slack) return SIDE_UP;
    }
    return -1;
}

/* ---- the tool: font size and window size --------------------------------------------------------- */
int tool_start(Tool *t) {
    memset(t, 0, sizeof *t);
    t->fd = -1;
    return 1; /* started on first use */
}

static void tool_send(Tool *t, const char *line) {
    if (use_mac()) { /* the same command line, split into osascript arguments: cmd|title|n|m */
        char buf[400], *parts[4] = {0}, *save = NULL;
        snprintf(buf, sizeof buf, "%s", line);
        int n = 0;
        for (char *p = strtok_r(buf, "|", &save); p && n < 4; p = strtok_r(NULL, "|", &save)) parts[n++] = p;
        if (n < 3 || !(strcmp(parts[0], "resize") == 0 || strcmp(parts[0], "screen") == 0 || strcmp(parts[0], "zoomfg") == 0 || strcmp(parts[0], "resetfg") == 0)) return; /* no font keys for a window without the focus */
        spawn_osa_detached(OSA_TOOL, (const char *const *)parts, n);
        return;
    }
    if (t->pid <= 0) {
        t->pid = spawn_ps(PS_TOOL, 1, 0, &t->fd, NULL);
        if (t->pid <= 0) return;
    }
    char buf[400];
    int n = snprintf(buf, sizeof buf, "%s\n", line);
    signal(SIGPIPE, SIG_IGN);
    if (write(t->fd, buf, (size_t)n) < 0) { close(t->fd); t->fd = -1; reap(t->pid); t->pid = 0; }
}

void tool_resize(Tool *t, const char *title, int w, int h) { char b[300]; snprintf(b, sizeof b, "resize|%s|%d|%d", title, w, h); tool_send(t, b); }
void tool_zoom(Tool *t, const char *title, int step, int only_if_focused, int live) {
    char b[300];
    snprintf(b, sizeof b, "%s|%s|%d", live ? "zoomlive" : only_if_focused ? "zoomfg" : "zoom", title, step);
    tool_send(t, b);
}
void tool_screen(Tool *t, const char *title, int w_percent, int h_percent) { char b[300]; snprintf(b, sizeof b, "screen|%s|%d|%d", title, w_percent, h_percent); tool_send(t, b); }
void tool_reset_zoom(Tool *t, const char *title) { char b[300]; snprintf(b, sizeof b, "resetfg|%s|0", title); tool_send(t, b); }
void tool_stop(Tool *t) {
    if (t->fd >= 0) close(t->fd);
    t->fd = -1;
    reap(t->pid);
    t->pid = 0;
}

/* ---- Fitter ---------------------------------------------------------------------------------------- */
void fitter_init(Fitter *f, Tool *tool, const char *title, int cols, int rows) {
    memset(f, 0, sizeof *f);
    f->tool = tool;
    snprintf(f->title, sizeof f->title, "%s", title);
    f->tc = cols; f->tr = rows;
}

void fitter_update(Fitter *f, double now, int cols, int rows, const int *rect) {
    if (f->done || now < f->next_at || !rect) return;
    f->next_at = now + 1.0;
    if (f->stage == 0) { /* zoom */
        int ok = cols >= f->tc && rows >= f->tr;
        int stuck = f->has_prev && f->prev_cols == cols && f->prev_rows == rows; /* the last zoom changed nothing: at the font size limit */
        if (f->steps >= 14 || stuck || (f->last == 1 && !ok)) {
            if (f->last == 1 && !ok) { tool_zoom(f->tool, f->title, -1, 0, 0); f->next_at = now + 1.0; } /* one step too far: go back */
            f->stage = 1; f->has_prev = 0;
            return;
        }
        if (ok && f->last == 2) { f->stage = 1; f->has_prev = 0; return; }
        f->last = ok ? 1 : 2;
        f->has_prev = 1; f->prev_cols = cols; f->prev_rows = rows;
        f->steps++;
        tool_zoom(f->tool, f->title, ok ? 1 : -1, 0, 0);
        return;
    }
    /* resize stage: move the window edges by the cells still missing */
    if (cols == f->tc && rows == f->tr) { f->done = 1; snprintf(f->message, sizeof f->message, "FIT OK %dX%d", f->tc, f->tr); return; }
    int w = rect[2] - rect[0], h = rect[3] - rect[1];
    if (!f->has_cw) { f->has_cw = 1; f->cw = fmax(4.0, (w - 30.0) / cols); f->ch = fmax(8.0, (h - 90.0) / rows); }
    else if (f->has_sent) { /* learn the real cell size from the last resize */
        int pc = f->sent[0], pr = f->sent[1], pw = f->sent[2], ph = f->sent[3];
        if (cols != pc && w != pw) f->cw = fmax(4.0, fabs((double)(w - pw) / (cols - pc)));
        if (rows != pr && h != ph) f->ch = fmax(8.0, fabs((double)(h - ph) / (rows - pr)));
    }
    if (f->steps >= 24) { f->done = 1; snprintf(f->message, sizeof f->message, "FIT FAILED (%dX%d)", cols, rows); return; }
    f->steps++;
    f->has_sent = 1; f->sent[0] = cols; f->sent[1] = rows; f->sent[2] = w; f->sent[3] = h;
    tool_resize(f->tool, f->title, (int)(w + (f->tc - cols) * f->cw), (int)(h + (f->tr - rows) * f->ch));
}

/* ---- OpenSetup --------------------------------------------------------------------------------------- */
enum { ST_RESET, ST_PROBE, ST_FIT, ST_FONT, ST_WAIT, ST_DONE };

void opensetup_init(OpenSetup *o, Tool *tool, const char *title, int font, const char *size, Reports *reports, int window, int profile_font) {
    memset(o, 0, sizeof *o);
    o->tool = tool; o->font = font; o->profile_font = profile_font; o->reports = reports;
    snprintf(o->title, sizeof o->title, "%s", title);
    snprintf(o->size, sizeof o->size, "%s", window ? size : "off");
    for (char *p = o->size; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    o->size_screen = !strcmp(o->size, "screen");
    o->size_off = !strcmp(o->size, "off");
    o->stage = ST_RESET;
    o->pane = -1;
}

double opensetup_window_px(const OpenSetup *o, const int *rect, const int *area, int cols, int rows, double out[2]) {
    if (!(rect && area && cols && rows)) return 0;
    int wc = 0, wr = 0;
    if (sscanf(o->size, "%dx%d", &wc, &wr) != 2) return 0;
    double k = (double)(o->font ? o->font : o->profile_font) / o->profile_font;
    double cw = (double)(area[2] - area[0]) / cols, ch = (double)(area[3] - area[1]) / rows;
    out[0] = rect[2] - rect[0] - (area[2] - area[0]) + wc * cw * k;
    out[1] = rect[3] - rect[1] - (area[3] - area[1]) + wr * ch * k;
    return 1;
}

int opensetup_update(OpenSetup *o, double now, int cols, int rows, const int *rect, const int *area) {
    o->ask_pending = 0;
    if (!o->has_started) { o->has_started = 1; o->started = now; }
    if (o->stage == ST_RESET) {
        /* in a pane of a split window: the window is shared and the font keys could reach another pane, so touch neither.
         * Still unknown after the wait: the same (a window we cannot find is not one we may size) */
        if (o->pane < 0 && now - o->started < OPEN_PANE_WAIT) return 1;
        if (o->pane != 0) { o->skipped = 1; o->stage = ST_DONE; return 0; }
        if (o->font) tool_reset_zoom(o->tool, o->title);
        o->reports->has_cell = o->reports->has_text = 0;
        o->stage = ST_PROBE; o->wait_until = now + 0.6;
    } else if (o->stage == ST_PROBE) {
        if (o->size_off) o->stage = ST_FONT;
        else if (o->size_screen) { tool_screen(o->tool, o->title, SCREEN_W_PERCENT, SCREEN_H_PERCENT); o->stage = ST_FONT; }
        else if (now >= o->wait_until) {
            double px[2];
            int have = opensetup_window_px(o, rect, area, cols, rows, px) != 0;
            int wc = 0, wr = 0;
            sscanf(o->size, "%dx%d", &wc, &wr);
            if ((o->font == 0 || o->font == o->profile_font) && abs(cols - wc) <= 2 && abs(rows - wr) <= 1) o->stage = ST_FONT; /* the window was opened at that size already (wt --size): leave it */
            else if (have) { tool_resize(o->tool, o->title, (int)px[0], (int)px[1]); o->stage = ST_FONT; }
            else if (now - o->started > 3.0) { /* no answer from the terminal: size the window the slow way */
                fitter_init(&o->fit, o->tool, o->title, wc, wr);
                o->fit.stage = 1; /* skip the font zoom: this sets the font */
                o->has_fit = 1; o->stage = ST_FIT;
            } else if (now - o->asked > 0.5) { o->asked = now; o->ask_pending = 1; }
        }
    } else if (o->stage == ST_FIT) {
        fitter_update(&o->fit, now, cols, rows, rect);
        if (o->fit.done) o->stage = ST_FONT;
    } else if (o->stage == ST_FONT) {
        if (o->font && o->font != o->profile_font) tool_zoom(o->tool, o->title, o->font - o->profile_font, 1, 0);
        o->stage = ST_WAIT; o->wait_until = now + 1.0;
    } else if (o->stage == ST_WAIT && now >= o->wait_until) o->stage = ST_DONE;
    return o->stage != ST_DONE;
}

/* ---- where the gates go --------------------------------------------------------------------------------- */
static double area_w(const int *a) { return a[2] - a[0] > 1 ? a[2] - a[0] : 1; }
static double area_h(const int *a) { return a[3] - a[1] > 1 ? a[3] - a[1] : 1; }

void cell_px(const int *area, int dpi, const Reports *rep, int cols, int rows, double *cw, double *ch) {
    *cw = area_w(area) / (cols > 1 ? cols : 1);
    *ch = area_h(area) / (rows > 1 ? rows : 1);
    if (rep->has_cell) {
        double rw = rep->cell[0] * dpi / 96.0, rh = rep->cell[1] * dpi / 96.0;
        if (fabs(rw * cols - area_w(area)) < 0.15 * area_w(area) && fabs(rh * rows - area_h(area)) < 0.15 * area_h(area)) { *cw = rw; *ch = rh; }
    }
}

Grid grid_params(const Room *room, const int *area, int dpi, const Reports *rep, int cols, int rows) {
    Grid g;
    cell_px(area, dpi, rep, cols, rows, &g.cw, &g.ch);
    g.ox = area[0]; g.oy = area[1];
    g.x0 = room->view_x; g.y0 = room->view_y; g.k = room->view_k;
    return g;
}

int my_band(const Room *room, const Grid *g, int band[2]) {
    if (!room->has_geo || !g) return 0;
    const Geo *geo = &room->geo;
    band[0] = (int)(g->oy + (g->y0 + geo->ymin * g->k) / 2 * g->ch);
    band[1] = (int)(g->oy + (g->y0 + (geo->ymax + 8) * g->k) / 2 * g->ch);
    return 1;
}

void snap_both(double lo, double hi, const double (*grids)[2], int n, int *index_out) {
    double best_err = 0, best_mid = 0, best_y = 0;
    int have = 0, steps = (int)(hi - lo) > 1 ? (int)(hi - lo) : 1;
    for (int i = 0; i <= steps; i++) {
        double y = lo + (hi - lo) * i / steps, err = 0;
        for (int g = 0; g < n; g++) {
            double A = grids[g][0], B = grids[g][1], e = fabs(A + B * nearbyint((y - A) / B) - y);
            if (e > err) err = e;
        }
        double mid = fabs(y - (lo + hi) / 2);
        if (!have || err < best_err || (err == best_err && mid < best_mid)) { have = 1; best_err = err; best_mid = mid; best_y = y; }
    }
    for (int g = 0; g < n; g++) index_out[g] = pyround((best_y - grids[g][0]) / grids[g][1]);
}

void plan_passages(const Room *room, const Watcher *w, const char *win_id, const Grid *my_grid, int has_grid, const Link *link, Passages *out) {
    memset(out, 0, sizeof *out);
    const int *mine = w ? watcher_area(w, win_id) : NULL;
    if (!mine || !has_grid) return;
    for (int i = 0; i < room->nnb; i++) {
        int side = room->nb[i].side;
        const int *other = watcher_area(w, room->nb[i].id);
        const LinkPeer *pinfo = NULL;
        for (int q = 0; q < link->npeers; q++) if (!strcmp(link->peers[q].id, room->nb[i].id)) pinfo = &link->peers[q];
        double pg[7] = {my_grid->ox, my_grid->oy, my_grid->cw, my_grid->ch, my_grid->k, my_grid->x0, my_grid->y0};
        if (pinfo && pinfo->has_grid) memcpy(pg, pinfo->grid, sizeof pg); /* (until it tells us, assume it looks like us) */
        if (!other) continue;
        double k = my_grid->k, cw = my_grid->cw, ch = my_grid->ch, ox = my_grid->ox, oy = my_grid->oy, x0 = my_grid->x0, y0 = my_grid->y0;
        int idx[2];
        if (side == SIDE_UP || side == SIDE_DOWN) {
            double lo = fmax(mine[0], other[0]), hi = fmin(mine[2], other[2]);
            double big = fmax(k * cw, pg[4] * pg[2]);
            out->has_scale[side] = 1; out->scale[side] = big / (k * cw); /* a room pixel is this much smaller here than next door */
            double half = HATCH_W * big / 2;
            lo += half; hi -= half;
            if (lo > hi) { out->closed[side] = 1; continue; }
            double clear = DOOR_CLEAR * big; /* stay clear of the doors at the side edges (both rooms see the same overlap) */
            if (lo + clear <= hi - clear) { lo += clear; hi -= clear; }
            double grids[2][2] = {{ox + (x0 + 0.5 * k) * cw, k * cw}, {pg[0] + (pg[5] + 0.5 * pg[4]) * pg[2], pg[4] * pg[2]}};
            snap_both(lo, hi, grids, 2, idx);
            out->has_pos[side] = 1; out->pos[side] = idx[0];
        } else {
            double lo = fmax(mine[1], other[1]), hi = fmin(mine[3], other[3]); /* overlap on screen */
            int band[2];
            if (my_band(room, my_grid, band)) { lo = fmax(lo, band[0]); hi = fmin(hi, band[1]); } /* both need floor to walk on */
            if (pinfo && pinfo->has_band) { lo = fmax(lo, pinfo->band[0]); hi = fmin(hi, pinfo->band[1]); }
            double big = fmax(k * ch, pg[4] * pg[3]);
            out->has_scale[side] = 1; out->scale[side] = big / (k * ch);
            double half = DOOR_H * big / 2 / 2; /* the door must fit in both */
            lo += half; hi -= half;
            if (lo > hi) { out->closed[side] = 1; continue; }
            double grids[2][2] = {{oy + (y0 + 5.5 * k) / 2 * ch, k * ch / 2}, {pg[1] + (pg[6] + 5.5 * pg[4]) / 2 * pg[3], pg[4] * pg[3] / 2}};
            snap_both(lo, hi, grids, 2, idx);
            out->has_pos[side] = 1; out->pos[side] = idx[0];
        }
    }
}
