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
    int pk, pi; Rgb prgb;                    /* the colour to paint: pk 0 erase, 1 the file's colour pi, 2 a new colour prgb */
    int hx, hy;                              /* the sprite pixel under the pointer (-1: none) */
    int slider;                              /* a colour slider being dragged (0..2), or -1 */
    int zoom, dirty_file;
    char msg[96];
    char *orig[NFILES]; int changed[NFILES]; /* each file as it was when first opened, to put back on discard */
    int asking;                               /* q with changes: keep or discard? */
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
enum { R_GRID, R_ERASE, R_FILE, R_NEW, R_SLIDER };
typedef struct { int kind, i, x0, y0, x1, y1; Rgb c; } Region;
static Region g_reg[160];
static int g_nreg;
static void region(int kind, int i, int x, int y, int w, int h, Rgb c) { if (g_nreg < 160) g_reg[g_nreg++] = (Region){kind, i, x, y, x + w, y + h, c}; }
static const Region *hit(int x, int y) { for (int k = g_nreg - 1; k >= 0; k--) if (x >= g_reg[k].x0 && x < g_reg[k].x1 && y >= g_reg[k].y0 - 1 && y < g_reg[k].y1 + 1) return &g_reg[k]; return NULL; }

typedef struct { int gx, gy, z, px; } Layout; /* the grid's origin and zoom; the panel's left edge */

/* the help: what each key and button does, as key caps (bright) and what they do (dim), in three groups */
typedef struct { const char *key, *what; } Help;
static const Help H_MOUSE[] = {{"LEFT", "PAINT"}, {"RIGHT", "ERASE"}, {"ALT+LEFT", "PICK A COLOUR"}};
static const Help H_EDIT[] = {{"U", "UNDO"}, {"R", "REDO"}, {"< >", "OTHER SPRITE"}, {"TAB", "OTHER FILE"}, {"+ -", "ZOOM"}};
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
static int help(Canvas *cv, int W, int H) { /* the rows the help takes (drawn at the bottom when cv is given) */
    int r = help_row(NULL, 0, "", H_MOUSE, 3, W) + help_row(NULL, 0, "", H_EDIT, 5, W) + help_row(NULL, 0, "", H_SIZE, 5, W);
    if (cv) {
        int y = H - r * 8 - 1;
        y += 8 * help_row(cv, y, "MOUSE", H_MOUSE, 3, W);
        y += 8 * help_row(cv, y, "EDIT", H_EDIT, 5, W);
        help_row(cv, y, "SIZE", H_SIZE, 5, W);
    }
    return r;
}

