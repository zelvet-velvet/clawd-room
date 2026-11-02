/* Part of editor.c, included there (it shares editor.c's statics): the screen: drawing the editor and what a click is on. */
/* ---- the screen: canvas pixels; one terminal cell is 1 pixel wide and 2 high ---------------------------------------- */
/* what is where, filled while drawing, so a click finds what it is on */
enum { R_GRID, R_TOOL, R_FILE, R_NEW, R_FIELD, R_HUE, R_HEX, R_NUM, R_ADD, R_FRAME, R_FADD, R_PLAY, R_SPEED, R_ONION, R_STRIP, R_DIV, R_FOLD, R_PITEM, R_PTAB, R_PVIEW,
       R_PRESET, R_PRESETALL, R_PMAKEDEF, R_PFIND, R_PUSE, R_POPEN, R_PBACK };
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
/* CLAWD_EDIT_GIF=FILE: the editor's pages are recorded as a GIF (a frame about every 0.12 s, each pixel CLAWD_EDIT_GIF_SCALE times bigger, 2 by default) and written when the editor ends; for the README */
static Rgb *g_rec[160];
static int g_nrec, g_rec_w, g_rec_h, g_rec_scale;
static double g_rec_last;
static void rec_frame(const Canvas *cv) {
    const char *path = getenv("CLAWD_EDIT_GIF");
    double now = wall_now();
    if (!path || g_nrec >= 160 || now - g_rec_last < 0.12) return;
    g_rec_last = now;
    int z = getenv("CLAWD_EDIT_GIF_SCALE") ? atoi(getenv("CLAWD_EDIT_GIF_SCALE")) : 2;
    if (z < 1) z = 1;
    if (g_nrec && (g_rec_w != cv->w * z || g_rec_h != cv->h * z)) return; /* (the window changed size: the frames of one size only) */
    g_rec_w = cv->w * z; g_rec_h = cv->h * z; g_rec_scale = z;
    Rgb *fr = malloc((size_t)g_rec_w * (size_t)g_rec_h * sizeof(Rgb));
    if (!fr) return;
    for (int y = 0; y < g_rec_h; y++) for (int x = 0; x < g_rec_w; x++) fr[y * g_rec_w + x] = cv->p[(y / z) * cv->w + x / z];
    g_rec[g_nrec++] = fr;
}
static void rec_finish(void) {
    const char *path = getenv("CLAWD_EDIT_GIF");
    if (path && g_nrec && gif_write_fit(path, g_rec, g_nrec, g_rec_w, g_rec_h, 12)) fprintf(stderr, "clawd-room: could not write %s (too many colours?)\n", path);
    for (int i = 0; i < g_nrec; i++) free(g_rec[i]);
    g_nrec = 0;
}

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
/* the fold button, top right: a small window in dim lines, its right part (the panel) shaded while it is open; brighter under the
   pointer (the stage page has one too) */
static void fold_button(Canvas *cv, int folded, int hover) {
    int fx = cv->w - 4 - 9, fy = 3;
    region(R_FOLD, 0, fx - 1, fy - 1, 11, 8, RGB(0, 0, 0));
    if (ui_icon(cv, folded ? "fold-closed" : "fold-open", fx, fy, hover ? 0.4 : 0)) return;
    Rgb k = hover ? RGB(150, 150, 175) : RGB(76, 76, 96), fill = hover ? RGB(90, 90, 112) : RGB(46, 46, 60);
    canvas_rect(cv, fx + 1, fy, 7, 1, k); canvas_rect(cv, fx + 1, fy + 4, 7, 1, k);       /* (its outline without the corner */
    canvas_rect(cv, fx, fy + 1, 1, 3, k); canvas_rect(cv, fx + 8, fy + 1, 1, 3, k);       /*  pixels: softly rounded) */
    if (!folded) canvas_rect(cv, fx + 6, fy + 1, 2, 3, fill);
    canvas_rect(cv, fx + 5, fy + 1, 1, 3, k);
}
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
