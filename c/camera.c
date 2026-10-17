#include "camera.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void camera_init(Camera *c) { memset(c, 0, sizeof *c); }

/* how much a Clawd wants the camera: 3 needs you, 2 just finished, 1 busy, 0 idle or asleep */
static int want(const Session *s, double now) {
    const char *st = session_shown(s, now);
    if (!strcmp(st, "wait")) return 3;
    if (!strcmp(st, "cheer")) return 2;
    if (!strcmp(st, "work") || !strcmp(st, "think")) return 1;
    return 0;
}

/* everybody the camera can follow: the room's own Clawds, then the visitors (a room may have only visitors) */
static int count(const Room *room) { return room->n + room->ng; }
static Session *nth(Room *room, int i) { return i < room->n ? room->s[i] : room->guests[i - room->n]; }

static Session *by_sid(Room *room, const char *sid) {
    for (int i = 0; i < count(room); i++) if (nth(room, i)->has_pos && !strcmp(nth(room, i)->sid, sid)) return nth(room, i);
    return NULL;
}

static void pick(Camera *c, const Session *s, double now) {
    snprintf(c->sid, sizeof c->sid, "%s", s ? s->sid : "");
    c->since = now;
}

/* auto follow: keep the current Clawd unless one wants the camera more; when nothing happens, take turns */
static Session *choose(Camera *c, Room *room, double now) {
    Session *cur = by_sid(room, c->sid), *best = NULL;
    int best_w = -1;
    for (int i = 0; i < count(room); i++) {
        Session *s = nth(room, i);
        if (!s->has_pos) continue;
        int w = want(s, now);
        if (w > best_w || (w == best_w && s->last > best->last)) { best = s; best_w = w; } /* ties: the most recent event */
    }
    if (!best) return NULL;
    int cur_w = cur ? want(cur, now) : -1;
    if (!cur || best_w > cur_w) { pick(c, best, now); return best; }
    if (best_w == 0 && now - c->since > CAM_CYCLE) { /* a quiet room: the next Clawd in seat order */
        int i = 0, n = count(room);
        while (i < n && nth(room, i) != cur) i++;
        for (int k = 1; k <= n; k++) {
            Session *s = nth(room, (i + k) % n);
            if (s->has_pos) { pick(c, s, now); return s; }
        }
    }
    return cur;
}

/* auto zoom: the biggest zoom whose view still holds every busy Clawd (with a margin); 1 when nobody is busy */
static void frame_busy(Camera *c, Room *room, double now, double *tx, double *ty) {
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    int n = 0;
    for (int i = 0; i < count(room); i++) {
        const Session *s = nth(room, i);
        if (!s->has_pos || want(s, now) == 0) continue;
        x0 = fmin(x0, s->px - 7); x1 = fmax(x1, s->px + 7); y0 = fmin(y0, s->py); y1 = fmax(y1, s->py + 8);
        n++;
    }
    int z = 1;
    if (n) {
        double bw = x1 - x0 + 2 * CAM_MARGIN, bh = y1 - y0 + 2 * CAM_MARGIN;
        while (z < CAM_MAX_ZOOM && bw <= room->size_w / (double)(z + 1) && bh <= room->size_h / (double)(z + 1)) z++;
        *tx = (x0 + x1) / 2; *ty = (y0 + y1) / 2;
    }
    int cur = room->cam_zoom > 1 ? room->cam_zoom : 1;
    if (z != cur && now - c->zoom_at >= CAM_ZOOM_EVERY) { room->cam_zoom = z; c->zoom_at = now; }
}

static int heading_out(const Session *s) { return s->has_pos && (s->has_travel || (s->guest && s->returning && s->leaving)); }

/* the Clawd walking out through a gate: keep the one already followed while it is still on its way, else the first one */
static Session *crossing(Camera *c, Room *room) {
    Session *first = NULL;
    for (int i = 0; i < count(room); i++) {
        Session *s = nth(room, i);
        if (!heading_out(s)) continue;
        if (c->cross_sid[0] && !strcmp(s->sid, c->cross_sid)) return s;
        if (!first) first = s;
    }
    return first;
}

void camera_update(Camera *c, Room *room, double now) {
    double tx = -1, ty = -1;
    if (c->autozoom) frame_busy(c, room, now, &tx, &ty);
    if (room->cam_zoom <= 1 || !room->size_w || c->mode == CAM_FREE) { c->cross_sid[0] = 0; return; } /* the whole room is shown, or the user steers */
    Session *out = crossing(c, room); /* to the gate with it; when it has crossed (gone), the whole room again */
    if (!out && c->cross_sid[0]) { c->cross_sid[0] = 0; room->cam_zoom = 1; c->autozoom = 0; return; }
    snprintf(c->cross_sid, sizeof c->cross_sid, "%s", out ? out->sid : "");
    if (out) { tx = out->px; ty = out->py + 4; }
    if (tx < 0) { /* not framing a group: follow one Clawd */
        Session *s = c->mode == CAM_LOCKED ? by_sid(room, c->sid) : NULL;
        if (!s) { c->mode = CAM_AUTO; s = choose(c, room, now); } /* the locked Clawd left: back to auto */
        if (!s) return;
        tx = s->px; ty = s->py + 4;
    }
    if (!room->cam_set) { room->cam_x = tx; room->cam_y = ty; room->cam_set = 1; return; }
    double vw = room->cam_vw ? room->cam_vw : room->size_w / (double)room->cam_zoom, vh = room->cam_vh ? room->cam_vh : room->size_h / (double)room->cam_zoom;
    double dx = tx - room->cam_x, dy = ty - room->cam_y, mx = vw * CAM_DEAD, my = vh * CAM_DEAD;
    /* only the part outside the dead zone pulls, a little each frame */
    if (fabs(dx) > mx) room->cam_x += (dx - copysign(mx, dx)) * CAM_EASE;
    if (fabs(dy) > my) room->cam_y += (dy - copysign(my, dy)) * CAM_EASE;
}

const char *camera_step(Camera *c, Room *room, double now, int dir) {
    Session *cur = by_sid(room, c->sid);
    int n = count(room), i = dir > 0 ? -1 : 0;
    for (int q = 0; q < n; q++) if (nth(room, q) == cur) i = q;
    for (int k = 1; k <= n; k++) {
        Session *s = nth(room, ((i + dir * k) % n + n) % n);
        if (s->has_pos) { pick(c, s, now); c->mode = CAM_LOCKED; return s->name[0] ? s->name : "CLAWD"; }
    }
    return NULL;
}
