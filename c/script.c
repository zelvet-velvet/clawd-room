#include "script.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "art.h"

typedef struct { double t; char kind[8]; char a[72], b[72], c[72], d[72], e[72]; double y; } Item; /* one timed script line */

static uint64_t fnv(uint64_t h, const char *s) {
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
    return h;
}

/* a digest of everybody's state: positions to 3 decimals, goals, stages, who they chat with, what they do */
static uint64_t digest(Room *r) {
    uint64_t h = 1469598103934665603ULL;
    char buf[400];
    for (int pass = 0; pass < 2; pass++) {
        int n = pass ? r->ngp : r->npool;
        for (int i = 0; i < n; i++) {
            const Session *s = pass ? &r->gpool[i] : &r->pool[i];
            snprintf(buf, sizeof buf, "%s:%s:%s:%s:%s:%s:%d:%d:%d:", s->sid, s->state, s->stage, s->chat_with, s->emote, s->part, s->chat_kind, s->moving, s->face);
            h = fnv(h, buf);
            if (s->has_pos) snprintf(buf, sizeof buf, "%.3f,%.3f:", s->px, s->py); else snprintf(buf, sizeof buf, "-:");
            h = fnv(h, buf);
            if (s->has_goal) snprintf(buf, sizeof buf, "%.3f,%.3f:", s->gx, s->gy); else snprintf(buf, sizeof buf, "-:");
            h = fnv(h, buf);
            snprintf(buf, sizeof buf, "%d:%d:%d:%d:%.3f;", s->has_left, s->has_away, s->has_travel, s->peek_on, s->react_until);
            h = fnv(h, buf);
        }
    }
    snprintf(buf, sizeof buf, "P%d", r->np);
    return fnv(h, buf);
}

