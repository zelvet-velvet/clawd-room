/* The pixel editor (Engine Design, step 3): clawd-room --edit [NAME] edits one sprite of the sprite files in iTerm2 (or any
 * terminal with mouse reporting), drawn with the room's own pixel canvas. It saves as you draw, so running rooms (which read
 * the files again when they change) show the edit at once.
 *   mouse: left paints, right erases, Alt + click picks the colour under it; a swatch picks a colour
 *   keys:  u undo, r redo, < > (or [ ]) the sprite before / after, Tab the next file, + - zoom, w n wider / narrower,
 *          t s taller / shorter, q quit */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include "canvas.h"
#include "term.h"

#define MAXW 64
#define MAXH 64
#define MAX_LINES 4000
#define MAX_UNDO 64

static const char *const FILES[] = {"props.txt", "items.txt", "clawd.txt"};
#define NFILES 3
/* colours to add (a 16-colour pixel-art set): painting with one gives it a letter in the file's palette */
static const Rgb EXTRA[16] = {{0, 0, 0}, {29, 43, 83}, {126, 37, 83}, {0, 135, 81}, {171, 82, 54}, {95, 87, 79}, {194, 195, 199}, {255, 241, 232},
                              {255, 0, 77}, {255, 163, 0}, {255, 236, 39}, {0, 228, 54}, {41, 173, 255}, {131, 118, 156}, {255, 119, 168}, {255, 204, 170}};
static const char FREE_CHARS[] = "abcdefghijklmnopqrstuvwxyzACDEFGHIJLMNOPQRSTUVXYZ0123456789";

typedef struct {
    char path[700];
    char *line[MAX_LINES]; int nline;
    char pal_ch[96]; Rgb pal_c[96]; int pal_body[96]; int npal; int last_pal_line;
    int sprite_line[512]; int nsprite;      /* the "@ name | key" line of each sprite */
} File;

typedef struct {
    File f; int file;                        /* which of FILES */
    int sp;                                  /* which sprite in it */
    char name[64];
    int w, h; char g[MAXH][MAXW + 1];        /* the grid being edited, as palette characters */
    char undo[MAX_UNDO][MAXH][MAXW + 1]; int uw[MAX_UNDO], uh[MAX_UNDO], nundo, nredo;
    int pick;                                /* the chosen swatch: 0 erase, 1.. the palette, then the extra colours */
    int zoom, dirty_file;
    char msg[96];
} Ed;

static void file_free(File *f) { for (int i = 0; i < f->nline; i++) free(f->line[i]); f->nline = 0; }

static int file_load(File *f, const char *path) {
    file_free(f);
    memset(f, 0, sizeof *f);
    snprintf(f->path, sizeof f->path, "%s", path);
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char buf[1024];
    f->last_pal_line = -1;
    while (fgets(buf, sizeof buf, fp) && f->nline < MAX_LINES) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        f->line[f->nline] = strdup(buf);
        if (buf[0] == ':' && f->npal < 96) { /* ": K = 20,20,30" or ": B = body" */
            const char *p = buf + 1;
            while (*p == ' ') p++;
            char ch = *p++;
            while (*p == ' ' || *p == '=') p++;
            f->pal_ch[f->npal] = ch;
            f->pal_body[f->npal] = !strncmp(p, "body", 4);
            int r = 0, g = 0, b = 0;
            if (!f->pal_body[f->npal]) sscanf(p, "%d,%d,%d", &r, &g, &b);
            f->pal_c[f->npal++] = f->pal_body[f->npal - 1] ? C_CLAWD : RGB(r, g, b);
            f->last_pal_line = f->nline;
        } else if (buf[0] == '@' && f->nsprite < 512) f->sprite_line[f->nsprite++] = f->nline;
        f->nline++;
    }
    fclose(fp);
    return 0;
}

static Rgb colour_of(const File *f, char ch, int *transparent) {
    *transparent = ch == '.' || ch == ' ';
    for (int i = 0; i < f->npal; i++) if (f->pal_ch[i] == ch) return f->pal_c[i];
    return RGB(255, 0, 255);
}

