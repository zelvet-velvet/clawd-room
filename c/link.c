#include "link.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "json.h"

#define MAX_LINE 2000

void link_clean_name(const char *in, char *out, size_t n) { /* lower case letters, digits, - and _ only */
    size_t k = 0;
    for (; *in && k < 24 && k + 1 < n; in++) {
        char c = (char)tolower((unsigned char)*in);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out[k++] = c;
    }
    out[k] = 0;
    if (!k) snprintf(out, n, "room");
}

static int id_ok(const char *s) { /* ^[a-z0-9_-]{1,24}-[0-9]{1,10}$ */
    size_t n = strlen(s), dash = 0;
    int found = 0;
    for (size_t i = n; i-- > 0;) if (s[i] == '-') { dash = i; found = 1; break; }
    if (!found || dash < 1 || dash > 24 || n - dash - 1 < 1 || n - dash - 1 > 10) return 0;
    for (size_t i = 0; i < dash; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0; }
    for (size_t i = dash + 1; i < n; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

static int pid_alive(long pid) {
    if (pid <= 0) return 0; /* (kill(0) would signal our own group) */
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno != ESRCH; /* exists but belongs to someone else */
}

static void path(const Link *l, const char *id, const char *kind, char *out, size_t n) { snprintf(out, n, "%s/%s.%s", l->dir, id, kind); }

static void mkdirs(const char *p) {
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char *q = tmp + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(tmp, 0700); *q = '/'; }
    mkdir(tmp, 0700);
}

void link_init(Link *l, const char *name, const char *dir) {
    memset(l, 0, sizeof *l);
    link_clean_name(name, l->name, sizeof l->name);
    if (dir) snprintf(l->dir, sizeof l->dir, "%s", dir);
    else if (getenv("CLAWD_LINK_DIR")) snprintf(l->dir, sizeof l->dir, "%s", getenv("CLAWD_LINK_DIR"));
    else snprintf(l->dir, sizeof l->dir, "%s/.clawd-room/link", getenv("HOME") ? getenv("HOME") : "");
    snprintf(l->id, sizeof l->id, "%s-%d", l->name, (int)getpid());
}

static double wall(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }

static void json_str(FILE *f, const char *s) { /* a JSON string literal */
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c < 0x20) fprintf(f, "\\u%04x", c);
        else fputc(c, f);
    }
    fputc('"', f);
}

static void beat(Link *l, double now, int cols, int rows, int force) { /* rewrite our presence file (atomically) */
    if (!force && now - l->last_beat < LINK_BEAT_EVERY) return;
    l->last_beat = now;
    char tmp[600], dst[600];
    path(l, l->id, "tmp", tmp, sizeof tmp);
    path(l, l->id, "peer", dst, sizeof dst);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;
    FILE *f = fdopen(fd, "w");
    fprintf(f, "{\"id\": "); json_str(f, l->id);
    fprintf(f, ", \"name\": "); json_str(f, l->name);
    fprintf(f, ", \"pid\": %d, \"t\": %.17g, \"cols\": %d, \"rows\": %d, \"theme\": ", (int)getpid(), now, cols, rows);
    json_str(f, l->theme);
    if (l->has_band) fprintf(f, ", \"band\": [%d, %d]", l->band[0], l->band[1]); else fprintf(f, ", \"band\": null");
    if (l->has_grid) { fprintf(f, ", \"grid\": ["); for (int i = 0; i < 7; i++) fprintf(f, "%s%.17g", i ? ", " : "", l->grid[i]); fprintf(f, "]"); }
    else fprintf(f, ", \"grid\": null");
    fprintf(f, ", \"homes\": [");
    for (int i = 0; i < l->nhomes; i++) { if (i) fprintf(f, ", "); json_str(f, l->homes[i]); }
    fprintf(f, "]}");
    fclose(f);
    rename(tmp, dst);
}

void link_start(Link *l) {
    mkdirs(l->dir);
    char p[600];
    path(l, l->id, "inbox", p, sizeof p);
    close(open(p, O_WRONLY | O_CREAT | O_TRUNC, 0600));
    l->pos = 0; free(l->buf); l->buf = NULL; l->buflen = 0;
    beat(l, wall(), 0, 0, 1);
}

