#include "json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *p, *end; } Cur;
static void ws(Cur *c) { while (c->p < c->end && (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r')) c->p++; }

static J *mk(JType t) { J *j = calloc(1, sizeof *j); j->type = t; return j; }

static char *read_str(Cur *c) { /* cursor on the opening quote; returns a malloc'd UTF-8 string or NULL */
    if (c->p >= c->end || *c->p != '"') return NULL;
    c->p++;
    size_t cap = 16, n = 0;
    char *out = malloc(cap);
    while (c->p < c->end && *c->p != '"') {
        unsigned char ch = (unsigned char)*c->p++;
        if (ch < 0x20) { free(out); return NULL; }
        unsigned cp = ch;
        int raw = 1;
        if (ch == '\\') {
            if (c->p >= c->end) { free(out); return NULL; }
            char e = *c->p++;
            raw = 0;
            switch (e) {
            case '"': case '\\': case '/': cp = (unsigned char)e; break;
            case 'b': cp = 8; break; case 'f': cp = 12; break; case 'n': cp = 10; break; case 'r': cp = 13; break; case 't': cp = 9; break;
            case 'u': {
                if (c->end - c->p < 4) { free(out); return NULL; }
                cp = 0;
                for (int i = 0; i < 4; i++) { int h = (unsigned char)c->p[i]; if (!isxdigit(h)) { free(out); return NULL; } cp = cp * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 32) - 'a' + 10); }
                c->p += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && c->end - c->p >= 6 && c->p[0] == '\\' && c->p[1] == 'u') {
                    unsigned lo = 0;
                    int ok = 1;
                    for (int i = 0; i < 4; i++) { int h = (unsigned char)c->p[2 + i]; if (!isxdigit(h)) ok = 0; else lo = lo * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 32) - 'a' + 10); }
                    if (ok && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); c->p += 6; }
                }
                break; }
            default: free(out); return NULL;
            }
        }
        if (n + 5 >= cap) { cap *= 2; out = realloc(out, cap); }
        if (raw || cp < 0x80) out[n++] = (char)cp;
        else if (cp < 0x800) { out[n++] = (char)(0xC0 | (cp >> 6)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out[n++] = (char)(0xE0 | (cp >> 12)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
        else { out[n++] = (char)(0xF0 | (cp >> 18)); out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
    }
    if (c->p >= c->end) { free(out); return NULL; }
    c->p++;
    out[n] = 0;
    return out;
}

static J *value(Cur *c, int depth);

static J *container(Cur *c, int depth, int obj) {
    char close = obj ? '}' : ']';
    J *j = mk(obj ? J_OBJ : J_ARR);
    c->p++;
    ws(c);
    if (c->p < c->end && *c->p == close) { c->p++; return j; }
    for (;;) {
        ws(c);
        char *key = NULL;
        if (obj) {
            key = read_str(c);
            if (!key) goto bad;
            ws(c);
            if (c->p >= c->end || *c->p++ != ':') { free(key); goto bad; }
        }
        J *v = value(c, depth + 1);
        if (!v) { free(key); goto bad; }
        j->kids = realloc(j->kids, sizeof(J *) * (size_t)(j->n + 1));
        j->kids[j->n] = v;
        if (obj) { j->keys = realloc(j->keys, sizeof(char *) * (size_t)(j->n + 1)); j->keys[j->n] = key; }
        j->n++;
        ws(c);
        if (c->p >= c->end) goto bad;
        if (*c->p == ',') { c->p++; continue; }
        if (*c->p == close) { c->p++; return j; }
        goto bad;
    }
bad:
    json_free(j);
    return NULL;
}

static J *value(Cur *c, int depth) {
    ws(c);
    if (c->p >= c->end || depth > 50) return NULL;
    char ch = *c->p;
    if (ch == '"') { char *s = read_str(c); if (!s) return NULL; J *j = mk(J_STR); j->str = s; return j; }
    if (ch == '{') return container(c, depth, 1);
    if (ch == '[') return container(c, depth, 0);
    if (c->end - c->p >= 4 && !strncmp(c->p, "true", 4)) { c->p += 4; J *j = mk(J_BOOL); j->num = 1; return j; }
    if (c->end - c->p >= 5 && !strncmp(c->p, "false", 5)) { c->p += 5; return mk(J_BOOL); }
    if (c->end - c->p >= 4 && !strncmp(c->p, "null", 4)) { c->p += 4; return mk(J_NULL); }
    const char *s = c->p;
    int frac = 0;
    if (*c->p == '-') c->p++;
    if (c->p >= c->end || !isdigit((unsigned char)*c->p)) return NULL;
    while (c->p < c->end && isdigit((unsigned char)*c->p)) c->p++;
    if (c->p < c->end && *c->p == '.') { frac = 1; c->p++; if (c->p >= c->end || !isdigit((unsigned char)*c->p)) return NULL; while (c->p < c->end && isdigit((unsigned char)*c->p)) c->p++; }
    if (c->p < c->end && (*c->p == 'e' || *c->p == 'E')) {
        frac = 1; c->p++;
        if (c->p < c->end && (*c->p == '+' || *c->p == '-')) c->p++;
        if (c->p >= c->end || !isdigit((unsigned char)*c->p)) return NULL;
        while (c->p < c->end && isdigit((unsigned char)*c->p)) c->p++;
    }
    char buf[64];
    size_t l = (size_t)(c->p - s);
    if (l >= sizeof buf) return NULL;
    memcpy(buf, s, l); buf[l] = 0;
    J *j = mk(J_NUM);
    j->num = strtod(buf, NULL);
    j->is_int = !frac;
    return j;
}

J *json_parse(const char *s, size_t len) {
    Cur c = {s, s + len};
    J *j = value(&c, 0);
    if (!j) return NULL;
    ws(&c);
    if (c.p != c.end) { json_free(j); return NULL; }
    return j;
}

void json_free(J *j) {
    if (!j) return;
    for (int i = 0; i < j->n; i++) { json_free(j->kids[i]); if (j->keys) free(j->keys[i]); }
    free(j->kids); free(j->keys); free(j->str); free(j);
}

const J *json_get(const J *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (int i = obj->n - 1; i >= 0; i--) if (!strcmp(obj->keys[i], key)) return obj->kids[i];
    return NULL;
}
