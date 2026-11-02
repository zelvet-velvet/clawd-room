/* Part of editor.c, included there (it shares editor.c's statics): your own sprites over the defaults. */
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
