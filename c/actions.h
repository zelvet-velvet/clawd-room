/* The actions the sprite sheet and GIFs show: a stand-in Session per action, sampled over one loop (ACTIONS in sprite_tool.py). */
#ifndef ACTIONS_H
#define ACTIONS_H
#include "clawd.h"

#define ACTION_STEPS 41
#define ACTION_FPS 12
typedef struct { const char *name, *state; void (*set)(Session *); } Action;
extern const Action ACTIONS[];
extern const int N_ACTIONS;

/* the poses of one loop of action i (t = 0.2 + step / 12 s, so the first frame is not a blink) */
void action_poses(int i, Pose out[ACTION_STEPS]);
#endif