static void sprite_open(Ed *e, int sp) {
    File *f = &e->f;
    if (f->nsprite == 0) { e->w = e->h = 0; e->name[0] = 0; return; }
    e->sp = (sp + f->nsprite) % f->nsprite;
    int at = f->sprite_line[e->sp];
    const char *nm = f->line[at] + 1;
    while (*nm == ' ') nm++;
    const char *bar = strchr(nm, '|');
    size_t nl = bar ? (size_t)(bar - nm) : strlen(nm);
    while (nl && nm[nl - 1] == ' ') nl--;
    snprintf(e->name, sizeof e->name, "%.*s", (int)nl, nm);
    e->w = e->h = 0;
    memset(e->g, '.', sizeof e->g);
    for (int i = at + 1; i < f->nline && f->line[i][0] && f->line[i][0] != '@' && f->line[i][0] != '#' && f->line[i][0] != ':' && e->h < MAXH; i++) {
        int n = (int)strlen(f->line[i]);
        if (n > MAXW) n = MAXW;
        memcpy(e->g[e->h], f->line[i], (size_t)n);
        if (n > e->w) e->w = n;
        e->h++;
    }
    for (int y = 0; y < MAXH; y++) e->g[y][MAXW] = 0;
    e->nundo = e->nredo = 0;
}

static void file_open(Ed *e, const char *dir, int which, const char *name) {
    char path[700];
    e->file = (which + NFILES) % NFILES;
    snprintf(path, sizeof path, "%s/%s", dir, FILES[e->file]);
    if (file_load(&e->f, path)) snprintf(e->msg, sizeof e->msg, "CANNOT READ %s", FILES[e->file]);
    int sp = 0;
    if (name) for (int i = 0; i < e->f.nsprite; i++) {
        const char *nm = e->f.line[e->f.sprite_line[i]] + 1;
        while (*nm == ' ') nm++;
        if (!strncmp(nm, name, strlen(name)) && (nm[strlen(name)] == ' ' || nm[strlen(name)] == '|' || !nm[strlen(name)])) sp = i;
    }
    sprite_open(e, sp);
}

/* writes the grid back into the file's lines (a new palette letter goes after the last palette line), then the file */
static int save(Ed *e) {
    File *f = &e->f;
    int at = f->sprite_line[e->sp], end = at + 1;
    while (end < f->nline && f->line[end][0] && f->line[end][0] != '@' && f->line[end][0] != '#' && f->line[end][0] != ':') end++;
    char tmp[720];
    snprintf(tmp, sizeof tmp, "%s.edit", f->path);
    FILE *out = fopen(tmp, "w");
    if (!out) { snprintf(e->msg, sizeof e->msg, "CANNOT SAVE: %s", strerror(errno)); return -1; }
    for (int i = 0; i <= at; i++) fprintf(out, "%s\n", f->line[i]);
    for (int y = 0; y < e->h; y++) { char row[MAXW + 1]; memcpy(row, e->g[y], (size_t)e->w); row[e->w] = 0; fprintf(out, "%s\n", row); }
    for (int i = end; i < f->nline; i++) fprintf(out, "%s\n", f->line[i]);
    fclose(out);
    if (rename(tmp, f->path)) { snprintf(e->msg, sizeof e->msg, "CANNOT SAVE"); return -1; }
    char name[64];
    snprintf(name, sizeof name, "%s", e->name);
    char dir[700];
    snprintf(dir, sizeof dir, "%s", f->path);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0;
    int sp = e->sp, w = e->w, h = e->h, nu = e->nundo, nr = e->nredo;
    static char keep[MAXH][MAXW + 1];
    memcpy(keep, e->g, sizeof keep);
    char again[700];
    snprintf(again, sizeof again, "%s", f->path); /* (file_load clears the File, path and all) */
    file_load(&e->f, again); /* (line numbers moved) */
    e->sp = sp; e->w = w; e->h = h; memcpy(e->g, keep, sizeof keep); e->nundo = nu; e->nredo = nr;
    (void)name;
    snprintf(e->msg, sizeof e->msg, "SAVED %s", FILES[e->file]);
    return 0;
}

