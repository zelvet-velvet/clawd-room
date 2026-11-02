/* Part of editor.c, included there (it shares editor.c's statics): animations: frame names and numbers. */
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
