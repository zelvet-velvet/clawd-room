#include "tail.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_HISTORY_BYTES 2000000

void tail_init(Tail *t, const char *path) { memset(t, 0, sizeof *t); snprintf(t->path, sizeof t->path, "%s", path); }
void tail_free(Tail *t) { free(t->buf); t->buf = NULL; t->buflen = 0; }

/* ---- a small JSON reader: just enough to take the five fields out of one object ------------- */
typedef struct { const char *p, *end; } Cur;

static void ws(Cur *c) { while (c->p < c->end && (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r')) c->p++; }

static int put_utf8(char *out, size_t cap, size_t *n, unsigned cp) {
    char t[4];
    int k = 0;
    if (cp < 0x80) t[k++] = (char)cp;
    else if (cp < 0x800) { t[k++] = (char)(0xC0 | (cp >> 6)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { t[k++] = (char)(0xE0 | (cp >> 12)); t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    else { t[k++] = (char)(0xF0 | (cp >> 18)); t[k++] = (char)(0x80 | ((cp >> 12) & 0x3F)); t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    for (int i = 0; i < k; i++) if (out && *n + 1 < cap) out[(*n)++] = t[i]; else if (!out) (*n)++;
    return 1;
}

static int hex4(Cur *c, unsigned *v) {
    if (c->end - c->p < 4) return 0;
    *v = 0;
    for (int i = 0; i < 4; i++) {
        int ch = (unsigned char)c->p[i];
        if (!isxdigit(ch)) return 0;
        *v = *v * 16 + (unsigned)(ch <= '9' ? ch - '0' : (ch | 32) - 'a' + 10);
    }
    c->p += 4;
    return 1;
}

/* reads a JSON string (cursor on the opening quote) into out (cap bytes, truncated); 0 if malformed */
static int read_string(Cur *c, char *out, size_t cap) {
    size_t n = 0;
    if (c->p >= c->end || *c->p != '"') return 0;
    c->p++;
    while (c->p < c->end && *c->p != '"') {
        unsigned char ch = (unsigned char)*c->p++;
        if (ch < 0x20) return 0; /* control characters are not allowed inside strings */
        if (ch != '\\') { if (out && n + 1 < cap) out[n] = (char)ch; n++; continue; }
        if (c->p >= c->end) return 0;
        char e = *c->p++;
        unsigned cp;
        switch (e) {
        case '"': case '\\': case '/': cp = (unsigned char)e; break;
        case 'b': cp = 8; break; case 'f': cp = 12; break; case 'n': cp = 10; break; case 'r': cp = 13; break; case 't': cp = 9; break;
        case 'u':
            if (!hex4(c, &cp)) return 0;
            if (cp >= 0xD800 && cp < 0xDC00 && c->end - c->p >= 6 && c->p[0] == '\\' && c->p[1] == 'u') { /* a surrogate pair */
                Cur save = *c;
                unsigned lo;
                c->p += 2;
                if (hex4(c, &lo) && lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                else *c = save;
            }
            break;
        default: return 0;
        }
        size_t before = n;
        put_utf8(NULL, 0, &n, cp);
        if (out) { size_t m = before; put_utf8(out, cap, &m, cp); }
    }
    if (c->p >= c->end) return 0;
    c->p++;
    if (out) out[n < cap ? n : cap - 1] = 0;
    return 1;
}

static int skip_value(Cur *c, int depth);

static int skip_number(Cur *c) {
    const char *s = c->p;
    if (c->p < c->end && *c->p == '-') c->p++;
    if (c->p >= c->end || !isdigit((unsigned char)*c->p)) return 0;
    while (c->p < c->end && isdigit((unsigned char)*c->p)) c->p++;
    if (c->p < c->end && *c->p == '.') { c->p++; if (c->p >= c->end || !isdigit((unsigned char)*c->p)) return 0; while (c->p < c->end && isdigit((unsigned char)*c->p)) c->p++; }
    if (c->p < c->end && (*c->p == 'e' || *c->p == 'E')) {
        c->p++;
        if (c->p < c->end && (*c->p == '+' || *c->p == '-')) c->p++;
        if (c->p >= c->end || !isdigit((unsigned char)*c->p)) return 0;
        while (c->p < c->end && isdigit((unsigned char)*c->p)) c->p++;
    }
    return c->p > s;
}

static int skip_value(Cur *c, int depth) {
    ws(c);
    if (c->p >= c->end || depth > 50) return 0;
    char ch = *c->p;
    if (ch == '"') return read_string(c, NULL, 0);
    if (ch == '{' || ch == '[') {
        char close = ch == '{' ? '}' : ']';
        c->p++;
        ws(c);
        if (c->p < c->end && *c->p == close) { c->p++; return 1; }
        for (;;) {
            ws(c);
            if (ch == '{') { if (!read_string(c, NULL, 0)) return 0; ws(c); if (c->p >= c->end || *c->p++ != ':') return 0; }
            if (!skip_value(c, depth + 1)) return 0;
            ws(c);
            if (c->p >= c->end) return 0;
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == close) { c->p++; return 1; }
            return 0;
        }
    }
    if (!strncmp(c->p, "true", 4) && c->end - c->p >= 4) { c->p += 4; return 1; }
    if (!strncmp(c->p, "false", 5) && c->end - c->p >= 5) { c->p += 5; return 1; }
    if (!strncmp(c->p, "null", 4) && c->end - c->p >= 4) { c->p += 4; return 1; }
    return skip_number(c);
}

/* the value of a field as text: strings as they are, numbers as written, null/false = "" ; kind: 's' string, 'n' number, 'z' null, 'o' other */
static int field_value(Cur *c, char *out, size_t cap, char *kind) {
    ws(c);
    if (c->p >= c->end) return 0;
    out[0] = 0;
    if (*c->p == '"') { *kind = 's'; return read_string(c, out, cap); }
    const char *s = c->p;
    if (!strncmp(c->p, "null", 4)) { *kind = 'z'; return skip_value(c, 0); }
    if (*c->p == '-' || isdigit((unsigned char)*c->p)) {
        if (!skip_number(c)) return 0;
        snprintf(out, cap, "%.*s", (int)(c->p - s), s);
        *kind = 'n';
        return 1;
    }
    *kind = 'o';
    return skip_value(c, 0);
}

int tail_parse(const char *line, size_t n, Event *e) {
    Cur c = {line, line + n};
    char key[32], val[256], kind;
    int have_t = 0, have_sid = 0, have_ev = 0;
    memset(e, 0, sizeof *e);
    ws(&c);
    if (c.p >= c.end || *c.p++ != '{') return 0;
    ws(&c);
    if (c.p < c.end && *c.p == '}') return 0;
    for (;;) {
        ws(&c);
        if (!read_string(&c, key, sizeof key)) return 0;
        ws(&c);
        if (c.p >= c.end || *c.p++ != ':') return 0;
        if (!strcmp(key, "t") || !strcmp(key, "sid") || !strcmp(key, "ev") || !strcmp(key, "cwd") || !strcmp(key, "tool")) {
            if (!field_value(&c, val, sizeof val, &kind)) return 0;
            if (!strcmp(key, "t")) { /* float(): a number or a numeric string */
                char *end;
                if (kind != 'n' && kind != 's') return 0;
                e->t = strtod(val, &end);
                if (end == val || *end) return 0;
                have_t = 1;
            } else if (!strcmp(key, "sid")) { have_sid = 1; snprintf(e->sid, sizeof e->sid, "%s", val); }
            else if (!strcmp(key, "ev")) { have_ev = 1; snprintf(e->ev, sizeof e->ev, "%s", val); }
            else if (!strcmp(key, "cwd")) snprintf(e->cwd, sizeof e->cwd, "%s", val);
            else snprintf(e->tool, sizeof e->tool, "%s", val);
        } else if (!skip_value(&c, 0)) return 0;
        ws(&c);
        if (c.p >= c.end) return 0;
        if (*c.p == ',') { c.p++; continue; }
        if (*c.p == '}') { c.p++; break; }
        return 0;
    }
    ws(&c);
    if (c.p != c.end) return 0; /* trailing junk */
    return have_t && have_sid && have_ev;
}

static int parse_lines(const char *data, size_t n, Event *out, int max, int count, double since, int use_since) {
    size_t i = 0;
    while (i < n && count < max) {
        size_t j = i;
        while (j < n && data[j] != '\n' && data[j] != '\r') j++;
        Event e;
        if (j > i && tail_parse(data + i, j - i, &e) && (!use_since || e.t >= since)) out[count++] = e;
        i = j + 1;
    }
    return count;
}

int tail_history(Tail *t, double since, Event *out, int max) {
    FILE *f = fopen(t->path, "rb");
    if (!f) { t->pos = 0; return 0; }
    struct stat st;
    fstat(fileno(f), &st);
    long size = (long)st.st_size, start = size > MAX_HISTORY_BYTES ? size - MAX_HISTORY_BYTES : 0;
    char *data = malloc((size_t)(size - start) + 1);
    fseek(f, start, SEEK_SET);
    size_t n = fread(data, 1, (size_t)(size - start), f);
    fclose(f);
    size_t off = 0;
    if (start) { while (off < n && data[off] != '\n') off++; if (off < n) off++; else off = n; } /* the first line is probably cut in half */
    size_t end = n;
    while (end > off && data[end - 1] != '\n') end--; /* a half-written last line is left for poll() */
    t->pos = size - (long)(n - end);
    int count = parse_lines(data + off, end - off, out, max, 0, since, 1);
    free(data);
    return count;
}

int tail_poll(Tail *t, Event *out, int max) {
    struct stat st;
    if (stat(t->path, &st) != 0) return 0;
    long size = (long)st.st_size;
    if (size < t->pos) { t->pos = 0; t->buflen = 0; } /* the file was truncated or replaced */
    if (size == t->pos) return 0;
    FILE *f = fopen(t->path, "rb");
    if (!f) return 0;
    fseek(f, t->pos, SEEK_SET);
    size_t want = (size_t)(size - t->pos);
    char *data = malloc(t->buflen + want + 1);
    memcpy(data, t->buf, t->buflen);
    size_t got = fread(data + t->buflen, 1, want, f);
    fclose(f);
    t->pos += (long)got;
    size_t total = t->buflen + got, end = total;
    while (end > 0 && data[end - 1] != '\n') end--; /* the rest stays in the buffer for next time */
    int count = parse_lines(data, end, out, max, 0, 0, 0);
    free(t->buf);
    t->buflen = total - end;
    t->buf = t->buflen ? malloc(t->buflen) : NULL;
    if (t->buflen) memcpy(t->buf, data + end, t->buflen);
    free(data);
    return count;
}