static char letter_for(Ed *e, Rgb c) { /* a palette letter for colour c, adding one to the file if needed */
    File *f = &e->f;
    for (int i = 0; i < f->npal; i++) if (!f->pal_body[i] && rgb_eq(f->pal_c[i], c)) return f->pal_ch[i];
    char ch = 0;
    for (const char *p = FREE_CHARS; *p && !ch; p++) {
        int used = 0;
        for (int i = 0; i < f->npal; i++) if (f->pal_ch[i] == *p) used = 1;
        if (!used) ch = *p;
    }
    if (!ch || f->nline >= MAX_LINES - 1) return '.';
    char buf[64];
    snprintf(buf, sizeof buf, ": %c = %d,%d,%d", ch, c.r, c.g, c.b);
    int at = f->last_pal_line >= 0 ? f->last_pal_line + 1 : 0;
    memmove(&f->line[at + 1], &f->line[at], sizeof(char *) * (size_t)(f->nline - at));
    f->line[at] = strdup(buf);
    f->nline++;
    for (int i = 0; i < f->nsprite; i++) if (f->sprite_line[i] >= at) f->sprite_line[i]++;
    f->last_pal_line = at;
    f->pal_ch[f->npal] = ch; f->pal_c[f->npal] = c; f->pal_body[f->npal] = 0; f->npal++;
    return ch;
}

static void push_undo(Ed *e) {
    if (e->nundo == MAX_UNDO) { memmove(e->undo[0], e->undo[1], sizeof e->undo[0] * (MAX_UNDO - 1)); memmove(e->uw, e->uw + 1, sizeof(int) * (MAX_UNDO - 1)); memmove(e->uh, e->uh + 1, sizeof(int) * (MAX_UNDO - 1)); e->nundo--; }
    memcpy(e->undo[e->nundo], e->g, sizeof e->g); e->uw[e->nundo] = e->w; e->uh[e->nundo] = e->h; e->nundo++;
    e->nredo = 0;
}
static void swap_undo(Ed *e, int at) {
    static char t[MAXH][MAXW + 1];
    memcpy(t, e->g, sizeof t);
    int tw = e->w, th = e->h;
    memcpy(e->g, e->undo[at], sizeof e->g); e->w = e->uw[at]; e->h = e->uh[at];
    memcpy(e->undo[at], t, sizeof t); e->uw[at] = tw; e->uh[at] = th;
}

/* ---- the screen: canvas pixels; one terminal cell is 1 pixel wide and 2 high ---------------------------------------- */
typedef struct { int gx, gy, z, px, py, sw, nsw; } Layout; /* grid origin and zoom; the swatches' origin, size, count */

/* the help, wrapped to the width: how many lines it takes (and, with cv, drawn above the bottom) */
static const char *const HELP[] = {"LEFT PAINT", "RIGHT ERASE", "ALT+CLICK PICK", "U UNDO", "R REDO", "< > SPRITE", "TAB FILE", "+ - ZOOM", "W WIDER", "N NARROWER", "T TALLER", "S SHORTER", "Q QUIT"};
static int help(Canvas *cv, int W, int H) {
    int lines = 1, x = 4;
    for (size_t i = 0; i < sizeof HELP / sizeof *HELP; i++) {
        int tw = text_width(HELP[i]);
        if (x > 4 && x + tw > W - 4) { lines++; x = 4; }
        x += tw + 8;
    }
    if (!cv) return lines;
    int y = H - 7 * lines;
    x = 4;
    for (size_t i = 0; i < sizeof HELP / sizeof *HELP; i++) {
        int tw = text_width(HELP[i]);
        if (x > 4 && x + tw > W - 4) { y += 7; x = 4; }
        canvas_text(cv, x, y, HELP[i], RGB(150, 150, 170));
        x += tw + 8;
    }
    return lines;
}

static Layout layout(const Ed *e, int W, int H) {
    Layout L;
    L.gx = 4; L.gy = 12;
    int room_w = W * 3 / 5 - 8, room_h = H - L.gy - 7 * help(NULL, W, H) - 6;
    L.z = e->zoom;
    if (L.z <= 0) { L.z = 2; while (L.z + 2 <= 16 && e->w * (L.z + 2) <= room_w && e->h * (L.z + 2) <= room_h) L.z += 2; }
    L.px = L.gx + e->w * L.z + 10; L.py = L.gy; L.sw = 8;
    L.nsw = 1 + e->f.npal + 16;
    return L;
}

