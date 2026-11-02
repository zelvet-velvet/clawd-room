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
#include "art.h"
#include "sprites.h"
#include "term.h"
#include "parts.h"
#include "settings.h"

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
    int pfocus;                               /* the picker's search box has the focus: letters go into it (1 as the picker opens: q still
                                                 quits while it is empty; 2 clicked: q is typed too; 0 a click elsewhere: nothing is typed) */
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

#include "editor_mine.inc" /* your own sprites over the defaults */
#include "editor_anim.inc" /* animations: frame names and numbers */
#include "editor_screen.inc" /* the screen: drawing the editor and what a click is on */
#include "editor_picker.inc" /* the sprite picker */
#include "editor_themes.inc" /* the THEMES tab and its actions */

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
#include "stage.inc"
int editor_find(const char *dir, const char *name, int *which);
int editor_run(const char *dir, const char *name) {
    if (name && !strncmp(name, "anims/", 6)) return stage_run(dir, name + 6); /* (a part animation: the stage page) */
    static Ed e;
    memset(&e, 0, sizeof e);
    e.pk = 1; e.pi = 0; e.hx = e.hy = -1; e.drag = -1; e.last_pk = 1; e.speed = 1; e.onion = 1; e.del_ask = -1;
    for (int f = 0; f < NFILES; f++) { char mp[700]; mine_path(mp, sizeof mp, f); e.orig[f] = slurp(mp); } /* (your files as they are: for a discard) */
    int which = 0; /* the file that has a sprite called NAME (props first), else an animation */
    if (name && editor_find(dir, name, &which)) return 1;
    file_open(&e, dir, which, name);
    if (!name) { picker_open(&e, dir, 0); if (g_start_tab >= 0) { e.ptab = g_start_tab; g_start_tab = -1; int sh[512]; if (picker_shown(&e, sh)) e.psel = sh[0]; } } /* (no name: the picker first) */
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
        if (c != cols || r != rows || g_redraw) { cols = c; rows = r; cells_free(&prev); prev.c = NULL; put("\x1b[H"); g_redraw = 0; }
        ui_watch(dir); /* (the editor's own icons, sprites/ui.txt: edited, they change at once) */
        if (e.sizing && wall_now() > e.sizing_until) { e.sizing = 0; if (strstr(e.msg, "ARROW")) e.msg[0] = 0; } /* (w / n let go) */
        Canvas cv = canvas_new(cols, rows * 2, RGB(18, 18, 24));
        draw(&e, &cv);
        rec_frame(&cv);
        if (getenv("CLAWD_EDIT_PNG")) png_write(getenv("CLAWD_EDIT_PNG"), &cv, 4); /* (a picture of the editor, to look at) */
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
                else if (e.page) quit = picker_escape(&e); /* (the picker: Esc clears the search, leaves the clicked box, leaves an open theme, else quits) */
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
                    quit = picker_escape(&e);
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
                        if (!motion && !release && btn == 0) e.pfocus = r && r->kind == R_PFIND ? 2 : 0; /* (a press on the search box focuses it, anywhere else not) */
                        if (motion || release || btn != 0 || !r) continue;
                        if (r->kind == R_PITEM) { if (r->i == e.psel) picker_pick(&e, dir); else e.psel = r->i; }
                        else if (r->kind == R_PUSE) theme_use(&e, dir);
                        else if (r->kind == R_PBACK) theme_close(&e);
                        else if (r->kind == R_PTAB) { e.ptab = r->i; g_theme_open[0] = 0; e.pscroll = 0; int sh[512]; if (picker_shown(&e, sh)) e.psel = sh[0]; }
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
                else if (ch == '\t') { e.ptab = (e.ptab + 1) % (THEMES + 1); g_theme_open[0] = 0; e.pscroll = 0; int sh[512]; if (picker_shown(&e, sh)) e.psel = sh[0]; }
                else if (ch == ' ' && theme_target(&e)) theme_use(&e, dir); /* (space: use the picked theme) */
                else if (ch == '\\') { e.pview = !e.pview; e.pscroll = 0; } /* (\: the other view; Ctrl+V is Windows Terminal's paste) */
                else if (ch == 0x03) quit = 1;
                else if ((ch == 'q' || ch == 'Q') && e.pfocus != 2 && (!fl || !e.pfocus)) quit = picker_quit(&e, dir); /* (q, the search empty or not focused, or Ctrl+C: quit; clicked into, q is typed) */
                else if (!e.pfocus) {} /* (the search not focused: letters are not typed) */
                else if ((ch == 127 || ch == 8) && fl) { e.find[fl - 1] = 0; picker_refind(&e); }
                else if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '~') && fl < sizeof e.find - 1) {
                    e.find[fl] = (char)(ch >= 'a' && ch <= 'z' ? ch - 32 : ch); e.find[fl + 1] = 0; picker_refind(&e);
                }
                continue;
            }
            if (e.asking == 5) { /* the new theme's name: a-z 0-9 -, Backspace, Enter */
                size_t nl = strlen(g_newname);
                if (ch == '\r' || ch == '\n') { theme_make(&e, dir); e.asking = 0; }
                else if ((ch == 127 || ch == 8) && nl) g_newname[nl - 1] = 0;
                else if (((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || (ch >= 'A' && ch <= 'Z')) && nl < sizeof g_newname - 1) { g_newname[nl] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch); g_newname[nl + 1] = 0; }
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
    rec_finish();
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
    if (!strncmp(name, "anims/", 6)) { /* a part animation of anims/ (yours or the default) */
        if (which) *which = -1;
        if (parts_get(name + 6)) return 0;
        char names[32][PARTS_PATH_NAME];
        int n = parts_names(names, 32);
        fprintf(stderr, "clawd-room --edit: no part animation called %s\n  there are:", name + 6);
        for (int i = 0; i < n; i++) fprintf(stderr, " anims/%s", names[i]);
        fprintf(stderr, "%s\n", n ? "" : " none");
        return 1;
    }
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
    { char names[32][PARTS_PATH_NAME]; int n = parts_names(names, 32); /* the part animations: opened with their folder, anims/NAME */
      if (n) { printf("[ anims ] %d part animation%s\n", n, n == 1 ? "" : "s"); printf("  "); for (int i = 0; i < n; i++) printf("%sanims/%s", i ? "    " : "", names[i]); printf("\n\n"); } }
    printf("open one: ./clawd-room.sh --edit NAME\n");
    return 0;
}
