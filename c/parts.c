/* Part animations: the loader and the evaluator (parts.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "parts.h"

static int ease_by_name(const char *s) {
    if (!strcmp(s, "linear")) return EASE_LINEAR;
    if (!strcmp(s, "out")) return EASE_OUT;
    if (!strcmp(s, "in-out")) return EASE_IN_OUT;
    if (!strcmp(s, "back")) return EASE_BACK;
    if (!strcmp(s, "in")) return EASE_IN;
    if (!strcmp(s, "sine")) return EASE_SINE;
    if (!strcmp(s, "sine-out")) return EASE_SINE_OUT;
    if (!strcmp(s, "sine-in")) return EASE_SINE_IN;
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
    PartKey k = p->nkey ? p->key[p->nkey - 1] : (PartKey){0, {0, 0, 0, 0, 1, 100}, EASE_LINEAR};
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
        else if (!strcmp(f, "grow")) k.pose.grow = atof(v);
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
/* part NAME [group GROUP] [box X Y W H] [sprite] */
static int parse_part(PartAnim *a, char **save, const char *path, int line) {
    char *name = strtok_r(NULL, " \t", save);
    Part *p = name ? add_part(a, name) : NULL;
    if (!p) { fprintf(stderr, "%s:%d: part needs a name (at most %d parts)\n", path, line, PARTS_MAX); return -1; }
    for (char *w = strtok_r(NULL, " \t", save); w; w = strtok_r(NULL, " \t", save)) {
        if (!strcmp(w, "group")) { char *g = strtok_r(NULL, " \t", save); if (!g) { fprintf(stderr, "%s:%d: group needs a name\n", path, line); return -1; } snprintf(p->group, sizeof p->group, "%s", g); }
        else if (!strcmp(w, "sprite")) p->sprite = 1;
        else if (!strcmp(w, "box")) {
            for (int i = 0; i < 4; i++) { char *v = strtok_r(NULL, " \t", save); if (!v) { fprintf(stderr, "%s:%d: box needs X Y W H\n", path, line); return -1; } p->box[i] = atoi(v); }
            p->has_box = 1;
        } else { fprintf(stderr, "%s:%d: part NAME [group GROUP] [box X Y W H] [sprite]\n", path, line); return -1; }
    }
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
        else if (!strcmp(word, "style")) { char *v = strtok_r(NULL, " \t", &save); if (!v) { fprintf(stderr, "%s:%d: style needs a name\n", path, line); rc = -1; } else snprintf(a->style, sizeof a->style, "%s", v); }
        else if (!strcmp(word, "part")) rc = parse_part(a, &save, path, line);
        else if (!strcmp(word, "key")) rc = parse_key(a, &save, path, line);
        else if (!strcmp(word, "scene")) { /* scene LINE: a line of the stage's fixture, as it is */
            char *rest = save ? save + strspn(save, " \t") : (char *)"";
            if (a->nscene >= PARTS_SCENE_LINES) { fprintf(stderr, "%s:%d: too many scene lines\n", path, line); rc = -1; }
            else snprintf(a->scene[a->nscene++], sizeof a->scene[0], "%s", rest);
        } else if (!strcmp(word, "view")) { /* view X Y */
            char *x = strtok_r(NULL, " \t", &save), *y = strtok_r(NULL, " \t", &save);
            if (!x || !y) { fprintf(stderr, "%s:%d: view X Y\n", path, line); rc = -1; } else { a->view_x = atoi(x); a->view_y = atoi(y); }
        }
        else { fprintf(stderr, "%s:%d: not understood: %s\n", path, line, word); rc = -1; }
    }
    fclose(f);
    if (!rc && !a->name[0]) { fprintf(stderr, "%s: no anim line\n", path); rc = -1; }
    return rc;
}

PartPose parts_pose(const PartAnim *a, int part, double t) {
    const Part *p = &a->part[part];
    if (!p->nkey) return (PartPose){0, 0, 0, 0, 1, 100};
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
    r.grow = k0->pose.grow + (k1->pose.grow - k0->pose.grow) * e;
    r.show = k0->pose.show; /* (shows or hides at the key, not in between) */
    return r;
}

