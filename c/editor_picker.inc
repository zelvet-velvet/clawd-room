/* Part of editor.c, included there (it shares editor.c's statics): the sprite picker. */
/* ---- the sprite picker: every sprite and animation as a card with its picture (or as a list with the picked one big), by
 * file; type to find; Enter or a click opens it in the editor; l there brings the picker back --------------------------- */
typedef struct { int file, sp, n, mine; char name[64]; } PickItem; /* (mine: one of its frames is yours); file NFILES: a part animation, sp its index in g_anim */
static File g_pf[NFILES];
#define ANIMS NFILES /* the picker's last tab: the part animations (anims/NAME.txt), opened on the stage page (stage.inc) */
#define THEMES (NFILES + 1) /* the picker's tab of gate themes: a card per folder anims/gates/NAME, opened on the ANIMS tab */
static char g_anim[32][PARTS_PATH_NAME];
static char g_theme[16][24];      /* the gate themes there are (the folders of anims/gates/ in both places), "simple" first */
static int g_nth;
static char g_theme_open[24];     /* the theme the ANIMS tab shows the files of ("": the animations that are in no theme) */
static char g_newname[24];        /* typing the name of a new theme */
static char g_back_stage[PARTS_PATH_NAME]; /* the animation whose stage page asked for its drawing (I): q in the picker goes back to it */
static int anim_theme_of(const char *path, char *out, size_t n) { /* gates/THEME/KIND: 1 and the theme */
    if (strncmp(path, "gates/", 6)) return 0;
    const char *e = strchr(path + 6, '/');
    if (!e) return 0;
    snprintf(out, n, "%.*s", (int)(e - (path + 6)), path + 6);
    return 1;
}
static int g_nanim;
static int theme_kind_index(const char *theme, const char *kind) { /* the picker's anim index of gates/THEME/gate-KIND, else simple's, else -1 */
    char nm[PARTS_PATH_NAME];
    for (int pass = 0; pass < 2; pass++) {
        snprintf(nm, sizeof nm, "gates/%s/gate-%s", pass ? "simple" : theme, kind);
        for (int i = 0; i < g_nanim; i++) if (!strcmp(g_anim[i], nm)) return i;
    }
    return -1;
}
static int g_nanim, g_redraw; /* (g_redraw: the stage page had the terminal: draw everything again) */
static void stage_dirs(const char *dir);
#define ST_PIXELS 2 /* stage_loop returns it when I asked for the pixel art of a sprite part */
int editor_run(const char *dir, const char *name);
static int g_start_tab = -1; /* the picker opens on this tab (the stage page's I key: CLAWD) */
static int stage_loop(const char *dir, const char *name);
static void picker_anim(Canvas *cv, int ai, double t, int x, int y, int w, int h);
static void anim_mine_path(char *out, size_t n, int ai) { snprintf(out, n, "%s/%s.txt", parts_mine_dir(), g_anim[ai]); }
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
    stage_dirs(dir);
    g_nanim = parts_names(g_anim, 32);
    for (int i = 0; i < g_nanim && g_npi < 512; i++) {
        const PartAnim *a = parts_get(g_anim[i]);
        char mp[700];
        anim_mine_path(mp, sizeof mp, i);
        int nf = a ? (int)lround(a->length * 16) + (a->loop ? 0 : 1) : 1;
        g_pi[g_npi] = (PickItem){ANIMS, i, nf, parts_mine_dir()[0] && access(mp, R_OK) == 0, ""};
        snprintf(g_pi[g_npi].name, sizeof g_pi[g_npi].name, "%s", g_anim[i]);
        char th[24];
        if (anim_theme_of(g_anim[i], th, sizeof th)) { /* a gate file: THEME KIND (the ANIMS tab shows it as KIND when its theme is open) */
            const char *kind = strrchr(g_anim[i], '/') + 1;
            snprintf(g_pi[g_npi].name, sizeof g_pi[g_npi].name, "%s %s", th, !strncmp(kind, "gate-", 5) ? kind + 5 : kind);
        }
        for (char *q = g_pi[g_npi].name; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        g_npi++;
    }
    g_nth = 0; /* the themes: simple first, then by name; a card each, and a card to make one */
    for (int i = 0; i < g_nanim; i++) {
        char th[24];
        if (!anim_theme_of(g_anim[i], th, sizeof th)) continue;
        int have = 0;
        for (int k = 0; k < g_nth; k++) if (!strcmp(g_theme[k], th)) have = 1;
        if (!have && g_nth < 16) snprintf(g_theme[g_nth++], sizeof g_theme[0], "%s", th);
    }
    for (int a = 0; a < g_nth; a++) for (int b = a + 1; b < g_nth; b++) {
        int sa = !strcmp(g_theme[a], "simple"), sb = !strcmp(g_theme[b], "simple");
        if (sb || (!sa && strcmp(g_theme[a], g_theme[b]) > 0)) { char t[24]; snprintf(t, sizeof t, "%s", g_theme[a]); snprintf(g_theme[a], sizeof g_theme[0], "%s", g_theme[b]); snprintf(g_theme[b], sizeof g_theme[0], "%s", t); }
    }
    for (int k = 0; k <= g_nth && g_npi < 512; k++) {
        int mine = 0;
        for (int i = 0; k < g_nth && i < g_npi; i++) if (g_pi[i].file == ANIMS) { char th[24]; if (anim_theme_of(g_anim[g_pi[i].sp], th, sizeof th) && !strcmp(th, g_theme[k]) && g_pi[i].mine) mine = 1; }
        g_pi[g_npi] = (PickItem){THEMES, k < g_nth ? k : -1, 3, mine, ""};
        snprintf(g_pi[g_npi].name, sizeof g_pi[g_npi].name, "%s", k < g_nth ? g_theme[k] : "NEW THEME");
        for (char *q = g_pi[g_npi].name; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        g_npi++;
    }
}
/* the items the picker shows, in order: matching what is typed; the cards only the file of their tab */
static int picker_shown(const Ed *e, int *out) {
    int n = 0;
    for (int i = 0; i < g_npi; i++) {
        if (e->pview == 0 && g_pi[i].file != e->ptab) continue;
        if (e->pview != 0 && g_pi[i].file == THEMES) continue; /* (the list has the files, not the theme cards) */
        if (e->pview == 0 && g_pi[i].file == ANIMS) { /* the ANIMS tab: the animations in no theme, or the open theme's files */
            char th[24];
            int gate = anim_theme_of(g_anim[g_pi[i].sp], th, sizeof th);
            if (g_theme_open[0] ? !(gate && !strcmp(th, g_theme_open)) : gate) continue;
        }
        if (e->find[0] && !strstr(g_pi[i].name, e->find)) continue;
        out[n++] = i;
    }
    return n;
}
/* a sprite of the picker's files, fitted into w x h (z at most), centred; frame k of its animation (playing: the picked one) */
static void picker_sprite(Canvas *cv, const PickItem *it, int k, int x, int y, int w, int h, int z) {
    if (it->file == THEMES) return;
    if (it->file == ANIMS) { /* (resting cards of an animation that plays once: part way in, so a gate shows) */
        double t = k * 0.3;
        const PartAnim *pa = k == 0 ? parts_get(g_anim[it->sp]) : NULL;
        if (pa && !pa->loop) t = pa->length * 0.55;
        picker_anim(cv, it->sp, t, x, y, w, h);
        return;
    } /* (a part animation: its room, the parts in the middle) */
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
static void picker_search(Canvas *cv, int x, int y, int w, const char *typed, int focus) { /* (focused: a yellow frame and the cursor) */
    canvas_rect(cv, x, y, w, 9, RGB(30, 30, 40));
    if (focus) frame(cv, x, y, w, 9, C_YELLOW);
    canvas_text(cv, x + 3, y + 2, typed[0] ? typed : focus ? "TYPE TO FIND" : "CLICK TO FIND", typed[0] ? C_WHITE : RGB(90, 90, 110));
    if (typed[0] && focus) canvas_rect(cv, x + 3 + text_width(typed) + 1, y + 2, 3, 5, C_YELLOW);
    region(R_PFIND, 0, x, y, w, 9, RGB(0, 0, 0));
}
/* a scroll bar in the right margin when the rows do not all fit: where the rows shown are among all of them (the wheel and the arrows scroll) */
static void picker_scrollbar(Canvas *cv, int y, int h, int first, int vis, int rows) {
    if (rows <= vis || h < 8) return;
    int x = cv->w - 4, th = h * vis / rows < 8 ? 8 : h * vis / rows, ty = y + (h - th) * first / (rows - vis);
    canvas_rect(cv, x, y, 3, h, RGB(40, 40, 52));
    canvas_rect(cv, x, ty, 3, th, RGB(140, 140, 160));
}
