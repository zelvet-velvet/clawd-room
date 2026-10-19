/* The pixel editor (Engine Design, step 3): clawd-room --edit [NAME] edits one sprite of the sprite files in iTerm2 (or any
 * terminal with mouse reporting), drawn with the room's own pixel canvas. It saves as you draw, so running rooms (which read
 * the files again when they change) show the edit at once.
 *   mouse: left paints, right erases, Alt + click picks the colour under it; a swatch picks a colour
 *   keys:  u undo, r redo, < > (or [ ]) the sprite before / after, Tab the next file, + - zoom, w n wider / narrower,
 *          t s taller / shorter, q quit */
#include <errno.h>
#include <math.h>
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
    int pk, pi; Rgb prgb;                    /* the colour to paint: pk 0 erase, 1 the file's colour pi, 2 a new colour prgb */
    int hx, hy;                              /* the sprite pixel under the pointer (-1: none) */
    int drag;                                /* dragging in the colour field (0) or the hue bar (1), or -1 */
    double hue;                              /* the colour field's hue (kept while the colour is grey) */
    int picking;                             /* PICK: the next click on the grid takes its colour */
    int last_pk, last_pi; Rgb last_prgb;     /* the colour before ERASE, for PAINT */
    int typing; char tbuf[8];                /* typing a colour: 0 none, 1 the hex code, 2..4 red / green / blue (0..255) */
    int zoom, dirty_file;
    char msg[96];
    char *orig[NFILES]; int changed[NFILES]; /* each file as it was when first opened, to put back on discard */
    int asking;                               /* q with changes: keep or discard? */
    int show_help;                            /* ?: the shortcuts in a box (when the window is too short to list them) */
} Ed;

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    if (b) b[n] = 0;
    fclose(f);
    return b;
}

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
            f->pal_c[f->npal] = f->pal_body[f->npal] ? C_CLAWD : RGB(r, g, b);
            f->npal++;
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
    if (!e->orig[e->file]) e->orig[e->file] = slurp(path); /* (kept for a discard) */
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
    e->changed[e->file] = 1;
    return 0;
}

