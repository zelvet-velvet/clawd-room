/* The pixel editor (Engine Design, step 3): clawd-room --edit [NAME] edits one sprite of the sprite files in iTerm2 (or any
 * terminal with mouse reporting), drawn with the room's own pixel canvas. It saves as you draw, so running rooms (which read
 * the files again when they change) show the edit at once.
 *   mouse: left paints, right erases, Alt + click picks the colour under it; a swatch picks a colour; FILL fills the
 *          touching pixels of one colour (click FILL again: FILL ALL, every pixel of that colour)
 *   keys:  u undo, r redo, < > the frame before / after in the animation, [ ] the sprite before / after, Tab the next file,
 *          Space play, o onion skin, b fill, f new frame / x delete frame (props), + - zoom, p fold the colours,
 *          w / n held + an arrow: grow / shrink on that side, q quit
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
#include "sprites.h"
#include "term.h"

#define MAXW 64
#define MAXH 64
#define MAX_LINES 4000
#define MAX_UNDO 64

static const char *const FILES[] = {"props.txt", "items.txt", "clawd.txt", "ui.txt"}; /* (ui.txt: the editor's own icons) */
#define NFILES 4
static const char FREE_CHARS[] = "abcdefghijklmnopqrstuvwxyzACDEFGHIJLMNOPQRSTUVXYZ0123456789";

