/* Part animations: the loader and the evaluator (parts.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parts.h"

static int ease_by_name(const char *s) {
    if (!strcmp(s, "linear")) return EASE_LINEAR;
    if (!strcmp(s, "out")) return EASE_OUT;
    if (!strcmp(s, "in-out")) return EASE_IN_OUT;
    if (!strcmp(s, "back")) return EASE_BACK;
    return -1;
}
int parts_find(const PartAnim *a, const char *part) {
    for (int i = 0; i < a->npart; i++) if (!strcmp(a->part[i].name, part)) return i;
    return -1;
}
static Part *add_part(PartAnim *a, const char *name) {
    int i = parts_find(a, name);
    if (i >= 0) return &a->part[i];
    if (a->npart >= PARTS_MAX) return NULL;
    Part *p = &a->part[a->npart++];
    memset(p, 0, sizeof *p);
    snprintf(p->name, sizeof p->name, "%s", name);
    return p;
}
/* key PART TIME [x=] [y=] [turn=] [glow=] [show=] [ease=]: what a key leaves out stays as the part's key before it had it */
static int parse_key(PartAnim *a, char **save, const char *path, int line) {
    char *name = strtok_r(NULL, " \t", save), *tm = strtok_r(NULL, " \t", save);
    Part *p = name ? add_part(a, name) : NULL;
    if (!p || !tm) { fprintf(stderr, "%s:%d: key needs a part and a time\n", path, line); return -1; }
    if (p->nkey >= PARTS_MAX_KEYS) { fprintf(stderr, "%s:%d: too many keys for %s\n", path, line, p->name); return -1; }
    PartKey k = p->nkey ? p->key[p->nkey - 1] : (PartKey){0, {0, 0, 0, 0, 1}, EASE_LINEAR};
    char *end;
    k.t = strtod(tm, &end);
    if (*end) { fprintf(stderr, "%s:%d: %s is not a time\n", path, line, tm); return -1; }
    k.ease = EASE_LINEAR;
    if (p->nkey && k.t <= p->key[p->nkey - 1].t) { fprintf(stderr, "%s:%d: keys of %s must go forward in time\n", path, line, p->name); return -1; }
    for (char *f = strtok_r(NULL, " \t", save); f; f = strtok_r(NULL, " \t", save)) {
        char *eq = strchr(f, '=');
        if (!eq) { fprintf(stderr, "%s:%d: %s is not field=value\n", path, line, f); return -1; }
        *eq = 0;
        const char *v = eq + 1;
        if (!strcmp(f, "x")) k.pose.x = atof(v);
        else if (!strcmp(f, "y")) k.pose.y = atof(v);
        else if (!strcmp(f, "turn")) k.pose.turn = atof(v);
        else if (!strcmp(f, "glow")) k.pose.glow = atof(v);
        else if (!strcmp(f, "show")) k.pose.show = atoi(v) != 0;
        else if (!strcmp(f, "ease")) { k.ease = ease_by_name(v); if (k.ease < 0) { fprintf(stderr, "%s:%d: no ease called %s\n", path, line, v); return -1; } }
        else { fprintf(stderr, "%s:%d: no field called %s\n", path, line, f); return -1; }
    }
    p->key[p->nkey++] = k;
    return 0;
}
/* anim NAME [length SECONDS] [loop | once]; part NAME [group GROUP] */
static int parse_anim(PartAnim *a, char **save, const char *path, int line) {
    char *name = strtok_r(NULL, " \t", save);
    if (!name) { fprintf(stderr, "%s:%d: anim needs a name\n", path, line); return -1; }
    snprintf(a->name, sizeof a->name, "%s", name);
    a->length = 1;
    for (char *w = strtok_r(NULL, " \t", save); w; w = strtok_r(NULL, " \t", save)) {
        if (!strcmp(w, "length")) { char *v = strtok_r(NULL, " \t", save); a->length = v ? atof(v) : 0; }
        else if (!strcmp(w, "loop")) a->loop = 1;
        else if (strcmp(w, "once")) { fprintf(stderr, "%s:%d: not understood: %s\n", path, line, w); return -1; }
    }
    if (a->length <= 0) { fprintf(stderr, "%s:%d: the length must be above 0\n", path, line); return -1; }
    return 0;
}
static int parse_part(PartAnim *a, char **save, const char *path, int line) {
    char *name = strtok_r(NULL, " \t", save), *w = strtok_r(NULL, " \t", save), *g = w ? strtok_r(NULL, " \t", save) : NULL;
    Part *p = name ? add_part(a, name) : NULL;
    if (!p) { fprintf(stderr, "%s:%d: part needs a name (at most %d parts)\n", path, line, PARTS_MAX); return -1; }
    if (w && (strcmp(w, "group") || !g)) { fprintf(stderr, "%s:%d: part NAME [group GROUP]\n", path, line); return -1; }
    if (g) snprintf(p->group, sizeof p->group, "%s", g);
    return 0;
}
int parts_load(PartAnim *a, const char *path) {
    memset(a, 0, sizeof *a);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "%s: cannot open\n", path); return -1; }
    char buf[256];
    int line = 0, rc = 0;
    while (!rc && fgets(buf, sizeof buf, f)) {
        line++;
        buf[strcspn(buf, "\r\n")] = 0;
        char *save, *word = strtok_r(buf, " \t", &save);
        if (!word || *word == '#') continue;
        if (!strcmp(word, "anim")) rc = parse_anim(a, &save, path, line);
        else if (!strcmp(word, "part")) rc = parse_part(a, &save, path, line);
        else if (!strcmp(word, "key")) rc = parse_key(a, &save, path, line);
        else { fprintf(stderr, "%s:%d: not understood: %s\n", path, line, word); rc = -1; }
    }
    fclose(f);
    if (!rc && !a->name[0]) { fprintf(stderr, "%s: no anim line\n", path); rc = -1; }
    return rc;
}

PartPose parts_pose(const PartAnim *a, int part, double t) {
    const Part *p = &a->part[part];
    if (!p->nkey) return (PartPose){0, 0, 0, 0, 1};
    if (a->loop) { t = fmod(t, a->length); if (t < 0) t += a->length; }
    if (t <= p->key[0].t) return p->key[0].pose;
    int i = 0;
    while (i + 1 < p->nkey && p->key[i + 1].t <= t) i++;
    if (i + 1 >= p->nkey) return p->key[i].pose; /* (after the last key it stays there) */
    const PartKey *k0 = &p->key[i], *k1 = &p->key[i + 1];
    double e = ease(k1->ease, (t - k0->t) / (k1->t - k0->t));
    PartPose r;
    r.x = k0->pose.x + (k1->pose.x - k0->pose.x) * e;
    r.y = k0->pose.y + (k1->pose.y - k0->pose.y) * e;
    r.turn = k0->pose.turn + (k1->pose.turn - k0->pose.turn) * e;
    r.glow = k0->pose.glow + (k1->pose.glow - k0->pose.glow) * e;
    r.show = k0->pose.show; /* (shows or hides at the key, not in between) */
    return r;
}
