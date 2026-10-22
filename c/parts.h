/* Part animations (Interaction, "Part animations in the pixel editor"): a thing made of parts (a gate's steps and rail, a
 * swing's seat and chains, the espresso machine's cup and steam, a Clawd) where each part has keyframes: where it is, how it
 * is turned, whether it shows, its glow. The engine moves a part between its keyframes, so one animation fits every room
 * size. An animation is a text file in anims/ (see the header of anims/swing.txt); this loads one and says where each part
 * is at a time. */
#ifndef PARTS_H
#define PARTS_H
#include "anim.h"

#define PARTS_MAX 24       /* parts in an animation */
#define PARTS_MAX_KEYS 32  /* keyframes of a part */

typedef struct { double x, y, turn, glow; int show; double grow; } PartPose; /* offsets from the part's rest place in room pixels, degrees, glow 0..100, show 0 or 1,
                                                                       grow 0..100: how much of the part has come in (a gate appearing; 100 when not given) */
typedef struct { double t; PartPose pose; int ease; } PartKey;   /* ease: how the part gets here from the key before */
typedef struct { char name[24], group[24]; PartKey key[PARTS_MAX_KEYS]; int nkey; int has_box, box[4], sprite; } Part; /* box: x y w h at rest, in room pixels (the editor outlines it); sprite: a drawn sprite (a Clawd): it moves, it does not rotate */
#define PARTS_SCENE_LINES 12
typedef struct {
    char style[16];   /* `style construct` (a gate theme): the room builds the whole gate from its own pieces, timed by the part `build` (its grow) */
    char name[24]; double length; int loop; Part part[PARTS_MAX]; int npart;
    char scene[PARTS_SCENE_LINES][160]; int nscene;  /* the editor's stage: lines of a fixture (see tools/fixtures), or `emote SID KIND`: that Clawd starts that emote when the animation does */
    int view_x, view_y;                               /* ... and the room pixel at the top left of what it shows */
} PartAnim;

int parts_load(PartAnim *a, const char *path);                   /* 0 ok; on an error says which line on stderr and returns -1 */
int parts_save(const PartAnim *a, const char *path);              /* 0 ok (makes the folder); comments in the old file are not kept */
void parts_set_dir(const char *dir);                              /* the folder of the animation files (the defaults, in the repo) */
void parts_set_mine_dir(const char *dir);                         /* ... and yours (~/.clawd-room/anims): a file there is used instead of the default */
const char *parts_mine_dir(void);
const char *parts_default_dir(void);
const PartAnim *parts_get(const char *name);                       /* anims/NAME.txt (yours first), loaded and read again when it changes; NULL if there is none */
#define PARTS_PATH_NAME 48                                         /* an animation's name as a path under anims/: gates/THEME/gate-door */
int parts_names(char names[][PARTS_PATH_NAME], int max);
void parts_set_gate_theme(const char *theme);                      /* the gate theme (a folder anims/gates/THEME); "" or NULL: simple */
const char *parts_gate_theme(void);
const PartAnim *parts_gate(const char *kind);                      /* kind: gate-door, gate-stairs, gate-well: the theme's file, else the simple theme's; NULL if neither */
void parts_inherit(PartAnim *a, const char *name);                 /* a part the default anims/NAME.txt marks `sprite` is one in yours too (yours may be older than the mark) */                        /* the animations there are: the names of the files in both folders, sorted */
int parts_find(const PartAnim *a, const char *part);              /* the part's index, or -1 */
PartPose parts_pose(const PartAnim *a, int part, double t);       /* where it is at time t seconds (a looping animation wraps t) */
#endif