/* F: a new frame of this sprite (NAME~2, ~3 ...), a copy of it, right after its last frame; then that one is open */
static int new_frame(Ed *e) {
    File *f = &e->f;
    char base[64];
    snprintf(base, sizeof base, "%s", e->name);
    char *tilde = strrchr(base, '~');
    if (tilde) *tilde = 0;
    int k = 2, after = e->sp;
    for (int i = 0; i < f->nsprite; i++) { /* the next number, and the last frame of this sprite */
        const char *nm = f->line[f->sprite_line[i]] + 1;
        while (*nm == ' ') nm++;
        size_t bl = strlen(base);
        if (strncmp(nm, base, bl)) continue;
        if (nm[bl] == '~') { int q = atoi(nm + bl + 1); if (q >= k) k = q + 1; after = i; }
        else if (nm[bl] == ' ' || nm[bl] == '|' || !nm[bl]) { if (after < i) after = i; }
    }
    int at = f->sprite_line[after] + 1;
    while (at < f->nline && f->line[at][0] && f->line[at][0] != '@' && f->line[at][0] != '#' && f->line[at][0] != ':') at++;
    if (f->nline + e->h + 3 >= MAX_LINES) return -1;
    char head[160];
    snprintf(head, sizeof head, "@ %s~%d | %s~%d", base, k, base, k);
    int add = e->h + 2; /* a blank line, the head, the rows */
    memmove(&f->line[at + add], &f->line[at], sizeof(char *) * (size_t)(f->nline - at));
    f->line[at] = strdup("");
    f->line[at + 1] = strdup(head);
    for (int y = 0; y < e->h; y++) { char row[MAXW + 1]; memcpy(row, e->g[y], (size_t)e->w); row[e->w] = 0; f->line[at + 2 + y] = strdup(row); }
    f->nline += add;
    FILE *out = fopen(f->path, "w");
    if (!out) return -1;
    for (int i = 0; i < f->nline; i++) fprintf(out, "%s\n", f->line[i]);
    fclose(out);
    char path[700];
    snprintf(path, sizeof path, "%s", f->path);
    file_load(f, path);
    e->changed[e->file] = 1;
    for (int i = 0; i < f->nsprite; i++) if (f->sprite_line[i] == at + 1) { sprite_open(e, i); break; }
    snprintf(e->msg, sizeof e->msg, "NEW FRAME %s~%d", base, k);
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
/* what is where, filled while drawing, so a click finds what it is on */
enum { R_GRID, R_TOOL, R_FILE, R_NEW, R_FIELD, R_HUE, R_HEX, R_NUM, R_ADD };
typedef struct { int kind, i, x0, y0, x1, y1; Rgb c; } Region;
static Region g_reg[160];
static int g_nreg;
static void region(int kind, int i, int x, int y, int w, int h, Rgb c) { if (g_nreg < 160) g_reg[g_nreg++] = (Region){kind, i, x, y, x + w, y + h, c}; }
static const Region *hit(int x, int y) { for (int k = g_nreg - 1; k >= 0; k--) if (x >= g_reg[k].x0 && x < g_reg[k].x1 && y >= g_reg[k].y0 - 1 && y < g_reg[k].y1 + 1) return &g_reg[k]; return NULL; }

typedef struct { int gx, gy, z, px, hy, hw; } Layout; /* the grid's origin and zoom; the panel's left edge; the help's top and width */

/* the help: what each key and button does, as key caps (bright) and what they do (dim), in three groups */
typedef struct { const char *key, *what; } Help;
static const Help H_MOUSE[] = {{"LEFT", "PAINT"}, {"RIGHT", "ERASE"}, {"ALT+LEFT", "PICK A COLOUR"}};
static const Help H_EDIT[] = {{"CTRL+Z", "UNDO"}, {"CTRL+Y", "REDO"}, {"< >", "OTHER SPRITE"}, {"TAB", "OTHER FILE"}, {"+ -", "ZOOM"}, {"#", "TYPE A COLOUR"}, {"F", "NEW FRAME"}};
static const Help H_SIZE[] = {{"W", "WIDER"}, {"N", "NARROWER"}, {"T", "TALLER"}, {"S", "SHORTER"}, {"Q", "QUIT (S SAVE / D DISCARD)"}};
static int help_row(Canvas *cv, int y, const char *title, const Help *h, int n, int W) { /* returns the rows it took */
    int x = 4, rows = 1;
    if (cv) canvas_text(cv, x, y + 1, title, RGB(110, 110, 130));
    x += 26;
    for (int i = 0; i < n; i++) {
        int kw = text_width(h[i].key) + 4, ww = text_width(h[i].what), need = kw + 3 + ww + 8;
        if (x > 30 && x + need > W - 2) { rows++; x = 30; y += 8; }
        if (cv) {
            canvas_rect(cv, x, y, kw, 7, RGB(200, 200, 215));
            canvas_rect(cv, x, y + 6, kw, 1, RGB(120, 120, 135));
            canvas_text(cv, x + 2, y + 1, h[i].key, RGB(20, 20, 28));
            canvas_text(cv, x + kw + 3, y + 1, h[i].what, RGB(150, 150, 170));
        }
        x += need;
    }
    return rows;
}
#define FIELD_W 96 /* the colour field (saturation across, brightness down) and the hue bar under it */
#define FIELD_H 44
/* the colour panel's height down to the end of SHADES (the real-size view below it fits itself in) */
static int panel_h(const Ed *e) { return 13 + FIELD_H + 3 + 8 + 11 + 7 + ((e->f.npal + 9) / 10) * 10 + 3 + 7 + 10; }
static int help_rows(int W) { return help_row(NULL, 0, "", H_MOUSE, 3, W) + help_row(NULL, 0, "", H_EDIT, 7, W) + help_row(NULL, 0, "", H_SIZE, 5, W); }
static void help(Canvas *cv, int y, int W) { /* the help from row y, wrapped at W */
    y += 8 * help_row(cv, y, "MOUSE", H_MOUSE, 3, W);
    y += 8 * help_row(cv, y, "EDIT", H_EDIT, 7, W);
    help_row(cv, y, "SIZE", H_SIZE, 5, W);
}

static int grid_zoom(const Ed *e, int room_w, int room_h) {
    int z = e->zoom;
    if (z <= 0) { z = 2; while (z + 2 <= 16 && e->w * (z + 2) <= room_w && e->h * (z + 2) <= room_h) z += 2; }
    return z;
}
/* The grid top left, the colour panel on its right, the help along the bottom. When the window is too short for the help
   under the panel, only a line "? SHORTCUTS  Q QUIT" is shown and ? opens the help in a box (hw = 0). */
static Layout layout(const Ed *e, int W, int H) {
    Layout L;
    L.gx = 4; L.gy = 12; L.hw = W;
    L.hy = H - help_rows(W) * 8 - 1;
    if (L.hy < L.gy + panel_h(e) + 2) { L.hw = 0; L.hy = H - 9; }
    L.z = grid_zoom(e, W - 120, L.hy - L.gy - 14);
    L.px = L.gx + e->w * L.z + 10;
    return L;
}

static Rgb pick_colour(const Ed *e, int *tr) { *tr = e->pk == 0; return e->pk == 1 ? e->f.pal_c[e->pi] : e->pk == 2 ? e->prgb : RGB(0, 0, 0); }
static void frame(Canvas *cv, int x, int y, int w, int h, Rgb c) { canvas_rect(cv, x - 1, y - 1, w + 2, 1, c); canvas_rect(cv, x - 1, y + h, w + 2, 1, c); canvas_rect(cv, x - 1, y - 1, 1, h + 2, c); canvas_rect(cv, x + w, y - 1, 1, h + 2, c); }
static void checker(Canvas *cv, int x, int y, int w, int h) { for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) canvas_px(cv, x + dx, y + dy, ((dx / 2) + (dy / 2)) % 2 ? RGB(70, 70, 80) : RGB(52, 52, 60)); }
static Rgb shade(Rgb c, int k) { /* k < 0 darker, k > 0 lighter */
    double f = k < 0 ? 1 + 0.17 * k : 0.17 * k;
    if (k < 0) return RGB((int)(c.r * f), (int)(c.g * f), (int)(c.b * f));
    return RGB((int)(c.r + (255 - c.r) * f), (int)(c.g + (255 - c.g) * f), (int)(c.b + (255 - c.b) * f));
}

