#include "sprites.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int set; SpritePx v; } Legend;

/* the sprites in a text (the file's lines); a sprite named again later replaces the earlier one */
static SpriteFile sprites_parse(char *text) {
    SpriteFile f = {NULL, 0};
    Legend leg[256];
    memset(leg, 0, sizeof leg);
    leg['.'].set = 1;
    char line[512];
    int cap = 0, rows_cap = 0;
    Sprite *cur = NULL;
    for (char *p = text; p && *p; ) {
        char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n); line[n] = 0;
        p = nl ? nl + 1 : NULL;
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (line[0] == ':') { /* ": K = 20,20,30" or ": B = body" */
            const char *p = line + 1;
            while (*p == ' ') p++;
            unsigned char ch = (unsigned char)*p++;
            while (*p == ' ' || *p == '=') p++;
            leg[ch].set = 1;
            if (!strncmp(p, "body", 4)) leg[ch].v.kind = PX_BODY;
            else { int r, g, b; sscanf(p, "%d,%d,%d", &r, &g, &b); leg[ch].v.kind = PX_RGB; leg[ch].v.c = RGB(r, g, b); }
        } else if (line[0] == '@') { /* "@ name | key" */
            if (f.n == cap) { cap = cap ? cap * 2 : 64; f.s = realloc(f.s, sizeof(Sprite) * (size_t)cap); }
            cur = &f.s[f.n++];
            memset(cur, 0, sizeof *cur);
            char *bar = strchr(line, '|');
            const char *nm = line + 1;
            while (*nm == ' ') nm++;
            size_t nl = (bar ? (size_t)(bar - nm) : strlen(nm));
            while (nl && nm[nl - 1] == ' ') nl--;
            snprintf(cur->name, sizeof cur->name, "%.*s", (int)nl, nm);
            if (bar) { const char *k = bar + 1; while (*k == ' ') k++; snprintf(cur->key, sizeof cur->key, "%s", k); }
            else snprintf(cur->key, sizeof cur->key, "%s", cur->name);
            rows_cap = 0;
        } else if (line[0] == '#') {
            continue;
        } else if (!line[0]) {
            cur = NULL;
        } else if (cur) {
            int w = (int)strlen(line);
            if (w > cur->w) { /* widen the grid */
                SpritePx *np = calloc((size_t)w * (size_t)(cur->h + 1), sizeof(SpritePx));
                for (int y = 0; y < cur->h; y++) memcpy(np + y * w, cur->px + y * cur->w, sizeof(SpritePx) * (size_t)cur->w);
                free(cur->px); cur->px = np; cur->w = w;
            } else cur->px = realloc(cur->px, sizeof(SpritePx) * (size_t)cur->w * (size_t)(cur->h + 1));
            SpritePx *row = cur->px + cur->h * cur->w;
            memset(row, 0, sizeof(SpritePx) * (size_t)cur->w);
            for (int x = 0; x < w; x++) row[x] = leg[(unsigned char)line[x]].v;
            cur->h++;
            (void)rows_cap;
        }
    }
    for (int i = f.n - 1; i > 0; i--) for (int j = i - 1; j >= 0; j--) if (!strcmp(f.s[j].name, f.s[i].name)) { /* (named twice: the later one) */
        free(f.s[j].px);
        memmove(&f.s[j], &f.s[j + 1], sizeof(Sprite) * (size_t)(f.n - j - 1));
        f.n--; i--;
    }
    return f;
}
static char *slurp_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *b = malloc((size_t)n + 2);
    if (b && fread(b, 1, (size_t)n, fp) != (size_t)n) { free(b); b = NULL; }
    if (b) b[n] = 0;
    fclose(fp);
    return b;
}
SpriteFile sprites_load(const char *path) {
    char *t = slurp_text(path);
    SpriteFile f = {NULL, 0};
    if (t) { f = sprites_parse(t); free(t); }
    return f;
}
/* the defaults with your own over them: the default file's colours, then yours (yours win), the default sprites, then
 * yours (a sprite of yours replaces the default one of that name; a new one is added) */
SpriteFile sprites_load_layered(const char *def_path, const char *mine_path) {
    char *d = slurp_text(def_path), *m = mine_path ? slurp_text(mine_path) : NULL;
    if (!m) { SpriteFile f = {NULL, 0}; if (d) { f = sprites_parse(d); free(d); } return f; }
    size_t nd = d ? strlen(d) : 0, nm = strlen(m);
    char *out = malloc(nd + nm + 8), *o = out;
    for (int pass = 0; pass < 4; pass++) { /* (pass 0 the default colours, 1 yours, 2 the default sprites, 3 yours) */
        const char *src = pass % 2 == 0 ? d : m;
        if (!src) continue;
        for (const char *p = src; *p; ) {
            const char *nl = strchr(p, '\n');
            size_t n = nl ? (size_t)(nl - p) + 1 : strlen(p);
            int pal = p[0] == ':';
            if ((pass < 2) == pal) { memcpy(o, p, n); o += n; if (!nl) *o++ = '\n'; }
            p += n;
        }
        *o++ = '\n';
    }
    *o = 0;
    SpriteFile f = sprites_parse(out);
    free(out); free(d); free(m);
    return f;
}
/* where your own sprites are: ~/.clawd-room/sprites (CLAWD_MY_SPRITES for the tests) */
const char *sprites_mine_dir(void) {
    static char dir[600];
    const char *env = getenv("CLAWD_MY_SPRITES");
    if (env) return env;
    snprintf(dir, sizeof dir, "%s/.clawd-room/sprites", getenv("HOME") ? getenv("HOME") : ".");
    return dir;
}

void sprites_free(SpriteFile *f) {
    for (int i = 0; i < f->n; i++) free(f->s[i].px);
    free(f->s); f->s = NULL; f->n = 0;
}

const Sprite *sprites_by_key(const SpriteFile *f, const char *key) {
    for (int i = 0; i < f->n; i++) if (!strcmp(f->s[i].key, key)) return &f->s[i];
    return NULL;
}

const Sprite *sprites_by_name(const SpriteFile *f, const char *name) {
    for (int i = 0; i < f->n; i++) if (!strcmp(f->s[i].name, name)) return &f->s[i];
    return NULL;
}

void sprite_blit(Canvas *cv, const Sprite *sp, int x, int y, Rgb body) {
    for (int dy = 0; dy < sp->h; dy++)
        for (int dx = 0; dx < sp->w; dx++) {
            SpritePx p = sp->px[dy * sp->w + dx];
            if (p.kind == PX_BODY) canvas_px(cv, x + dx, y + dy, body);
            else if (p.kind == PX_RGB) canvas_px(cv, x + dx, y + dy, p.c);
        }
}
