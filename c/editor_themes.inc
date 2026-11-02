/* Part of editor.c, included there (it shares editor.c's statics): the THEMES tab and its actions. */
/* ---- the THEMES tab: a card per gate theme, with its three gates side by side, and a card to make one ------------------- */
static char g_src_theme[24] = "simple"; /* the theme last picked: what NEW THEME copies */
static const char *theme_target(const Ed *e) { /* the theme USE / OPEN act on: the picked card, else the open theme */
    if (e->ptab == THEMES && e->psel >= 0 && e->psel < g_npi && g_pi[e->psel].file == THEMES && g_pi[e->psel].sp >= 0) return g_theme[g_pi[e->psel].sp];
    return g_theme_open[0] ? g_theme_open : NULL;
}
static void ring(Canvas *cv, int x, int y, Rgb c) { /* a 5 x 5 ring: not on */
    canvas_rect(cv, x + 1, y, 3, 1, c); canvas_rect(cv, x + 1, y + 4, 3, 1, c); canvas_rect(cv, x, y + 1, 1, 3, c); canvas_rect(cv, x + 4, y + 1, 1, 3, c);
}
static void theme_button(Canvas *cv, int *x, int y, const char *t, int live, int kind) { /* a quiet outlined button; dim when it would do nothing */
    int w = text_width(t) + 8;
    frame(cv, *x, y, w - 1, 9, live ? RGB(90, 90, 110) : RGB(44, 44, 56));
    canvas_text(cv, *x + 4, y + 2, t, live ? RGB(190, 190, 205) : RGB(70, 70, 86));
    if (live) region(kind, 0, *x - 1, y - 1, w + 1, 11, RGB(0, 0, 0));
    *x += w + 5;
}
static void draw_theme_cards(const Ed *e, Canvas *cv, int y0, const int *sh, int n, double now) {
    static const char *const KIND[3] = {"door", "stairs", "well"};
    int W = cv->w, gap = 4, cols = (W - 8 + gap) / (100 + gap);
    if (cols < 1) cols = 1;
    int bw = (W - 8 + gap) / cols - gap, bh = 62, k = (int)fmod(now / 0.3, 1e6), picked = -1;
    g_pick_cols = cols;
    int H = cv->h, by = H - 24 - 13, at = 0;
    if (by - 3 - y0 < bh) bh = by - 3 - y0 < 28 ? 28 : by - 3 - y0; /* (a short window: the cards get lower, the pictures and the lines with them) */
    int th = bh >= 62 ? 26 : bh - 18 < 10 ? 10 : bh - 18, vis = (by - 3 - y0 + gap) / (bh + gap); /* (the buttons just above the bar at the bottom; the rows that fit scroll) */
    if (vis < 1) vis = 1;
    for (int j = 0; j < n; j++) if (sh[j] == e->psel) at = j;
    int first = e->pscroll;
    if (at / cols < first) first = at / cols;
    if (at / cols >= first + vis) first = at / cols - vis + 1;
    if (first < 0) first = 0;
    ((Ed *)e)->pscroll = first;
    for (int j = 0; j < n; j++) {
        const PickItem *it = &g_pi[sh[j]];
        int on = sh[j] == e->psel;
        if (on && it->sp >= 0) { picked = it->sp; snprintf(g_src_theme, sizeof g_src_theme, "%s", g_theme[it->sp]); }
        if (j / cols < first || j / cols >= first + vis) continue;
        int cx = 4 + (j % cols) * (bw + gap), cy = y0 + (j / cols - first) * (bh + gap);
        if (it->sp < 0) { /* the card to make one: dashed, a plus */
            for (int i = 0; i < bw; i += 3) { canvas_px(cv, cx + i, cy, RGB(90, 90, 110)); canvas_px(cv, cx + i, cy + bh - 1, RGB(90, 90, 110)); }
            for (int i = 0; i < bh; i += 3) { canvas_px(cv, cx, cy + i, RGB(90, 90, 110)); canvas_px(cv, cx + bw - 1, cy + i, RGB(90, 90, 110)); }
            canvas_rect(cv, cx + bw / 2 - 5, cy + bh / 2 - 9, 11, 3, C_YELLOW); canvas_rect(cv, cx + bw / 2 - 1, cy + bh / 2 - 13, 3, 11, C_YELLOW);
            canvas_text(cv, cx + bw / 2 - text_width("NEW THEME") / 2, cy + bh - 17, "NEW THEME", C_WHITE);
            char sub[48]; snprintf(sub, sizeof sub, "A COPY OF %s", g_src_theme);
            for (char *q = sub; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            while (sub[0] && text_width(sub) > bw - 4) sub[strlen(sub) - 1] = 0;
            canvas_text(cv, cx + bw / 2 - text_width(sub) / 2, cy + bh - 10, sub, RGB(90, 90, 110));
            if (on) frame(cv, cx, cy, bw, bh, C_YELLOW);
            region(R_PITEM, sh[j], cx, cy, bw, bh, RGB(0, 0, 0));
            continue;
        }
        canvas_rect(cv, cx, cy, bw, bh, on ? RGB(44, 44, 60) : RGB(28, 28, 36));
        int tw = (bw - 6 - 2 * 2) / 3, stairs = theme_kind_index(g_theme[it->sp], "stairs"); /* (the three pictures: equal gaps, the row in the middle of the card) */
        int tx0 = cx + 3 + (bw - 6 - (3 * tw + 2 * 2)) / 2;
        for (int q = 0; q < 3; q++) {
            int ai = theme_kind_index(g_theme[it->sp], KIND[q]), tx = tx0 + q * (tw + 2);
            checker(cv, tx, cy + 3, tw, th);
            if (ai >= 0) picker_anim(cv, ai, on ? k * 0.3 : 0.6, tx, cy + 3, tw, th); /* (the picked one plays, the others rest part way) */
        }
        int l1 = cy + 3 + th + 6; /* (the lines under the pictures, each only when it fits in the card) */
        canvas_text(cv, cx + 4, l1, it->name, on ? C_WHITE : RGB(190, 190, 205));
        const PartAnim *sa = stairs >= 0 ? parts_get(g_anim[stairs]) : NULL;
        if (l1 + 17 <= cy + bh) {
            if (!strcmp(g_theme[it->sp], parts_gate_theme())) { canvas_rect(cv, cx + 4, l1 + 10, 5, 5, C_YELLOW); canvas_text(cv, cx + 12, l1 + 10, "ACTIVE", C_YELLOW); }
            else { char st[32] = "PARTS IN TURN"; if (sa && sa->style[0]) { snprintf(st, sizeof st, "STYLE %s", sa->style); for (char *q = st; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32; } canvas_text(cv, cx + 4, l1 + 10, st, RGB(90, 90, 110)); }
        } else if (!strcmp(g_theme[it->sp], parts_gate_theme())) { canvas_rect(cv, cx + bw - 9, l1, 5, 5, C_YELLOW); } /* (no room for the word: just the mark) */
        if (l1 + 25 <= cy + bh) canvas_text(cv, cx + 4, l1 + 18, "DOOR  STAIRS  WELL", RGB(90, 90, 110));
        if (it->mine && l1 + 17 <= cy + bh) { canvas_rect(cv, cx + bw - 9, l1 + 1, 5, 5, RGB(18, 18, 24)); canvas_rect(cv, cx + bw - 8, l1 + 2, 3, 3, RGB(120, 200, 255)); } /* (yours) */
        if (on) frame(cv, cx, cy, bw, bh, C_YELLOW);
        region(R_PITEM, sh[j], cx, cy, bw, bh, RGB(0, 0, 0));
    }
    int rows = (n + cols - 1) / cols, live = picked >= 0;
    int is_on = live && !strcmp(g_theme[picked], parts_gate_theme()), is_simple = live && !strcmp(g_theme[picked], "simple");
    const char *bl = is_on ? "MAKE INACTIVE" : "MAKE ACTIVE"; /* (the active one: back to simple); at the right, opening is Enter or a click on the picked card */
    int bx = W - 4 - (text_width(bl) + 8);
    theme_button(cv, &bx, by + 1, bl, live && !(is_on && is_simple), R_PUSE);
    int ty = y0 + (rows < vis ? rows : vis) * (bh + gap) + 2; /* (what a theme is, when there is room for it between the cards and the buttons) */
    if (ty + 8 <= by - 2) canvas_text(cv, 4, ty, "A THEME IS A FOLDER OF GATE FILES: HOW A GATE APPEARS.", RGB(110, 110, 130));
    if (ty + 17 <= by - 2) canvas_text(cv, 4, ty + 9, "YOURS GO TO ~/.CLAWD-ROOM.", RGB(110, 110, 130));
    picker_scrollbar(cv, y0, by - 3 - y0, first, vis, rows);
}
static void draw_picker(const Ed *e, Canvas *cv, double now) {
    int W = cv->w, H = cv->h, sh[512], n = picker_shown(e, sh), k = (int)fmod(now / 0.3, 1e6); /* (k: the picked one plays; fmod: the wall clock / 0.3 is past what an int holds) */
    canvas_text(cv, 4, 3, "OPEN A SPRITE", C_YELLOW);
    picker_view_button(cv, W - 4 - 24, 3, 0, e->pview == 0);
    picker_view_button(cv, W - 4 - 9, 3, 1, e->pview == 1);
    static const char *TABS[NFILES + 2] = {"PROPS", "ITEMS", "CLAWD", "UI", "ANIMS", "THEMES"};
    if (e->pview == 0) { /* cards: a tab per file, the search box, the cards */
        int tx = 4, ty = 14;
        for (int f = 0; f <= THEMES; f++) {
            int tw = text_width(TABS[f]) + 8;
            canvas_rect(cv, tx, ty, tw, 10, f == e->ptab ? RGB(52, 54, 70) : RGB(28, 28, 36));
            if (f == e->ptab) canvas_rect(cv, tx + 2, ty + 9, tw - 4, 1, C_YELLOW);
            canvas_text(cv, tx + 4, ty + 3, TABS[f], f == e->ptab ? C_WHITE : C_GREY);
            region(R_PTAB, f, tx, ty, tw, 10, RGB(0, 0, 0));
            tx += tw + 2;
        }
        int sw = W - 4 - tx - 6 < 90 ? W - 4 - tx - 6 : 90;
        if (sw > 40) picker_search(cv, W - 4 - sw, ty, sw, e->find, e->pfocus);
        int gap = 4, cols = (W - 8 + gap) / (46 + gap) > 0 ? (W - 8 + gap) / (46 + gap) : 1, cw = (W - 8 + gap) / cols - gap, chh = 42, y0 = ty + 16;
        if (e->ptab == THEMES) draw_theme_cards(e, cv, y0, sh, n, now);
        else {
        if (e->ptab == ANIMS && g_theme_open[0]) { /* a theme's files: THEMES > NAME (THEMES is the way back); at the right ACTIVE, or INACTIVE (a click makes it active) */
            char nm[24], on = !strcmp(g_theme_open, parts_gate_theme());
            snprintf(nm, sizeof nm, "%s", g_theme_open);
            for (char *q = nm; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            int tw = text_width("THEMES"), x = 4 + tw + 5;
            canvas_text(cv, 4, y0 + 2, "THEMES", RGB(190, 190, 205)); canvas_rect(cv, 4, y0 + 9, tw, 1, RGB(90, 90, 110));
            region(R_PBACK, 0, 4, y0, tw, 11, RGB(0, 0, 0));
            canvas_text(cv, x, y0 + 2, ">", RGB(90, 90, 110)); x += text_width(">") + 5;
            canvas_text(cv, x, y0 + 2, nm, C_WHITE);
            if (on) { /* (a click on ACTIVE: back to simple, unless this is simple) */
                const char *t = "ACTIVE"; int w = text_width(t) + 16, bx = W - 4 - w, can = strcmp(g_theme_open, "simple") != 0;
                if (can) frame(cv, bx, y0, w - 1, 9, RGB(90, 90, 110));
                canvas_rect(cv, bx + 4, y0 + 2, 5, 5, C_YELLOW); canvas_text(cv, bx + 12, y0 + 2, t, C_YELLOW);
                if (can) region(R_PUSE, 0, bx - 1, y0 - 1, w + 1, 11, RGB(0, 0, 0));
            }
            else {
                const char *t = "INACTIVE"; int w = text_width(t) + 16, bx = W - 4 - w;
                frame(cv, bx, y0, w - 1, 9, RGB(90, 90, 110)); ring(cv, bx + 4, y0 + 2, RGB(190, 190, 205)); canvas_text(cv, bx + 12, y0 + 2, t, RGB(190, 190, 205));
                region(R_PUSE, 0, bx - 1, y0 - 1, w + 1, 11, RGB(0, 0, 0));
            }
            canvas_rect(cv, 4, y0 + 12, W - 8, 1, RGB(40, 40, 52));
            y0 += 14;
        }
        int ph = 26; /* the picture's height */
        if (H - 24 - y0 < chh) { chh = H - 24 - y0 < 26 ? 26 : H - 24 - y0; ph = chh - 16 < 8 ? 8 : chh - 16; } /* (a short window: the cards get lower, the picture with them) */
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
            checker(cv, cx + 3, cy + 3, cw - 6, ph);
            picker_sprite(cv, it, on ? k : 0, cx + 3, cy + 3, cw - 6, ph, 3);
            const char *cn = it->name; /* (an open theme's file: DOOR, not THEME DOOR) */
            if (it->file == ANIMS && g_theme_open[0] && strlen(cn) > strlen(g_theme_open) + 1) cn += strlen(g_theme_open) + 1;
            char nm[24]; snprintf(nm, sizeof nm, "%.*s", (cw - 4) / 4, cn);
            canvas_text(cv, cx + 3, cy + 3 + ph + 3, nm, on ? C_WHITE : RGB(190, 190, 205));
            if (it->n > 1) { char fn[8]; snprintf(fn, sizeof fn, "%d", it->n); canvas_rect(cv, cx + cw - text_width(fn) - 6, cy + 4, text_width(fn) + 4, 7, RGB(18, 18, 24)); canvas_text(cv, cx + cw - text_width(fn) - 4, cy + 5, fn, C_YELLOW); }
            if (it->mine) { canvas_rect(cv, cx + 4, cy + 4, 4, 4, RGB(18, 18, 24)); canvas_rect(cv, cx + 5, cy + 5, 2, 2, RGB(120, 200, 255)); } /* (yours) */
            if (on) frame(cv, cx, cy, cw, chh, C_YELLOW);
            region(R_PITEM, sh[j], cx, cy, cw, chh, RGB(0, 0, 0));
        }
        picker_scrollbar(cv, y0, vis * (chh + gap) - gap, first, vis, (n + cols - 1) / cols);
        }
    } else { /* a list by file, the picked one big on the right with its file, size and frames */
        int lw = W / 3 < 92 ? 92 : W / 3, y = 26;
        picker_search(cv, 4, 14, lw - 4, e->find, e->pfocus);
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
            static char g[MAXH][MAXW + 1]; int w = 0, h = 0;
            if (it->file != ANIMS) grid_of(&g_pf[it->file], it->sp, g, &w, &h);
            char info[96];
            if (it->file == ANIMS) { const PartAnim *a = parts_get(g_anim[it->sp]); snprintf(info, sizeof info, "%s   ANIMS/%s.TXT   %d PARTS   %d FRAMES", it->name, it->name, a ? a->npart : 0, it->n); }
            else snprintf(info, sizeof info, "%s   %s   %dX%d   %d FRAME%s", it->name, FILES[it->file], w, h, it->n, it->n == 1 ? "" : "S");
            for (char *q = info; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            canvas_text(cv, px, 14 + ph + 4, info, C_YELLOW);
            for (int f = 0, fx = px; f < it->n && fx + 32 <= W - 4; f += it->file == ANIMS ? 4 : 1, fx += 35) { checker(cv, fx, 14 + ph + 13, 32, 22); picker_sprite(cv, it, f, fx, 14 + ph + 13, 32, 22, 2); }
        }
    }
    if (!n) { char t[64]; snprintf(t, sizeof t, "NOTHING CALLED %s", e->find); canvas_text(cv, (W - text_width(t)) / 2, H / 2, t, C_GREY); }
    { /* the bar above the hints: how many are yours (left); RESET, RESET ALL, MAKE DEFAULT (right; dim when they would do nothing) */
        int by = H - 21, yours = 0, file_mine = 0, picked_mine = e->psel >= 0 && e->psel < g_npi && g_pi[e->psel].mine;
        for (int i = 0; i < g_npi; i++) { if (g_pi[i].file != THEMES) yours += g_pi[i].mine; if (e->psel >= 0 && e->psel < g_npi && g_pi[i].file == g_pi[e->psel].file && g_pi[i].mine) file_mine = 1; }
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
    canvas_text(cv, 4, H - 7, e->pview == 0 ? e->ptab == THEMES ? "ARROWS PICK  SPACE ACTIVE / INACTIVE  ENTER OPEN  TAB NEXT TAB  Q QUIT" : g_back_stage[0] ? "ARROWS PICK  ENTER OPEN  Q BACK TO THE STAGE" : g_theme_open[0] && e->ptab == ANIMS ? "ARROWS PICK  ENTER OPEN  ESC BACK TO THEMES  Q QUIT" : "ARROWS PICK  ENTER OPEN  TAB NEXT TAB  \\ LIST  Q QUIT" : "UP DOWN PICK  ENTER OPEN  \\ CARDS  Q QUIT", RGB(150, 150, 170));
}
/* the picker opens: the items again, the open sprite's animation picked */
static void picker_open(Ed *e, const char *dir, int from_edit) {
    picker_gather(dir);
    e->page = 1; e->came_from_edit = from_edit; e->find[0] = 0; e->pfocus = 1; e->pscroll = 0; e->ptab = e->file; e->psel = -1;
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
    if (e->pview == 0 && !picker_shown(e, sh)) for (int f = 0; f <= THEMES; f++) { e->ptab = f; if (picker_shown(e, sh)) break; }
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
    if (it->file == THEMES) { /* a theme: its files of yours go (a theme only yours goes altogether) */
        for (int i = 0; i < g_nanim && it->sp >= 0; i++) { char th[24]; if (anim_theme_of(g_anim[i], th, sizeof th) && !strcmp(th, g_theme[it->sp])) { char mp[700]; anim_mine_path(mp, sizeof mp, i); remove(mp); } }
        picker_gather(dir); snprintf(e->msg, sizeof e->msg, "RESET"); return;
    }
    if (it->file == ANIMS) { char mp[700]; anim_mine_path(mp, sizeof mp, it->sp); remove(mp); picker_gather(dir); snprintf(e->msg, sizeof e->msg, "RESET"); return; } /* (a part animation: yours goes) */
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
    for (int i = 0; i < g_nanim; i++) { char mp[700]; anim_mine_path(mp, sizeof mp, i); remove(mp); } /* (and your part animations) */
    picker_gather(dir);
    (void)e;
}
/* MAKE DEFAULT: yours in the picked one's file become the default (written into the repo's file; your file goes) */
static void picker_make_default(Ed *e, const char *dir) {
    if (e->psel < 0 || e->psel >= g_npi) return;
    int fi = g_pi[e->psel].file;
    if (fi == THEMES) { /* a theme: each file of yours is copied over the default (the folder made), then goes */
        for (int i = 0; i < g_nanim && g_pi[e->psel].sp >= 0; i++) {
            char th[24], mp[700], dp[700], dd[700];
            if (!anim_theme_of(g_anim[i], th, sizeof th) || strcmp(th, g_theme[g_pi[e->psel].sp])) continue;
            anim_mine_path(mp, sizeof mp, i);
            char *t = slurp(mp);
            if (!t) continue;
            snprintf(dp, sizeof dp, "%s/%s.txt", parts_default_dir(), g_anim[i]);
            snprintf(dd, sizeof dd, "%s", dp); char *sl = strrchr(dd, '/'); if (sl) { *sl = 0; char *p2 = strrchr(dd, '/'); if (p2) { *p2 = 0; mkdir(dd, 0755); *p2 = '/'; } mkdir(dd, 0755); }
            FILE *o = fopen(dp, "w");
            if (o) { fputs(t, o); fclose(o); remove(mp); }
            free(t);
        }
        picker_gather(dir);
        return;
    }
    if (fi == ANIMS) { /* a part animation: yours is copied over the default, then goes */
        char mp[700], dp[700];
        anim_mine_path(mp, sizeof mp, g_pi[e->psel].sp);
        snprintf(dp, sizeof dp, "%s/%s.txt", parts_default_dir(), g_anim[g_pi[e->psel].sp]);
        char *t = slurp(mp);
        if (!t) return;
        FILE *o = fopen(dp, "w");
        if (o) { fputs(t, o); fclose(o); remove(mp); }
        free(t);
        picker_gather(dir);
        return;
    }
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
/* ---- the THEMES tab's actions ------------------------------------------------------------------------------------------ */
static void theme_use(Ed *e, const char *dir) { /* USE: gate_theme = NAME in settings.conf (the room reads it again by itself) */
    const char *t = theme_target(e);
    if (!t) return;
    char name[24];
    snprintf(name, sizeof name, "%s", t);
    if (!strcmp(name, parts_gate_theme())) { /* the active one again: back to simple (simple itself is the one to go back to: nothing) */
        if (!strcmp(name, "simple")) return;
        snprintf(name, sizeof name, "simple");
    }
    if (settings_set("gate_theme", name)) { snprintf(e->msg, sizeof e->msg, "CANNOT WRITE SETTINGS.CONF"); return; }
    const char *env = getenv("CLAWD_GATE_THEME");
    snprintf(e->msg, sizeof e->msg, env && env[0] ? "SAVED, BUT CLAWD_GATE_THEME WINS" : "%s IS ACTIVE", name);
    for (char *q = e->msg; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
    picker_gather(dir);
}
static void theme_open(Ed *e) { /* OPEN: the ANIMS tab with that theme's files */
    const char *t = theme_target(e);
    if (!t) return;
    snprintf(g_theme_open, sizeof g_theme_open, "%s", t);
    e->ptab = ANIMS; e->pscroll = 0; e->find[0] = 0;
    int sh[512]; if (picker_shown(e, sh)) e->psel = sh[0];
}
static void theme_close(Ed *e) { /* back to the theme cards, the one that was open picked */
    char was[24]; snprintf(was, sizeof was, "%s", g_theme_open);
    g_theme_open[0] = 0; e->ptab = THEMES; e->pscroll = 0;
    for (int i = 0; i < g_npi; i++) if (g_pi[i].file == THEMES && g_pi[i].sp >= 0 && !strcmp(g_theme[g_pi[i].sp], was)) e->psel = i;
}
static int picker_escape(Ed *e) { /* Esc in the picker: one step out (the search cleared, the search box let go, an open theme left); it never quits: that is q */
    if (e->find[0]) { e->find[0] = 0; picker_refind(e); return 0; }
    if (e->pfocus == 2) { e->pfocus = 0; return 0; }
    if (g_theme_open[0] && e->ptab == ANIMS) { theme_close(e); return 0; }
    if (e->pfocus) e->pfocus = 0;
    return 0;
}
static void theme_make(Ed *e, const char *dir) { /* the new theme: a copy of the one last picked, as yours (anims/gates/NAME) */
    char nm[24];
    snprintf(nm, sizeof nm, "%s", g_newname);
    size_t l = strlen(nm);
    if (!l) { snprintf(e->msg, sizeof e->msg, "GIVE IT A NAME"); return; }
    for (int i = 0; i < g_nth; i++) if (!strcmp(g_theme[i], nm)) { snprintf(e->msg, sizeof e->msg, "THERE IS A THEME CALLED THAT"); return; }
    if (!parts_mine_dir()[0]) { snprintf(e->msg, sizeof e->msg, "NOWHERE TO SAVE"); return; }
    static PartAnim a;
    static const char *const KIND[3] = {"door", "stairs", "well"};
    for (int q = 0; q < 3; q++) {
        int ai = theme_kind_index(g_src_theme, KIND[q]);
        if (ai < 0) continue;
        char src[700], dst[700];
        snprintf(src, sizeof src, "%s/%s.txt", parts_mine_dir(), g_anim[ai]);
        if (access(src, R_OK)) snprintf(src, sizeof src, "%s/%s.txt", parts_default_dir(), g_anim[ai]);
        if (parts_load(&a, src)) continue;
        snprintf(dst, sizeof dst, "%s/gates/%s/gate-%s.txt", parts_mine_dir(), nm, KIND[q]);
        parts_save(&a, dst);
    }
    picker_gather(dir);
    for (int i = 0; i < g_npi; i++) if (g_pi[i].file == THEMES && g_pi[i].sp >= 0 && !strcmp(g_theme[g_pi[i].sp], nm)) e->psel = i;
    snprintf(e->msg, sizeof e->msg, "MADE THEME %s", nm);
    for (char *q = e->msg; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
}
/* the stage page of an animation, in this window. I there (a sprite part's drawing) comes back as the CLAWD tab, and q in the picker then goes back to the stage page */
static void open_stage(Ed *e, const char *dir, const char *nm) {
    int rc = stage_loop(dir, nm);
    if (rc == 1) snprintf(e->msg, sizeof e->msg, "CANNOT OPEN ANIMS/%s", nm);
    g_redraw = 1;
    g_back_stage[0] = 0;
    picker_gather(dir);
    if (rc == ST_PIXELS) { /* (I on the stage: the CLAWD tab) */
        snprintf(g_back_stage, sizeof g_back_stage, "%s", nm);
        e->ptab = 2; e->pscroll = 0; e->find[0] = 0;
        int sh[512]; if (picker_shown(e, sh)) e->psel = sh[0];
        snprintf(e->msg, sizeof e->msg, "PICK THE PART'S DRAWING. Q: BACK TO THE STAGE");
        return;
    }
    char th[24]; /* (back from the stage: the picker on that animation) */
    if (anim_theme_of(nm, th, sizeof th)) snprintf(g_theme_open, sizeof g_theme_open, "%s", th); else g_theme_open[0] = 0;
    e->ptab = ANIMS; e->pscroll = 0;
    for (int i = 0; i < g_npi; i++) if (g_pi[i].file == ANIMS && !strcmp(g_anim[g_pi[i].sp], nm)) e->psel = i;
}
static int picker_quit(Ed *e, const char *dir) { /* q in the picker: back to the stage page it came from, else quit */
    if (!g_back_stage[0]) return 1;
    char nm[PARTS_PATH_NAME];
    snprintf(nm, sizeof nm, "%s", g_back_stage);
    open_stage(e, dir, nm);
    return 0;
}
/* the picked one opens in the editor */
static void picker_pick(Ed *e, const char *dir) {
    if (e->psel < 0 || e->psel >= g_npi) return;
    if (g_pi[e->psel].file == THEMES) { /* a theme card opens its files; the dashed card asks for a name */
        if (g_pi[e->psel].sp >= 0) theme_open(e);
        else { g_newname[0] = 0; e->asking = 5; }
        return;
    }
    if (g_pi[e->psel].file == ANIMS) { /* the stage page, in this window; q there comes back here */
        char nm[PARTS_PATH_NAME];
        snprintf(nm, sizeof nm, "%s", g_anim[g_pi[e->psel].sp]);
        open_stage(e, dir, nm);
        return;
    }
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
    if (e->asking == 5) { snprintf(a, sizeof a, "NAME THE NEW THEME:  %s_", g_newname); for (char *q = a; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32; b = "ENTER MAKE IT    ESC BACK"; }
    else if (e->asking == 3) { int n = 0; for (int i = 0; i < g_npi; i++) n += g_pi[i].mine; snprintf(a, sizeof a, "RESET %d OF YOURS TO THE DEFAULT?", n); b = "Y RESET    N KEEP THEM"; }
    else if (e->asking == 4) {
        if (e->psel >= 0 && e->psel < g_npi && g_pi[e->psel].file == ANIMS)
            snprintf(a, sizeof a, "MAKE YOUR ANIMS/%s.TXT THE DEFAULT?", g_pi[e->psel].name);
        else
            snprintf(a, sizeof a, "MAKE YOURS IN %s THE DEFAULT?", e->psel >= 0 && e->psel < g_npi ? FILES[g_pi[e->psel].file] : "");
        for (char *q = a; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        b = "Y MAKE IT THE DEFAULT    N NOT NOW";
    }
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
    fold_button(cv, g_panel_folded, g_fold_hover);
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