static Rgb hsv_rgb(double h, double s, double v) {
    double c = v * s, x = c * (1 - fabs(fmod(h / 60, 2) - 1)), m = v - c, r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; } else if (h < 120) { r = x; g = c; } else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; } else if (h < 300) { r = x; b = c; } else { r = c; b = x; }
    return RGB((int)lround((r + m) * 255), (int)lround((g + m) * 255), (int)lround((b + m) * 255));
}
static void rgb_hsv(Rgb c, double keep_h, double *h, double *s, double *v) { /* a grey keeps the hue it had */
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0, mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b)), d = mx - mn;
    *v = mx; *s = mx > 0 ? d / mx : 0;
    if (d <= 0) *h = keep_h;
    else if (mx == r) *h = fmod(60 * ((g - b) / d) + 360, 360);
    else if (mx == g) *h = 60 * ((b - r) / d) + 120;
    else *h = 60 * ((r - g) / d) + 240;
}
static void button(Canvas *cv, int x, int y, const char *t, int on, int kind, int i) {
    int w = text_width(t) + 6;
    canvas_rect(cv, x, y, w, 9, on ? RGB(90, 90, 120) : RGB(34, 34, 44));
    if (on) frame(cv, x, y, w, 9, C_YELLOW);
    canvas_text(cv, x + 3, y + 2, t, on ? C_WHITE : C_GREY);
    region(kind, i, x, y, w, 9, RGB(0, 0, 0));
}

