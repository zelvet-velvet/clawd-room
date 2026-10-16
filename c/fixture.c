#include "fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int side_of(const char *s) { return !strcmp(s, "left") ? SIDE_LEFT : !strcmp(s, "right") ? SIDE_RIGHT : !strcmp(s, "up") ? SIDE_UP : SIDE_DOWN; }

static void set_field(Session *s, const char *kv) {
    char k[32], v[64];
    if (sscanf(kv, "%31[^=]=%63s", k, v) != 2) return;
    if (!strcmp(k, "name")) snprintf(s->name, sizeof s->name, "%s", v);
    else if (!strcmp(k, "state")) snprintf(s->state, sizeof s->state, "%s", v);
    else if (!strcmp(k, "tool")) snprintf(s->tool, sizeof s->tool, "%s", v);
    else if (!strcmp(k, "px")) s->px = atof(v);
    else if (!strcmp(k, "py")) s->py = atof(v);
    else if (!strcmp(k, "created")) { s->created = atof(v); s->last = s->state_t = s->created; }
    else if (!strcmp(k, "last")) s->last = atof(v);
    else if (!strcmp(k, "state_t")) s->state_t = atof(v);
    else if (!strcmp(k, "hold")) s->hold_work_until = atof(v);
    else if (!strcmp(k, "moving")) s->moving = atoi(v);
    else if (!strcmp(k, "vclimb")) s->vclimb = atoi(v);
    else if (!strcmp(k, "face")) s->face = atoi(v);
    else if (!strcmp(k, "stage")) snprintf(s->stage, sizeof s->stage, "%s", v);
    else if (!strcmp(k, "chat_with")) snprintf(s->chat_with, sizeof s->chat_with, "%s", v);
    else if (!strcmp(k, "chat_kind")) s->chat_kind = atoi(v);
    else if (!strcmp(k, "part")) snprintf(s->part, sizeof s->part, "%s", v);
    else if (!strcmp(k, "emote")) { char kind[12]; double st; if (sscanf(v, "%11[^@]@%lf", kind, &st) == 2) { snprintf(s->emote, sizeof s->emote, "%s", kind); s->emote_start = st; } }
    else if (!strcmp(k, "peek")) s->peeking = atoi(v);
    else if (!strcmp(k, "react")) s->react_until = atof(v);
    else if (!strcmp(k, "cup")) s->cup_until = atof(v);
    else if (!strcmp(k, "left_at")) { s->left_at = atof(v); s->has_left = 1; }
    else if (!strcmp(k, "seed")) s->seed = atof(v);
    else if (!strcmp(k, "away")) s->away = atoi(v);
    else if (!strcmp(k, "walk_phase")) s->walk_phase = atof(v);
}

static int by_created(const void *a, const void *b) {
    const Session *x = a, *y = b;
    if (x->created != y->created) return x->created < y->created ? -1 : 1;
    return strcmp(x->sid, y->sid);
}

int fixture_load(Fixture *f, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 1;
    memset(f, 0, sizeof *f);
    Room *r = &f->room;
    r->theme = theme_get("office");
    snprintf(r->ratio, sizeof r->ratio, "auto");
    r->scale = 1;
    f->cols = 100; f->rows = 24; f->now = 1000.0; f->t = 0.0;
    char line[512];
    int ng = 0, ns = 0;
    while (fgets(line, sizeof line, fp)) {
        char *tok[40];
        int n = 0;
        for (char *p = strtok(line, " \t\r\n"); p && n < 40; p = strtok(NULL, " \t\r\n")) tok[n++] = p;
        if (!n || tok[0][0] == '#') continue;
        if (!strcmp(tok[0], "theme") && n > 1) r->theme = theme_get(tok[1]);
        else if (!strcmp(tok[0], "title") && n > 1) snprintf(r->title, sizeof r->title, "%s", tok[1]);
        else if (!strcmp(tok[0], "terminal") && n > 2) { f->cols = atoi(tok[1]); f->rows = atoi(tok[2]); }
        else if (!strcmp(tok[0], "time") && n > 2) { f->now = atof(tok[1]); f->t = atof(tok[2]); }
        else if (!strcmp(tok[0], "scale") && n > 1) r->scale = atoi(tok[1]);
        else if (!strcmp(tok[0], "ratio") && n > 1) snprintf(r->ratio, sizeof r->ratio, "%s", tok[1]);
        else if (!strcmp(tok[0], "peer") && n > 2 && r->npeers < 8) {
            snprintf(r->peers[r->npeers].name, sizeof r->peers[0].name, "%s", tok[1]);
            snprintf(r->peers[r->npeers].side, sizeof r->peers[0].side, "%s", !strcmp(tok[2], "-") ? "" : tok[2]);
            r->npeers++;
        } else if (!strcmp(tok[0], "neighbor") && n > 2) {
            r->nb[r->nnb].side = side_of(tok[1]);
            snprintf(r->nb[r->nnb].theme, sizeof r->nb[0].theme, "%s", tok[2]);
            r->nnb++;
        } else if (!strcmp(tok[0], "linkpos") && n > 2) { r->has_link[side_of(tok[1])] = 1; r->link_pos[side_of(tok[1])] = atof(tok[2]); }
        else if (!strcmp(tok[0], "gate") && n > 2) { r->has_gate[side_of(tok[1])] = 1; r->gate_scale[side_of(tok[1])] = atof(tok[2]); }
        else if (!strcmp(tok[0], "particle") && n > 9 && r->np < 64) {
            Particle *q = &f->parts[r->np++];
            *q = (Particle){atof(tok[1]), atof(tok[2]), atof(tok[3]), atof(tok[4]), atof(tok[5]), RGB(atoi(tok[6]), atoi(tok[7]), atoi(tok[8])), atoi(tok[9])};
        } else if ((!strcmp(tok[0], "session") || !strcmp(tok[0], "guest")) && n > 1) {
            int guest = !strcmp(tok[0], "guest");
            Session *s = guest ? &f->g[ng++] : &f->s[ns++];
            session_init(s, tok[1]);
            s->guest = guest;
            for (int i = 2; i < n; i++) set_field(s, tok[i]);
        }
    }
    fclose(fp);
    qsort(f->s, (size_t)ns, sizeof(Session), by_created);
    r->s = f->s; r->n = ns; r->guests = f->g; r->ng = ng; r->parts = f->parts;
    return 0;
}