/* ---- sending ------------------------------------------------------------------------------------ */
static int send_line(Link *l, const char *to, const char *line) {
    char p[600];
    path(l, to, "inbox", p, sizeof p);
    int fd = open(p, O_WRONLY | O_APPEND); /* not created: the peer must exist */
    if (fd < 0) return 0;
    ssize_t w = write(fd, line, strlen(line)); /* one small write = one whole line */
    close(fd);
    return w > 0;
}

int link_send_simple(Link *l, const char *to, const char *type, const char *key, const char *value) {
    if (!id_ok(to)) return 0;
    char line[MAX_LINE + 64];
    if (key) snprintf(line, sizeof line, "{\"t\": %.17g, \"from\": \"%s\", \"to\": \"%s\", \"type\": \"%s\", \"data\": {\"%s\": \"%s\"}}\n", wall(), l->id, to, type, key, value);
    else snprintf(line, sizeof line, "{\"t\": %.17g, \"from\": \"%s\", \"to\": \"%s\", \"type\": \"%s\", \"data\": {}}\n", wall(), l->id, to, type);
    return send_line(l, to, line);
}

int link_send_num(Link *l, const char *to, const char *type, const char *key, double value) {
    if (!id_ok(to)) return 0;
    char line[MAX_LINE + 64];
    snprintf(line, sizeof line, "{\"t\": %.17g, \"from\": \"%s\", \"to\": \"%s\", \"type\": \"%s\", \"data\": {\"%s\": %.17g}}\n", wall(), l->id, to, type, key, value);
    return send_line(l, to, line);
}

int link_send_msg(Link *l, const char *to, const Msg *m) {
    if (!id_ok(to)) return 0;
    char *buf = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&buf, &size);
    fprintf(f, "{\"t\": %.17g, \"from\": ", wall()); json_str(f, l->id);
    fprintf(f, ", \"to\": "); json_str(f, to);
    fprintf(f, ", \"type\": \"%s\", \"data\": {\"sid\": ", m->walk ? "walk" : "recall");
    json_str(f, m->sid);
    if (m->walk) {
        if (!m->home) { fprintf(f, ", \"name\": "); json_str(f, m->name); } /* (a Clawd coming home carries no name) */
        if (m->home) fprintf(f, ", \"home\": 1");
        if (m->move) fprintf(f, ", \"move\": 1");
        fprintf(f, ", \"side\": \"%s\"", SIDE_NAME[m->side]);
        fprintf(f, ", \"y\": %d", (int)m->y);
    }
    fprintf(f, "}}\n");
    fclose(f);
    int ok = size <= MAX_LINE + 1 && send_line(l, to, buf);
    free(buf);
    return ok;
}

/* ---- other rooms -------------------------------------------------------------------------------- */
static void remove_files(Link *l, const char *id, const char *k1, const char *k2) {
    char p[600];
    path(l, id, k1, p, sizeof p); unlink(p);
    if (k2) { path(l, id, k2, p, sizeof p); unlink(p); }
}

static int cmp_peer(const void *a, const void *b) { return strcmp(((const LinkPeer *)a)->id, ((const LinkPeer *)b)->id); }

