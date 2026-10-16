#include "script.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        else if (!strcmp(tok[0], "neighbor") && n > 2) {
            Neighbor *nb = &room.nb[room.nnb++];
            nb->side = side_index(tok[1]);
            snprintf(nb->theme, sizeof nb->theme, "%s", tok[2]);
            snprintf(nb->id, sizeof nb->id, "id-%s", tok[1]);
        } else if (!strcmp(tok[0], "linkpos") && n > 2) { int s = side_index(tok[1]); room.has_link[s] = 1; room.link_pos[s] = atof(tok[2]); }
        else if (ni < 512 && (!strcmp(tok[0], "ev") || !strcmp(tok[0], "msg") || !strcmp(tok[0], "gone") || !strcmp(tok[0], "key")) && n > 1) {
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
            } else if (!strcmp(it->kind, "gone")) room_peer_gone(&room, it->a);
            else if (!strcmp(it->kind, "key")) { /* the g key: needs the geometry of the last frame */
                Geo geo;
                char buf[64];
                room_order(&room);
                geo_init(&geo, &room, room.size_w, room.size_h);
                printf("key %d %s\n", f, room_send_one(&room, &geo, now, buf, sizeof buf));
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
        if (f % every == 0) printf("f %d %016" PRIx64 " %016" PRIx64 " %s\n", f, canvas_hash(&cv), digest(&room), note);
        canvas_free(&cv);
    }
    return 0;
}
