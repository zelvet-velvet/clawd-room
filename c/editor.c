/* The pixel editor (Engine Design, step 3): clawd-room --edit [NAME] edits one sprite of the sprite files in iTerm2 (or any
 * terminal with mouse reporting), drawn with the room's own pixel canvas. It saves as you draw, so running rooms (which read
 * the files again when they change) show the edit at once.
 *   mouse: left paints, right erases, Alt + click picks the colour under it; a swatch picks a colour; FILL fills the
 *          touching pixels of one colour (click FILL again: FILL ALL, every pixel of that colour)
 *   keys:  u undo, r redo, < > the frame before / after in the animation, [ ] the sprite before / after, Tab the next file,
 *          Space play, o onion skin, b fill, f new frame / x delete frame (props), + - zoom, w n wider / narrower,
 *          t s taller / shorter, q quit
 * An animation is the sprites named NAME-1, NAME-2 ... (Clawd frames, items) or NAME, NAME~2 ... (props); the timeline
 * along the bottom shows its frames. PLAY runs it at the room's speed: Clawd frames through the room's own poses
 * (actions.c), props every 0.3 s, items three times a second. */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include "actions.h"
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
    int playing; double play_t0, speed;       /* Space: the animation plays in the real-size view (speed 0.5, 1 or 2) */
    int onion;                                /* o: the frames before and after show faintly under the grid */
    int filling;                              /* FILL: 0 off, 1 the touching pixels of one colour, 2 every pixel of it */
    int del_ask;                              /* x once: the frame to delete if x comes again (-1 none) */
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

/* sprite sp's name and key, as in its line "@ NAME | key" */
static void sprite_name(const File *f, int sp, char *out, size_t n) {
    const char *nm = f->line[f->sprite_line[sp]] + 1;
    while (*nm == ' ') nm++;
    const char *bar = strchr(nm, '|');
    size_t nl = bar ? (size_t)(bar - nm) : strlen(nm);
    while (nl && nm[nl - 1] == ' ') nl--;
    snprintf(out, n, "%.*s", (int)nl, nm);
}
static void sprite_key(const File *f, int sp, char *out, size_t n) {
    const char *bar = strchr(f->line[f->sprite_line[sp]], '|');
    if (!bar) { sprite_name(f, sp, out, n); return; }
    bar++;
    while (*bar == ' ') bar++;
    snprintf(out, n, "%s", bar);
}
/* sprite sp's grid as palette characters ('.' where nothing was written) */
static void grid_of(const File *f, int sp, char g[MAXH][MAXW + 1], int *w, int *h) {
    *w = *h = 0;
    memset(g, '.', sizeof(char) * MAXH * (MAXW + 1));
    for (int i = f->sprite_line[sp] + 1; i < f->nline && f->line[i][0] && f->line[i][0] != '@' && f->line[i][0] != '#' && f->line[i][0] != ':' && *h < MAXH; i++) {
        int n = (int)strlen(f->line[i]);
        if (n > MAXW) n = MAXW;
        memcpy(g[*h], f->line[i], (size_t)n);
        if (n > *w) *w = n;
        (*h)++;
    }
    for (int y = 0; y < MAXH; y++) g[y][MAXW] = 0;
}

static void sprite_open(Ed *e, int sp) {
    File *f = &e->f;
    if (f->nsprite == 0) { e->w = e->h = 0; e->name[0] = 0; return; }
    e->sp = (sp + f->nsprite) % f->nsprite;
    sprite_name(f, e->sp, e->name, sizeof e->name);
    grid_of(f, e->sp, e->g, &e->w, &e->h);
    e->nundo = e->nredo = 0;
    e->del_ask = -1;
}

/* ---- animations: NAME-1, NAME-2 ... (Clawd frames, items) or NAME, NAME~2 ... (props) ------------------------------------ */
/* the animation a name is a frame of and its number; returns 1 for NAME-N, 2 for NAME~N, 0 for a name without a number */
static int split_name(const char *name, char *base, size_t n, int *num) {
    const char *t = strrchr(name, '~'), *d = strrchr(name, '-');
    const char *s = t && (!d || t > d) ? t : d;
    if (s && s[1] >= '0' && s[1] <= '9') {
        const char *q = s + 1;
        while (*q >= '0' && *q <= '9') q++;
        if (!*q) { snprintf(base, n, "%.*s", (int)(s - name), name); *num = atoi(s + 1); return *s == '~' ? 2 : 1; }
    }
    snprintf(base, n, "%s", name); *num = 1;
    return 0;
}
/* the frames of sprite sp's animation in order, as sprite numbers; returns how many (1: a sprite on its own) */
static int anim_frames(const File *f, int sp, int *out, int max) {
    char name[64], base[64], b2[64], n2[64];
    int num, k = 0, nums[128];
    if (max > 128) max = 128;
    sprite_name(f, sp, name, sizeof name);
    int kind = split_name(name, base, sizeof base, &num);
    for (int i = 0; i < f->nsprite && k < max; i++) {
        sprite_name(f, i, n2, sizeof n2);
        int kd = split_name(n2, b2, sizeof b2, &num);
        if (strcmp(b2, base)) continue;
        if (kind == 1 ? kd != 1 : kd == 1) continue; /* (NAME-N frames go together; NAME and its NAME~N frames go together) */
        int at = k++;
        while (at > 0 && nums[at - 1] > num) { nums[at] = nums[at - 1]; out[at] = out[at - 1]; at--; }
        nums[at] = num; out[at] = i;
    }
    if (k == 0) { out[0] = sp; k = 1; }
    return k;
}
static int frame_index(const int *fr, int n, int sp) { for (int i = 0; i < n; i++) if (fr[i] == sp) return i; return 0; }
/* the sprite called NAME, or else the first frame of the animation called NAME (--edit walk); -1 if none */
static int find_sprite(const File *f, const char *name) {
    char n2[64], b2[64];
    int num;
    for (int i = 0; i < f->nsprite; i++) { sprite_name(f, i, n2, sizeof n2); if (!strcmp(n2, name)) return i; }
    for (int i = 0; i < f->nsprite; i++) {
        sprite_name(f, i, n2, sizeof n2);
        if (split_name(n2, b2, sizeof b2, &num) && !strcmp(b2, name)) { int fr[128]; anim_frames(f, i, fr, 128); return fr[0]; }
    }
    return -1;
}