/* looks at the presence files; fills joined/left (presence copies) */
static void scan(Link *l, double now, LinkPeer *joined, int *nj, LinkPeer *left, int *nl) {
    LinkPeer found[MAX_LINK_PEERS];
    int nf = 0;
    DIR *d = opendir(l->dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            size_t n = strlen(de->d_name);
            if (n < 6 || strcmp(de->d_name + n - 5, ".peer")) continue;
            char rid[64];
            snprintf(rid, sizeof rid, "%.*s", (int)(n - 5), de->d_name);
            if (!strcmp(rid, l->id) || !id_ok(rid)) continue;
            char p[700], data[8193];
            snprintf(p, sizeof p, "%s/%s", l->dir, de->d_name);
            FILE *f = fopen(p, "rb");
            if (!f) continue;
            size_t got = fread(data, 1, 8192, f);
            fclose(f);
            J *rec = json_parse(data, got);
            const J *jt = json_get(rec, "t"), *jpid = json_get(rec, "pid"), *jid = json_get(rec, "id");
            if (!rec || !jt || jt->type != J_NUM || !jpid || jpid->type != J_NUM || !jid || jid->type != J_STR) { json_free(rec); continue; }
            long pid = (long)jpid->num;
            int alive = now - jt->num < LINK_ALIVE_FOR && pid_alive(pid);
            if (strcmp(jid->str, rid)) { json_free(rec); continue; }
            if (alive && nf < MAX_LINK_PEERS) {
                LinkPeer *q = &found[nf++];
                memset(q, 0, sizeof *q);
                snprintf(q->id, sizeof q->id, "%s", rid);
                const J *v = json_get(rec, "name");
                link_clean_name(v && v->type == J_STR ? v->str : "", q->name, sizeof q->name);
                v = json_get(rec, "cols"); q->cols = v && v->type == J_NUM ? (int)v->num : 0;
                v = json_get(rec, "rows"); q->rows = v && v->type == J_NUM ? (int)v->num : 0;
                v = json_get(rec, "theme");
                link_clean_name(v && v->type == J_STR ? v->str : "", q->theme, sizeof q->theme);
                if (!v || v->type != J_STR || !v->str[0]) snprintf(q->theme, sizeof q->theme, "room"); /* (clean_name of "" is "room", like Python) */
                v = json_get(rec, "band");
                if (v && v->type == J_ARR && v->n == 2 && v->kids[0]->type == J_NUM && v->kids[1]->type == J_NUM) { q->has_band = 1; q->band[0] = (int)v->kids[0]->num; q->band[1] = (int)v->kids[1]->num; }
                v = json_get(rec, "grid");
                if (v && v->type == J_ARR && v->n == 7) {
                    int ok = 1;
                    for (int i = 0; i < 7; i++) if (v->kids[i]->type != J_NUM) ok = 0;
                    if (ok) { q->has_grid = 1; for (int i = 0; i < 7; i++) q->grid[i] = v->kids[i]->num; }
                }
                v = json_get(rec, "homes");
                if (v && v->type == J_ARR)
                    for (int i = 0; i < v->n && q->nhomes < MAX_CLAIMS; i++) if (v->kids[i]->type == J_STR) snprintf(q->homes[q->nhomes++], 72, "%.64s", v->kids[i]->str);
            } else if (!alive && !pid_alive(pid)) remove_files(l, rid, "peer", "inbox"); /* tidy up after a room that crashed */
            json_free(rec);
        }
        closedir(d);
    }
    qsort(found, (size_t)nf, sizeof(LinkPeer), cmp_peer);
    *nj = *nl = 0;
    for (int i = 0; i < nf; i++) {
        int had = 0;
        for (int k = 0; k < l->npeers; k++) if (!strcmp(l->peers[k].id, found[i].id)) had = 1;
        if (!had) joined[(*nj)++] = found[i];
    }
    for (int k = 0; k < l->npeers; k++) {
        int still = 0;
        for (int i = 0; i < nf; i++) if (!strcmp(l->peers[k].id, found[i].id)) still = 1;
        if (!still) left[(*nl)++] = l->peers[k];
    }
    memcpy(l->peers, found, sizeof(LinkPeer) * (size_t)nf);
    l->npeers = nf;
}

