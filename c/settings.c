#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "parts.h"
#include "settings.h"

#define MAX_KEYS 16
static char g_path[600], g_flag[24];
static struct { char key[32], val[64]; } g_kv[MAX_KEYS];
static int g_n, g_has_flag;
static time_t g_mt;
static double g_next;

static double now_s(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }

static void read_file(void) {
    g_n = 0;
    FILE *f = g_path[0] ? fopen(g_path, "r") : NULL;
    if (!f) return;
    char buf[256];
    while (fgets(buf, sizeof buf, f) && g_n < MAX_KEYS) {
        char *hash = strchr(buf, '#');
        if (hash) *hash = 0;
        char *eq = strchr(buf, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(buf), *v = trim(eq + 1);
        if (!*k || !*v) continue;
        snprintf(g_kv[g_n].key, sizeof g_kv[g_n].key, "%s", k);
        snprintf(g_kv[g_n].val, sizeof g_kv[g_n].val, "%s", v);
        g_n++;
    }
    fclose(f);
}
static time_t mtime(void) { struct stat st; return g_path[0] && stat(g_path, &st) == 0 ? st.st_mtime : 0; }

const char *settings_path(void) { return g_path; }
void settings_init(const char *path) {
    snprintf(g_path, sizeof g_path, "%s", path ? path : "");
    g_mt = mtime();
    read_file();
}
const char *settings_get(const char *key) {
    for (int i = 0; i < g_n; i++) if (!strcmp(g_kv[i].key, key)) return g_kv[i].val;
    return NULL;
}
void settings_apply(const char *gate_flag) {
    if (gate_flag) { snprintf(g_flag, sizeof g_flag, "%s", gate_flag); g_has_flag = 1; }
    const char *env = getenv("CLAWD_GATE_THEME"), *v = g_has_flag ? g_flag : env && env[0] ? env : settings_get("gate_theme");
    parts_set_gate_theme(v); /* (NULL: simple) */
}
int settings_poll(void) {
    double t = now_s();
    if (t < g_next) return 0;
    g_next = t + 0.5;
    time_t m = mtime();
    if (m == g_mt) return 0;
    g_mt = m;
    read_file();
    settings_apply(NULL);
    return 1;
}