static void file_open(Ed *e, const char *dir, int which, const char *name) {
    char path[700];
    e->file = (which + NFILES) % NFILES;
    snprintf(path, sizeof path, "%s/%s", dir, FILES[e->file]);
    if (file_load(&e->f, path)) snprintf(e->msg, sizeof e->msg, "CANNOT READ %s", FILES[e->file]);
    if (!e->orig[e->file]) e->orig[e->file] = slurp(path); /* (kept for a discard) */
    int sp = name ? find_sprite(&e->f, name) : 0;
    sprite_open(e, sp < 0 ? 0 : sp);
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

/* x x: delete the open frame of a prop (not its first: that is the prop); the frames after it are numbered on, so they
 * stay NAME~2, ~3 ... without a gap (the room stops at the first missing number) */
static int delete_frame(Ed *e) {
    File *f = &e->f;
    int fr[128], n = anim_frames(f, e->sp, fr, 128), k = frame_index(fr, n, e->sp);
    if (k == 0) return -1;
    char base[64], name[64];
    int num;
    sprite_name(f, e->sp, name, sizeof name);
    split_name(name, base, sizeof base, &num);
    int at = f->sprite_line[e->sp], end = at + 1;
    while (end < f->nline && f->line[end][0] && f->line[end][0] != '@' && f->line[end][0] != '#' && f->line[end][0] != ':') end++;
    int from = at > 0 && !f->line[at - 1][0] ? at - 1 : at; /* (and the blank line before it) */
    for (int i = from; i < end; i++) free(f->line[i]);
    memmove(&f->line[from], &f->line[end], sizeof(char *) * (size_t)(f->nline - end));
    f->nline -= end - from;
    for (int j = k + 1; j < n; j++) { /* the frames after it: one number down */
        int ln = f->sprite_line[fr[j]] - (f->sprite_line[fr[j]] > at ? end - from : 0);
        char head[160];
        snprintf(head, sizeof head, "@ %s~%d | %s~%d", base, j, base, j);
        free(f->line[ln]); f->line[ln] = strdup(head);
    }
    FILE *out = fopen(f->path, "w");
    if (!out) return -1;
    for (int i = 0; i < f->nline; i++) fprintf(out, "%s\n", f->line[i]);
    fclose(out);
    char path[700];
    snprintf(path, sizeof path, "%s", f->path);
    file_load(f, path);
    e->changed[e->file] = 1;
    char prev[80]; /* the frame before it: NAME, or NAME~K (frame K, at index K - 1) */
    if (k == 1) snprintf(prev, sizeof prev, "%s", base); else snprintf(prev, sizeof prev, "%s~%d", base, k);
    int sp = find_sprite(f, prev);
    sprite_open(e, sp < 0 ? 0 : sp);
    snprintf(e->msg, sizeof e->msg, "DELETED %s", name);
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
enum { R_GRID, R_TOOL, R_FILE, R_NEW, R_FIELD, R_HUE, R_HEX, R_NUM, R_ADD, R_FRAME, R_FADD, R_PLAY, R_SPEED, R_ONION, R_STRIP, R_DIV };
typedef struct { int kind, i, x0, y0, x1, y1; Rgb c; } Region;
static Region g_reg[320];
static int g_nreg;
static void region(int kind, int i, int x, int y, int w, int h, Rgb c) { if (g_nreg < 320) g_reg[g_nreg++] = (Region){kind, i, x, y, x + w, y + h, c}; }
static const Region *hit(int x, int y) { for (int k = g_nreg - 1; k >= 0; k--) if (x >= g_reg[k].x0 && x < g_reg[k].x1 && y >= g_reg[k].y0 - 1 && y < g_reg[k].y1 + 1) return &g_reg[k]; return NULL; }

typedef struct { int gx, gy, z, px, hy, hw, ty, ah; } Layout; /* the grid's origin and zoom; the panel's left edge; the help's top and width;
                                                              the top of the canvas and the panel, and their height (the same) */

/* the help: what each key and button does, as key caps (bright) and what they do (dim), in three groups */
typedef struct { const char *key, *what; } Help;
static const Help H_MOUSE[] = {{"LEFT", "PAINT"}, {"RIGHT", "ERASE"}, {"ALT+LEFT", "PICK A COLOUR"}};
static const Help H_EDIT[] = {{"CTRL+Z", "UNDO"}, {"CTRL+Y", "REDO"}, {"[ ]", "OTHER SPRITE"}, {"TAB", "OTHER FILE"}, {"B", "FILL"}, {"+ -", "ZOOM"}, {"#", "TYPE A COLOUR"}};
static const Help H_ANIM[] = {{"LEFT RIGHT", "FRAME"}, {"SPACE", "PLAY"}, {"O", "ONION SKIN"}, {"F", "NEW FRAME"}, {"X X", "DELETE FRAME"}};
#define NH(a) ((int)(sizeof a / sizeof a[0]))
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
static int g_field_w = 96; /* the colour field (saturation across, brightness down) and the hue bar under it: 96 to 160 wide, */
#define FIELD_W g_field_w   /* as wide as the window leaves (set by draw) */
static int g_field_h = 44; /* ... and 16 to 44 high: shorter when the window is, so the frames along the bottom stay */
#define FIELD_H g_field_h
/* the colour panel's height down to the end of SHADES (the real-size view below it fits itself in) */
#define TOOL_ROW 23 /* the tools: a key cap (13), a gap, its name */
#define SECTION 12 /* a section's title (dim), a gap, a line under it, a gap; then its swatches */
#define SW_ROW 13  /* rows of the file's colours, so a ring never touches the row above */
static int panel_h(const Ed *e) { return TOOL_ROW + FIELD_H + 3 + 8 + 11 + SECTION + ((e->f.npal + 9) / 10) * SW_ROW + 2 + SECTION + 10; }
static int help_rows(int W) { return help_row(NULL, 0, "", H_MOUSE, NH(H_MOUSE), W) + help_row(NULL, 0, "", H_EDIT, NH(H_EDIT), W) + help_row(NULL, 0, "", H_ANIM, NH(H_ANIM), W) + help_row(NULL, 0, "", H_SIZE, NH(H_SIZE), W); }
static void help(Canvas *cv, int y, int W) { /* the help from row y, wrapped at W */
    y += 8 * help_row(cv, y, "MOUSE", H_MOUSE, NH(H_MOUSE), W);
    y += 8 * help_row(cv, y, "EDIT", H_EDIT, NH(H_EDIT), W);
    y += 8 * help_row(cv, y, "FRAMES", H_ANIM, NH(H_ANIM), W);
    help_row(cv, y, "SIZE", H_SIZE, NH(H_SIZE), W);
}

static int grid_zoom(const Ed *e, int room_w, int room_h) {
    int z = e->zoom;
    if (z <= 0) { z = 2; while (z + 1 <= 16 && e->w * (z + 1) <= room_w && e->h * (z + 1) <= room_h) z++; } /* (as big as it fits: the canvas first) */
    return z;
}
/* The grid top left, the colour panel on its right, the help along the bottom. When the window is too short for the help
   under the panel, only a line "? SHORTCUTS  Q QUIT" is shown and ? opens the help in a box (hw = 0). */
/* the timeline along the bottom: a line (the name and frame, the buttons), the frames, their numbers, a scroll bar. The
 * frames have one size for the window, as big as its height allows (2x or 1x; the timeline at most a quarter of it), whatever their
 * number: more than fit scroll sideways. A short window gets only the line, a very short one no timeline. */
static int panel_h(const Ed *e);
static int tl_full(const Ed *e, int z) { return 15 + e->h * z + 9 + 3; }
/* The divider above the timeline can be dragged (g_tl_user: the height asked for; 0 the automatic one, double-click it to
 * go back): the frames and the sprite on the left grow with it, the canvas above gives up the room, never all of it. */
static int g_tl_user, g_div_drag, g_div_hover;
#define TOP_LEAST 64 /* a dragged timeline leaves the canvas and the panel at least this: the tools, the colour field, the hex line */
static int tl_most(const Ed *e, int H) { (void)e; return H - 12 - TOP_LEAST; }
static int thumb_zoom(const Ed *e, int H);
static int timeline_h(const Ed *e, int H) {
    if (g_tl_user) { int lo = tl_full(e, 1), hi = tl_most(e, H), h = g_tl_user < lo ? lo : g_tl_user > hi ? hi : g_tl_user; return h >= lo ? h : 0; }
    int full = tl_full(e, thumb_zoom(e, H)), rest = panel_h(e) - FIELD_H + 16;
    return H - 12 - full >= rest ? full : H - 12 - 14 >= rest ? 14 : 0;
}
static int thumb_zoom(const Ed *e, int H) {
    if (g_tl_user) { int th = timeline_h(e, H), z = 1; while (z < 8 && tl_full(e, z + 1) <= th) z++; return z; } /* (as big as the dragged height allows) */
    int rest = panel_h(e) - FIELD_H + 16; /* (the panel with its smallest colour field) */
    if (H - 12 - tl_full(e, 2) >= rest && tl_full(e, 2) <= H / 4) return 2;
    return 1;
}
static int g_tl_first, g_tl_seen = -1; /* the first frame the strip shows (scrolled), and the frame it last kept in view */
/* The canvas and the colour panel side by side, the same height: the panel's colour field (16 to 44 high) makes it as tall
 * as the canvas; a canvas shorter than the smallest panel is drawn in a frame as tall as the panel, the sprite in its middle */
static Layout layout(const Ed *e, int W, int H) {
    int th = timeline_h(e, H), rest = panel_h(e) - FIELD_H;
    H -= th;
    Layout L;
    L.gx = 4; L.ty = 12; L.hw = W;
    L.hy = H - help_rows(W) * 8 - 1;
    if (L.hy < L.ty + rest + 44 + 2) { L.hw = 0; L.hy = th ? H - 1 : H - 9; } /* (short: "? SHORTCUTS  Q QUIT" goes in the timeline's top line) */
    int most = L.hy - 3 - L.ty - rest; /* (the tallest colour field there is room for) */
    most = most < 16 ? 16 : most > 44 ? 44 : most;
    L.z = grid_zoom(e, W - 118, L.hy - L.ty - 14); /* (the panel keeps what it needs: 100 pixels, its widest row of colours) */
    int gh = e->h * L.z, f = gh + 2 - rest; /* (the panel's last row ends 2 pixels above its height) */
    g_field_h = f < 16 ? 16 : f > most ? most : f;
    int ph = rest + g_field_h - 2;
    L.ah = gh > ph ? gh : ph;
    if (L.ah > L.hy - 5 - L.ty) L.ah = L.hy - 5 - L.ty; /* (a dragged timeline: the panel shows what still fits; room for the handle) */
    if (L.ah < gh) L.ah = gh;
    int gap = ((L.hy - 1) - 9 - L.ah) / 2; /* (the canvas and the panel in the middle between the title and the timeline; the
                                            pointer's line goes in the gap under them, if it fits there) */
    if (gap >= 10) L.ty = 9 + gap;
    L.gy = L.ty + (L.ah - gh) / 2;
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

/* the tools as key caps with a pixel icon (9 x 9): a pencil, an eraser, an eyedropper, a paint bucket, and the bucket
 * with a sparkle for FILL ALL; the one in use is pressed in, with a yellow line under it.
 * k outline, w light, y / Y pencil, m metal, L lead, p / P eraser, r bulb, b blue paint */
static const char *const TOOL_ICON[5][9] = { /* 9 x 9 */
    {"......kk.", ".....kppk", "....kyykk", "...kyyyk.", "..kyyyk..", ".kwyyk...", ".kwwk....", "kLkk.....", "k........"},
    {".........", "....kkkk.", "...kwwwwk", "..kwwwwk.", ".kppppk..", "kppppk...", "kPPPk....", ".kkk.....", "........."},
    {"......rr.", ".....rrrr", "....mrrr.", "...kmm...", "..kwwk...", ".kwbk....", ".kbk.....", "b........", "........."},
    {"..kkk....", ".k...k...", "k.....k..", "kmmmmmkb.", "kwwwwwkb.", ".kwbbk.b.", ".kwwwk.bb", "..kkk..bb", "........."},
    {"..kkk..y.", ".k...kyyy", "k.....ky.", "kmmmmmkb.", "kwwwwwkb.", ".kwbbk.b.", ".kwwwk.bb", "..kkk..bb", "........."},
};
static Rgb tool_colour(char ch) {
    switch (ch) {
        case 'k': return RGB(16, 16, 22);   case 'w': return RGB(236, 236, 244); case 'y': return RGB(240, 196, 90);
        case 'Y': return RGB(214, 160, 60); case 'm': return RGB(180, 184, 196); case 'L': return RGB(60, 60, 70);
        case 'p': return RGB(240, 150, 170); case 'P': return RGB(200, 110, 130); case 'r': return RGB(230, 90, 80);
        case 'b': return RGB(110, 170, 250);
    }
    return RGB(255, 0, 255);
}
static void tool_button(Canvas *cv, int x, int y, int which, int on) {
    int w = 13, h = 13, dy = on ? 1 : 0;
    canvas_rect(cv, x + 1, y + 1, w - 2, h, RGB(14, 14, 20)); canvas_rect(cv, x, y + 2, w, h - 2, RGB(14, 14, 20)); /* (its shadow) */
    Rgb cap = on ? RGB(70, 72, 100) : RGB(52, 54, 70);
    canvas_rect(cv, x + 1, y + dy, w - 2, h - dy, cap); canvas_rect(cv, x, y + dy + 1, w, h - dy - 2, cap);    /* (rounded corners) */
    canvas_rect(cv, x + 2, y + dy, w - 4, 1, on ? RGB(110, 112, 145) : RGB(86, 88, 110));                   /* (light on its top) */
    for (int yy = 0; yy < 9; yy++) for (int xx = 0; xx < 9 && TOOL_ICON[which][yy][xx]; xx++)
        if (TOOL_ICON[which][yy][xx] != '.') canvas_px(cv, x + 2 + xx, y + 2 + dy + yy, tool_colour(TOOL_ICON[which][yy][xx]));
    if (on) canvas_rect(cv, x + 2, y + h + 1, w - 4, 1, C_YELLOW);
}

/* a section of the panel: its title, dim, and a line under it as wide as the colour field */
static void section(Canvas *cv, int x, int y, const char *title) {
    canvas_text(cv, x, y, title, RGB(84, 84, 104));
    canvas_rect(cv, x, y + 7, FIELD_W, 1, RGB(40, 40, 52));
}

static double wall_now(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec + tv.tv_usec / 1e6; }

/* the frame PLAY shows now (a sprite number; -1: a Clawd pose the file has no frame for, drawn by the code like the room
 * does); *p the Clawd's pose (for its bob, and the drawing by code) when the animation is one of the room's actions */
static int play_frame(const Ed *e, double now, Pose *p, int *is_pose) {
    int fr[128], n = anim_frames(&e->f, e->sp, fr, 128), num;
    char name[64], base[64], key[128], k2[128];
    double t = (now - e->play_t0) * e->speed;
    sprite_name(&e->f, e->sp, name, sizeof name);
    split_name(name, base, sizeof base, &num);
    *is_pose = 0;
    if (!strcmp(FILES[e->file], "clawd.txt")) for (int a = 0; a < N_ACTIONS; a++) if (!strcmp(ACTIONS[a].name, base)) {
        Session s; /* (the same stand-in as the sprite sheet: actions.c) */
        session_init(&s, "x", 1000.0);
        s.seed = 0.0; s.state_t = 1000.0;
        ACTIONS[a].set(&s);
        double tt = 0.2 + t;
        s.walk_phase = tt * 12;
        *p = pose_for(&s, ACTIONS[a].state, tt, 1000.0 + tt);
        *is_pose = 1;
        pose_key(p, key, sizeof key);
        for (int i = 0; i < e->f.nsprite; i++) { sprite_key(&e->f, i, k2, sizeof k2); if (!strcmp(k2, key)) return i; }
        return -1;
    }
    double per = !strcmp(FILES[e->file], "items.txt") ? 1 / 3.0 : 0.3; /* (props: every 0.3 s; the thinking dots three a second) */
    return fr[(int)(t / per) % n];
}

/* a sprite's pixels: the open one as it is being drawn, the others from the file */
static void grid_now(const Ed *e, int sp, char g[MAXH][MAXW + 1], int *w, int *h) {
    if (sp == e->sp) { memcpy(g, e->g, sizeof e->g); *w = e->w; *h = e->h; }
    else grid_of(&e->f, sp, g, w, h);
}
static void draw_grid_at(Canvas *cv, const Ed *e, char g[MAXH][MAXW + 1], int w, int h, int x, int y, int z, int see) {
    if (see) checker(cv, x, y, w * z, h * z);
    for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) { int tr; Rgb c = colour_of(&e->f, g[yy][xx], &tr); if (!tr) canvas_rect(cv, x + xx * z, y + yy * z, z, z, c); }
}

/* the sprite as the room shows it, k times bigger at (x, y), pad rows above it; while PLAY is on, the frame playing (a
 * Clawd with its bounce) */
static int is_action(const Ed *e) {
    char name[64], base[64]; int num;
    sprite_name(&e->f, e->sp, name, sizeof name); split_name(name, base, sizeof base, &num);
    if (strcmp(FILES[e->file], "clawd.txt")) return 0;
    for (int a = 0; a < N_ACTIONS; a++) if (!strcmp(ACTIONS[a].name, base)) return 1;
    return 0;
}
static void draw_preview(Canvas *cv, const Ed *e, double now, int x, int y, int k, int pad) {
    static char pg[MAXH][MAXW + 1];
    int pw = e->w, ph = e->h, psp = e->sp, ip = 0;
    Pose pose;
    memset(&pose, 0, sizeof pose);
    if (e->playing) psp = play_frame(e, now, &pose, &ip);
    if (psp >= 0) grid_now(e, psp, pg, &pw, &ph);
    Canvas c1 = canvas_new(e->w, e->h + pad, RGB(196, 204, 218)); /* drawn at 1x, then shown k times bigger */
    if (psp >= 0) draw_grid_at(&c1, e, pg, pw, ph, 0, pad + (pad ? pose.bob : 0), 1, 0);
    else draw_clawd(&c1, 7, pad + 1, &pose, NULL, C_CLAWD, NULL, ""); /* a pose the file has no frame for: by the code, as the room does */
    for (int yy = 0; yy < c1.h; yy++) for (int xx = 0; xx < c1.w; xx++) canvas_rect(cv, x + xx * k, y + yy * k, k, k, c1.p[yy * c1.w + xx]);
    canvas_free(&c1);
}

/* FILL: the pixels a click at (x, y) would fill: the touching ones of its colour, or (FILL ALL) every one of it */
static int fill_area(const Ed *e, int x, int y, unsigned char m[MAXH][MAXW]) {
    memset(m, 0, sizeof(unsigned char) * MAXH * MAXW);
    char ch = e->g[y][x];
    int n = 0;
    if (e->filling == 2) { for (int yy = 0; yy < e->h; yy++) for (int xx = 0; xx < e->w; xx++) if (e->g[yy][xx] == ch) { m[yy][xx] = 1; n++; } return n; }
    static int q[MAXW * MAXH];
    q[n++] = y * MAXW + x; m[y][x] = 1;
    for (int i = 0; i < n; i++) for (int d = 0; d < 4; d++) {
        int xx = q[i] % MAXW + (d == 0) - (d == 1), yy = q[i] / MAXW + (d == 2) - (d == 3);
        if (xx < 0 || yy < 0 || xx >= e->w || yy >= e->h || m[yy][xx] || e->g[yy][xx] != ch) continue;
        m[yy][xx] = 1; q[n++] = yy * MAXW + xx;
    }
    return n;
}

/* the timeline along the bottom: the animation's name and frame, PLAY, the speed, ONION; the frames (click one to open
 * it; the open one framed in yellow, the one playing marked under it); + adds a frame (props) */
/* the timeline along the bottom. On the right: the animation's name and frame, PLAY, the speed, ONION, and under them its
 * frames (click one to open it; the open one framed in yellow, the one playing marked under it; + adds a frame to a
 * prop). On the left: the short help ("? SHORTCUTS  Q QUIT") in a short window, and the frame playing when the panel has
 * no room for the real-size view. */
static void draw_timeline(Canvas *cv, const Ed *e, double now, int preview, int short_help) {
    int fr[128], n = anim_frames(&e->f, e->sp, fr, 128), at = frame_index(fr, n, e->sp), num, props = !strcmp(FILES[e->file], "props.txt");
    int th = timeline_h(e, cv->h), y0 = cv->h - th, y = y0 + 3, full = th >= tl_full(e, 1);
    if (!th) return;
    char name[64], base[64], t[96];
    sprite_name(&e->f, e->sp, name, sizeof name);
    split_name(name, base, sizeof base, &num);
    Rgb div = g_div_drag || g_div_hover ? RGB(120, 120, 150) : RGB(44, 44, 56);
    canvas_rect(cv, 0, y0, cv->w, 1, div); /* the divider: drag it to make the timeline taller or shorter; a grip in its middle */
    { /* (a handle: a short rounded bar on the line, brighter under the pointer) */
        Rgb hk = g_div_drag || g_div_hover ? RGB(170, 170, 200) : RGB(74, 74, 92);
        int hx = cv->w / 2 - 10;
        canvas_rect(cv, hx + 1, y0 - 1, 18, 3, hk); canvas_rect(cv, hx, y0, 20, 1, hk);
    }
    region(R_DIV, 0, 0, y0 - 2, cv->w, 5, RGB(0, 0, 0));
    if (n > 1) snprintf(t, sizeof t, "[ %s ]  FRAME %d OF %d", base, at + 1, n); else snprintf(t, sizeof t, "[ %s ]  ONE FRAME", base);
    for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
    /* the left column: the short help, under it the sprite as in the room, as big as the column lets it be */
    const char *sh = cv->w >= 220 ? "? SHORTCUTS  Q QUIT" : "? HELP  Q QUIT";
    int lw = text_width(sh) + 16, sx0 = lw, sx1 = cv->w - 4; /* (the block on the right, from sx0 to sx1; a line between, at sx0 - 8) */
    canvas_rect(cv, sx0 - 8, y0 + 1, 1, th - 1, RGB(40, 40, 52));
    if (short_help) canvas_text(cv, 4, y + 2, sh, RGB(150, 150, 170));
    /* the block on the right: the name and frame at its left, PLAY / the speed / ONION at its right */
    canvas_text(cv, sx0, y + 2, t, C_YELLOW);
    const char *play = e->playing ? "STOP" : "PLAY", *sp = e->speed < 0.75 ? "0.5X" : e->speed > 1.5 ? "2X" : "1X";
    int bw = text_width("ONION") + 9 + text_width(sp) + 9 + text_width(play) + 6, bx = sx1 - bw; /* (ONION, the speed, PLAY) */
    if (sx0 + text_width(t) + 8 + bw > sx1) { /* (no room: the short way, NAME 2/9) */
        canvas_rect(cv, sx0, y + 1, text_width(t), 7, RGB(18, 18, 24));
        if (n > 1) snprintf(t, sizeof t, "[ %s ] %d/%d", base, at + 1, n); else snprintf(t, sizeof t, "[ %s ]", base);
        for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        canvas_text(cv, sx0, y + 2, t, C_YELLOW);
    }
    if (bx < sx0 + text_width(t) + 8) bx = sx0 + text_width(t) + 8;
    button(cv, bx, y, "ONION", e->onion, R_ONION, 0); bx += text_width("ONION") + 9;
    button(cv, bx, y, sp, 0, R_SPEED, 0); bx += text_width(sp) + 9;
    button(cv, bx, y, play, e->playing, R_PLAY, 0);
    if (!full) return; /* (short window: the line only) */
    int z = thumb_zoom(e, cv->h), gap = 6, ty = y + 12, bottom = cv->h - 2;
    if (preview) { /* (playing: the frame playing) */
        int py = short_help ? ty : y, ah = bottom - py, aw = lw - 16, pz = aw / e->w < ah / e->h ? aw / e->w : ah / e->h; /* (left of the line, a gap to it) */
        if (pz < 1) pz = 1;
        draw_preview(cv, e, now, 4 + (aw - e->w * pz) / 2, py, pz, 0);
    }
    /* the strip: the frames from the first one scrolled to; it keeps the open frame in view when that changes */
    static char g[MAXH][MAXW + 1];
    int fwid[128];
    for (int i = 0; i < n; i++) { int w, h; grid_of(&e->f, fr[i], g, &w, &h); fwid[i] = w * z + gap; }
    int stripw = sx1 - sx0 + gap, maxfirst = n - 1;
    for (int wsum = 0; maxfirst >= 0 && wsum + fwid[maxfirst] <= stripw; maxfirst--) wsum += fwid[maxfirst];
    maxfirst++; /* (the first frame from which all the rest fit) */
    if (e->sp != g_tl_seen) { /* the open frame changed: bring it into view */
        if (at < g_tl_first) g_tl_first = at;
        for (;;) { int wsum = 0; for (int i = g_tl_first; i <= at; i++) wsum += fwid[i]; if (wsum <= stripw || g_tl_first >= at) break; g_tl_first++; }
        g_tl_seen = e->sp;
    }
    if (g_tl_first > maxfirst) g_tl_first = maxfirst;
    if (g_tl_first < 0) g_tl_first = 0;
    int playing = -1;
    if (e->playing) { Pose p; int ip; playing = play_frame(e, now, &p, &ip); }
    region(R_STRIP, 0, sx0, ty - 1, sx1 - sx0, e->h * z + 12, RGB(0, 0, 0)); /* (the mouse wheel scrolls it; before the frames, so a click finds them) */
    int x = sx0, shown = 0;
    for (int i = g_tl_first; i < n; i++) {
        int w, h;
        grid_now(e, fr[i], g, &w, &h);
        if (x + w * z > sx1) break;
        canvas_rect(cv, x - 1, ty - 1, w * z + 2, h * z + 2, i == at ? RGB(70, 70, 92) : RGB(34, 34, 44));
        draw_grid_at(cv, e, g, w, h, x, ty, z, 1);
        if (i == at) frame(cv, x - 1, ty - 1, w * z + 2, h * z + 2, C_YELLOW);
        region(R_FRAME, fr[i], x - 1, ty - 1, w * z + 2, h * z + 2, RGB(0, 0, 0));
        char nb[8];
        snprintf(nb, sizeof nb, "%d", i + 1);
        canvas_text(cv, x + (w * z - text_width(nb)) / 2, ty + h * z + 2, nb, i == at ? C_YELLOW : C_GREY);
        if (fr[i] == playing) canvas_rect(cv, x, ty + h * z + 1, w * z, 1, C_WHITE); /* (playing now) */
        x += w * z + gap; shown++;
    }
    if (props && g_tl_first + shown == n && x + 12 <= sx1) { canvas_rect(cv, x, ty, 12, e->h * z, RGB(34, 34, 44)); canvas_text(cv, x + 4, ty + e->h * z / 2 - 2, "+", C_GREY); region(R_FADD, 0, x, ty, 12, e->h * z, RGB(0, 0, 0)); }
    if (shown < n) { /* a scroll bar: where the frames shown are among all of them */
        int by = ty + e->h * z + 9, tw = sx1 - sx0, k0 = sx0 + tw * g_tl_first / n, k1 = sx0 + tw * (g_tl_first + shown) / n;
        canvas_rect(cv, sx0, by, tw, 2, RGB(40, 40, 52));
        canvas_rect(cv, k0, by, k1 - k0 > 4 ? k1 - k0 : 4, 2, RGB(140, 140, 160));
    }
}

static void draw(const Ed *e, Canvas *cv) {
    g_nreg = 0;
    double now = wall_now();
    canvas_rect(cv, 0, 0, cv->w, cv->h, RGB(18, 18, 24));
    char t[160];
    snprintf(t, sizeof t, "EDIT %s  %s  %dX%d", e->name, FILES[e->file], e->w, e->h);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 4, 3, t, C_YELLOW);
    int msg_top = e->msg[0] && 4 + text_width(t) + 12 + text_width(e->msg) + 4 <= cv->w; /* (a message that does not fit by the title goes under the grid) */
    if (msg_top) canvas_text(cv, cv->w - text_width(e->msg) - 4, 3, e->msg, C_GREY);
    Layout L = layout(e, cv->w, cv->h);
    /* onion skin: where this frame is see-through, the frame before shows faintly red, the one after blue (both: purple) */
    static char og[2][MAXH][MAXW + 1];
    int ow[2] = {0, 0}, oh[2] = {0, 0}, ofr[128], on = e->onion ? anim_frames(&e->f, e->sp, ofr, 128) : 1;
    if (on > 1) {
        int at = frame_index(ofr, on, e->sp);
        grid_of(&e->f, ofr[(at + on - 1) % on], og[0], &ow[0], &oh[0]);
        if (on > 2) grid_of(&e->f, ofr[(at + 1) % on], og[1], &ow[1], &oh[1]);
    }
    static char gg[MAXH][MAXW + 1]; /* what the grid shows: the open frame, or while PLAY is on the frame playing */
    memcpy(gg, e->g, sizeof gg);
    if (e->playing) {
        Pose pp; int ip, pw, ph, psp = play_frame(e, now, &pp, &ip);
        if (psp >= 0) { grid_now(e, psp, gg, &pw, &ph); if (pw != e->w || ph != e->h) memcpy(gg, e->g, sizeof gg); }
        oh[0] = oh[1] = 0; /* (no onion skin while it plays) */
    }
    if (L.ah > e->h * L.z) canvas_rect(cv, L.gx, L.ty, e->w * L.z, L.ah, RGB(26, 26, 34)); /* (the canvas's frame, taller than the sprite) */
    for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) { /* the grid, see-through as a checker */
        int tr, tp = 1, tn = 1;
        Rgb c = colour_of(&e->f, gg[y][x], &tr);
        if (tr && y < oh[0] && x < ow[0]) colour_of(&e->f, og[0][y][x], &tp);
        if (tr && y < oh[1] && x < ow[1]) colour_of(&e->f, og[1][y][x], &tn);
        Rgb tint = !tp && !tn ? RGB(170, 120, 200) : !tp ? RGB(230, 90, 90) : RGB(90, 150, 240);
        for (int dy = 0; dy < L.z; dy++) for (int dx = 0; dx < L.z; dx++) {
            Rgb k = tr ? (((dx * 2 / L.z) + (dy * 2 / L.z)) % 2 ? RGB(70, 70, 80) : RGB(52, 52, 60)) : c;
            if (tr && !(tp && tn)) k = RGB((k.r * 6 + tint.r * 4) / 10, (k.g * 6 + tint.g * 4) / 10, (k.b * 6 + tint.b * 4) / 10);
            canvas_px(cv, L.gx + x * L.z + dx, L.gy + y * L.z + dy, k);
        }
    }
    frame(cv, L.gx, L.ty, e->w * L.z, L.ah, RGB(90, 90, 110)); /* (round the canvas, as tall as the panel) */
    if (e->filling && e->hx >= 0 && e->hx < e->w && e->hy < e->h) { /* FILL: what a click would fill, outlined in dashes */
        static unsigned char m[MAXH][MAXW];
        fill_area(e, e->hx, e->hy, m);
        for (int y = 0; y < e->h; y++) for (int x = 0; x < e->w; x++) if (m[y][x]) {
            int X = L.gx + x * L.z, Y = L.gy + y * L.z, z = L.z;
            for (int k = 0; k < z; k++) {
                if (((X + Y + k) / 2) % 2) continue;
                if (y == 0 || !m[y - 1][x]) canvas_px(cv, X + k, Y, C_WHITE);
                if (y == e->h - 1 || !m[y + 1][x]) canvas_px(cv, X + k, Y + z - 1, C_WHITE);
                if (x == 0 || !m[y][x - 1]) canvas_px(cv, X, Y + k, C_WHITE);
                if (x == e->w - 1 || !m[y][x + 1]) canvas_px(cv, X + z - 1, Y + k, C_WHITE);
            }
        }
    } else if (e->hx >= 0 && e->hx < e->w && e->hy < e->h) frame(cv, L.gx + e->hx * L.z, L.gy + e->hy * L.z, L.z, L.z, RGB(255, 255, 255)); /* the pixel under the pointer */
    region(R_GRID, 0, L.gx, L.gy, e->w * L.z, e->h * L.z, RGB(0, 0, 0));
    /* under the grid: the pixel under the pointer */
    if (e->hx >= 0 && e->hx < e->w && e->hy < e->h) {
        int tr; char ch = e->g[e->hy][e->hx];
        Rgb c = colour_of(&e->f, ch, &tr);
        if (tr) snprintf(t, sizeof t, "X %d  Y %d  SEE-THROUGH", e->hx, e->hy);
        else snprintf(t, sizeof t, "X %d  Y %d  %d %d %d", e->hx, e->hy, c.r, c.g, c.b);
        canvas_text(cv, L.gx, L.ty + L.ah + 4, t, C_GREY);
    } else if (e->msg[0] && !msg_top) { /* the message, wrapped to the grid's width */
        int mw = L.px - L.gx - 8, ly = L.ty + L.ah + 4, lx = 0;
        char word[64];
        for (const char *p = e->msg; *p; ) {
            int n = 0;
            while (p[n] && p[n] != ' ' && n < 63) n++;
            snprintf(word, sizeof word, "%.*s", n, p);
            int ww = text_width(word);
            if (lx && lx + 4 + ww > mw) { lx = 0; ly += 7; }
            canvas_text(cv, L.gx + lx + (lx ? 4 : 0), ly, word, C_YELLOW);
            lx += (lx ? 4 : 0) + ww;
            p += n;
            while (*p == ' ') p++;
        }
    }
    /* the panel (like Figma's colour picker): PAINT / ERASE / PICK, a colour field and a hue bar, the colour's hex and red
       green blue (click to type), the file's colours (+ ADD puts a new one in), shades, the real-size view */
    int x = L.px, y = L.ty, tr;
    g_field_w = cv->w - x - 4 < 96 ? 96 : cv->w - x - 4 > 128 ? 128 : cv->w - x - 4;
    Rgb cur = pick_colour(e, &tr);
    int body = e->pk == 1 && e->f.pal_body[e->pi];
    static const char *TOOL[4] = {"PAINT", "ERASE", "PICK", "FILL"};
    int lit[4] = {!tr && !e->picking, tr && !e->picking, e->picking, e->filling > 0};
    int roww = -7; /* (the row centred over the colour field) */
    for (int i = 0; i < 4; i++) roww += (text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13) + 7;
    for (int i = 0, tx = x + (FIELD_W - roww) / 2; i < 4; i++) {
        const char *nm = i == 3 && e->filling == 2 ? "ALL" : TOOL[i];
        int cw = text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13;
        tool_button(cv, tx + (cw - 13) / 2, y, i == 3 && e->filling == 2 ? 4 : i, lit[i]);
        canvas_text(cv, tx + (cw - text_width(nm)) / 2, y + 16, nm, lit[i] ? C_WHITE : C_GREY);
        region(R_TOOL, i, tx, y, cw, 21, RGB(0, 0, 0));
        tx += cw + 7;
    }
    y += TOOL_ROW;
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
    int pbot = L.ty + L.ah; /* (a dragged timeline may leave less than the whole panel: what does not fit is left out, the shades first) */
    if (y + SECTION + 8 <= pbot) section(cv, x, y, "IN THIS FILE");
    if (y + SECTION + 8 <= pbot && e->pk == 2) { canvas_text(cv, x + FIELD_W - text_width("+ ADD"), y, "+ ADD", C_GREY); region(R_ADD, 0, x + FIELD_W - text_width("+ ADD") - 1, y - 1, text_width("+ ADD") + 2, 7, RGB(0, 0, 0)); }
    y += SECTION;
    for (int i = 0; i < e->f.npal; i++) {
        int sx = x + (i % 10) * 10, sy = y + (i / 10) * SW_ROW;
        if (sy + 8 > pbot) break;
        canvas_rect(cv, sx, sy, 8, 8, e->f.pal_c[i]);
        region(R_FILE, i, sx, sy, 8, 8, e->f.pal_c[i]);
        int used = 0;
        for (int yy = 0; yy < e->h && !used; yy++) for (int xx = 0; xx < e->w; xx++) if (e->g[yy][xx] == e->f.pal_ch[i]) { used = 1; break; }
        if (used) canvas_rect(cv, sx + 3, sy + 9, 2, 1, RGB(200, 200, 215)); /* (used in this sprite) */
        if (e->pk == 1 && e->pi == i) frame(cv, sx - 1, sy - 1, 10, 10, RGB(150, 150, 170)); /* (the one painting: a soft ring, a dark gap inside it) */
    }
    y += ((e->f.npal + 9) / 10) * SW_ROW + 2;
    if (y + SECTION + 8 <= pbot) section(cv, x, y, "SHADES");
    y += SECTION;
    for (int k = -3; k <= 3 && y + 8 <= pbot; k++) { /* darker to lighter */
        int sx = x + (k + 3) * 10;
        Rgb c = shade(cur, k);
        if (!tr) { canvas_rect(cv, sx, y, 8, 8, c); region(R_NEW, 0, sx, y, 8, 8, c); }
        if (k == 0 && !tr) frame(cv, sx - 1, y - 1, 10, 10, RGB(90, 90, 108)); /* (this colour: a dimmer ring) */
    }
    y += 12;
    if (L.hw) help(cv, L.hy, L.hw);
    else if (!timeline_h(e, cv->h)) { /* very short window, no timeline: one line, the rest behind ? */
        static const Help H_SHORT[] = {{"?", "SHORTCUTS"}, {"Q", "QUIT"}};
        help_row(cv, L.hy, "", H_SHORT, 2, cv->w);
    }
    /* the sprite at its real size (and twice); while PLAY is on, the animation as the room shows it */
    int bottom = L.hy - 3, pad = e->playing && is_action(e) ? 4 : 0, shown = 0; /* (pad: room above for a Clawd's bounce) */
    int tl_frames = timeline_h(e, cv->h) >= tl_full(e, 1); /* (the timeline shows the frames: the real-size view is there, at its left) */
    int rx = x + FIELD_W + 12, lw = text_width("AS IN THE ROOM");
    int ux = L.gx, uy = L.ty + L.ah + 18, uw = L.px - L.gx - 10; /* (under the canvas, below the pointer's line) */
    if (tl_frames) shown = 1;
    else if (uy + 9 + (e->h + pad) * 2 <= bottom && e->w * 3 + 6 <= uw && lw <= uw) { /* room under the grid: there, 1x beside 2x */
        canvas_text(cv, ux, uy, e->playing ? "AS IN THE ROOM  PLAYING" : "AS IN THE ROOM", RGB(110, 110, 130));
        for (int k = 1; k <= 2; k++) draw_preview(cv, e, now, ux + (k - 1) * (e->w + 6), uy + 9, k, pad);
        shown = 1;
    } else if (rx + (e->w * 2 > lw ? e->w * 2 : lw) + 2 <= cv->w) { /* a wide window: in the column right of the panel, 1x over 2x */
        int ry = L.ty;
        canvas_text(cv, rx, ry, "AS IN THE ROOM", RGB(110, 110, 130));
        if (e->playing) canvas_text(cv, rx, ry + 7, "PLAYING", C_YELLOW);
        ry += e->playing ? 16 : 9;
        for (int k = 1; k <= 2 && ry + (e->h + pad) * k <= bottom; k++) { draw_preview(cv, e, now, rx, ry, k, pad); ry += (e->h + pad) * k + 4; shown = 1; }
    } else { /* under the shades, 1x beside 2x, as there is room */
        int big = y + 8 + e->h * 2 + 4 <= bottom && x + e->w * 3 + 6 <= cv->w ? 2 : 1;
        if (y + 8 + e->h <= bottom) canvas_text(cv, x, y, e->playing ? "AS IN THE ROOM  PLAYING" : "AS IN THE ROOM", RGB(110, 110, 130));
        if (pad && y + 8 + e->h + pad > bottom) pad = bottom - (y + 8 + e->h) > 0 ? bottom - (y + 8 + e->h) : 0;
        for (int k = 1; k <= big && y + 8 + e->h + pad <= bottom; k++) { draw_preview(cv, e, now, x + (k - 1) * (e->w + 6), y + 8, k, pad); shown = 1; }
    }
    draw_timeline(cv, e, now, tl_frames || (!shown && e->playing), !L.hw);
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
    e.pk = 1; e.pi = 0; e.hx = e.hy = -1; e.drag = -1; e.last_pk = 1; e.speed = 1; e.onion = 1; e.del_ask = -1;
    int which = 0, found = 0;
    for (int pass = 0; pass < 2 && name && !found; pass++) /* the file that has a sprite called NAME (props first), else an animation */
        for (int f = 0; f < NFILES && !found; f++) {
            static File tf;
            memset(&tf, 0, sizeof tf);
            char path[700], n2[64];
            snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
            if (file_load(&tf, path) == 0) for (int i = 0; i < tf.nsprite && !found; i++) {
                sprite_name(&tf, i, n2, sizeof n2);
                if (pass == 0 ? !strcmp(n2, name) : find_sprite(&tf, name) >= 0) { which = f; found = 1; }
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
        struct timeval tv = {0, e.playing ? 62500 : 200000}; /* (16 frames a second while it plays) */
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
                    if (b & 64) { /* the mouse wheel: over the frames it scrolls them (up / left back, down / right on); elsewhere nothing */
                        if (r && (r->kind == R_STRIP || r->kind == R_FRAME || r->kind == R_FADD)) g_tl_first += (b & 3) == 0 || (b & 3) == 2 ? -1 : 1;
                        continue;
                    }
                    if (g_div_drag) { if (release) g_div_drag = 0; else if (motion) g_tl_user = rows * 2 - py; continue; } /* (dragging the divider) */
                    g_div_hover = r && r->kind == R_DIV;
                    if (r && r->kind == R_DIV && btn == 0 && !motion && !release) { /* the divider: drag it; a double-click: back to automatic */
                        static double last_click;
                        double now2 = wall_now();
                        if (now2 - last_click < 0.4) g_tl_user = 0; else { g_div_drag = 1; g_tl_user = timeline_h(&e, rows * 2); }
                        last_click = now2;
                        continue;
                    }
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
                    if (r && r->kind == R_GRID && e.playing && !motion && btn != 3) e.playing = 0; /* (a click on the grid stops PLAY: you draw on the open frame) */
                    if (r && r->kind == R_GRID && e.filling && (btn == 0 || btn == 2)) { /* FILL: one click, not a drag */
                        if (motion) continue;
                        char ch = '.';
                        if (btn == 0 && e.pk == 1) ch = e.f.pal_ch[e.pi];
                        else if (btn == 0 && e.pk == 2) { ch = letter_for(&e, e.prgb); for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; } }
                        static unsigned char m[MAXH][MAXW];
                        fill_area(&e, e.hx, e.hy, m);
                        push_undo(&e);
                        for (int yy = 0; yy < e.h; yy++) for (int xx = 0; xx < e.w; xx++) if (m[yy][xx]) e.g[yy][xx] = ch;
                        save(&e);
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
                        else if (r->kind == R_TOOL && r->i == 2) { e.picking = !e.picking; e.filling = 0; }
                        else if (r->kind == R_TOOL && r->i == 3) { e.filling = (e.filling + 1) % 3; e.picking = 0; } /* FILL, FILL ALL, off */
                        else if (r->kind == R_FRAME) { sprite_open(&e, r->i); e.msg[0] = 0; }
                        else if (r->kind == R_FADD) { new_frame(&e); e.zoom = 0; }
                        else if (r->kind == R_PLAY) { e.playing = !e.playing; e.play_t0 = wall_now(); }
                        else if (r->kind == R_SPEED) e.speed = e.speed < 0.75 ? 1 : e.speed < 1.5 ? 2 : 0.5;
                        else if (r->kind == R_ONION) e.onion = !e.onion;
                        else if (r->kind == R_ADD && e.pk == 2) { char ch = letter_for(&e, e.prgb); for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; } save(&e); }
                        else if (r->kind == R_FILE) { e.pk = 1; e.pi = r->i; e.picking = 0; }
                        else if (r->kind == R_NEW) { /* a colour already in the file is that one; else a new colour */
                            e.pk = 2; e.prgb = r->c;
                            for (int q = 0; q < e.f.npal; q++) if (!e.f.pal_body[q] && rgb_eq(e.f.pal_c[q], r->c)) { e.pk = 1; e.pi = q; }
                        }
                    }
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin && (in[i + 2] == 'C' || in[i + 2] == 'D')) { /* right / left arrow: the frame after / before */
                    int fr[128], n = anim_frames(&e.f, e.sp, fr, 128), at = frame_index(fr, n, e.sp);
                    sprite_open(&e, fr[(at + (in[i + 2] == 'C' ? 1 : n - 1)) % n]); e.msg[0] = 0;
                    i += 3; continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin) { i += 3; continue; } /* (up / down and the like: not used) */
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
            int was_del = e.del_ask; e.del_ask = -1;
            if (ch == 'f' || ch == 'F' || ch == 'x' || ch == 'X') { /* a new frame / delete this one: props only (the room animates NAME~2, ~3 ...; the code picks the other frames) */
                int fr[128], n = anim_frames(&e.f, e.sp, fr, 128), at = frame_index(fr, n, e.sp);
                if (strcmp(FILES[e.file], "props.txt")) snprintf(e.msg, sizeof e.msg, "NEW FRAMES ARE FOR PROPS ONLY");
                else if (ch == 'f' || ch == 'F') { new_frame(&e); e.zoom = 0; }
                else if (at == 0) snprintf(e.msg, sizeof e.msg, "THE FIRST FRAME IS THE PROP: NOT DELETED");
                else if (was_del == e.sp) delete_frame(&e);
                else { e.del_ask = e.sp; snprintf(e.msg, sizeof e.msg, "X AGAIN: DELETE %s", e.name); for (char *q = e.msg; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32; }
                continue;
            }
            if (ch == ' ') { e.playing = !e.playing; e.play_t0 = wall_now(); continue; }
            if (ch == 'o' || ch == 'O') { e.onion = !e.onion; continue; }
            if (ch == 'b' || ch == 'B') { e.filling = (e.filling + 1) % 3; e.picking = 0; continue; }
            if (ch == 0x1A) ch = 'u'; /* Ctrl+Z: undo; Ctrl+Y: redo (U / R too) */
            else if (ch == 0x19) ch = 'r';
            else if (ch == 0x03) ch = 'q'; /* Ctrl+C: quit, asking about changes */
            if (ch == 'q') { int any = 0; for (int f = 0; f < NFILES; f++) any |= e.changed[f]; if (any) e.asking = 1; else quit = 1; }
            else if (ch == 'u' && e.nundo) { e.nundo--; swap_undo(&e, e.nundo); e.nredo++; save(&e); }
            else if (ch == 'r' && e.nredo) { swap_undo(&e, e.nundo); e.nundo++; e.nredo--; save(&e); }
            else if (ch == '>' || ch == '<' || ch == '.' || ch == ',') { /* the frame after / before in the animation (wrapping) */
                int fr[128], n = anim_frames(&e.f, e.sp, fr, 128), at = frame_index(fr, n, e.sp);
                sprite_open(&e, fr[(at + (ch == '>' || ch == '.' ? 1 : n - 1)) % n]); e.msg[0] = 0;
            }
            else if (ch == ']' || ch == '[') { /* the sprite after / before: the first frame of the next animation */
                int fr[128], n = anim_frames(&e.f, e.sp, fr, 128), i = e.sp, go = e.sp;
                for (int step = 0; step < e.f.nsprite; step++) {
                    i = (i + (ch == ']' ? 1 : -1) + e.f.nsprite) % e.f.nsprite;
                    int mine = 0;
                    for (int k = 0; k < n; k++) if (fr[k] == i) mine = 1;
                    if (!mine) { int f2[128]; anim_frames(&e.f, i, f2, 128); go = f2[0]; break; }
                }
                sprite_open(&e, go); e.zoom = 0; e.msg[0] = 0;
            }
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

/* --edit list: what can be opened, by file: each animation once with how many frames it has, in columns that fit the
 * terminal (a name is never cut across two lines); in each column the names line up on the left, the frame counts on
 * the right */
int editor_list(const char *dir) {
    struct winsize ws;
    int width = isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col ? ws.ws_col : 80;
    for (int f = 0; f < NFILES; f++) {
        static File tf;
        memset(&tf, 0, sizeof tf);
        char path[700];
        snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
        if (file_load(&tf, path)) continue;
        static char item[512][64];
        int cnt[512], ni = 0, nanim = 0, namew = 0, numw = 0; /* (cnt 0: a sprite on its own, no count) */
        static unsigned char seen[512];
        memset(seen, 0, sizeof seen);
        for (int i = 0; i < tf.nsprite && ni < 512; i++) {
            if (seen[i]) continue;
            int fr[128], n = anim_frames(&tf, i, fr, 128), num;
            for (int k = 0; k < n; k++) seen[fr[k]] = 1;
            char name[64], base[64];
            sprite_name(&tf, fr[0], name, sizeof name);
            int numbered = split_name(name, base, sizeof base, &num) == 1; /* (NAME-1 alone is an animation of one frame) */
            cnt[ni] = n > 1 || numbered ? n : 0;
            snprintf(item[ni], sizeof item[ni], "%s", cnt[ni] ? base : name);
            if (cnt[ni]) { nanim++; char d[12]; int dl = snprintf(d, sizeof d, "%d", n); if (dl > numw) numw = dl; }
            if ((int)strlen(item[ni]) > namew) namew = (int)strlen(item[ni]);
            ni++;
        }
        int alone = ni - nanim; /* (sprites on their own; an animation's frames count as the animation) */
        char head[160], dash[160];
        int hl = snprintf(head, sizeof head, "[ %s ] %d sprite%s | %d animation%s (name + frames) |", FILES[f], alone, alone == 1 ? "" : "s", nanim, nanim == 1 ? "" : "s");
        memset(dash, '-', (size_t)hl); dash[hl] = 0; /* (a line of dashes above and below, as long as the heading) */
        printf("%s\n%s\n%s\n", dash, head, dash);
        int cellw = namew + (numw ? 1 + numw : 0), colw = cellw + 4; /* a name, a space, its count (right-aligned), a gap */
        int cols = (width - 2 + 4) / colw > 0 ? (width - 2 + 4) / colw : 1;
        for (int i = 0; i < ni; i++) {
            int last = i % cols == cols - 1 || i == ni - 1;
            if (i % cols == 0) printf("  ");
            if (cnt[i]) printf("%-*s %*d", namew, item[i], numw, cnt[i]);
            else printf("%-*s", last ? 0 : cellw, item[i]);
            if (last) printf("\n"); else printf("    ");
        }
        printf("\n");
        file_free(&tf);
    }
    printf("open one: ./clawd-room.sh --edit NAME\n");
    return 0;
}