static Layout layout(const Ed *e, int W, int H) {
    Layout L;
    L.gx = 4; L.gy = 12;
    int room_w = W - 120, room_h = H - L.gy - 8 * help(NULL, W, H) - 14;
    L.z = e->zoom;
    if (L.z <= 0) { L.z = 2; while (L.z + 2 <= 16 && e->w * (L.z + 2) <= room_w && e->h * (L.z + 2) <= room_h) L.z += 2; }
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
    /* the panel: the colour to paint (with sliders), its shades, the file's colours, more colours, the real-size view */
    int x = L.px, y = L.gy, tr;
    Rgb cur = pick_colour(e, &tr);
    canvas_text(cv, x, y, "COLOUR", RGB(110, 110, 130));
    y += 7;
    if (tr) checker(cv, x, y, 16, 16); else canvas_rect(cv, x, y, 16, 16, cur);
    frame(cv, x, y, 16, 16, RGB(200, 200, 215));
    if (tr) canvas_text(cv, x + 20, y + 5, "ERASE", C_WHITE);
    else {
        int body = e->pk == 1 && e->f.pal_body[e->pi];
        snprintf(t, sizeof t, "%s", e->pk == 2 ? "NEW" : body ? "BODY" : "");
        if (e->pk == 1 && !body) snprintf(t, sizeof t, "%c", e->f.pal_ch[e->pi]);
        canvas_text(cv, x + 20, y, t, C_WHITE);
        if (!body) for (int k = 0; k < 3; k++) { /* sliders: red, green, blue */
            static const char *RGBN[3] = {"R", "G", "B"};
            int v = k == 0 ? cur.r : k == 1 ? cur.g : cur.b, sy = y + 6 + k * 4, sx = x + 26;
            canvas_text(cv, x + 20, sy - 1 + 0, "", C_GREY);
            canvas_rect(cv, sx, sy, 64, 3, RGB(40, 40, 50));
            canvas_rect(cv, sx, sy, v * 64 / 256, 3, k == 0 ? RGB(220, 70, 70) : k == 1 ? RGB(70, 200, 90) : RGB(80, 130, 240));
            canvas_rect(cv, sx + v * 64 / 256, sy - 1, 1, 5, C_WHITE);
            char num[8]; snprintf(num, sizeof num, "%s%d", RGBN[k], v);
            region(R_SLIDER, k, sx, sy - 1, 64, 5, RGB(0, 0, 0));
            (void)num;
        }
    }
    y += 20;
    canvas_text(cv, x, y, "SHADES", RGB(110, 110, 130));
    y += 7;
    for (int k = -3; k <= 3; k++) { /* darker to lighter */
        int sx = x + (k + 3) * 10;
        Rgb c = tr ? RGB(128, 128, 128) : shade(cur, k);
        if (!tr) { canvas_rect(cv, sx, y, 8, 8, c); region(R_NEW, 0, sx, y, 8, 8, c); }
        if (k == 0 && !tr) frame(cv, sx, y, 8, 8, RGB(120, 120, 135));
    }
    y += 12;
    canvas_text(cv, x, y, "IN THIS FILE", RGB(110, 110, 130));
    y += 7;
    checker(cv, x, y, 8, 8); region(R_ERASE, 0, x, y, 8, 8, RGB(0, 0, 0));
    if (e->pk == 0) frame(cv, x, y, 8, 8, C_WHITE);
    for (int i = 0; i < e->f.npal; i++) {
        int k = i + 1, sx = x + (k % 8) * 10, sy = y + (k / 8) * 10;
        canvas_rect(cv, sx, sy, 8, 8, e->f.pal_c[i]);
        region(R_FILE, i, sx, sy, 8, 8, e->f.pal_c[i]);
        int used = 0;
        for (int yy = 0; yy < e->h && !used; yy++) for (int xx = 0; xx < e->w; xx++) if (e->g[yy][xx] == e->f.pal_ch[i]) { used = 1; break; }
        if (used) canvas_rect(cv, sx + 3, sy + 9, 2, 1, RGB(200, 200, 215)); /* (used in this sprite) */
        if (e->pk == 1 && e->pi == i) frame(cv, sx, sy, 8, 8, C_WHITE);
    }
    y += ((e->f.npal + 1 + 7) / 8) * 10 + 4;
    canvas_text(cv, x, y, "MORE COLOURS", RGB(110, 110, 130));
    y += 7;
    for (int i = 0; i < 24; i++) { /* the 16-colour set and a grey ramp */
        Rgb c = i < 16 ? EXTRA[i] : RGB((i - 16) * 255 / 7, (i - 16) * 255 / 7, (i - 16) * 255 / 7);
        int sx = x + (i % 8) * 10, sy = y + (i / 8) * 10;
        canvas_rect(cv, sx, sy, 8, 8, c);
        region(R_NEW, 0, sx, sy, 8, 8, c);
        if (e->pk == 2 && rgb_eq(e->prgb, c)) frame(cv, sx, sy, 8, 8, C_WHITE);
    }
    y += 32;
    int bottom = cv->h - 8 * help(cv, cv->w, cv->h) - 3, big = y + 8 + e->h * 2 <= bottom && x + e->w * 3 + 6 <= cv->w ? 2 : 1;
    if (y + 8 + e->h <= bottom) canvas_text(cv, x, y, "AS IN THE ROOM", RGB(110, 110, 130)); /* the sprite at its real size (and twice) */
    for (int k = 1; k <= big && y + 8 + e->h <= bottom; k++) {
        int ox = x + (k - 1) * (e->w + 6), oy = y + 8;
        canvas_rect(cv, ox, oy, e->w * k, e->h * k, RGB(196, 204, 218));
        for (int yy = 0; yy < e->h; yy++) for (int xx = 0; xx < e->w; xx++) { int t2; Rgb c = colour_of(&e->f, e->g[yy][xx], &t2); if (!t2) canvas_rect(cv, ox + xx * k, oy + yy * k, k, k, c); }
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

/* a file colour changed by a slider: its palette line too (every pixel using it changes, in every sprite of the file) */
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
    t = g_old; t.c_lflag &= ~(tcflag_t)(ICANON | ECHO); t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    put("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?1000h\x1b[?1003h\x1b[?1006h\x1b]0;Clawd Room pixel editor\x07"); /* alt screen, mouse (SGR) */
}
static void tty_off(void) { put("\x1b[?1006l\x1b[?1003l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"); tcsetattr(STDIN_FILENO, TCSADRAIN, &g_old); }
#include <sys/ioctl.h>
int editor_run(const char *dir, const char *name) {
    static Ed e;
    memset(&e, 0, sizeof e);
    e.pk = 1; e.pi = 0; e.hx = e.hy = -1; e.slider = -1;
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
                    if (release) { if (painting || e.slider >= 0) save(&e); painting = 0; e.slider = -1; continue; }
                    if (motion && btn == 3) continue; /* (just moving: the pointer's pixel is shown) */
                    if (e.slider >= 0 || (r && r->kind == R_SLIDER && !motion)) { /* a colour slider: the colour to paint, or a file colour (every pixel using it) */
                        int k = e.slider >= 0 ? e.slider : r->i, sx0 = 0;
                        for (int q = 0; q < g_nreg; q++) if (g_reg[q].kind == R_SLIDER && g_reg[q].i == k) sx0 = g_reg[q].x0;
                        int v = (px - sx0) * 256 / 64;
                        v = v < 0 ? 0 : v > 255 ? 255 : v;
                        if (e.slider < 0) { push_undo(&e); e.slider = k; }
                        int t0;
                        Rgb c = pick_colour(&e, &t0);
                        if (k == 0) c.r = (uint8_t)v; else if (k == 1) c.g = (uint8_t)v; else c.b = (uint8_t)v;
                        if (e.pk == 1) set_file_colour(&e, e.pi, c); else e.prgb = c;
                        continue;
                    }
                    if (r && r->kind == R_GRID && alt) { /* pick the colour under the pointer */
                        char ch = e.g[e.hy][e.hx];
                        e.pk = 0;
                        for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; }
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
                    if (r && !motion) {
                        if (r->kind == R_ERASE) e.pk = 0;
                        else if (r->kind == R_FILE) { e.pk = 1; e.pi = r->i; }
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