static void draw(const Ed *e, Canvas *cv) {
    g_nreg = 0;
    canvas_rect(cv, 0, 0, cv->w, cv->h, RGB(18, 18, 24));
    char t[160];
    snprintf(t, sizeof t, "EDIT %s  %s  %dX%d", e->name, FILES[e->file], e->w, e->h);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 4, 3, t, C_YELLOW);
    if (e->msg[0]) canvas_text(cv, cv->w - text_width(e->msg) - 4, 3, e->msg, C_GREY);
    Layout L = layout(e, cv->w, cv->h);
    for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) { /* the grid, see-through as a checker */
        int tr;
        Rgb c = colour_of(&e->f, e->g[y][x], &tr);
        for (int dy = 0; dy < L.z; dy++) for (int dx = 0; dx < L.z; dx++) {
            Rgb k = tr ? (((dx * 2 / L.z) + (dy * 2 / L.z)) % 2 ? RGB(70, 70, 80) : RGB(52, 52, 60)) : c;
            canvas_px(cv, L.gx + x * L.z + dx, L.gy + y * L.z + dy, k);
        }
    }
    frame(cv, L.gx, L.gy, e->w * L.z, e->h * L.z, RGB(90, 90, 110));
    if (e->hx >= 0 && e->hx < e->w && e->hy < e->h) frame(cv, L.gx + e->hx * L.z, L.gy + e->hy * L.z, L.z, L.z, RGB(255, 255, 255)); /* the pixel under the pointer */
    region(R_GRID, 0, L.gx, L.gy, e->w * L.z, e->h * L.z, RGB(0, 0, 0));
    /* under the grid: the pixel under the pointer */
    if (e->hx >= 0 && e->hx < e->w && e->hy < e->h) {
        int tr; char ch = e->g[e->hy][e->hx];
        Rgb c = colour_of(&e->f, ch, &tr);
        if (tr) snprintf(t, sizeof t, "X %d  Y %d  SEE-THROUGH", e->hx, e->hy);
        else snprintf(t, sizeof t, "X %d  Y %d  %d %d %d", e->hx, e->hy, c.r, c.g, c.b);
        canvas_text(cv, L.gx, L.gy + e->h * L.z + 4, t, C_GREY);
    }
    /* the panel (like Figma's colour picker): PAINT / ERASE / PICK, a colour field and a hue bar, the colour's hex and red
       green blue (click to type), the file's colours (+ ADD puts a new one in), shades, the real-size view */
    int x = L.px, y = L.gy, tr;
    Rgb cur = pick_colour(e, &tr);
    int body = e->pk == 1 && e->f.pal_body[e->pi];
    button(cv, x, y, "PAINT", !tr && !e->picking, R_TOOL, 0);
    button(cv, x + 26, y, "ERASE", tr && !e->picking, R_TOOL, 1);
    button(cv, x + 52, y, "PICK", e->picking, R_TOOL, 2);
    y += 13;
    double h, sat, val;
    rgb_hsv(cur, e->hue, &h, &sat, &val);
    for (int dy = 0; dy < FIELD_H; dy++) for (int dx = 0; dx < FIELD_W; dx++)
        canvas_px(cv, x + dx, y + dy, hsv_rgb(h, (double)dx / (FIELD_W - 1), 1 - (double)dy / (FIELD_H - 1)));
    if (!tr) frame(cv, x + (int)lround(sat * (FIELD_W - 1)) - 1, y + (int)lround((1 - val) * (FIELD_H - 1)) - 1, 3, 3, C_WHITE);
    region(R_FIELD, 0, x, y, FIELD_W, FIELD_H, RGB(0, 0, 0));
    y += FIELD_H + 3;
    for (int dx = 0; dx < FIELD_W; dx++) canvas_rect(cv, x + dx, y, 1, 4, hsv_rgb(dx * 359.0 / (FIELD_W - 1), 1, 1));
    canvas_rect(cv, x + (int)lround(h / 359 * (FIELD_W - 1)), y - 1, 1, 6, C_WHITE);
    region(R_HUE, 0, x, y - 1, FIELD_W, 6, RGB(0, 0, 0));
    y += 8;
    if (tr) checker(cv, x, y, 9, 7); else canvas_rect(cv, x, y, 9, 7, cur);
    frame(cv, x, y, 9, 7, RGB(200, 200, 215));
    char hex[12];
    if (e->typing == 1) snprintf(hex, sizeof hex, "#%s_", e->tbuf); else if (tr) snprintf(hex, sizeof hex, "ERASE"); else snprintf(hex, sizeof hex, "#%02X%02X%02X", cur.r, cur.g, cur.b);
    int hx = x + 12, hw = text_width("#DDDDDD_") + 4;
    canvas_rect(cv, hx, y, hw, 7, e->typing == 1 ? RGB(60, 60, 80) : RGB(34, 34, 44));
    if (e->typing == 1) frame(cv, hx, y, hw, 7, C_YELLOW);
    canvas_text(cv, hx + 2, y + 1, hex, e->typing == 1 ? C_YELLOW : C_WHITE);
    region(R_HEX, 0, hx, y, hw, 7, RGB(0, 0, 0));
    if (body) canvas_text(cv, hx + hw + 3, y + 1, "BODY", C_GREY);
    else if (!tr) for (int k = 0, nx = hx + hw + 3; k < 3; k++) { /* red, green, blue: click to type 0..255 */
        int v = k == 0 ? cur.r : k == 1 ? cur.g : cur.b;
        char num[8];
        if (e->typing == 2 + k) snprintf(num, sizeof num, "%s_", e->tbuf); else snprintf(num, sizeof num, "%d", v);
        int nw = text_width("255") + 2;
        if (e->typing == 2 + k) { canvas_rect(cv, nx - 1, y, nw + 2, 7, RGB(60, 60, 80)); frame(cv, nx - 1, y, nw + 2, 7, C_YELLOW); }
        canvas_text(cv, nx, y + 1, num, e->typing == 2 + k ? C_YELLOW : k == 0 ? RGB(230, 120, 120) : k == 1 ? RGB(120, 210, 130) : RGB(130, 160, 240));
        region(R_NUM, k, nx - 1, y, nw + 2, 7, RGB(0, 0, 0));
        nx += nw + 3;
    }
    y += 11;
    canvas_text(cv, x, y, "IN THIS FILE", RGB(110, 110, 130));
    if (e->pk == 2) { canvas_text(cv, x + FIELD_W - text_width("+ ADD"), y, "+ ADD", C_GREY); region(R_ADD, 0, x + FIELD_W - text_width("+ ADD") - 1, y - 1, text_width("+ ADD") + 2, 7, RGB(0, 0, 0)); }
    y += 7;
    for (int i = 0; i < e->f.npal; i++) {
        int sx = x + (i % 10) * 10, sy = y + (i / 10) * 10;
        canvas_rect(cv, sx, sy, 8, 8, e->f.pal_c[i]);
        region(R_FILE, i, sx, sy, 8, 8, e->f.pal_c[i]);
        int used = 0;
        for (int yy = 0; yy < e->h && !used; yy++) for (int xx = 0; xx < e->w; xx++) if (e->g[yy][xx] == e->f.pal_ch[i]) { used = 1; break; }
        if (used) canvas_rect(cv, sx + 3, sy + 9, 2, 1, RGB(200, 200, 215)); /* (used in this sprite) */
        if (e->pk == 1 && e->pi == i) frame(cv, sx, sy, 8, 8, C_WHITE);
    }
    y += ((e->f.npal + 9) / 10) * 10 + 3;
    canvas_text(cv, x, y, "SHADES", RGB(110, 110, 130));
    y += 7;
    for (int k = -3; k <= 3; k++) { /* darker to lighter */
        int sx = x + (k + 3) * 10;
        Rgb c = shade(cur, k);
        if (!tr) { canvas_rect(cv, sx, y, 8, 8, c); region(R_NEW, 0, sx, y, 8, 8, c); }
        if (k == 0 && !tr) frame(cv, sx, y, 8, 8, RGB(120, 120, 135));
    }
    y += 12;
    if (L.hw) help(cv, L.hy, L.hw);
    else { /* short window: one line, the rest behind ? */
        static const Help H_SHORT[] = {{"?", "SHORTCUTS"}, {"Q", "QUIT"}};
        help_row(cv, L.hy, "", H_SHORT, 2, cv->w);
    }
    int bottom = L.hy - 3, big = y + 8 + e->h * 2 <= bottom && x + e->w * 3 + 6 <= cv->w ? 2 : 1;
    if (y + 8 + e->h <= bottom) canvas_text(cv, x, y, "AS IN THE ROOM", RGB(110, 110, 130)); /* the sprite at its real size (and twice) */
    for (int k = 1; k <= big && y + 8 + e->h <= bottom; k++) {
        int ox = x + (k - 1) * (e->w + 6), oy = y + 8;
        canvas_rect(cv, ox, oy, e->w * k, e->h * k, RGB(196, 204, 218));
        for (int yy = 0; yy < e->h; yy++) for (int xx = 0; xx < e->w; xx++) { int t2; Rgb c = colour_of(&e->f, e->g[yy][xx], &t2); if (!t2) canvas_rect(cv, ox + xx * k, oy + yy * k, k, k, c); }
    }
    if (e->show_help && !L.hw) { /* ?: the shortcuts in a box over the editor */
        int bw = cv->w - 16 < 300 ? cv->w - 16 : 300, rows = help_rows(bw - 4), bh = rows * 8 + 14, bx = (cv->w - bw) / 2, by = (cv->h - bh) / 2;
        canvas_rect(cv, bx - 1, by - 1, bw + 2, bh + 2, C_GREY);
        canvas_rect(cv, bx, by, bw, bh, RGB(20, 20, 28));
        canvas_text(cv, bx + 4, by + 3, "SHORTCUTS  (? CLOSES)", C_YELLOW);
        /* the rows are laid out from x = 4: draw them on a canvas shifted into the box */
        Canvas box = canvas_new(bw, bh - 11, RGB(20, 20, 28));
        help(&box, 0, bw - 4);
        for (int yy = 0; yy < box.h; yy++) for (int xx = 0; xx < box.w; xx++) canvas_px(cv, bx + xx, by + 11 + yy, box.p[yy * box.w + xx]);
        canvas_free(&box);
    }
    if (e->asking) { /* q with changes */
        const char *a = "SAVE THE CHANGES?", *b = "S SAVE    D DISCARD    ESC BACK";
        int w = (text_width(b) > text_width(a) ? text_width(b) : text_width(a)) + 12, h = 24, bx = (cv->w - w) / 2, by = (cv->h - h) / 2;
        canvas_rect(cv, bx - 1, by - 1, w + 2, h + 2, C_GREY);
        canvas_rect(cv, bx, by, w, h, RGB(20, 20, 28));
        canvas_text(cv, bx + (w - text_width(a)) / 2, by + 4, a, C_YELLOW);
        canvas_text(cv, bx + (w - text_width(b)) / 2, by + 14, b, C_WHITE);
    }
}