static void swatch_xy(const Layout *L, int i, int *x, int *y) { /* 6 to a row; the extra colours on rows of their own */
    int npal = L->nsw - 16;
    if (i < npal) { *x = L->px + (i % 6) * (L->sw + 2); *y = L->py + 8 + (i / 6) * (L->sw + 2); }
    else { int k = i - npal, rows = (npal + 5) / 6; *x = L->px + (k % 8) * (L->sw + 2); *y = L->py + 8 + rows * (L->sw + 2) + 10 + (k / 8) * (L->sw + 2); }
}
static Rgb swatch_colour(const Ed *e, int i, int *transparent) {
    *transparent = 0;
    if (i == 0) { *transparent = 1; return RGB(0, 0, 0); }
    if (i <= e->f.npal) return e->f.pal_c[i - 1];
    return EXTRA[i - 1 - e->f.npal];
}

static void draw(const Ed *e, Canvas *cv) {
    canvas_rect(cv, 0, 0, cv->w, cv->h, RGB(18, 18, 24));
    char t[160];
    snprintf(t, sizeof t, "EDIT %s  %s  %dX%d", e->name, FILES[e->file], e->w, e->h);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 4, 3, t, C_YELLOW);
    Layout L = layout(e, cv->w, cv->h);
    for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) { /* the grid, transparent as a checker */
        int tr;
        Rgb c = colour_of(&e->f, e->g[y][x], &tr);
        for (int dy = 0; dy < L.z; dy++) for (int dx = 0; dx < L.z; dx++) {
            Rgb k = tr ? (((dx * 2 / L.z) + (dy * 2 / L.z)) % 2 ? RGB(70, 70, 80) : RGB(52, 52, 60)) : c;
            canvas_px(cv, L.gx + x * L.z + dx, L.gy + y * L.z + dy, k);
        }
    }
    canvas_rect(cv, L.gx - 1, L.gy - 1, e->w * L.z + 2, 1, RGB(90, 90, 110)); canvas_rect(cv, L.gx - 1, L.gy + e->h * L.z, e->w * L.z + 2, 1, RGB(90, 90, 110));
    canvas_rect(cv, L.gx - 1, L.gy - 1, 1, e->h * L.z + 2, RGB(90, 90, 110)); canvas_rect(cv, L.gx + e->w * L.z, L.gy - 1, 1, e->h * L.z + 2, RGB(90, 90, 110));
    canvas_text(cv, L.px, L.py, "PALETTE", C_GREY);
    for (int i = 0; i < L.nsw; i++) {
        int x, y, tr;
        swatch_xy(&L, i, &x, &y);
        Rgb c = swatch_colour(e, i, &tr);
        if (i == 1 + e->f.npal) canvas_text(cv, L.px, y - 8, "ADD", C_GREY);
        if (tr) { canvas_rect(cv, x, y, L.sw, L.sw, RGB(52, 52, 60)); canvas_rect(cv, x, y, L.sw / 2, L.sw / 2, RGB(70, 70, 80)); canvas_rect(cv, x + L.sw / 2, y + L.sw / 2, L.sw / 2, L.sw / 2, RGB(70, 70, 80)); }
        else canvas_rect(cv, x, y, L.sw, L.sw, c);
        if (i == e->pick) { canvas_rect(cv, x - 1, y - 1, L.sw + 2, 1, C_WHITE); canvas_rect(cv, x - 1, y + L.sw, L.sw + 2, 1, C_WHITE); canvas_rect(cv, x - 1, y - 1, 1, L.sw + 2, C_WHITE); canvas_rect(cv, x + L.sw, y - 1, 1, L.sw + 2, C_WHITE); }
    }
    int lx, ly;
    swatch_xy(&L, L.nsw - 1, &lx, &ly);
    ly += L.sw + 8;
    int bottom = cv->h - 7 * help(cv, cv->w, cv->h) - 3, big = ly + 8 + e->h * 2 <= bottom && L.px + e->w * 3 + 6 <= cv->w ? 2 : 1;
    if (ly + 8 + e->h <= bottom) canvas_text(cv, L.px, ly, "AS IN THE ROOM", C_GREY); /* the sprite at its real size (and twice, if there is room) */
    for (int k = 1; k <= big && ly + 8 + e->h <= bottom; k++) {
        int ox = L.px + (k - 1) * (e->w + 6), oy = ly + 8;
        canvas_rect(cv, ox, oy, e->w * k, e->h * k, RGB(196, 204, 218));
        for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) { int tr; Rgb c = colour_of(&e->f, e->g[y][x], &tr); if (!tr) canvas_rect(cv, ox + x * k, oy + y * k, k, k, c); }
    }
    if (e->msg[0]) canvas_text(cv, cv->w - text_width(e->msg) - 4, 3, e->msg, C_GREY);
}