int parts_save(const PartAnim *a, const char *path) {
    char dir[700];
    snprintf(dir, sizeof dir, "%s", path);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = 0; for (char *q = dir + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(dir, 0755); *q = '/'; } mkdir(dir, 0755); } /* (every folder on the way: a theme's) */
    char tmp[720];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# %s: a part animation (the pixel editor wrote this file; the format is in anims/swing.txt)\n", a->name);
    fprintf(f, "anim %s length %g %s\n", a->name, a->length, a->loop ? "loop" : "once");
    if (a->style[0]) fprintf(f, "style %s\n", a->style);
    for (int i = 0; i < a->nscene; i++) fprintf(f, "scene %s\n", a->scene[i]);
    fprintf(f, "view %d %d\n", a->view_x, a->view_y);
    static const char *const EASE[] = {"linear", "out", "in-out", "back", "in", "sine", "sine-out", "sine-in"};
    for (int i = 0; i < a->npart; i++) {
        const Part *p = &a->part[i];
        fprintf(f, "part %s", p->name);
        if (p->group[0]) fprintf(f, " group %s", p->group);
        if (p->has_box) fprintf(f, " box %d %d %d %d", p->box[0], p->box[1], p->box[2], p->box[3]);
        if (p->sprite) fprintf(f, " sprite");
        fprintf(f, "\n");
    }
    for (int i = 0; i < a->npart; i++) {
        const Part *p = &a->part[i];
        for (int k = 0; k < p->nkey; k++) {
            const PartKey *q = &p->key[k];
            fprintf(f, "key %s %g x=%g y=%g turn=%g glow=%g show=%d", p->name, q->t, q->pose.x, q->pose.y, q->pose.turn, q->pose.glow, q->pose.show);
            fprintf(f, " grow=%g", q->pose.grow); /* (always: a key without it takes the one before, so a 100 after a 0 must be written) */
            if (q->ease != EASE_LINEAR) fprintf(f, " ease=%s", EASE[q->ease]);
            fprintf(f, "\n");
        }
    }
    if (fclose(f)) return -1;
    return rename(tmp, path); /* (all or nothing: a room reading it never sees half a file) */
}

/* the anims folder (next to sprites/ and rooms/) and the animations in it, each read again when its file changes (like the room files) */
static char g_dir[600], g_mine[600];
void parts_set_dir(const char *dir) { snprintf(g_dir, sizeof g_dir, "%s", dir ? dir : ""); }
void parts_set_mine_dir(const char *dir) { snprintf(g_mine, sizeof g_mine, "%s", dir ? dir : ""); }
const char *parts_mine_dir(void) { return g_mine; }
const char *parts_default_dir(void) { return g_dir; }
void parts_inherit(PartAnim *a, const char *name) {
    char path[700];
    if (!g_dir[0]) return;
    snprintf(path, sizeof path, "%s/%s.txt", g_dir, name);
    static PartAnim d;
    FILE *f = fopen(path, "r");
    if (!f) return;
    fclose(f);
    if (parts_load(&d, path)) return;
    for (int i = 0; i < a->npart; i++) { int j = parts_find(&d, a->part[i].name); if (j >= 0 && d.part[j].sprite) a->part[i].sprite = 1; }
}

