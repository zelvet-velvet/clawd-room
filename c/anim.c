/* The animator: named values eased over time. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "anim.h"

double ease(int kind, double x) {
    x = x < 0 ? 0 : x > 1 ? 1 : x;
    switch (kind) {
    case EASE_OUT: return 1 - (1 - x) * (1 - x);
    case EASE_IN_OUT: return x < 0.5 ? 2 * x * x : 1 - 2 * (1 - x) * (1 - x);
    case EASE_BACK: { const double c = 1.6; double y = x - 1; return 1 + (c + 1) * y * y * y + c * y * y; }
    case EASE_IN: return x * x;
    case EASE_SINE: return 0.5 - 0.5 * cos(M_PI * x);
    case EASE_SINE_OUT: return sin(M_PI / 2 * x);
    case EASE_SINE_IN: return 1 - cos(M_PI / 2 * x);
    default: return x;
    }
}

static Tween *find(const Animator *a, const char *key) {
    for (int i = 0; i < MAX_TWEENS; i++) if (a->tw[i].used && !strcmp(a->tw[i].key, key)) return (Tween *)&a->tw[i];
    return NULL;
}
static Tween *slot(Animator *a, const char *key) {
    Tween *t = find(a, key);
    for (int i = 0; i < MAX_TWEENS && !t; i++) if (!a->tw[i].used) t = &a->tw[i];
    if (!t) t = &a->tw[0]; /* (full: reuse the first) */
    snprintf(t->key, sizeof t->key, "%s", key);
    t->used = 1;
    return t;
}

void anim_start(Animator *a, const char *key, double from, double to, double t0, double len, int kind) {
    Tween *t = slot(a, key);
    t->from = from; t->to = to; t->t0 = t0; t->len = len > 0 ? len : 0; t->ease = kind;
}
double anim_value(const Animator *a, const char *key, double now, double fallback) {
    const Tween *t = find(a, key);
    if (!t) return fallback;
    if (t->len <= 0 || now >= t->t0 + t->len) return t->to;
    if (now <= t->t0) return t->from;
    return t->from + (t->to - t->from) * ease(t->ease, (now - t->t0) / t->len);
}
void anim_to(Animator *a, const char *key, double to, double now, double len, int kind) {
    double from = anim_value(a, key, now, to);
    Tween *t = find(a, key);
    double span = t ? fabs(t->to - t->from) : 0, part = span > 0 ? fabs(to - from) / span : 1; /* (turning back half way takes half the time) */
    anim_start(a, key, from, to, now, len * (t ? (part > 1 ? 1 : part) : 1), kind);
}
void anim_set(Animator *a, const char *key, double v) { anim_start(a, key, v, v, 0, 0, EASE_LINEAR); }
double anim_target(const Animator *a, const char *key, double fallback) { const Tween *t = find(a, key); return t ? t->to : fallback; }