/* ---- the terminal ---------------------------------------------------------------------------------------------------- */
static struct termios g_old;
static void put(const char *s) { size_t n = strlen(s); while (n) { ssize_t k = write(STDOUT_FILENO, s, n); if (k <= 0) return; s += k; n -= (size_t)k; } }
static void tty_on(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &g_old);
    t = g_old; t.c_lflag &= ~(tcflag_t)(ICANON | ECHO); t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    put("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?1000h\x1b[?1002h\x1b[?1006h\x1b]0;Clawd Room pixel editor\x07"); /* alt screen, mouse (SGR) */
}
static void tty_off(void) { put("\x1b[?1006l\x1b[?1002l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"); tcsetattr(STDIN_FILENO, TCSADRAIN, &g_old); }
#include <sys/ioctl.h>
int editor_run(const char *dir, const char *name) {
    static Ed e;
    memset(&e, 0, sizeof e);
    e.pick = 1;
    int which = 0, found = 0;
    for (int f = 0; f < NFILES && name && !found; f++) { /* the file that has NAME (props first) */
        File tf;
        memset(&tf, 0, sizeof tf);
        char path[700];
        snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
        if (file_load(&tf, path) == 0) for (int i = 0; i < tf.nsprite && !found; i++) {
            const char *nm = tf.line[tf.sprite_line[i]] + 1;
            while (*nm == ' ') nm++;
            size_t l = strlen(name);
            if (!strncmp(nm, name, l) && (nm[l] == ' ' || nm[l] == '|' || !nm[l])) { which = f; found = 1; }
        }
        file_free(&tf);
    }
    if (name && !found) { fprintf(stderr, "clawd-room --edit: no sprite called %s in %s/{props,items,clawd}.txt\n", name, dir); return 1; }
    file_open(&e, dir, which, name);
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) { fprintf(stderr, "clawd-room --edit needs a terminal\n"); return 1; }
    tty_on();
    Cells prev = {0, 0, NULL};
    int quit = 0, painting = 0, cols = 0, rows = 0;
    char in[256]; int nin = 0;
    while (!quit) {
        struct winsize ws;
        int c = 120, r = 40;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col) { c = ws.ws_col; r = ws.ws_row; }
        if (c != cols || r != rows) { cols = c; rows = r; cells_free(&prev); prev.c = NULL; put("\x1b[H"); }
        Canvas cv = canvas_new(cols, rows * 2, RGB(18, 18, 24));
        draw(&e, &cv);
        Cells cur = cells_from_canvas(&cv, NULL);
        size_t len;
        char *s = diff_frame(&cur, prev.c ? &prev : NULL, &len);
        put(s); put("\x1b[H");
        free(s);
        cells_free(&prev); prev = cur;
        Layout L = layout(&e, cv.w, cv.h);
        canvas_free(&cv);
        fd_set fs; FD_ZERO(&fs); FD_SET(STDIN_FILENO, &fs);
        struct timeval tv = {0, 200000};
        if (select(STDIN_FILENO + 1, &fs, NULL, NULL, &tv) <= 0) continue;
        ssize_t k = read(STDIN_FILENO, in + nin, sizeof in - 1 - (size_t)nin);
        if (k <= 0) continue;
        nin += (int)k; in[nin] = 0;
        int i = 0;
        while (i < nin) {
            if (in[i] == 0x1b) {
                if (i + 1 >= nin) break; /* (more to come) */
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] == '<') { /* SGR mouse: ESC [ < b ; x ; y (M | m) */
                    int b, x, y, used = 0;
                    char fin;
                    if (sscanf(in + i, "\x1b[<%d;%d;%d%c%n", &b, &x, &y, &fin, &used) < 4 || !used) { if (strchr(in + i, 'M') || strchr(in + i, 'm')) { i++; continue; } break; }
                    i += used;
                    int px = x - 1, py = (y - 1) * 2, btn = b & 3, alt = b & 8, release = fin == 'm';
                    int sx = (px - L.gx) / L.z, sy = (py - L.gy) / L.z, on_grid = px >= L.gx && py >= L.gy && sx < e.w && sy < e.h;
                    if (release) { if (painting) { painting = 0; save(&e); } continue; }
                    if (on_grid && alt) { /* pick the colour under the pointer */
                        char ch = e.g[sy][sx];
                        e.pick = 0;
                        for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) e.pick = q + 1;
                        continue;
                    }
                    if (on_grid && (btn == 0 || btn == 2)) {
                        if (!painting) { push_undo(&e); painting = 1; }
                        char ch = '.';
                        if (btn == 0 && e.pick > 0) {
                            if (e.pick <= e.f.npal) ch = e.f.pal_ch[e.pick - 1];
                            else ch = letter_for(&e, EXTRA[e.pick - 1 - e.f.npal]); /* (a new colour: it gets a letter in the palette) */
                        }
                        e.g[sy][sx] = ch;
                        if (e.pick > e.f.npal && btn == 0) { for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) e.pick = q + 1; } /* (the new colour is in the palette now) */
                        continue;
                    }
                    if (!(b & 32)) for (int q = 0; q < L.nsw; q++) { int wx, wy; swatch_xy(&L, q, &wx, &wy); if (px >= wx && px < wx + L.sw && py >= wy - 1 && py < wy + L.sw) e.pick = q; }
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin) { i += 3; continue; } /* (arrows and the like: not used) */
                i += 1; continue;
            }
            char ch = in[i++];
            if (ch == 'q') quit = 1;
            else if (ch == 'u' && e.nundo) { e.nundo--; swap_undo(&e, e.nundo); e.nredo++; save(&e); }
            else if (ch == 'r' && e.nredo) { swap_undo(&e, e.nundo); e.nundo++; e.nredo--; save(&e); }
            else if (ch == ']' || ch == '[' || ch == '>' || ch == '<') { sprite_open(&e, e.sp + (ch == ']' || ch == '>' ? 1 : -1)); e.zoom = 0; e.msg[0] = 0; }
            else if (ch == '\t') { file_open(&e, dir, e.file + 1, NULL); e.zoom = 0; e.pick = 1; e.msg[0] = 0; }
            else if (ch == '+' || ch == '=') e.zoom = (L.z + 2 > 16 ? 16 : L.z + 2);
            else if (ch == '-') e.zoom = (L.z - 2 < 2 ? 2 : L.z - 2);
            else if (((ch == 'w' || ch == 'W') && e.w < MAXW) || ((ch == 'n' || ch == 'N') && e.w > 1)) { push_undo(&e); if (ch == 'w' || ch == 'W') { for (int y = 0; y < e.h; y++) e.g[y][e.w] = '.'; e.w++; } else e.w--; save(&e); }
            else if (((ch == 't' || ch == 'T') && e.h < MAXH) || ((ch == 's' || ch == 'S') && e.h > 1)) { push_undo(&e); if (ch == 't' || ch == 'T') { memset(e.g[e.h], '.', MAXW); e.h++; } else e.h--; save(&e); }
        }
        memmove(in, in + i, (size_t)(nin - i)); nin -= i;
    }
    tty_off();
    cells_free(&prev);
    file_free(&e.f);
    return 0;
}
