/* The scene: items on layers, drawn back to front; optionally measuring what each item covers and who paints over whom. */
#include <stdlib.h>
#include <string.h>
#include "scene.h"

static const char *LAYER_NAMES[N_LAYERS] = {"sky", "wall", "struct", "floor", "depth", "particle", "hud"};
const char *scene_layer_name(int layer) { return layer >= 0 && layer < N_LAYERS ? LAYER_NAMES[layer] : "?"; }

void scene_clear(Scene *s) { s->n = 0; s->nhit = 0; }

int scene_add(Scene *s, SceneItem it) {
    if (s->n >= SCENE_MAX) return -1;
    it.seq = s->n;
    it.has_box = 0;
    s->it[s->n] = it;
    return s->n++;
}

/* the sort key: an item with an owner sorts with its owner, right after it */
static void key_of(const Scene *s, int i, int *layer, double *depth, int *group, int *sub) {
    const SceneItem *it = &s->it[i];
    if (it->owner >= 0 && it->owner < s->n) {
        const SceneItem *o = &s->it[it->owner];
        *layer = o->layer; *depth = o->layer == LAYER_DEPTH ? o->depth : 0; *group = o->seq; *sub = 1;
    } else { *layer = it->layer; *depth = it->layer == LAYER_DEPTH ? it->depth : 0; *group = it->seq; *sub = 0; }
}

static int before(const Scene *s, int a, int b) { /* a strictly before b */
    int la, ga, sa, lb, gb, sb;
    double da, db;
    key_of(s, a, &la, &da, &ga, &sa);
    key_of(s, b, &lb, &db, &gb, &sb);
    if (la != lb) return la < lb;
    if (da != db) return da < db;
    if (ga != gb) return ga < gb;
    if (sa != sb) return sa < sb;
    return s->it[a].seq < s->it[b].seq;
}

static void add_hit(Scene *s, int a, int b) {
    for (int i = 0; i < s->nhit; i++) if (s->hit[i].a == a && s->hit[i].b == b) { s->hit[i].n++; return; }
    if (s->nhit < SCENE_MAX_HITS) s->hit[s->nhit++] = (SceneHit){a, b, 1};
}

void scene_draw(Scene *s, Canvas *cv, void *ctx) {
    for (int i = 0; i < s->n; i++) s->order[i] = i;
    for (int i = 1; i < s->n; i++) { /* stable insertion sort (the lists are short) */
        int o = s->order[i], j = i;
        while (j > 0 && before(s, o, s->order[j - 1])) { s->order[j] = s->order[j - 1]; j--; }
        s->order[j] = o;
    }
    s->nhit = 0;
    size_t np = (size_t)cv->w * (size_t)cv->h;
    Rgb *prev = NULL;
    short *who = NULL; /* the item that last painted each pixel (-1: none) */
    if (s->measure && np) {
        prev = malloc(np * sizeof *prev);
        who = malloc(np * sizeof *who);
        if (!prev || !who) { free(prev); free(who); prev = NULL; who = NULL; }
        else for (size_t p = 0; p < np; p++) who[p] = -1;
    }
    for (int k = 0; k < s->n; k++) {
        SceneItem *it = &s->it[s->order[k]];
        if (!it->draw) continue;
        if (!prev) { it->draw(cv, it, ctx); continue; }
        memcpy(prev, cv->p, np * sizeof *prev);
        it->draw(cv, it, ctx);
        int x0 = cv->w, y0 = cv->h, x1 = -1, y1 = -1;
        for (int y = 0; y < cv->h; y++) for (int x = 0; x < cv->w; x++) {
            size_t p = (size_t)y * (size_t)cv->w + (size_t)x;
            if (rgb_eq(prev[p], cv->p[p])) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
            if (who[p] >= 0 && who[p] != (short)it->seq) add_hit(s, who[p], it->seq);
            who[p] = (short)it->seq;
        }
        if (x1 >= 0) { it->has_box = 1; it->bx0 = x0; it->by0 = y0; it->bx1 = x1 + 1; it->by1 = y1 + 1; }
    }
    free(prev);
    free(who);
}