typedef struct {
    char path[700], mpath[700];             /* the default file (in the repo) and yours (~/.clawd-room/sprites; "" none) */
    unsigned char mine[512];                 /* the sprites of yours (in your file) */
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
    int recol;                               /* a file colour double-clicked (its index + 1): changing the colour changes it in the file */
    int typing; char tbuf[8];                /* typing a colour: 0 none, 1 the hex code, 2..4 red / green / blue (0..255) */
    int zoom, dirty_file;
    char msg[96];
    char *orig[NFILES]; int changed[NFILES]; /* each file as it was when first opened, to put back on discard */
    char *visit[NFILES]; int vchanged[NFILES]; /* ... and as it was when this sprite was opened (q: keep or discard this visit's changes) */
    int asking;                               /* q with changes: keep or discard? */
    int show_help;                            /* ?: the shortcuts in a box (when the window is too short to list them) */
    int playing; double play_t0, speed;       /* Space: the animation plays in the real-size view (speed 0.5, 1 or 2) */
    int onion;                                /* o: the frames before and after show faintly under the grid */
    int filling;                              /* FILL: 0 off, 1 the touching pixels of one colour, 2 every pixel of it */
    int del_ask;                              /* x once: the frame to delete if x comes again (-1 none) */
    int sizing; double sizing_until;          /* w or n held ('w' / 'n'): an arrow grows / shrinks the sprite on that side */
    int page;                                 /* 0 editing, 1 the sprite picker (--edit with no name, or l) */
    char find[32]; int psel, ptab, pview, pscroll, came_from_edit; /* the picker: what is typed, the one picked, the file tab,
                                                 the view (0 cards, 1 a list), how far it scrolled, opened from editing */
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

/* a line more at the end of the file's lines (a colour, a sprite's head or a row) */
static void file_add_line(File *f, const char *buf) {
    if (f->nline >= MAX_LINES) return;
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
static int file_load(File *f, const char *path) {
    file_free(f);
    memset(f, 0, sizeof *f);
    snprintf(f->path, sizeof f->path, "%s", path);
    f->last_pal_line = -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char buf[1024];
    while (fgets(buf, sizeof buf, fp) && f->nline < MAX_LINES) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        file_add_line(f, buf);
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

/* ---- your own sprites over the defaults: the repo's file is the default, ~/.clawd-room/sprites/FILE holds only what you
 * changed (colours you added or changed, sprites that differ, frames you added). The editor shows them merged and writes
 * only yours; the room shows yours over the defaults; RESET in the picker takes yours away. ------------------------------ */
static int sprite_find(const File *f, const char *name) {
    char n2[64];
    for (int i = 0; i < f->nsprite; i++) { sprite_name(f, i, n2, sizeof n2); if (!strcmp(n2, name)) return i; }
    return -1;
}
static char pal_letter(const char *line) { const char *p = line + 1; while (*p == ' ') p++; return *p; }
static int block_end(const File *f, int head) { /* the line after a sprite's rows */
    int i = head + 1;
    while (i < f->nline && f->line[i][0] && f->line[i][0] != '@' && f->line[i][0] != '#' && f->line[i][0] != ':') i++;
    return i;
}
/* the default file with yours laid over it: your colours in place of the default's (or added after its last colour), your
 * sprites in place of the default ones of the same name, the ones only you have at the end */
static int file_load_merged(File *f, const char *dpath, const char *mpath) {
    static File d, m;
    int rc = file_load(&d, dpath);
    if (file_load(&m, mpath) != 0) m.nline = m.nsprite = m.npal = 0;
    file_free(f);
    memset(f, 0, sizeof *f);
    f->last_pal_line = -1;
    if (rc && !m.nline) { snprintf(f->path, sizeof f->path, "%s", dpath); return rc; }
    for (int i = 0; i < d.nline; i++) {
        const char *l = d.line[i];
        if (l[0] == ':') {
            int theirs = 0;
            for (int k = 0; k < m.nline; k++) if (m.line[k][0] == ':' && pal_letter(m.line[k]) == pal_letter(l)) theirs = 1;
            if (!theirs) file_add_line(f, l);
            if (i == d.last_pal_line) for (int k = 0; k < m.nline; k++) if (m.line[k][0] == ':') file_add_line(f, m.line[k]);
            continue;
        }
        if (l[0] == '@') {
            char nm[64];
            int di = -1;
            for (int k = 0; k < d.nsprite; k++) if (d.sprite_line[k] == i) di = k;
            sprite_name(&d, di, nm, sizeof nm);
            int mi = sprite_find(&m, nm);
            if (mi >= 0) { /* yours instead: its head and rows */
                int e2 = block_end(&m, m.sprite_line[mi]);
                for (int k = m.sprite_line[mi]; k < e2; k++) file_add_line(f, m.line[k]);
                f->mine[f->nsprite - 1] = 1;
                i = block_end(&d, i) - 1;
                continue;
            }
        }
        file_add_line(f, l);
    }
    if (d.last_pal_line < 0) for (int k = 0; k < m.nline; k++) if (m.line[k][0] == ':') file_add_line(f, m.line[k]);
    for (int k = 0; k < m.nsprite; k++) { /* the sprites only you have */
        char nm[64];
        sprite_name(&m, k, nm, sizeof nm);
        if (sprite_find(&d, nm) >= 0) continue;
        file_add_line(f, "");
        int e2 = block_end(&m, m.sprite_line[k]);
        for (int j = m.sprite_line[k]; j < e2; j++) file_add_line(f, m.line[j]);
        f->mine[f->nsprite - 1] = 1;
    }
    snprintf(f->path, sizeof f->path, "%s", dpath);
    snprintf(f->mpath, sizeof f->mpath, "%s", mpath);
    return 0;
}
static void mine_path(char *out, size_t n, int file) { snprintf(out, n, "%s/%s", sprites_mine_dir(), FILES[file]); }
/* yours, from the merged lines: the colours that are new or differ from the default's, the sprites that differ or that the
 * default has not; nothing of yours: your file goes */
static int write_mine(File *f) {
    static File d;
    if (file_load(&d, f->path) != 0) d.nline = d.nsprite = d.npal = 0;
    char *out = malloc(1 << 20);
    size_t o = 0, cap = 1 << 20;
    int any = 0;
    o += (size_t)snprintf(out + o, cap - o, "# Your own sprites over the defaults in the repo (the pixel editor writes this; RESET in its picker takes them away).\n");
    for (int i = 0; i < f->nline; i++) if (f->line[i][0] == ':') {
        int same = 0, theirs = 0, used = 0;
        char ch = pal_letter(f->line[i]);
        for (int k = 0; k < d.nline; k++) if (d.line[k][0] == ':') { if (!strcmp(d.line[k], f->line[i])) same = 1; if (pal_letter(d.line[k]) == ch) theirs = 1; }
        for (int k = 0; k < f->nline && !used; k++) /* (is it in any sprite's rows) */
            if (f->line[k][0] && f->line[k][0] != ':' && f->line[k][0] != '@' && f->line[k][0] != '#' && strchr(f->line[k], ch)) used = 1;
        if (!theirs && !used) continue; /* (a colour you added that nothing uses any more: it goes) */
        if (!same) { o += (size_t)snprintf(out + o, cap - o, "%s\n", f->line[i]); any = 1; }
    }
    static char g1[MAXH][MAXW + 1], g2[MAXH][MAXW + 1];
    for (int s = 0; s < f->nsprite; s++) {
        char nm[64];
        int w1, h1, w2, h2;
        sprite_name(f, s, nm, sizeof nm);
        grid_of(f, s, g1, &w1, &h1);
        int di = sprite_find(&d, nm), same = 0;
        if (di >= 0) {
            grid_of(&d, di, g2, &w2, &h2);
            same = w1 == w2 && h1 == h2 && !strcmp(f->line[f->sprite_line[s]], d.line[d.sprite_line[di]]);
            for (int y = 0; y < h1 && same; y++) if (memcmp(g1[y], g2[y], (size_t)w1)) same = 0;
        }
        if (same) continue;
        o += (size_t)snprintf(out + o, cap - o, "\n%s\n", f->line[f->sprite_line[s]]);
        for (int y = 0; y < h1 && o + MAXW + 2 < cap; y++) o += (size_t)snprintf(out + o, cap - o, "%.*s\n", w1, g1[y]);
        any = 1;
    }
    int rc = 0;
    if (!any) remove(f->mpath); /* (nothing of yours left: the defaults) */
    else {
        char dir[700], tmp[720];
        snprintf(dir, sizeof dir, "%s", f->mpath);
        char *sl = strrchr(dir, '/');
        if (sl) { *sl = 0; char mk[760]; snprintf(mk, sizeof mk, "mkdir -p '%s'", dir); if (system(mk)) {} }
        snprintf(tmp, sizeof tmp, "%s.edit", f->mpath);
        FILE *fp = fopen(tmp, "w");
        if (!fp || fwrite(out, 1, o, fp) != o) rc = -1;
        if (fp) fclose(fp);
        if (!rc && rename(tmp, f->mpath)) rc = -1;
    }
    free(out);
    return rc;
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
    e->recol = 0;
    char mpath[700];
    snprintf(path, sizeof path, "%s/%s", dir, FILES[e->file]);
    mine_path(mpath, sizeof mpath, e->file);
    if (file_load_merged(&e->f, path, mpath)) snprintf(e->msg, sizeof e->msg, "CANNOT READ %s", FILES[e->file]);
    int sp = name ? find_sprite(&e->f, name) : 0;
    sprite_open(e, sp < 0 ? 0 : sp);
}

/* the merged lines again from their text (after lines were put in or taken out) */
static void file_reparse(File *f) {
    static File t;
    file_free(&t);
    memset(&t, 0, sizeof t);
    t.last_pal_line = -1;
    for (int i = 0; i < f->nline; i++) file_add_line(&t, f->line[i]);
    snprintf(t.path, sizeof t.path, "%s", f->path); snprintf(t.mpath, sizeof t.mpath, "%s", f->mpath);
    file_free(f);
    memcpy(f, &t, sizeof t);
    memset(&t, 0, sizeof t); /* (its lines are f's now) */
}
/* yours written (write_mine), then the merged lines read again (line numbers moved); the open sprite stays */
static int save_mine(Ed *e) {
    File *f = &e->f;
    if (write_mine(f)) { snprintf(e->msg, sizeof e->msg, "CANNOT SAVE: %s", strerror(errno)); return -1; }
    char path[700], mpath[700];
    snprintf(path, sizeof path, "%s", f->path); snprintf(mpath, sizeof mpath, "%s", f->mpath);
    int sp = e->sp, w = e->w, h = e->h, nu = e->nundo, nr = e->nredo;
    static char keep[MAXH][MAXW + 1];
    memcpy(keep, e->g, sizeof keep);
    char held = e->pk == 1 ? e->f.pal_ch[e->pi] : 0; /* (the colour in hand: its swatch may go if nothing uses it now) */
    Rgb held_c = e->pk == 1 ? e->f.pal_c[e->pi] : RGB(0, 0, 0);
    file_load_merged(f, path, mpath);
    e->recol = 0;
    if (held) { /* (still in the file: the same swatch; gone: kept in hand as a new colour, painting brings it back) */
        int found = -1;
        for (int q = 0; q < f->npal; q++) if (f->pal_ch[q] == held) found = q;
        if (found >= 0) e->pi = found; else { e->pk = 2; e->prgb = held_c; }
    }
    e->sp = sp < f->nsprite ? sp : 0; e->w = w; e->h = h; memcpy(e->g, keep, sizeof keep); e->nundo = nu; e->nredo = nr;
    e->changed[e->file] = e->vchanged[e->file] = 1;
    return 0;
}
/* the grid into the open sprite's rows, then yours written: only what differs from the default */
static int save(Ed *e) {
    File *f = &e->f;
    int at = f->sprite_line[e->sp], end = block_end(f, at);
    int keep = f->nline - end, add = e->h - (end - at - 1);
    if (f->nline + add >= MAX_LINES) return -1;
    for (int i = at + 1; i < end; i++) free(f->line[i]);
    memmove(&f->line[at + 1 + e->h], &f->line[end], sizeof(char *) * (size_t)keep);
    for (int y = 0; y < e->h; y++) { char row[MAXW + 1]; memcpy(row, e->g[y], (size_t)e->w); row[e->w] = 0; f->line[at + 1 + y] = strdup(row); }
    f->nline += add;
    file_reparse(f);
    if (save_mine(e)) return -1;
    snprintf(e->msg, sizeof e->msg, "SAVED %s", e->f.mine[e->sp] ? "AS YOURS" : "(AS THE DEFAULT)");
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
    file_reparse(f);
    if (save_mine(e)) return -1; /* (a frame only you have: in your file) */
    char nf[80];
    snprintf(nf, sizeof nf, "%s~%d", base, k);
    int sp = sprite_find(f, nf);
    if (sp >= 0) sprite_open(e, sp);
    snprintf(e->msg, sizeof e->msg, "NEW FRAME %s~%d", base, k);
    return 0;
}

/* x x: delete the open frame of a prop (not its first: that is the prop); the frames after it are numbered on, so they
 * stay NAME~2, ~3 ... without a gap (the room stops at the first missing number) */
static int delete_frame(Ed *e) {
    File *f = &e->f;
    int fr[128], n = anim_frames(f, e->sp, fr, 128), k = frame_index(fr, n, e->sp);
    if (k == 0) return -1;
    { /* (the defaults stay as they are: only a frame you added, the last one, goes) */
        static File d;
        char nm0[64];
        sprite_name(f, e->sp, nm0, sizeof nm0);
        if (file_load(&d, f->path) != 0) d.nsprite = 0;
        if (sprite_find(&d, nm0) >= 0 || k != n - 1) { snprintf(e->msg, sizeof e->msg, "ONLY YOUR LAST ADDED FRAME CAN GO"); return -1; }
    }
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
    file_reparse(f);
    if (save_mine(e)) return -1;
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
enum { R_GRID, R_TOOL, R_FILE, R_NEW, R_FIELD, R_HUE, R_HEX, R_NUM, R_ADD, R_FRAME, R_FADD, R_PLAY, R_SPEED, R_ONION, R_STRIP, R_DIV, R_FOLD, R_PITEM, R_PTAB, R_PVIEW,
       R_PRESET, R_PRESETALL, R_PMAKEDEF };
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
static const Help H_EDIT[] = {{"CTRL+Z", "UNDO"}, {"CTRL+Y", "REDO"}, {"L", "PICK A SPRITE"}, {"[ ]", "OTHER SPRITE"}, {"TAB", "OTHER FILE"}, {"B", "FILL"}, {"P", "FOLD THE COLOURS"}, {"+ -", "ZOOM"}, {"#", "TYPE A COLOUR"}};
static const Help H_ANIM[] = {{"LEFT RIGHT", "FRAME"}, {"SPACE", "PLAY"}, {"O", "ONION SKIN"}, {"F", "NEW FRAME"}, {"X X", "DELETE FRAME"}};
#define NH(a) ((int)(sizeof a / sizeof a[0]))
static const Help H_SIZE[] = {{"W + ARROW", "GROW ON THAT SIDE"}, {"N + ARROW", "SHRINK ON THAT SIDE"}, {"Q", "BACK TO THE SPRITES (Q THERE: QUIT)"}};
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
#define SW_COLS ((FIELD_W + 4) / 10) /* swatches in a row: as many as the colour field is wide (a wider window, more) */
static int panel_h(const Ed *e) { return TOOL_ROW + FIELD_H + 3 + 8 + 11 + SECTION + ((e->f.npal + SW_COLS) / SW_COLS) * SW_ROW + 2 + SECTION + 10; }
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
/* The divider above the timeline can be dragged (g_tl_user: the height asked for, -1 folded to its top line; 0 the automatic
 * one, double-click it to go back): the frames and the sprite on the left grow with it, the canvas above gives up the room, never all of it. */
static int g_tl_user, g_div_drag, g_div_hover;
static int g_fold_hover; /* (the pointer is on the fold button) */
static int g_panel_folded = 1; /* (folded when the editor opens) */ /* p or the fold button: the colour panel folded to a ribbon of the tools and the colour (like Obsidian's sidebar) */
#define RIBBON 13      /* the ribbon's width: a key cap */
#define TOP_LEAST 64 /* a dragged timeline leaves the canvas and the panel at least this: the tools, the colour field, the hex line */
static int tl_most(const Ed *e, int H) { (void)e; return H - 12 - TOP_LEAST; }
static int thumb_zoom(const Ed *e, int H);
static int timeline_h(const Ed *e, int H) {
    if (g_tl_user < 0) return H - 12 - 14 >= TOP_LEAST ? 14 : 0; /* (dragged down: folded to its top line) */
    if (g_tl_user) { int lo = tl_full(e, 1), hi = tl_most(e, H), h = g_tl_user < lo ? lo : g_tl_user > hi ? hi : g_tl_user; return h >= lo ? h : 0; }
    int full = tl_full(e, thumb_zoom(e, H)), rest = panel_h(e) - FIELD_H + 16;
    return H - 12 - full >= rest ? full : H - 12 - 14 >= rest ? 14 : 0;
}
static int thumb_zoom(const Ed *e, int H) {
    if (g_tl_user > 0) { int th = timeline_h(e, H), z = 1; while (z < 8 && tl_full(e, z + 1) <= th) z++; return z; } /* (as big as the dragged height allows) */
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
    L.z = grid_zoom(e, g_panel_folded ? W - 18 - RIBBON - 4 : W - 118, L.hy - L.ty - 14); /* (the panel keeps what it needs: 100
                                                         pixels, its widest row of colours; folded, the ribbon) */
    int gh = e->h * L.z, f = gh + 2 - rest; /* (the panel's last row ends 2 pixels above its height) */
    g_field_h = f < 16 ? 16 : f > most ? most : f;
    int ph = g_panel_folded ? 4 * 17 + 12 : rest + g_field_h - 2;
    L.ah = gh > ph ? gh : ph;
    if (L.ah > L.hy - 5 - L.ty) L.ah = L.hy - 5 - L.ty; /* (a dragged timeline: the panel shows what still fits; room for the handle) */
    if (L.ah < gh) L.ah = gh;
    int gap = ((L.hy - 1) - 9 - L.ah) / 2; /* (the canvas and the panel in the middle between the title and the timeline; the
                                            pointer's line goes in the gap under them, if it fits there) */
    if (gap >= 10) L.ty = 9 + gap;
    L.gy = L.ty + (L.ah - gh) / 2;
    L.px = g_panel_folded ? W - 4 - RIBBON : L.gx + e->w * L.z + 10;
    if (g_panel_folded) { int free = L.px - 10 - 4 - e->w * L.z; if (free > 0) L.gx = 4 + free / 2; } /* (folded: the canvas in the middle of its room) */
    return L;
}

static Rgb pick_colour(const Ed *e, int *tr) { *tr = e->pk == 0; return e->pk == 1 ? e->f.pal_c[e->pi] : e->pk == 2 ? e->prgb : RGB(0, 0, 0); }
static void frame(Canvas *cv, int x, int y, int w, int h, Rgb c) { canvas_rect(cv, x - 1, y - 1, w + 2, 1, c); canvas_rect(cv, x - 1, y + h, w + 2, 1, c); canvas_rect(cv, x - 1, y - 1, 1, h + 2, c); canvas_rect(cv, x + w, y - 1, 1, h + 2, c); }
static void checker(Canvas *cv, int x, int y, int w, int h) { for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) canvas_px(cv, x + dx, y + dy, ((dx / 2) + (dy / 2)) % 2 ? RGB(70, 70, 80) : RGB(52, 52, 60)); }
static Rgb shade(Rgb c, int k, int n) { /* k < 0 darker, k > 0 lighter, of n a side: to 40% dark, to 75% light */
    double f = k < 0 ? 1 + 0.6 * k / n : 0.75 * k / n;
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
/* the editor's own icons from sprites/ui.txt (read again when it changes; one left out: the built-in drawing) */
#include <sys/stat.h>
static SpriteFile g_ui;
static time_t g_ui_mtime;
static void ui_watch(const char *dir) {
    char path[700];
    struct stat st;
    snprintf(path, sizeof path, "%s/ui.txt", dir);
    if (stat(path, &st) != 0) { if (g_ui.n) sprites_free(&g_ui); g_ui_mtime = 0; return; }
    char mp[700];
    struct stat ms;
    snprintf(mp, sizeof mp, "%s/ui.txt", sprites_mine_dir());
    time_t when = st.st_mtime + (stat(mp, &ms) == 0 ? ms.st_mtime * 2 + 1 : 0); /* (the default's or yours changed) */
    if (when == g_ui_mtime && g_ui.n) return;
    sprites_free(&g_ui); g_ui = sprites_load_layered(path, mp); g_ui_mtime = when; /* (yours over the defaults) */
}
/* a ui.txt sprite at (x, y), its colours brightened by `lift` (0 none, 1 white); 0 if the file has no such sprite */
static int ui_icon(Canvas *cv, const char *name, int x, int y, double lift) {
    const Sprite *sp = g_ui.n ? sprites_by_name(&g_ui, name) : NULL;
    if (!sp) return 0;
    for (int dy = 0; dy < sp->h; dy++) for (int dx = 0; dx < sp->w; dx++) {
        SpritePx p = sp->px[dy * sp->w + dx];
        if (p.kind != PX_RGB) continue;
        Rgb c = p.c;
        canvas_px(cv, x + dx, y + dy, RGB((int)(c.r + (255 - c.r) * lift), (int)(c.g + (255 - c.g) * lift), (int)(c.b + (255 - c.b) * lift)));
    }
    return 1;
}
static void tool_button(Canvas *cv, int x, int y, int which, int on) {
    int w = 13, h = 13, dy = on ? 1 : 0;
    canvas_rect(cv, x + 1, y + 1, w - 2, h, RGB(14, 14, 20)); canvas_rect(cv, x, y + 2, w, h - 2, RGB(14, 14, 20)); /* (its shadow) */
    Rgb cap = on ? RGB(70, 72, 100) : RGB(52, 54, 70);
    canvas_rect(cv, x + 1, y + dy, w - 2, h - dy, cap); canvas_rect(cv, x, y + dy + 1, w, h - dy - 2, cap);    /* (rounded corners) */
    canvas_rect(cv, x + 2, y + dy, w - 4, 1, on ? RGB(110, 112, 145) : RGB(86, 88, 110));                   /* (light on its top) */
    static const char *const UI_NAME[5] = {"tool-paint", "tool-erase", "tool-pick", "tool-fill", "tool-fill-all"};
    if (!ui_icon(cv, UI_NAME[which], x + 2, y + 2 + dy, 0))
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

/* w / n + an arrow: a column or a row more (grow) or less (shrink) on that side; on the left or the top the drawing moves
 * over by one, so nothing is cut but what is taken away. arrow: 'A' up, 'B' down, 'C' right, 'D' left */
static void resize_side(Ed *e, int grow, char arrow) {
    int cols = arrow == 'C' || arrow == 'D', first = arrow == 'D' || arrow == 'A';
    if (cols ? (grow ? e->w >= MAXW : e->w <= 1) : (grow ? e->h >= MAXH : e->h <= 1)) return;
    push_undo(e);
    if (cols) {
        for (int y = 0; y < e->h; y++) {
            if (grow && first) { memmove(&e->g[y][1], &e->g[y][0], (size_t)e->w); e->g[y][0] = '.'; }
            else if (grow) e->g[y][e->w] = '.';
            else if (first) memmove(&e->g[y][0], &e->g[y][1], (size_t)e->w - 1);
        }
        e->w += grow ? 1 : -1;
    } else {
        if (grow && first) { memmove(e->g[1], e->g[0], sizeof e->g[0] * (size_t)e->h); memset(e->g[0], '.', MAXW); }
        else if (grow) memset(e->g[e->h], '.', MAXW);
        else if (first) memmove(e->g[0], e->g[1], sizeof e->g[0] * (size_t)(e->h - 1));
        e->h += grow ? 1 : -1;
    }
    save(e);
    static const char *SIDE[4] = {"TOP", "BOTTOM", "RIGHT", "LEFT"};
    snprintf(e->msg, sizeof e->msg, "%s: %dX%d", SIDE[arrow - 'A'], e->w, e->h);
}

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


/* ---- the sprite picker: every sprite and animation as a card with its picture (or as a list with the picked one big), by
 * file; type to find; Enter or a click opens it in the editor; l there brings the picker back --------------------------- */
typedef struct { int file, sp, n, mine; char name[64]; } PickItem; /* (mine: one of its frames is yours) */
static File g_pf[NFILES];
static PickItem g_pi[512];
static int g_npi, g_pick_cols = 1;
static void picker_gather(const char *dir) {
    g_npi = 0;
    for (int f = 0; f < NFILES; f++) {
        char path[700];
        char mp[700];
        snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
        mine_path(mp, sizeof mp, f);
        if (file_load_merged(&g_pf[f], path, mp)) continue;
        static unsigned char seen[512];
        memset(seen, 0, sizeof seen);
        for (int i = 0; i < g_pf[f].nsprite && i < 512 && g_npi < 512; i++) {
            if (seen[i]) continue;
            int fr[128], n = anim_frames(&g_pf[f], i, fr, 128), num;
            for (int k = 0; k < n; k++) if (fr[k] < 512) seen[fr[k]] = 1;
            char name[64], base[64];
            sprite_name(&g_pf[f], fr[0], name, sizeof name);
            int numbered = split_name(name, base, sizeof base, &num) == 1;
            g_pi[g_npi] = (PickItem){f, fr[0], n, 0, ""};
            for (int k = 0; k < n; k++) if (g_pf[f].mine[fr[k]]) g_pi[g_npi].mine = 1;
            snprintf(g_pi[g_npi].name, sizeof g_pi[g_npi].name, "%s", n > 1 || numbered ? base : name);
            for (char *q = g_pi[g_npi].name; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            g_npi++;
        }
    }
}
/* the items the picker shows, in order: matching what is typed; the cards only the file of their tab */
static int picker_shown(const Ed *e, int *out) {
    int n = 0;
    for (int i = 0; i < g_npi; i++) {
        if (e->pview == 0 && g_pi[i].file != e->ptab) continue;
        if (e->find[0] && !strstr(g_pi[i].name, e->find)) continue;
        out[n++] = i;
    }
    return n;
}
/* a sprite of the picker's files, fitted into w x h (z at most), centred; frame k of its animation (playing: the picked one) */
static void picker_sprite(Canvas *cv, const PickItem *it, int k, int x, int y, int w, int h, int z) {
    int fr[128], n = anim_frames(&g_pf[it->file], it->sp, fr, 128), sw, sh;
    static char g[MAXH][MAXW + 1];
    grid_of(&g_pf[it->file], fr[k % n], g, &sw, &sh);
    while (z > 1 && (sw * z > w || sh * z > h)) z--;
    draw_grid_at(cv, &(Ed){.f = g_pf[it->file]}, g, sw, sh, x + (w - sw * z) / 2, y + (h - sh * z) / 2, z, 0);
}
/* the view buttons, each a small picture of its page (9 x 7): the cards a grid of them; the list a column of names, a big
 * picture and its frames under it. From sprites/ui.txt (view-cards, view-list), else these; brighter when on */
static const char *const VIEW_ICON[2][7] = {
    {"####.####", "####.####", "####.####", ".........", "####.####", "####.####", "####.####"},
    {"###.#####", "....#####", "###.#####", "....#####", "###......", "....##.##", "###.##.##"},
};
static void picker_view_button(Canvas *cv, int x, int y, int which, int on) {
    canvas_rect(cv, x - 2, y - 2, 13, 11, on ? RGB(52, 54, 70) : RGB(28, 28, 36));
    if (!ui_icon(cv, which ? "view-list" : "view-cards", x, y, on ? 0.6 : 0)) {
        Rgb k = on ? RGB(220, 220, 235) : RGB(90, 90, 110);
        for (int yy = 0; yy < 7; yy++) for (int xx = 0; xx < 9; xx++) if (VIEW_ICON[which][yy][xx] == '#') canvas_px(cv, x + xx, y + yy, k);
    }
    region(R_PVIEW, which, x - 2, y - 2, 13, 11, RGB(0, 0, 0));
}
static void picker_search(Canvas *cv, int x, int y, int w, const char *typed) {
    canvas_rect(cv, x, y, w, 9, RGB(30, 30, 40));
    canvas_text(cv, x + 3, y + 2, typed[0] ? typed : "TYPE TO FIND", typed[0] ? C_WHITE : RGB(90, 90, 110));
    if (typed[0]) canvas_rect(cv, x + 3 + text_width(typed) + 1, y + 2, 3, 5, C_YELLOW);
}
static void draw_picker(const Ed *e, Canvas *cv, double now) {
    int W = cv->w, H = cv->h, sh[512], n = picker_shown(e, sh), k = (int)(now / 0.3); /* (k: the picked one plays) */
    canvas_text(cv, 4, 3, "OPEN A SPRITE", C_YELLOW);
    picker_view_button(cv, W - 4 - 24, 3, 0, e->pview == 0);
    picker_view_button(cv, W - 4 - 9, 3, 1, e->pview == 1);
    static const char *TABS[NFILES] = {"PROPS", "ITEMS", "CLAWD", "UI"};
    if (e->pview == 0) { /* cards: a tab per file, the search box, the cards */
        int tx = 4, ty = 14;
        for (int f = 0; f < NFILES; f++) {
            int tw = text_width(TABS[f]) + 8;
            canvas_rect(cv, tx, ty, tw, 10, f == e->ptab ? RGB(52, 54, 70) : RGB(28, 28, 36));
            if (f == e->ptab) canvas_rect(cv, tx + 2, ty + 9, tw - 4, 1, C_YELLOW);
            canvas_text(cv, tx + 4, ty + 3, TABS[f], f == e->ptab ? C_WHITE : C_GREY);
            region(R_PTAB, f, tx, ty, tw, 10, RGB(0, 0, 0));
            tx += tw + 2;
        }
        int sw = W - 4 - tx - 6 < 90 ? W - 4 - tx - 6 : 90;
        if (sw > 40) picker_search(cv, W - 4 - sw, ty, sw, e->find);
        int gap = 4, cols = (W - 8 + gap) / (46 + gap) > 0 ? (W - 8 + gap) / (46 + gap) : 1, cw = (W - 8 + gap) / cols - gap, chh = 42, y0 = ty + 16;
        int vis = (H - 24 - y0 + gap) / (chh + gap) > 0 ? (H - 24 - y0 + gap) / (chh + gap) : 1;
        g_pick_cols = cols;
        int at = 0; for (int j = 0; j < n; j++) if (sh[j] == e->psel) at = j;
        int first = e->pscroll; /* (the rows shown keep the picked one in view) */
        if (at / cols < first) first = at / cols;
        if (at / cols >= first + vis) first = at / cols - vis + 1;
        if (first < 0) first = 0;
        ((Ed *)e)->pscroll = first;
        for (int j = first * cols; j < n && j < (first + vis) * cols; j++) {
            const PickItem *it = &g_pi[sh[j]];
            int cx = 4 + (j % cols) * (cw + gap), cy = y0 + (j / cols - first) * (chh + gap), on = sh[j] == e->psel;
            canvas_rect(cv, cx, cy, cw, chh, on ? RGB(44, 44, 60) : RGB(28, 28, 36));
            checker(cv, cx + 3, cy + 3, cw - 6, 26);
            picker_sprite(cv, it, on ? k : 0, cx + 3, cy + 3, cw - 6, 26, 3);
            char nm[24]; snprintf(nm, sizeof nm, "%.*s", (cw - 4) / 4, it->name);
            canvas_text(cv, cx + 3, cy + 32, nm, on ? C_WHITE : RGB(190, 190, 205));
            if (it->n > 1) { char fn[8]; snprintf(fn, sizeof fn, "%d", it->n); canvas_rect(cv, cx + cw - text_width(fn) - 6, cy + 4, text_width(fn) + 4, 7, RGB(18, 18, 24)); canvas_text(cv, cx + cw - text_width(fn) - 4, cy + 5, fn, C_YELLOW); }
            if (it->mine) { canvas_rect(cv, cx + 4, cy + 4, 4, 4, RGB(18, 18, 24)); canvas_rect(cv, cx + 5, cy + 5, 2, 2, RGB(120, 200, 255)); } /* (yours) */
            if (on) frame(cv, cx, cy, cw, chh, C_YELLOW);
            region(R_PITEM, sh[j], cx, cy, cw, chh, RGB(0, 0, 0));
        }
        if (first > 0) canvas_text(cv, W / 2 - 1, y0 - 6, "^", C_GREY);
        if ((first + vis) * cols < n) canvas_text(cv, W / 2 - 1, H - 29, "V", C_GREY);
    } else { /* a list by file, the picked one big on the right with its file, size and frames */
        int lw = W / 3 < 92 ? 92 : W / 3, y = 26;
        picker_search(cv, 4, 14, lw - 4, e->find);
        int rows = (H - 26 - y) / 7, at = 0, line = 0;
        for (int j = 0; j < n; j++) { if (sh[j] == e->psel) at = j; }
        (void)rows;
        int first = e->pscroll; /* (in items; scrolled so the picked one is in view, the files' titles counted) */
        if (at < first) first = at;
        for (;;) { /* the last item that fits from `first` on, as drawn below */
            int yy = y, lf = -1, last = first - 1;
            for (int j = first; j < n; j++) {
                if (g_pi[sh[j]].file != lf) { if (lf >= 0) yy += 3; if (yy + 17 > H - 24) break; yy += 10; lf = g_pi[sh[j]].file; }
                if (yy + 7 > H - 24) break;
                last = j; yy += 7;
            }
            if (at <= last || first >= at) break;
            first++;
        }
        if (first < 0) first = 0;
        ((Ed *)e)->pscroll = first;
        int lastf = -1;
        for (int j = first; j < n && line < rows; j++) {
            const PickItem *it = &g_pi[sh[j]];
            if (it->file != lastf) { /* a file's title (a gap before it, after the names of the file before) */
                if (lastf >= 0) y += 3;
                if (y + 17 > H - 24) break;
                canvas_text(cv, 4, y, TABS[it->file], RGB(84, 84, 104)); canvas_rect(cv, 4, y + 7, lw - 4, 1, RGB(40, 40, 52)); y += 10; line++; lastf = it->file;
            }
            if (y + 7 > H - 24) break; /* (no line half under the hint) */
            int on = sh[j] == e->psel;
            if (on) canvas_rect(cv, 2, y - 1, lw, 7, RGB(44, 44, 60));
            if (it->mine) canvas_rect(cv, 3, y + 2, 2, 2, RGB(120, 200, 255)); /* (yours) */
            canvas_text(cv, 7, y, it->name, on ? C_WHITE : RGB(190, 190, 205));
            if (it->n > 1) { char fn[8]; snprintf(fn, sizeof fn, "%d", it->n); canvas_text(cv, lw - text_width(fn) - 4, y, fn, on ? C_YELLOW : RGB(110, 110, 130)); }
            region(R_PITEM, sh[j], 2, y - 1, lw, 7, RGB(0, 0, 0));
            y += 7; line++;
        }
        canvas_rect(cv, lw + 4, 14, 1, H - 38, RGB(40, 40, 52));
        if (n && e->psel >= 0) {
            const PickItem *it = &g_pi[e->psel];
            int px = lw + 12, pw = W - px - 6, ph = H - 14 - 26 - 36;
            if (ph < 20) ph = 20;
            checker(cv, px, 14, pw, ph);
            picker_sprite(cv, it, k, px, 14, pw, ph, 8);
            static char g[MAXH][MAXW + 1]; int w, h;
            grid_of(&g_pf[it->file], it->sp, g, &w, &h);
            char info[96];
            snprintf(info, sizeof info, "%s   %s   %dX%d   %d FRAME%s", it->name, FILES[it->file], w, h, it->n, it->n == 1 ? "" : "S");
            for (char *q = info; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            canvas_text(cv, px, 14 + ph + 4, info, C_YELLOW);
            for (int f = 0, fx = px; f < it->n && fx + 32 <= W - 4; f++, fx += 35) { checker(cv, fx, 14 + ph + 13, 32, 22); picker_sprite(cv, it, f, fx, 14 + ph + 13, 32, 22, 2); }
        }
    }
    if (!n) { char t[64]; snprintf(t, sizeof t, "NOTHING CALLED %s", e->find); canvas_text(cv, (W - text_width(t)) / 2, H / 2, t, C_GREY); }
    { /* the bar above the hints: how many are yours (left); RESET, RESET ALL, MAKE DEFAULT (right; dim when they would do nothing) */
        int by = H - 21, yours = 0, file_mine = 0, picked_mine = e->psel >= 0 && e->psel < g_npi && g_pi[e->psel].mine;
        for (int i = 0; i < g_npi; i++) { yours += g_pi[i].mine; if (e->psel >= 0 && e->psel < g_npi && g_pi[i].file == g_pi[e->psel].file && g_pi[i].mine) file_mine = 1; }
        canvas_rect(cv, 0, by - 2, W, 1, RGB(40, 40, 52));
        char yt[24];
        snprintf(yt, sizeof yt, "YOURS: %d", yours);
        canvas_rect(cv, 5, by + 3, 2, 2, RGB(120, 200, 255));
        canvas_text(cv, 10, by + 2, yt, RGB(150, 150, 170));
        static const char *BT[3] = {"RESET", "RESET ALL", "MAKE DEFAULT"};
        int live[3] = {picked_mine, yours > 0, file_mine}, bx = W - 4;
        for (int k = 2; k >= 0; k--) {
            int bw = text_width(BT[k]) + 6;
            bx -= bw;
            canvas_rect(cv, bx, by, bw, 9, live[k] ? RGB(52, 54, 70) : RGB(28, 28, 36));
            canvas_text(cv, bx + 3, by + 2, BT[k], live[k] ? C_WHITE : RGB(70, 70, 86));
            if (live[k]) region(R_PRESET + k, 0, bx, by, bw, 9, RGB(0, 0, 0));
            bx -= 3;
        }
    }
    canvas_rect(cv, 0, H - 10, W, 1, RGB(40, 40, 52));
    canvas_text(cv, 4, H - 7, e->pview == 0 ? "ARROWS PICK  ENTER OPEN  TAB FILE  \\ LIST  Q QUIT" : "UP DOWN PICK  ENTER OPEN  \\ CARDS  Q QUIT", RGB(150, 150, 170));
}
/* the picker opens: the items again, the open sprite's animation picked */
static void picker_open(Ed *e, const char *dir, int from_edit) {
    picker_gather(dir);
    e->page = 1; e->came_from_edit = from_edit; e->find[0] = 0; e->pscroll = 0; e->ptab = e->file; e->psel = -1;
    char nm[64], base[64], b2[64]; int num;
    snprintf(nm, sizeof nm, "%s", e->name);
    split_name(nm, base, sizeof base, &num);
    for (int i = 0; i < g_npi; i++) if (g_pi[i].file == e->file) {
        snprintf(b2, sizeof b2, "%s", g_pi[i].name);
        for (char *q = b2; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
        if (!strcmp(b2, base) || !strcmp(b2, nm)) e->psel = i;
    }
    if (e->psel < 0) { int sh[512]; e->psel = picker_shown(e, sh) ? sh[0] : 0; }
}
/* what is typed changed: the first match picked (the cards: on the first tab that has one) */
static void picker_refind(Ed *e) {
    int sh[512];
    if (e->pview == 0 && !picker_shown(e, sh)) for (int f = 0; f < NFILES; f++) { e->ptab = f; if (picker_shown(e, sh)) break; }
    int n = picker_shown(e, sh);
    if (n) e->psel = sh[0];
    e->pscroll = 0;
}
static void picker_move(Ed *e, int d) { /* the picked one d places on (the cards: a row is g_pick_cols) */
    int sh[512], n = picker_shown(e, sh), at = 0;
    if (!n) return;
    for (int j = 0; j < n; j++) if (sh[j] == e->psel) at = j;
    at += d;
    at = at < 0 ? 0 : at >= n ? n - 1 : at;
    e->psel = sh[at];
}
/* a sprite is opened: the files as they are now, to put back if its changes are discarded when leaving it */
/* (your files as they were: put back as they were, or taken away if there was none) */
static void put_back(int f, const char *was) {
    char path[700];
    mine_path(path, sizeof path, f);
    if (!was) { remove(path); return; }
    FILE *o = fopen(path, "wb");
    if (o) { fputs(was, o); fclose(o); }
}
static void visit_start(Ed *e, const char *dir) {
    (void)dir;
    for (int f = 0; f < NFILES; f++) {
        char path[700];
        mine_path(path, sizeof path, f);
        free(e->visit[f]); e->visit[f] = slurp(path); e->vchanged[f] = 0;
    }
}
static void visit_discard(Ed *e, const char *dir) { /* (D when leaving a sprite: your files back as they were when it was opened) */
    (void)dir;
    for (int f = 0; f < NFILES; f++) if (e->vchanged[f]) { put_back(f, e->visit[f]); e->vchanged[f] = 0; }
}
/* RESET: the picked one back to the default (its frames that only you have go) */
static void picker_reset(Ed *e, const char *dir) {
    if (e->psel < 0 || e->psel >= g_npi) return;
    const PickItem *it = &g_pi[e->psel];
    File *f = &g_pf[it->file];
    static File d;
    if (file_load(&d, f->path) != 0) d.nsprite = 0;
    int fr[128], n = anim_frames(f, it->sp, fr, 128);
    for (int k = n - 1; k >= 0; k--) { /* (from the last line up: the earlier line numbers stay) */
        int sp = fr[k], at = f->sprite_line[sp], end = block_end(f, at);
        char nm[64];
        sprite_name(f, sp, nm, sizeof nm);
        int di = sprite_find(&d, nm), from = di >= 0 ? at : at > 0 && !f->line[at - 1][0] ? at - 1 : at;
        int dat = di >= 0 ? d.sprite_line[di] : 0, dend = di >= 0 ? block_end(&d, dat) : 0, put = di >= 0 ? dend - dat : 0;
        for (int i = from; i < end; i++) free(f->line[i]);
        memmove(&f->line[from + put], &f->line[end], sizeof(char *) * (size_t)(f->nline - end));
        for (int i = 0; i < put; i++) f->line[from + i] = strdup(d.line[dat + i]);
        f->nline += put - (end - from);
    }
    file_reparse(f);
    write_mine(f);
    picker_gather(dir);
    snprintf(e->msg, sizeof e->msg, "RESET");
}
static void picker_reset_all(Ed *e, const char *dir) { /* RESET ALL: every file of yours goes */
    for (int f = 0; f < NFILES; f++) put_back(f, NULL);
    picker_gather(dir);
    (void)e;
}
/* MAKE DEFAULT: yours in the picked one's file become the default (written into the repo's file; your file goes) */
static void picker_make_default(Ed *e, const char *dir) {
    if (e->psel < 0 || e->psel >= g_npi) return;
    int fi = g_pi[e->psel].file;
    File *f = &g_pf[fi];
    char tmp[720];
    snprintf(tmp, sizeof tmp, "%s.edit", f->path);
    FILE *o = fopen(tmp, "w");
    if (!o) return;
    for (int i = 0; i < f->nline; i++) fprintf(o, "%s\n", f->line[i]);
    fclose(o);
    if (rename(tmp, f->path)) return;
    put_back(fi, NULL);
    picker_gather(dir);
}
/* the picked one opens in the editor */
static void picker_pick(Ed *e, const char *dir) {
    if (e->psel < 0 || e->psel >= g_npi) return;
    char nm[64];
    sprite_name(&g_pf[g_pi[e->psel].file], g_pi[e->psel].sp, nm, sizeof nm);
    file_open(e, dir, g_pi[e->psel].file, nm);
    e->page = 0; e->zoom = 0; e->msg[0] = 0; e->playing = 0;
    visit_start(e, dir);
}

/* quitting with changes: keep (save) or discard */
static void draw_asking(Canvas *cv, const Ed *e) {
    char a[96];
    const char *b = "S SAVE    D DISCARD    ESC BACK";
    if (e->asking == 3) { int n = 0; for (int i = 0; i < g_npi; i++) n += g_pi[i].mine; snprintf(a, sizeof a, "RESET %d SPRITE%s TO THE DEFAULT?", n, n == 1 ? "" : "S"); b = "Y RESET    N KEEP THEM"; }
    else if (e->asking == 4) { snprintf(a, sizeof a, "MAKE YOURS IN %s THE DEFAULT?", e->psel >= 0 && e->psel < g_npi ? FILES[g_pi[e->psel].file] : ""); for (char *q = a; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32; b = "Y MAKE IT THE DEFAULT    N NOT NOW"; }
    else if (e->asking == 2) { snprintf(a, sizeof a, "SAVE THE CHANGES TO %s?", e->name); for (char *q = a; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32; }
    else snprintf(a, sizeof a, "SAVE THE CHANGES?");
    int w = (text_width(b) > text_width(a) ? text_width(b) : text_width(a)) + 12, h = 24, bx = (cv->w - w) / 2, by = (cv->h - h) / 2;
    canvas_rect(cv, bx - 1, by - 1, w + 2, h + 2, C_GREY);
    canvas_rect(cv, bx, by, w, h, RGB(20, 20, 28));
    canvas_text(cv, bx + (w - text_width(a)) / 2, by + 4, a, C_YELLOW);
    canvas_text(cv, bx + (w - text_width(b)) / 2, by + 14, b, C_WHITE);
}
static void draw(const Ed *e, Canvas *cv) {
    g_nreg = 0;
    double now = wall_now();
    if (e->page) { canvas_rect(cv, 0, 0, cv->w, cv->h, RGB(18, 18, 24)); draw_picker(e, cv, now); if (e->asking) draw_asking(cv, e); return; }
    canvas_rect(cv, 0, 0, cv->w, cv->h, RGB(18, 18, 24));
    char t[160];
    snprintf(t, sizeof t, "EDIT %s  %s  %dX%d", e->name, FILES[e->file], e->w, e->h);
    for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    canvas_text(cv, 4, 3, t, C_YELLOW);
    int msg_top = e->msg[0] && 4 + text_width(t) + 12 + text_width(e->msg) + 4 + 16 <= cv->w; /* (a message that does not fit by the title goes under the grid) */
    if (msg_top) canvas_text(cv, cv->w - 16 - text_width(e->msg) - 4, 3, e->msg, C_GREY);
    { /* the fold button, top right: a small window in dim lines, its right part (the panel) shaded while it is open;
         brighter under the pointer */
        int fx = cv->w - 4 - 9, fy = 3;
        if (ui_icon(cv, g_panel_folded ? "fold-closed" : "fold-open", fx, fy, g_fold_hover ? 0.4 : 0)) { region(R_FOLD, 0, fx - 1, fy - 1, 11, 8, RGB(0, 0, 0)); goto folded_icon; }
        Rgb k = g_fold_hover ? RGB(150, 150, 175) : RGB(76, 76, 96), fill = g_fold_hover ? RGB(90, 90, 112) : RGB(46, 46, 60);
        canvas_rect(cv, fx + 1, fy, 7, 1, k); canvas_rect(cv, fx + 1, fy + 4, 7, 1, k);       /* (its outline without the corner */
        canvas_rect(cv, fx, fy + 1, 1, 3, k); canvas_rect(cv, fx + 8, fy + 1, 1, 3, k);       /*  pixels: softly rounded) */
        if (!g_panel_folded) canvas_rect(cv, fx + 6, fy + 1, 2, 3, fill);
        canvas_rect(cv, fx + 5, fy + 1, 1, 3, k);
        region(R_FOLD, 0, fx - 1, fy - 1, 11, 8, RGB(0, 0, 0));
        folded_icon:;
    }
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
       green blue (click to type), the file's colours (the + slot first: a changed colour goes there, never into a file colour unless double-clicked), shades, the real-size view */
    int x = L.px, y = L.ty, tr;
    { /* the field (and the rows of colours) as wide as the window leaves, beside the real-size view if that goes right of the panel */
        int tlf = timeline_h(e, cv->h) >= tl_full(e, 1), lw = text_width("AS IN THE ROOM"), uw = L.px - L.gx - 10;
        int under = tlf || (L.ty + L.ah + 18 + 9 + (e->h + 4) * 2 <= L.hy - 3 && e->w * 3 + 6 <= uw && lw <= uw);
        int room = cv->w - x - 4 - (under ? 0 : (e->w * 2 > lw ? e->w * 2 : lw) + 14);
        if (room < 96) room = cv->w - x - 4; /* (no room for that column anyway: it goes under the shades, the panel takes it all) */
        g_field_w = room < 96 ? 96 : room > 200 ? 200 : room;
    }
    Rgb cur = pick_colour(e, &tr);
    int body = e->pk == 1 && e->f.pal_body[e->pi];
    static const char *TOOL[4] = {"PAINT", "ERASE", "PICK", "FILL"};
    int lit[4] = {!tr && !e->picking, tr && !e->picking, e->picking, e->filling > 0};
    if (g_panel_folded) { /* folded: a ribbon at the right, the tools one over another, then the colour */
        int ry = y;
        for (int i = 0; i < 4; i++, ry += 17) { tool_button(cv, x, ry, i == 3 && e->filling == 2 ? 4 : i, lit[i]); region(R_TOOL, i, x, ry, 13, 15, RGB(0, 0, 0)); }
        if (tr) checker(cv, x, ry + 2, 13, 9); else canvas_rect(cv, x, ry + 2, 13, 9, cur);
        frame(cv, x, ry + 2, 13, 9, RGB(150, 150, 170));
    } else {
    int roww = 0; /* (the row spread over the colour field: the first at its left edge, the last at its right, even gaps) */
    for (int i = 0; i < 4; i++) roww += text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13;
    for (int i = 0, used = 0; i < 4; i++) {
        const char *nm = i == 3 && e->filling == 2 ? "ALL" : TOOL[i];
        int cw = text_width(TOOL[i]) > 13 ? text_width(TOOL[i]) : 13;
        int tx = x + used + (FIELD_W - roww) * i / 3;
        used += cw;
        tool_button(cv, tx + (cw - 13) / 2, y, i == 3 && e->filling == 2 ? 4 : i, lit[i]);
        canvas_text(cv, tx + (cw - text_width(nm)) / 2, y + 16, nm, lit[i] ? C_WHITE : C_GREY);
        region(R_TOOL, i, tx, y, cw, 21, RGB(0, 0, 0));
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
    y += SECTION;
    if (y + 8 <= pbot) { /* the + slot: empty, a plus; a new colour (not in the file yet) sits in it */
        if (e->pk == 2) { canvas_rect(cv, x, y, 8, 8, e->prgb); frame(cv, x - 1, y - 1, 10, 10, RGB(150, 150, 170)); }
        else { canvas_rect(cv, x, y, 8, 8, RGB(30, 30, 40)); canvas_rect(cv, x + 2, y + 4, 5, 1, RGB(150, 150, 170)); canvas_rect(cv, x + 4, y + 2, 1, 5, RGB(150, 150, 170)); }
        region(R_ADD, 0, x, y, 8, 8, RGB(0, 0, 0));
    }
    for (int i = 0; i < e->f.npal; i++) {
        int sx = x + ((i + 1) % SW_COLS) * 10, sy = y + ((i + 1) / SW_COLS) * SW_ROW;
        if (sy + 8 > pbot) break;
        canvas_rect(cv, sx, sy, 8, 8, e->f.pal_c[i]);
        region(R_FILE, i, sx, sy, 8, 8, e->f.pal_c[i]);
        int used = 0;
        for (int yy = 0; yy < e->h && !used; yy++) for (int xx = 0; xx < e->w; xx++) if (e->g[yy][xx] == e->f.pal_ch[i]) { used = 1; break; }
        if (used) canvas_rect(cv, sx + 3, sy + 9, 2, 1, RGB(200, 200, 215)); /* (used in this sprite) */
        if (e->pk == 1 && e->pi == i) frame(cv, sx - 1, sy - 1, 10, 10, e->recol == i + 1 ? RGB(230, 200, 90) : RGB(150, 150, 170)); /* (the one painting: a soft ring, a dark gap inside it; yellow: being changed in the file) */
    }
    y += ((e->f.npal + SW_COLS) / SW_COLS) * SW_ROW + 2;
    if (y + SECTION + 8 <= pbot) section(cv, x, y, "SHADES");
    y += SECTION;
    int dark = (SW_COLS - 1) / 2, light = SW_COLS - 1 - dark;
    for (int k = -dark; k <= light && y + 8 <= pbot; k++) { /* darker to lighter, as many as a row of the file's colours */
        int sx = x + (k + dark) * 10;
        Rgb c = shade(cur, k, k < 0 ? dark : light);
        if (!tr) { canvas_rect(cv, sx, y, 8, 8, c); region(R_NEW, 0, sx, y, 8, 8, c); }
        if (k == 0 && !tr) frame(cv, sx - 1, y - 1, 10, 10, RGB(90, 90, 108)); /* (this colour: a dimmer ring) */
    }
    y += 12;
    } /* (the panel, unfolded) */
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
    } else if (g_panel_folded) { /* (folded: no room by the ribbon) */
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
    if (e->asking) draw_asking(cv, e);
}

static void set_file_colour(Ed *e, int i, Rgb c);
/* the colour to paint becomes c: a new colour in the + slot (one already in the file is that one); a double-clicked file
   colour is changed in the file (all its pixels) */
static void apply_colour(Ed *e, Rgb c) {
    if (e->pk == 1 && e->recol == e->pi + 1 && !e->f.pal_body[e->pi]) { push_undo(e); set_file_colour(e, e->pi, c); save(e); }
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
int editor_find(const char *dir, const char *name, int *which);
int editor_run(const char *dir, const char *name) {
    static Ed e;
    memset(&e, 0, sizeof e);
    e.pk = 1; e.pi = 0; e.hx = e.hy = -1; e.drag = -1; e.last_pk = 1; e.speed = 1; e.onion = 1; e.del_ask = -1;
    for (int f = 0; f < NFILES; f++) { char mp[700]; mine_path(mp, sizeof mp, f); e.orig[f] = slurp(mp); } /* (your files as they are: for a discard) */
    int which = 0; /* the file that has a sprite called NAME (props first), else an animation */
    if (name && editor_find(dir, name, &which)) return 1;
    file_open(&e, dir, which, name);
    if (!name) picker_open(&e, dir, 0); /* (no name: the picker first) */
    else visit_start(&e, dir);
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
        ui_watch(dir); /* (the editor's own icons, sprites/ui.txt: edited, they change at once) */
        if (e.sizing && wall_now() > e.sizing_until) { e.sizing = 0; if (strstr(e.msg, "ARROW")) e.msg[0] = 0; } /* (w / n let go) */
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
        struct timeval tv = {0, e.playing ? 62500 : e.page ? 100000 : 200000}; /* (16 frames a second while it plays; the picker's picked one plays too) */
        if (select(STDIN_FILENO + 1, &fs, NULL, NULL, &tv) <= 0) { /* nothing came; an Esc with nothing after it is Esc itself */
            if (nin == 1 && in[0] == 0x1b) {
                nin = 0;
                if (e.asking) e.asking = 0;
                else if (e.typing) e.typing = 0;
                else if (e.page) { if (e.find[0]) { e.find[0] = 0; picker_refind(&e); } else quit = 1; } /* (the picker: Esc clears the search, else quits) */
            }
            continue;
        }
        ssize_t k = read(STDIN_FILENO, in + nin, sizeof in - 1 - (size_t)nin);
        if (k <= 0) continue;
        nin += (int)k; in[nin] = 0;
        int i = 0;
        while (i < nin) {
            if (in[i] == 0x1b) {
                if (i + 1 >= nin) break; /* (more to come) */
                if (e.asking && in[i + 1] != '[') { e.asking = 0; i++; continue; } /* (Esc: back to editing) */
                if (e.page && !e.asking && in[i + 1] != '[') { /* the picker, Esc: clears the search, else quits */
                    i++;
                    if (e.find[0]) { e.find[0] = 0; picker_refind(&e); } else quit = 1;
                    continue;
                }
                if (e.page && in[i + 1] == '[' && i + 2 < nin && in[i + 2] >= 'A' && in[i + 2] <= 'D') { /* the picker's arrows */
                    char a = in[i + 2];
                    picker_move(&e, a == 'C' ? 1 : a == 'D' ? -1 : (e.pview == 0 ? g_pick_cols : 1) * (a == 'B' ? 1 : -1));
                    i += 3; continue;
                }
                if (e.typing && in[i + 1] != '[') { e.typing = 0; i++; continue; } /* (Esc: stop typing) */
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] == '<') { /* SGR mouse: ESC [ < b ; x ; y (M | m) */
                    int b, x, y, used = 0;
                    char fin;
                    if (sscanf(in + i, "\x1b[<%d;%d;%d%c%n", &b, &x, &y, &fin, &used) < 4 || !used) { if (strchr(in + i, 'M') || strchr(in + i, 'm')) { i++; continue; } break; }
                    i += used;
                    if (e.asking) continue;
                    int px = x - 1, py = (y - 1) * 2, btn = b & 3, alt = b & 8, motion = b & 32, release = fin == 'm';
                    const Region *r = hit(px, py);
                    if (e.page) { /* the picker: a click picks (on the picked one: opens it); a tab, a view; the wheel moves */
                        if (b & 64) { picker_move(&e, ((b & 3) == 0 ? -1 : 1) * (e.pview == 0 ? g_pick_cols : 1)); continue; }
                        if (motion || release || btn != 0 || !r) continue;
                        if (r->kind == R_PITEM) { if (r->i == e.psel) picker_pick(&e, dir); else e.psel = r->i; }
                        else if (r->kind == R_PTAB) { e.ptab = r->i; e.pscroll = 0; int sh[512]; if (picker_shown(&e, sh)) e.psel = sh[0]; }
                        else if (r->kind == R_PVIEW) { e.pview = r->i; e.pscroll = 0; }
                        else if (r->kind == R_PRESET) picker_reset(&e, dir);
                        else if (r->kind == R_PRESETALL) e.asking = 3; /* (asks first) */
                        else if (r->kind == R_PMAKEDEF) e.asking = 4;
                        continue;
                    }
                    e.hx = e.hy = -1;
                    if (r && r->kind == R_GRID) { e.hx = (px - L.gx) / L.z; e.hy = (py - L.gy) / L.z; }
                    if (b & 64) { /* the mouse wheel: over the frames it scrolls them (up / left back, down / right on); elsewhere nothing */
                        if (r && (r->kind == R_STRIP || r->kind == R_FRAME || r->kind == R_FADD)) g_tl_first += (b & 3) == 0 || (b & 3) == 2 ? -1 : 1;
                        continue;
                    }
                    if (g_div_drag) { /* (dragging the divider: below two thirds of the smallest timeline it folds to its top line) */
                        if (release) g_div_drag = 0;
                        else if (motion) { int h = rows * 2 - py; g_tl_user = h < tl_full(&e, 1) * 2 / 3 ? -1 : h; }
                        continue;
                    }
                    g_div_hover = r && r->kind == R_DIV;
                    g_fold_hover = r && r->kind == R_FOLD;
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
                        int inplace = e.pk == 1 && e.recol == e.pi + 1 && !e.f.pal_body[e.pi];
                        if (e.drag < 0) { e.drag = k; if (inplace) push_undo(&e); else { e.pk = 2; e.prgb = was; } } /* (a double-clicked file colour changes in the file; else a new colour, in the + slot) */
                        Rgb c = hsv_rgb(h, sat, val);
                        if (inplace) set_file_colour(&e, e.pi, c); else e.prgb = c;
                        e.picking = 0;
                        continue;
                    }
                    if (r && r->kind == R_GRID && (alt || (e.picking && btn == 0))) { /* pick the colour under the pointer */
                        char ch = e.g[e.hy][e.hx];
                        e.pk = 0; e.recol = 0;
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
                        if (e.g[e.hy][e.hx] == ch) continue; /* (already that colour: nothing to fill, no undo step) */
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
                        else if (r->kind == R_FOLD) g_panel_folded = !g_panel_folded;
                        else if (r->kind == R_ADD && e.pk == 2) { char ch = letter_for(&e, e.prgb); for (int q = 0; q < e.f.npal; q++) if (e.f.pal_ch[q] == ch) { e.pk = 1; e.pi = q; } e.recol = 0; save(&e); } /* (the new colour into the file) */
                        else if (r->kind == R_ADD) { /* an empty + slot: a new colour, from the one in hand */
                            Rgb c = e.pk == 1 ? e.f.pal_c[e.pi] : e.last_pk == 1 ? e.f.pal_c[e.last_pi] : e.last_pk == 2 ? e.last_prgb : RGB(255, 255, 255);
                            e.pk = 2; e.prgb = c; e.recol = 0; e.picking = 0;
                        }
                        else if (r->kind == R_FILE) { /* a file colour to paint with; double-clicked: changing the colour changes it in the file */
                            static double last_t; static int last_i = -1;
                            double t = wall_now();
                            e.recol = t - last_t < 0.4 && last_i == r->i && !e.f.pal_body[r->i] ? r->i + 1 : 0;
                            last_t = t; last_i = r->i;
                            e.pk = 1; e.pi = r->i; e.picking = 0;
                        }
                        else if (r->kind == R_NEW) { /* a colour already in the file is that one; else a new colour */
                            e.pk = 2; e.prgb = r->c; e.recol = 0;
                            for (int q = 0; q < e.f.npal; q++) if (!e.f.pal_body[q] && rgb_eq(e.f.pal_c[q], r->c)) { e.pk = 1; e.pi = q; }
                        }
                    }
                    continue;
                }
                if (in[i + 1] == '[' && i + 2 < nin && in[i + 2] >= 'A' && in[i + 2] <= 'D' && e.sizing && wall_now() < e.sizing_until) { /* w / n held: an arrow grows / shrinks on that side */
                    resize_side(&e, e.sizing == 'w', in[i + 2]);
                    e.sizing_until = wall_now() + 1.0;
                    i += 3; continue;
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
            if (e.page && !e.asking) { /* the picker: type to find, Backspace, Enter opens, Tab the next file, \ the view */
                size_t fl = strlen(e.find);
                if (ch == '\r' || ch == '\n') picker_pick(&e, dir);
                else if (ch == '\t') { e.ptab = (e.ptab + 1) % NFILES; e.pscroll = 0; int sh[512]; if (picker_shown(&e, sh)) e.psel = sh[0]; }
                else if (ch == '\\') { e.pview = !e.pview; e.pscroll = 0; } /* (\: the other view; Ctrl+V is Windows Terminal's paste) */
                else if (ch == 0x03 || ((ch == 'q' || ch == 'Q') && !fl)) quit = 1; /* (q, the search empty, or Ctrl+C: quit) */
                else if ((ch == 127 || ch == 8) && fl) { e.find[fl - 1] = 0; picker_refind(&e); }
                else if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '~') && fl < sizeof e.find - 1) {
                    e.find[fl] = (char)(ch >= 'a' && ch <= 'z' ? ch - 32 : ch); e.find[fl + 1] = 0; picker_refind(&e);
                }
                continue;
            }
            if (e.asking == 3 || e.asking == 4) { /* RESET ALL / MAKE DEFAULT: Y does it, N (or Esc) not */
                if (ch == 'y' || ch == 'Y') { if (e.asking == 3) picker_reset_all(&e, dir); else picker_make_default(&e, dir); e.asking = 0; }
                else if (ch == 'n' || ch == 'N') e.asking = 0;
                continue;
            }
            if (e.asking == 2) { /* leaving a sprite (q): keep or discard its changes, then the picker */
                if (ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y') { e.asking = 0; picker_open(&e, dir, 1); }
                else if (ch == 'd' || ch == 'D' || ch == 'n' || ch == 'N') { e.asking = 0; visit_discard(&e, dir); file_open(&e, dir, e.file, e.name); picker_open(&e, dir, 1); }
                continue;
            }
            if (e.asking) { /* keep or discard */
                if (ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y') quit = 1; /* (save: they are in the file already) */
                else if (ch == 'd' || ch == 'D' || ch == 'n' || ch == 'N') { /* discard: your files back as they were when the editor opened */
                    for (int f = 0; f < NFILES; f++) if (e.changed[f]) put_back(f, e.orig[f]);
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
            if (ch == 'p' || ch == 'P') { g_panel_folded = !g_panel_folded; continue; } /* (the colour panel folds / unfolds) */
            if (ch == 'l' || ch == 'L') ch = 'q'; /* (l: the same as q) */
            if (ch == 'w' || ch == 'W' || ch == 'n' || ch == 'N') { /* held (its repeats keep it), an arrow picks the side */
                e.sizing = ch == 'w' || ch == 'W' ? 'w' : 'n';
                e.sizing_until = wall_now() + 1.0;
                snprintf(e.msg, sizeof e.msg, "%s + ARROW: %s ON THAT SIDE", e.sizing == 'w' ? "W" : "N", e.sizing == 'w' ? "GROW" : "SHRINK");
                continue;
            }
            if (ch == 'b' || ch == 'B') { e.filling = (e.filling + 1) % 3; e.picking = 0; continue; }
            if (ch == 0x1A) ch = 'u'; /* Ctrl+Z: undo; Ctrl+Y: redo (U / R too) */
            else if (ch == 0x19) ch = 'r';
            else if (ch == 0x03) { { int any = 0; for (int f = 0; f < NFILES; f++) any |= e.changed[f]; if (any) e.asking = 1; else quit = 1; } continue; } /* Ctrl+C: quit, asking about changes */
            if (ch == 'q') { /* q: back to the sprite picker; changed: keep or discard first */
                e.playing = 0;
                int any = 0; for (int f = 0; f < NFILES; f++) any |= e.vchanged[f];
                if (any) e.asking = 2; else picker_open(&e, dir, 1);
            }
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
        }
        memmove(in, in + i, (size_t)(nin - i)); nin -= i;
    }
    tty_off();
    cells_free(&prev);
    file_free(&e.f);
    for (int f = 0; f < NFILES; f++) free(e.orig[f]);
    return 0;
}

/* --edit NAME when nothing is called NAME: the error, and the names closest to it (by edit distance, or containing it) */
static int edit_distance(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b), d[65][65];
    if (la > 64) la = 64;
    if (lb > 64) lb = 64;
    for (int i = 0; i <= la; i++) d[i][0] = i;
    for (int j = 0; j <= lb; j++) d[0][j] = j;
    for (int i = 1; i <= la; i++) for (int j = 1; j <= lb; j++) {
        int c = d[i - 1][j - 1] + (a[i - 1] != b[j - 1]), u = d[i - 1][j] + 1, l = d[i][j - 1] + 1;
        d[i][j] = c < u ? (c < l ? c : l) : (u < l ? u : l);
    }
    return d[la][lb];
}
/* 0 if the sprites have a sprite or an animation called NAME (the file it is in in *which); else the error is printed */
int editor_find(const char *dir, const char *name, int *which) {
    char best[3][64] = {"", "", ""}, n2[64], b2[64];
    int bd[3] = {99, 99, 99}, num;
    for (int pass = 0; pass < 2; pass++)
        for (int f = 0; f < NFILES; f++) {
            static File tf;
            memset(&tf, 0, sizeof tf);
            char path[700];
            snprintf(path, sizeof path, "%s/%s", dir, FILES[f]);
            { char mp[700]; mine_path(mp, sizeof mp, f); if (file_load_merged(&tf, path, mp)) continue; }
            int found = 0;
            for (int i = 0; i < tf.nsprite && !found; i++) {
                sprite_name(&tf, i, n2, sizeof n2);
                if (pass == 0 ? !strcmp(n2, name) : find_sprite(&tf, name) >= 0) found = 1;
                if (pass == 1) { /* (the closest names, for the error) */
                    const char *cand = split_name(n2, b2, sizeof b2, &num) ? b2 : n2;
                    int dist = strstr(cand, name) || strstr(name, cand) ? 1 : edit_distance(cand, name), dup = 0;
                    for (int k = 0; k < 3; k++) if (!strcmp(best[k], cand)) dup = 1;
                    for (int k = 0; k < 3 && !dup; k++) if (dist < bd[k]) {
                        for (int m = 2; m > k; m--) { bd[m] = bd[m - 1]; snprintf(best[m], 64, "%s", best[m - 1]); }
                        bd[k] = dist; snprintf(best[k], 64, "%s", cand); break;
                    }
                }
            }
            file_free(&tf);
            if (found) { if (which) *which = f; return 0; }
        }
    fprintf(stderr, "clawd-room --edit: nothing called %s in %s ({props,items,clawd,ui}.txt)\n", name, dir);
    int limit = (int)strlen(name) / 2 > 2 ? (int)strlen(name) / 2 : 2, any = 0;
    for (int k = 0; k < 3; k++) if (best[k][0] && bd[k] <= limit) { fprintf(stderr, any ? ", %s" : "  did you mean: %s", best[k]); any = 1; }
    if (any) fprintf(stderr, "\n");
    fprintf(stderr, "  ./clawd-room.sh --edit list   shows what there is\n");
    return 1;
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
        { char mp[700]; mine_path(mp, sizeof mp, f); if (file_load_merged(&tf, path, mp)) continue; }
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
        int hl = snprintf(head, sizeof head, "[ %s ] %d sprite%s | %d animation%s (name + frames)", FILES[f], alone, alone == 1 ? "" : "s", nanim, nanim == 1 ? "" : "s");
        memset(dash, '-', (size_t)hl + 4); dash[hl + 4] = 0; /* (a box round the heading: dashes above and below, / at its sides) */
        printf("%s\n/ %s /\n%s\n", dash, head, dash);
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
