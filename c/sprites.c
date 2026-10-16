#include "sprites.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int set; SpritePx v; } Legend;

SpriteFile sprites_load(const char *path) {
    SpriteFile f = {NULL, 0};
    FILE *fp = fopen(path, "r");
    if (!fp) return f;
    Legend leg[256];
    memset(leg, 0, sizeof leg);
    leg['.'].set = 1;
    char line[512];
    int cap = 0, rows_cap = 0;
    Sprite *cur = NULL;
    while (fgets(line, sizeof line, fp)) {
        size_t n = strlen(line);
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
    fclose(fp);
    return f;
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
