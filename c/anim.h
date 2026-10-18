/* The animator (Engine Design, step 4): named values eased from one number to another over a time, instead of each
 * animation keeping its own timer. The gates appearing and the doors swinging use it; anything can. */
#ifndef ANIM_H
#define ANIM_H

enum { EASE_LINEAR, EASE_OUT, EASE_IN_OUT, EASE_BACK }; /* steady / fast then slowing / slow, fast, slow / a little past, then back */

typedef struct { char key[16]; double from, to, t0, len; int ease, used; } Tween;
#define MAX_TWEENS 24
typedef struct { Tween tw[MAX_TWEENS]; } Animator;

double ease(int kind, double x);                                                                 /* x 0..1 -> 0..1 */
void anim_start(Animator *a, const char *key, double from, double to, double t0, double len, int kind); /* (t0 may be ahead: it waits) */
void anim_to(Animator *a, const char *key, double to, double now, double len, int kind);          /* from where it is now */
void anim_set(Animator *a, const char *key, double v);                                           /* there at once */
double anim_value(const Animator *a, const char *key, double now, double fallback);              /* fallback: no such value */
double anim_target(const Animator *a, const char *key, double fallback);                         /* where it is going */
#endif