/* ---- messages ----------------------------------------------------------------------------------- */
static int parse_msg(Link *l, const char *line, size_t n, LinkEvent *ev) {
    J *m = json_parse(line, n);
    if (!m) return 0;
    const J *type = json_get(m, "type"), *from = json_get(m, "from"), *to = json_get(m, "to"), *data = json_get(m, "data");
    int ok = type && type->type == J_STR && from && from->type == J_STR && to && to->type == J_STR && data && data->type == J_OBJ
             && (!strcmp(type->str, "hello") || !strcmp(type->str, "bye") || !strcmp(type->str, "ping") || !strcmp(type->str, "pong")
                 || !strcmp(type->str, "walk") || !strcmp(type->str, "recall"))
             && id_ok(from->str) && !strcmp(to->str, l->id) && json_get(m, "t") && json_get(m, "t")->type == J_NUM;
    if (!ok) { json_free(m); return 0; }
    memset(ev, 0, sizeof *ev);
    ev->kind = 2;
    snprintf(ev->type, sizeof ev->type, "%s", type->str);
    Msg *g = &ev->msg;
    snprintf(g->peer, sizeof g->peer, "%s", from->str);
    g->walk = !strcmp(type->str, "walk");
    g->side = -1;
    for (int i = 0; i < data->n; i++) { /* keys cut to 16 characters; only strings, numbers and booleans are kept */
        char key[17];
        snprintf(key, sizeof key, "%s", data->keys[i]);
        const J *v = data->kids[i];
        if (v->type == J_STR) {
            if (!strcmp(key, "sid")) snprintf(g->sid, sizeof g->sid, "%.64s", v->str);
            else if (!strcmp(key, "name")) snprintf(g->name, sizeof g->name, "%.32s", v->str);
            else if (!strcmp(key, "side")) g->side = side_index(v->str);
        } else if (v->type == J_NUM || v->type == J_BOOL) {
            if (!strcmp(key, "home")) g->home = v->num != 0;
            else if (!strcmp(key, "move")) g->move = v->num != 0;
            else if (!strcmp(key, "y")) { g->has_y = 1; g->y = v->num; }
        }
    }
    json_free(m);
    return 1;
}

static int receive(Link *l, LinkEvent *out, int max) {
    char p[600];
    path(l, l->id, "inbox", p, sizeof p);
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    long size = (long)st.st_size;
    if (size < l->pos) { l->pos = 0; l->buflen = 0; }
    if (size == l->pos) return 0;
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, l->pos, SEEK_SET);
    size_t want = (size_t)(size - l->pos);
    char *data = malloc(l->buflen + want + 1);
    if (l->buflen) memcpy(data, l->buf, l->buflen);
    size_t got = fread(data + l->buflen, 1, want, f);
    fclose(f);
    l->pos += (long)got;
    size_t total = l->buflen + got, end = total;
    while (end > 0 && data[end - 1] != '\n') end--;
    int count = 0;
    for (size_t i = 0; i < end && count < max;) {
        size_t j = i;
        while (j < end && data[j] != '\n') j++;
        if (j > i && parse_msg(l, data + i, j - i, &out[count])) count++;
        i = j + 1;
    }
    free(l->buf);
    l->buflen = total - end;
    l->buf = l->buflen ? malloc(l->buflen) : NULL;
    if (l->buflen) memcpy(l->buf, data + end, l->buflen);
    free(data);
    return count;
}

int link_update(Link *l, double now, int cols, int rows, LinkEvent *out, int max) {
    beat(l, now, cols, rows, 0);
    int n = 0;
    if (now - l->last_scan >= LINK_SCAN_EVERY) {
        l->last_scan = now;
        LinkPeer joined[MAX_LINK_PEERS], left[MAX_LINK_PEERS];
        int nj, nl;
        scan(l, now, joined, &nj, left, &nl);
        for (int i = 0; i < nj && n < max; i++) {
            link_send_simple(l, joined[i].id, "hello", "room", l->name);
            memset(&out[n], 0, sizeof out[n]);
            out[n].kind = 0; out[n].peer = joined[i];
            n++;
        }
        for (int i = 0; i < nl && n < max; i++) { memset(&out[n], 0, sizeof out[n]); out[n].kind = 1; out[n].peer = left[i]; n++; }
    }
    if (n < max) n += receive(l, out + n, max - n);
    return n;
}

void link_close(Link *l) {
    for (int i = 0; i < l->npeers; i++) link_send_simple(l, l->peers[i].id, "bye", NULL, NULL);
    remove_files(l, l->id, "peer", "inbox");
    remove_files(l, l->id, "tmp", NULL);
    free(l->buf);
    l->buf = NULL;
}