static void set_file_colour(Ed *e, int i, Rgb c);
/* the colour to paint becomes c: a file colour is changed in the file (all its pixels); erase or the body: a new colour */
static void apply_colour(Ed *e, Rgb c) {
    if (e->pk == 1 && !e->f.pal_body[e->pi]) { push_undo(e); set_file_colour(e, e->pi, c); save(e); }
    else { e->pk = 2; e->prgb = c; for (int q = 0; q < e->f.npal; q++) if (!e->f.pal_body[q] && rgb_eq(e->f.pal_c[q], c)) { e->pk = 1; e->pi = q; } }
}

/* a file colour changed (the colour field, the hue bar, typing): its palette line too (every pixel using it changes, in every sprite of the file) */
static void set_file_colour(Ed *e, int i, Rgb c) {
    File *f = &e->f;
    f->pal_c[i] = c;
    for (int l = 0; l < f->nline; l++) if (f->line[l][0] == ':') {
        const char *p = f->line[l] + 1;
        while (*p == ' ') p++;
        if (*p != f->pal_ch[i]) continue;
        char buf[64];
        snprintf(buf, sizeof buf, ": %c = %d,%d,%d", f->pal_ch[i], c.r, c.g, c.b);
        free(f->line[l]); f->line[l] = strdup(buf);
        break;
    }
}

