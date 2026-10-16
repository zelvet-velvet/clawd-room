#ifndef CLAWD_H
#define CLAWD_H
#include <stddef.h>
#include "canvas.h"
#include "session.h"
#include "sprites.h"
#include "theme.h"

typedef struct { int bob, lean, arms[2], legs; const char *eyes; /* "open" "happy" "shut" */ int look[2]; } Pose;

Pose pose_for(const Session *s, const char *state, double t, double now);
void pose_key(const Pose *p, char *out, size_t n); /* what picks a frame in the sprite file (not the bob) */
/* (cx, y) = body centre and top; draws the frame from `frames` if the file has it, else the code's drawing */
void draw_clawd(Canvas *cv, int cx, int y, const Pose *p, const SpriteFile *frames, Rgb body, const Theme *theme, const char *role);
#endif
