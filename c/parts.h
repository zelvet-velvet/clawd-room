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

typedef struct { double x, y, turn, glow; int show; } PartPose; /* offsets from the part's rest place in room pixels, degrees, 0..100, 0 or 1 */
typedef struct { double t; PartPose pose; int ease; } PartKey;   /* ease: how the part gets here from the key before */
typedef struct { char name[24], group[24]; PartKey key[PARTS_MAX_KEYS]; int nkey; } Part;
typedef struct { char name[24]; double length; int loop; Part part[PARTS_MAX]; int npart; } PartAnim;

int parts_load(PartAnim *a, const char *path);                   /* 0 ok; on an error says which line on stderr and returns -1 */
int parts_find(const PartAnim *a, const char *part);              /* the part's index, or -1 */
PartPose parts_pose(const PartAnim *a, int part, double t);       /* where it is at time t seconds (a looping animation wraps t) */
#endif