/* ---- the terminal ---------------------------------------------------------------------------------------------------- */
static struct termios g_old;
static void put(const char *s) { size_t n = strlen(s); while (n) { ssize_t k = write(STDOUT_FILENO, s, n); if (k <= 0) return; s += k; n -= (size_t)k; } }
static void tty_on(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &g_old);
    t = g_old; t.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN); t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0; /* (no signal keys: Ctrl+Z / Ctrl+Y are undo / redo, Ctrl+C asks like q) */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    put("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?1000h\x1b[?1003h\x1b[?1006h\x1b]0;Clawd Room pixel editor\x07"); /* alt screen, mouse (SGR) */
}
static void tty_off(void) { put("\x1b[?1006l\x1b[?1003l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"); tcsetattr(STDIN_FILENO, TCSADRAIN, &g_old); }
#include <sys/ioctl.h>
int editor_run(const char *dir, const char *name) {
    static Ed e;
    memset(&e, 0, sizeof e);
    e.pk = 1; e.pi = 0; e.hx = e.hy = -1; e.drag = -1; e.last_pk = 1;
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
                if (e.asking && in[i + 1] != '[') { e.asking = 0; i++; continue; } /* (Esc: back to editing) */
                if (e.typing && in[i + 1] != '[') { e.typing = 0; i++; continue; } /* (Esc: stop typing) */
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] == '<') { /* SGR mouse: ESC [ < b ; x ; y (M | m) */
                    int b, x, y, used = 0;
                    char fin;
                    if (sscanf(in + i, "\x1b[<%d;%d;%d%c%n", &b, &x, &y, &fin, &used) < 4 || !used) { if (strchr(in + i, 'M') || strchr(in + i, 'm')) { i++; continue; } break; }
                    i += used;
                    if (e.asking) continue;
                    int px = x - 1, py = (y - 1) * 2, btn = b & 3, alt = b & 8, motion = b & 32, release = fin == 'm';
                    const Region *r = hit(px, py);
                    e.hx = e.hy = -1;
                    if (r && r->kind == R_GRID) { e.hx = (px - L.gx) / L.z; e.hy = (py - L.gy) / L.z; }
                    if (release) { if (painting || e.drag >= 0) save(&e); painting = 0; e.drag = -1; continue; }
                    if (motion && btn == 3) continue; /* (just moving: the pointer's pixel is shown) */
                    if (e.drag >= 0 || (r && (r->kind == R_FIELD || r->kind == R_HUE) && !motion && btn == 0)) { /* the colour field or the hue bar */
                        int k = e.drag >= 0 ? e.drag : r->kind == R_HUE, x0 = 0, y0 = 0;
                        for (int q = 0; q < g_nreg; q++) if (g_reg[q].kind == (k ? R_HUE : R_FIELD)) { x0 = g_reg[q].x0; y0 = g_reg[q].y0; }
                        int t0;
                        Rgb was = pick_colour(&e, &t0);
                        double h, sat, val;
                        rgb_hsv(was, e.hue, &h, &sat, &val);
                        if (t0) { sat = 1; val = 1; } /* (from ERASE: a full colour) */
                        double fx = (double)(px - x0) / (FIELD_W - 1), fy = (double)(py - y0) / (FIELD_H - 1);
                        fx = fx < 0 ? 0 : fx > 1 ? 1 : fx; fy = fy < 0 ? 0 : fy > 1 ? 1 : fy;
                        if (k) h = fx * 359; else { sat = fx; val = 1 - fy; }
                        e.hue = h;
                        if (e.drag < 0) { e.drag = k; if (e.pk == 1 && !e.f.pal_body[e.pi]) push_undo(&e); else { e.pk = 2; e.prgb = was; } } /* (a file colour changes in the file; erase or the body: a new colour) */
                        Rgb c = hsv_rgb(h, sat, val);
                        if (e.pk == 1) set_file_colour(&e, e.pi, c); else e.prgb = c;
                        e.picking = 0;
                        continue;
                    }
                    if (r && r->kind == R_GRID && (alt || (e.picking && btn == 0))) { /* pick the colour under the pointer */
                        char ch = e.g[e.hy][e.hx];
                        e.pk = 0;
                        for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; }
                        e.picking = 0;
                        continue;
                    }
                    if (r && r->kind == R_GRID && (btn == 0 || btn == 2)) {
                        if (!painting) { push_undo(&e); painting = 1; }
                        char ch = '.';
                        if (btn == 0 && e.pk == 1) ch = e.f.pal_ch[e.pi];
                        else if (btn == 0 && e.pk == 2) { /* a new colour: it gets a letter in the file's palette */
                            ch = letter_for(&e, e.prgb);
                            for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; }
                        }
                        e.g[e.hy][e.hx] = ch;
                        continue;
                    }
                    if (r && !motion && (r->kind == R_HEX || r->kind == R_NUM)) { e.typing = r->kind == R_HEX ? 1 : 2 + r->i; e.tbuf[0] = 0; continue; } /* type a colour */
                    if (!motion && e.typing) e.typing = 0; /* (a click elsewhere: never mind) */
                    if (r && !motion) {
                        if (r->kind == R_TOOL && r->i == 0) { e.picking = 0; if (e.pk == 0) { e.pk = e.last_pk ? e.last_pk : 1; e.pi = e.last_pi; e.prgb = e.last_prgb; } }
                        else if (r->kind == R_TOOL && r->i == 1) { e.picking = 0; if (e.pk) { e.last_pk = e.pk; e.last_pi = e.pi; e.last_prgb = e.prgb; } e.pk = 0; }
                        else if (r->kind == R_TOOL && r->i == 2) e.picking = !e.picking;
                        else if (r->kind == R_ADD && e.pk == 2) { char ch = letter_for(&e, e.prgb); for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; } save(&e); }
                        else if (r->kind == R_FILE) { e.pk = 1; e.pi = r->i; e.picking = 0; }
                        else if (r->kind == R_NEW) { /* a colour already in the file is that one; else a new colour */
                            e.pk = 2; e.prgb = r->c;
                            for (int q = 0; q < e.f.npal; q++) if (!e.f.pal_body[q] && rgb_eq(e.f.pal_c[q], r->c)) { e.pk = 1; e.pi = q; }
                        }
                    }
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin) { i += 3; continue; } /* (arrows and the like: not used) */
                i += 1; continue;
            }
            char ch = in[i++];
            if (e.asking) { /* keep or discard */
                if (ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y') quit = 1; /* (save: they are in the file already) */
                else if (ch == 'd' || ch == 'D' || ch == 'n' || ch == 'N') { /* discard: put every changed file back as it was */
                    for (int f = 0; f < NFILES; f++) if (e.changed[f] && e.orig[f]) {
                        char path[700];
                        snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
                        FILE *o = fopen(path, "wb");
                        if (o) { fputs(e.orig[f], o); fclose(o); }
                    }
                    quit = 1;
                }
                continue;
            }
            if (e.typing) { /* typing a colour: hex digits or 0..255; Backspace, Enter */
                size_t n = strlen(e.tbuf);
                int hexd = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
                if ((ch == 127 || ch == 8) && n) e.tbuf[n - 1] = 0;
                else if (ch == '\r' || ch == '\n') {
                    int tr0;
                    Rgb c = pick_colour(&e, &tr0);
                    if (tr0) c = RGB(0, 0, 0);
                    if (e.typing == 1) { unsigned v; if (n == 6 && sscanf(e.tbuf, "%6x", &v) == 1) apply_colour(&e, RGB((v >> 16) & 255, (v >> 8) & 255, v & 255)); else if (n == 3 && sscanf(e.tbuf, "%3x", &v) == 1) apply_colour(&e, RGB(((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17)); }
                    else if (n) { int v = atoi(e.tbuf); v = v > 255 ? 255 : v; if (e.typing == 2) c.r = (uint8_t)v; else if (e.typing == 3) c.g = (uint8_t)v; else c.b = (uint8_t)v; apply_colour(&e, c); }
                    e.typing = 0;
                } else if (e.typing == 1 && hexd && n < 6) { e.tbuf[n] = (char)(ch >= 'a' ? ch - 32 : ch); e.tbuf[n + 1] = 0; }
                else if (e.typing > 1 && ch >= '0' && ch <= '9' && n < 3) { e.tbuf[n] = ch; e.tbuf[n + 1] = 0; }
                continue;
            }
            if (ch == '#') { e.typing = 1; e.tbuf[0] = 0; continue; }
            if (ch == '?') { e.show_help = !e.show_help; continue; }
            if (ch == 'f' || ch == 'F') { new_frame(&e); e.zoom = 0; continue; } /* a new frame (props with frames are animated in the room) */
            if (ch == 0x1A) ch = 'u'; /* Ctrl+Z: undo; Ctrl+Y: redo (U / R too) */
            else if (ch == 0x19) ch = 'r';
            else if (ch == 0x03) ch = 'q'; /* Ctrl+C: quit, asking about changes */
            if (ch == 'q') { int any = 0; for (int f = 0; f < NFILES; f++) any |= e.changed[f]; if (any) e.asking = 1; else quit = 1; }
            else if (ch == 'u' && e.nundo) { e.nundo--; swap_undo(&e, e.nundo); e.nredo++; save(&e); }
            else if (ch == 'r' && e.nredo) { swap_undo(&e, e.nundo); e.nundo++; e.nredo--; save(&e); }
            else if (ch == ']' || ch == '[' || ch == '>' || ch == '<') { sprite_open(&e, e.sp + (ch == ']' || ch == '>' ? 1 : -1)); e.zoom = 0; e.msg[0] = 0; }
            else if (ch == '\t') { file_open(&e, dir, e.file + 1, NULL); e.zoom = 0; e.pk = 1; e.pi = 0; e.msg[0] = 0; }
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
    for (int f = 0; f < NFILES; f++) free(e.orig[f]);
    return 0;
}

/* --edit list: what can be opened, by file */
int editor_list(const char *dir) {
    for (int f = 0; f < NFILES; f++) {
        static File tf;
        memset(&tf, 0, sizeof tf);
        char path[700];
        snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
        if (file_load(&tf, path)) continue;
        printf("%s (%d):\n ", FILES[f], tf.nsprite);
        int col = 1;
        for (int i = 0; i < tf.nsprite; i++) {
            const char *nm = tf.line[tf.sprite_line[i]] + 1;
            while (*nm == ' ') nm++;
            const char *bar = strchr(nm, '|');
            int nl = bar ? (int)(bar - nm) : (int)strlen(nm);
            while (nl && nm[nl - 1] == ' ') nl--;
            if (col + nl + 1 > 100) { printf("\n "); col = 1; }
            printf(" %.*s", nl, nm);
            col += nl + 1;
        }
        printf("\n\n");
        file_free(&tf);
    }
    printf("open one with: ./clawd-room.sh --edit NAME\n");
    return 0;
}
