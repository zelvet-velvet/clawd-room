/* Where the camera looks when it is zoomed in (draw.c shows the view): it follows a Clawd, or the busy ones.
 * Auto follow picks who: the one that needs you (wait) > the one that just finished (cheer) > the most recently active
 * (work, think) > if nothing happens, the next Clawd every CAM_CYCLE seconds. Visitors count too. While following,
 * left / right (A / D) or Tab pick the previous / next Clawd and stay on it; c switches between following and a free
 * camera (only then do the arrows move the view). Auto zoom (z) also sets the zoom so the busy Clawds fit in the view.
 * A Clawd walking out through a gate (sent next door, visiting, or a visitor going home) takes the camera until it has
 * crossed; then the camera zooms out to the whole room (not in free mode). */
#ifndef CAMERA_H
#define CAMERA_H
#include "room.h"

#define CAM_CYCLE 10.0     /* seconds on one Clawd when nothing happens */
#define CAM_DEAD 0.25      /* the target may move this part of the view from the centre before the camera follows */
#define CAM_EASE 0.18      /* part of the way the camera moves each frame */
#define CAM_ZOOM_EVERY 2.0 /* auto zoom changes at most this often (seconds) */
#define CAM_MARGIN 12      /* room pixels kept around what auto zoom frames */

enum { CAM_AUTO, CAM_LOCKED, CAM_FREE };

typedef struct {
    int mode;               /* CAM_AUTO, CAM_LOCKED or CAM_FREE */
    int autozoom;           /* z: the zoom follows the busy Clawds */
    char sid[72];           /* the Clawd followed (auto or locked), "" if none */
    double since;           /* when that Clawd was picked */
    double zoom_at;         /* when auto zoom last changed the zoom */
    char cross_sid[72];     /* the Clawd followed to a gate, "" if none */
} Camera;

void camera_init(Camera *c);
/* picks the target and moves room->cam_x / cam_y (and cam_zoom with auto zoom); call once a frame before drawing */
void camera_update(Camera *c, Room *room, double now);
/* Tab, right, left: lock on the next (dir 1) or previous (dir -1) Clawd; returns its name (or NULL if the room is empty) */
const char *camera_step(Camera *c, Room *room, double now, int dir);
#endif