int script_run(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 1;
    static Room room;
    room_init(&room);
    int cols = 100, rows = 24, frames = 160, fps = 16, every = 8;
    double start = 1000.0;
    int gif_on = 0, gif_every = 2, gif_scale = 4, gif_crop[4] = {0, 0, 0, 0}, gif_n = 0;
    double gif_t0 = 0, gif_t1 = 0;
    static Rgb *gif_frames[240];
    static Item items[512];
    int ni = 0;
    char line[512];
    while (fgets(line, sizeof line, fp)) {
        char *tok[16];
        int n = 0;
        for (char *p = strtok(line, " \t\r\n"); p && n < 16; p = strtok(NULL, " \t\r\n")) tok[n++] = p;
        if (!n || tok[0][0] == '#') continue;
        if (!strcmp(tok[0], "theme") && n > 1) room.theme = theme_get(tok[1]);
        else if (!strcmp(tok[0], "title") && n > 1) snprintf(room.title, sizeof room.title, "%s", tok[1]);
        else if (!strcmp(tok[0], "terminal") && n > 2) { cols = atoi(tok[1]); rows = atoi(tok[2]); }
        else if (!strcmp(tok[0], "scale") && n > 1) room.scale = atoi(tok[1]);
        else if (!strcmp(tok[0], "run") && n > 3) { frames = atoi(tok[1]); fps = atoi(tok[2]); every = atoi(tok[3]); }
        else if (!strcmp(tok[0], "start") && n > 1) start = atof(tok[1]);
        else if (!strcmp(tok[0], "gif") && n > 3) { /* gif FROM TO EVERY [X Y W H [SCALE]]: with CLAWD_GIF=FILE the frames from FROM to TO s (every EVERY-th) are written as a GIF, cropped (canvas pixels), each pixel SCALE times bigger */
            gif_t0 = atof(tok[1]); gif_t1 = atof(tok[2]); gif_every = atoi(tok[3]);
            if (n > 7) { gif_crop[0] = atoi(tok[4]); gif_crop[1] = atoi(tok[5]); gif_crop[2] = atoi(tok[6]); gif_crop[3] = atoi(tok[7]); }
            if (n > 8) gif_scale = atoi(tok[8]);
            gif_on = getenv("CLAWD_GIF") != NULL;
        }
        else if (!strcmp(tok[0], "neighbor") && n > 2) {
            Neighbor *nb = &room.nb[room.nnb++];
            nb->side = side_index(tok[1]);
            snprintf(nb->theme, sizeof nb->theme, "%s", tok[2]);
            snprintf(nb->id, sizeof nb->id, "id-%s", tok[1]);
        } else if (!strcmp(tok[0], "linkpos") && n > 2) { int s = side_index(tok[1]); room.has_link[s] = 1; room.link_pos[s] = atof(tok[2]); }
        else if (ni < 512 && (!strcmp(tok[0], "ev") || !strcmp(tok[0], "msg") || !strcmp(tok[0], "gone") || !strcmp(tok[0], "key") || !strcmp(tok[0], "emote") || !strcmp(tok[0], "appear")) && n > 1) {
            Item *it = &items[ni++];
            memset(it, 0, sizeof *it);
            snprintf(it->kind, sizeof it->kind, "%s", tok[0]);
            it->t = atof(tok[1]);
            if (n > 2) snprintf(it->a, sizeof it->a, "%s", tok[2]);
            if (n > 3) snprintf(it->b, sizeof it->b, "%s", tok[3]);
            if (n > 4) snprintf(it->c, sizeof it->c, "%s", tok[4]);
            if (n > 5) snprintf(it->d, sizeof it->d, "%s", tok[5]);
            if (n > 6) snprintf(it->e, sizeof it->e, "%s", tok[6]);
            if (n > 7) it->y = atof(tok[7]);
        }
    }
    fclose(fp);
    int next = 0;
    for (int f = 0; f < frames; f++) {
        double t = (double)f / fps, now = start + t;
        for (; next < ni && items[next].t <= t; next++) {
            Item *it = &items[next];
            if (!strcmp(it->kind, "ev")) room_apply(&room, start + it->t, it->a, it->b, !strcmp(it->c, "-") ? "" : it->c, !strcmp(it->d, "-") ? "" : it->d);
            else if (!strcmp(it->kind, "msg")) { /* msg T from walk|recall sid name side home y */
                Msg m;
                memset(&m, 0, sizeof m);
                snprintf(m.peer, sizeof m.peer, "%s", it->a);
                m.walk = !strcmp(it->b, "walk");
                snprintf(m.sid, sizeof m.sid, "%s", it->c);
                snprintf(m.name, sizeof m.name, "%s", !strcmp(it->d, "-") ? "" : it->d);
                m.side = side_index(it->e);
                m.home = it->y > 0;
                room_receive(&room, &m);
            } else if (!strcmp(it->kind, "appear") && room.nnb < SIDES) { /* appear T side theme: a window comes next to this one, its gate appears */
                Neighbor *nb = &room.nb[room.nnb++];
                nb->side = side_index(it->a);
                snprintf(nb->theme, sizeof nb->theme, "%s", it->b);
                snprintf(nb->id, sizeof nb->id, "id-%s", it->a);
            } else if (!strcmp(it->kind, "emote")) room_emote(&room, it->a, it->b, start + it->t, it->c[0] ? atof(it->c) : 6.0); /* emote T sid kind [seconds] */
            else if (!strcmp(it->kind, "gone")) room_peer_gone(&room, it->a);
            else if (!strcmp(it->kind, "key") && !strcmp(it->a, "m")) { /* the m key (key T m SID SIDE): that Clawd moves to the room on that side */
                char buf[64];
                room_order(&room);
                printf("key %d %s\n", f, room_move_to(&room, &room.geo, now, it->b, side_index(it->c), buf, sizeof buf));
            } else if (!strcmp(it->kind, "key")) { /* the g key (key T g [SIDE]): needs the geometry of the last frame */
                char buf[64];
                room_order(&room);
                printf("key %d %s\n", f, it->b[0] ? room_send_to(&room, &room.geo, now, side_index(it->b), buf, sizeof buf) : room_send_one(&room, &room.geo, now, buf, sizeof buf)); /* (key T g SIDE: picked from the list) */
            }
        }
        room_prune(&room, now);
        char note[32];
        Canvas cv = render(cols, rows, &room, now, t, note, sizeof note);
        for (int i = 0; i < room.nout; i++) {
            const Msg *m = &room.outbox[i];
            if (m->walk) printf("out %d %s walk %s %s %s %d %d\n", f, m->peer, m->sid, m->name[0] ? m->name : "-", SIDE_NAME[m->side], m->home, m->has_y ? (int)m->y : -1);
            else printf("out %d %s recall %s\n", f, m->peer, m->sid);
        }
        room.nout = 0;
        if (getenv("CLAWD_DUMP") && f == atoi(getenv("CLAWD_DUMP")))
            for (int pass = 0; pass < 2; pass++) for (int i = 0; i < (pass ? room.ngp : room.npool); i++) {
                const Session *s = pass ? &room.gpool[i] : &room.pool[i];
                printf("  %s state=%s stage=%s chat=%s emote=%s part=%s kind=%d moving=%d face=%d pos=%d %.3f,%.3f goal=%d %.3f,%.3f left=%d away=%d travel=%d peek=%d react=%.3f\n", s->sid, s->state, s->stage, s->chat_with, s->emote, s->part, s->chat_kind, s->moving, s->face, s->has_pos, s->px, s->py, s->has_goal, s->gx, s->gy, s->has_left, s->has_away, s->has_travel, s->peek_on, s->react_until);
            }
        if (getenv("CLAWD_PNG_DIR") && f % every == 0) { char pp[700]; snprintf(pp, sizeof pp, "%s/f%03d.png", getenv("CLAWD_PNG_DIR"), f); png_write(pp, &cv, 4); } /* (a picture of each printed frame, to look at) */
        if (gif_on && t >= gif_t0 && t <= gif_t1 && f % gif_every == 0 && gif_n < 240) { /* (a frame of the GIF: cropped, scaled) */
            int cx = gif_crop[2] > 0 ? gif_crop[0] : 0, cy = gif_crop[2] > 0 ? gif_crop[1] : 0, cw = gif_crop[2] > 0 ? gif_crop[2] : cv.w, chh = gif_crop[2] > 0 ? gif_crop[3] : cv.h;
            if (cx + cw > cv.w) cw = cv.w - cx;
            if (cy + chh > cv.h) chh = cv.h - cy;
            Rgb *fr = malloc((size_t)(cw * gif_scale) * (size_t)(chh * gif_scale) * sizeof(Rgb));
            for (int y = 0; y < chh * gif_scale; y++) for (int x = 0; x < cw * gif_scale; x++) fr[y * cw * gif_scale + x] = cv.p[(cy + y / gif_scale) * cv.w + cx + x / gif_scale];
            gif_frames[gif_n++] = fr;
            gif_crop[2] = cw; gif_crop[3] = chh; gif_crop[0] = cx; gif_crop[1] = cy;
        }
        if (f % every == 0) printf("f %d %016" PRIx64 " %016" PRIx64 " %s\n", f, canvas_hash(&cv), digest(&room), note);
        canvas_free(&cv);
    }
    if (gif_on && gif_n) {
        int rc = gif_write_fit(getenv("CLAWD_GIF"), gif_frames, gif_n, gif_crop[2] * gif_scale, gif_crop[3] * gif_scale, (int)(100.0 * gif_every / fps + 0.5));
        printf("gif %s %d frames %s\n", getenv("CLAWD_GIF"), gif_n, rc ? "FAILED" : "ok");
        for (int i = 0; i < gif_n; i++) free(gif_frames[i]);
    }
    return 0;
}