void parts_name_theme(PartAnim *a, const char *name) {
    a->theme[0] = 0;
    if (strncmp(name, "gates/", 6)) return;
    const char *e = strchr(name + 6, '/');
    if (e && e - (name + 6) < (int)sizeof a->theme) snprintf(a->theme, sizeof a->theme, "%.*s", (int)(e - (name + 6)), name + 6);
}
#define NAMED 16
const PartAnim *parts_get(const char *name) {
    static struct { char name[PARTS_PATH_NAME], path[700]; PartAnim a; time_t mt; int ok, tried; } C[NAMED];
    struct stat st;
    char path[700];
    int k = 0;
    while (k < NAMED && C[k].tried && strcmp(C[k].name, name)) k++;
    if (k == NAMED || !g_dir[0]) return NULL;
    snprintf(path, sizeof path, "%s/%s.txt", g_mine, name);
    if (!g_mine[0] || stat(path, &st) != 0) snprintf(path, sizeof path, "%s/%s.txt", g_dir, name); /* (yours first, else the default) */
    if (stat(path, &st) != 0) { C[k].tried = 1; C[k].ok = 0; snprintf(C[k].name, sizeof C[k].name, "%s", name); return NULL; }
    if (!C[k].tried || st.st_mtime != C[k].mt || strcmp(path, C[k].path)) { /* (new, or the file changed: a file with a mistake keeps the last good one) */
        static PartAnim fresh;
        C[k].tried = 1; C[k].mt = st.st_mtime; snprintf(C[k].name, sizeof C[k].name, "%s", name); snprintf(C[k].path, sizeof C[k].path, "%s", path);
        if (parts_load(&fresh, path) == 0) { parts_inherit(&fresh, name); parts_name_theme(&fresh, name); C[k].a = fresh; C[k].ok = 1; }
    }
    return C[k].ok ? &C[k].a : NULL;
}

static int cmp_names(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
/* the files of one folder (and of the folders in it, one level at a time, down to a few levels), as names under anims/ */
static int names_in(const char *dir, const char *prefix, char names[][PARTS_PATH_NAME], int n, int max, int depth) {
    DIR *dp = opendir(dir);
    if (!dp) return n;
    for (struct dirent *e; (e = readdir(dp)); ) {
        size_t l = strlen(e->d_name);
        char nm[PARTS_PATH_NAME], sub[700];
        if (e->d_name[0] == '.') continue;
        if (l >= 5 && !strcmp(e->d_name + l - 4, ".txt")) {
            if (snprintf(nm, sizeof nm, "%s%.*s", prefix, (int)(l - 4), e->d_name) >= (int)sizeof nm) continue;
            int have = 0;
            for (int i = 0; i < n; i++) if (!strcmp(names[i], nm)) have = 1;
            if (!have && n < max) snprintf(names[n++], PARTS_PATH_NAME, "%s", nm);
        } else if (depth < 3) { /* (a folder: a theme's files) */
            struct stat st;
            snprintf(sub, sizeof sub, "%s/%s", dir, e->d_name);
            if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            if (snprintf(nm, sizeof nm, "%s%s/", prefix, e->d_name) >= (int)sizeof nm) continue;
            n = names_in(sub, nm, names, n, max, depth + 1);
        }
    }
    closedir(dp);
    return n;
}
int parts_names(char names[][PARTS_PATH_NAME], int max) {
    int n = 0;
    const char *dirs[2] = {g_dir, g_mine};
    for (int d = 0; d < 2; d++) if (dirs[d][0]) n = names_in(dirs[d], "", names, n, max, 0);
    qsort(names, (size_t)n, PARTS_PATH_NAME, cmp_names);
    return n;
}

/* the gate theme: the folder anims/gates/THEME holds gate-door, gate-stairs, gate-well (and any more a theme wants); a theme missing a
 * file uses the simple theme's, so a new theme can be started with one file */
static char g_gate_theme[24] = "simple";
void parts_set_gate_theme(const char *theme) { snprintf(g_gate_theme, sizeof g_gate_theme, "%s", theme && theme[0] ? theme : "simple"); }
const char *parts_gate_theme(void) { return g_gate_theme; }
const PartAnim *parts_gate(const char *kind) {
    char nm[PARTS_PATH_NAME];
    snprintf(nm, sizeof nm, "gates/%s/%s", g_gate_theme, kind);
    const PartAnim *a = parts_get(nm);
    if (a || !strcmp(g_gate_theme, "simple")) return a;
    snprintf(nm, sizeof nm, "gates/simple/%s", kind);
    return parts_get(nm);
}
