/* The scene: everything drawn in the room as a list of items, each on a layer, drawn back to front (Engine Design, step 1).
 * Items keep their drawing code (a callback); the scene decides the order, can measure what each item covers (its box)
 * and which items paint over each other (for the overlap check and the debug overlay). */
#ifndef SCENE_H
#define SCENE_H
#include "canvas.h"

enum { /* the layers, drawn in this order */
    LAYER_SKY,      /* sky, ceiling, walls, floor (the cached background) */
    LAYER_WALL,     /* things on the back wall */
    LAYER_STRUCT,   /* stairs, platform, doors, the park's buildings, the well; the room's sign */
    LAYER_FLOOR,    /* marks flat on the floor */
    LAYER_DEPTH,    /* desks, props, Clawds, the railing: by how far forward they stand */
    LAYER_PARTICLE, /* sparks and hearts */
    LAYER_HUD,      /* words over the room */
    N_LAYERS
};

typedef struct SceneItem SceneItem;
typedef void (*ItemDraw)(Canvas *cv, const SceneItem *it, void *ctx);

struct SceneItem {
    const char *kind;  /* "background", "gate", "sign", "desk", "clawd", "rail", "marks", ... */
    int layer;
    double depth;      /* LAYER_DEPTH: how far forward it stands (feet or front edge, room pixels); ties keep the order added */
    int owner;         /* -1, or the item this one belongs to: drawn right after it (a Clawd's dot, its swing ropes) */
    double ax, ay;     /* the anchor (room pixels): where it stands */
    ItemDraw draw;
    int i;             /* for the draw callback: which desk / person / side */
    int j;             /* ... a second one (which part of a gate) */
    double p;          /* for the draw callback: a parameter (a gate's appear animation) */
    const char *part;  /* a gate part's name in the theme's gate file (anims/gates/THEME): its x / y / glow / show are applied when it is drawn */
    /* filled by scene_draw when measuring */
    int seq, has_box, bx0, by0, bx1, by1; /* what it painted: [bx0, bx1) x [by0, by1) */
};

#define SCENE_MAX 1200
#define SCENE_MAX_HITS 256
typedef struct { int a, b, n; } SceneHit; /* item b painted n pixels over item a */

typedef struct {
    SceneItem it[SCENE_MAX];
    int n;
    int order[SCENE_MAX]; /* the drawing order (after scene_draw) */
    int measure;          /* measure boxes and overlaps while drawing (the overlay, the check) */
    SceneHit hit[SCENE_MAX_HITS];
    int nhit;
} Scene;

void scene_clear(Scene *s);
int scene_add(Scene *s, SceneItem it); /* returns the item's index (-1 when full) */
void scene_draw(Scene *s, Canvas *cv, void *ctx);
const char *scene_layer_name(int layer);
#endif
