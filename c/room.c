/* The room's state machine (port of Room in room_tui.py): events, who sits where, visits, tag, solo acts,
 * walking between windows. The random calls are made in the same order as in Python (see rng.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "room.h"

#define HOLD_WORK 0.8
#define STALE_AFTER 1800.0
#define LEAVE_MAX 12.0
#define ARRIVE_WALK 3.0
#define AWAY_MAX 150.0
#define MOVE_WAIT 10.0 /* seconds a Clawd that moved out waits for the other room to claim it, then comes back */
#define WALK_SPEED 26.0
#define LEAVE_SPEED 50.0
#define MAX_GUESTS_SHOWN 4

#define PINK RGB(220, 120, 230) /* (room_tui.py defines PINK twice: this is the one that wins) */
#define BLUE RGB(110, 200, 240)

const char *SIDE_NAME[SIDES] = {"left", "right", "up", "down"};
int side_index(const char *name) {
    for (int i = 0; i < SIDES; i++) if (!strcmp(SIDE_NAME[i], name)) return i;
    return -1;
}

static const int ALPHA[SIDES] = {SIDE_DOWN, SIDE_LEFT, SIDE_RIGHT, SIDE_UP}; /* sorted(geo.neighbors) */

void room_init(Room *r) {
    memset(r, 0, sizeof *r);
    r->theme = theme_get("office");
    snprintf(r->ratio, sizeof r->ratio, "auto");
    r->scale = 1;
    r->view_k = 1;
}

static int by_created(const void *a, const void *b) {
    const Session *x = *(Session *const *)a, *y = *(Session *const *)b;
    if (x->created != y->created) return x->created < y->created ? -1 : 1;
    return strcmp(x->sid, y->sid);
}

static int is_mine(const Room *r, const char *sid) { for (int i = 0; i < r->nmine; i++) if (!strcmp(r->mine[i], sid)) return 1; return 0; }
static void add_mine(Room *r, const char *sid) { if (!is_mine(r, sid) && r->nmine < MAX_CLAIMS) snprintf(r->mine[r->nmine++], 72, "%s", sid); }
static void drop_mine(Room *r, const char *sid) {
    for (int i = 0; i < r->nmine; i++) if (!strcmp(r->mine[i], sid)) { memmove(r->mine[i], r->mine[i + 1], 72 * (size_t)(r->nmine - i - 1)); r->nmine--; return; }
}
static const char *claimed_by(const Room *r, const char *sid) { /* the other room it moved into, if that room runs */
    for (int i = 0; i < r->nclaim; i++)
        if (!strcmp(r->claim_sid[i], sid)) for (int k = 0; k < r->n_owner_ids; k++) if (!strcmp(r->owner_ids[k], r->claim_room[i])) return r->claim_room[i];
    return NULL;
}

/* which sessions this room shows (when several rooms share them): one moved here (m key) or on its way out; one moved
 * to another room that runs is not ours; the rest by their crc32 */
static int owns(const Room *r, const Session *s) {
    if (!r->split || r->n_owner_ids < 1) return 1;
    if (s->move && (s->has_travel || s->has_away)) return 1;
    if (is_mine(r, s->sid)) return 1;
    if (claimed_by(r, s->sid)) return 0;
    return !strcmp(r->owner_ids[crc32_str(s->sid) % (unsigned)r->n_owner_ids], r->owner_me);
}

void room_order(Room *r) {
    r->n = 0;
    for (int i = 0; i < r->npool; i++) {
        Session *s = &r->pool[i];
        if (owns(r, s)) r->s[r->n++] = s;
        else if (s->has_away && !s->move) { s->has_away = 0; s->pack = 0; s->has_pos = 0; } /* (moved away for good: if it comes back, at its desk) */
    }
    qsort(r->s, (size_t)r->n, sizeof(Session *), by_created);
    r->ng = 0;
    for (int i = 0; i < r->ngp; i++) if (r->gpool[i].has_pos) r->guests[r->ng++] = &r->gpool[i];
}

static Session *find_session(Room *r, const char *sid) {
    for (int i = 0; i < r->npool; i++) if (!strcmp(r->pool[i].sid, sid)) return &r->pool[i];
    return NULL;
}

static Session *find_any(Room *r, const char *sid) {
    Session *s = find_session(r, sid);
    if (s) return s;
    for (int i = 0; i < r->ngp; i++) if (!strcmp(r->gpool[i].sid, sid)) return &r->gpool[i];
    return NULL;
}

/* ---- events ---------------------------------------------------------------------------------- */
static void set_state(Session *s, const char *state, double t) { snprintf(s->state, sizeof s->state, "%s", state); s->state_t = t; }

void room_apply(Room *r, double t, const char *sid, const char *ev, const char *cwd, const char *tool) {
    Session *s = find_session(r, sid);
    if (!strcmp(ev, "SessionEnd")) {
        if (s && !s->has_left) { s->left_at = t; s->has_left = 1; }
        return;
    }
    if (!s) {
        if (r->npool >= MAX_SESS) return;
        s = &r->pool[r->npool++];
        session_init(s, sid, t);
    } else if (s->has_left) s->has_left = 0; /* the id came back (resume) */
    s->last = t;
    if (cwd && cwd[0]) snprintf(s->name, sizeof s->name, "%s", cwd);
    if (!strcmp(ev, "SessionStart")) set_state(s, "idle", t);
    else if (!strcmp(ev, "UserPromptSubmit")) set_state(s, "think", t);
    else if (!strcmp(ev, "PreToolUse")) {
        snprintf(s->tool, sizeof s->tool, "%s", tool ? tool : "");
        set_state(s, "work", t);
        s->hold_work_until = t + HOLD_WORK;
    } else if (!strcmp(ev, "PostToolUse")) {
        set_state(s, "think", t);
        if (t + HOLD_WORK > s->hold_work_until) s->hold_work_until = t + HOLD_WORK;
    } else if (!strcmp(ev, "Notification")) set_state(s, "wait", t);
    else if (!strcmp(ev, "Stop")) set_state(s, "cheer", t);
}

void room_prune(Room *r, double now) {
    for (int i = 0; i < r->npool;) {
        Session *s = &r->pool[i];
        if (s->has_left && (s->gone || now - s->left_at > LEAVE_MAX)) {
            memmove(s, s + 1, sizeof(Session) * (size_t)(r->npool - i - 1));
            r->npool--;
            continue;
        }
        if (now - s->last > STALE_AFTER && !s->has_left) { s->left_at = now; s->has_left = 1; } /* walk out, then get removed */
        i++;
    }
}

/* ---- geometry helpers ------------------------------------------------------------------------ */
int geo_home(const Geo *g, const Room *room, const Session *s, int *cx, int *by) {
    for (int i = 0; i < g->shown; i++)
        if (room->s[i] == s) { if (cx) *cx = g->home[i][0]; if (by) *by = g->home[i][1]; return 1; }
    return 0;
}

const char *geo_role(const Geo *g, const Room *room, const Session *s) {
    (void)g;
    for (int i = 0; i < room->n; i++) if (room->s[i] == s) return theme_role(room->theme, i);
    return "";
}

static int has_nb(const Room *r, int side) { for (int i = 0; i < r->nnb; i++) if (r->nb[i].side == side) return 1; return 0; }
static const char *nb_id(const Room *r, int side) { for (int i = 0; i < r->nnb; i++) if (r->nb[i].side == side) return r->nb[i].id; return ""; }

static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- small helpers on a Clawd ---------------------------------------------------------------- */
static void drop_social(Session *s) {
    s->chat_with[0] = 0;
    s->peek_on = s->peek_has_end = 0;
    s->emote[0] = 0;
    s->has_travel = 0; s->move = 0;
    s->has_then = 0;
    s->nroute = 0;
    s->vclimb = 0;
    s->stage[0] = 0;
    s->run = 1.0;
}

static void set_goal(Session *s, double x, double y) { s->has_goal = 1; s->gx = x; s->gy = y; }
/* a goal on the walking floor: x kept inside the floor's width on that row (the side walls slant in towards the back) */
static void set_floor_goal(Session *s, const Geo *geo, double x, double y) {
    double xl, xr;
    view_floor_span(&geo->view, y + 8, &xl, &xr);
    set_goal(s, fmax(xl + 7, fmin(xr - 7, x)), y);
}

/* coming in by a gate: appear where it goes through (the door, the top of the stairs, the stair tower, down the well) and
 * walk / climb to where it steps off it */
static void enter(Session *s, int side, const Geo *geo) {
    s->nroute = 0; s->has_goal = 0;
    s->has_pos = 1; s->px = geo->gate_out[side][0]; s->py = geo->gate_out[side][1] - 8;
    if (geo->gate_has_mid[side]) { /* along the platform, then down the stairs */
        set_goal(s, geo->gate_mid[side][0], geo->gate_mid[side][1] - 8);
        s->nroute = 1; s->rx[0] = geo->gate_in[side][0]; s->ry[0] = geo->gate_in[side][1] - 8;
    } else set_goal(s, geo->gate_in[side][0], geo->gate_in[side][1] - 8);
    s->vclimb = fabs(s->gx - s->px) < 1; /* climbing pose only straight down a ladder; along the platform and down stairs it walks */
}

/* going out by a gate: walk to where it steps on, then on to where it goes through; when that is reached it has crossed */
static void exit_route(Session *s, int edge, const Geo *geo) {
    s->leaving = 1;
    set_goal(s, geo->gate_in[edge][0], geo->gate_in[edge][1] - 8);
    s->nroute = 0;
    if (geo->gate_has_mid[edge]) { s->rx[s->nroute] = geo->gate_mid[edge][0]; s->ry[s->nroute++] = geo->gate_mid[edge][1] - 8; }
    s->rx[s->nroute] = geo->gate_out[edge][0]; s->ry[s->nroute++] = geo->gate_out[edge][1] - 8;
}

/* crossed: off the picture, or a Clawd on its way out has come to the end of its gate route */
static int outside(const Session *s, const Geo *geo) {
    if (s->leaving && !s->has_goal && !s->nroute) return 1;
    return !(-6 < s->px && s->px < geo->w + 6 && -9 < s->py && s->py < geo->h + 1);
}

static int frac_of(const Session *s, const Geo *geo) {
    double d = (double)(geo->ymax - geo->ymin > 1 ? geo->ymax - geo->ymin : 1);
    return (int)(1000 * fmin(1.0, fmax(0.0, (s->py - geo->ymin) / d)));
}

static void push_out(Room *r, const char *peer, int walk, const char *sid, const char *name, int side, int home, int has_y, double y) {
    if (r->nout >= MAX_OUT) return;
    Msg *m = &r->outbox[r->nout++];
    memset(m, 0, sizeof *m);
    snprintf(m->peer, sizeof m->peer, "%s", peer);
    m->walk = walk; snprintf(m->sid, sizeof m->sid, "%s", sid); snprintf(m->name, sizeof m->name, "%s", name ? name : "");
    m->side = side; m->home = home; m->has_y = has_y; m->y = y;
}

static void poof(Room *r, double x, double y) { /* a ring of little white clouds (packing and unpacking) */
    for (int i = 0; i < 10 && r->np < MAX_PARTS; i++) {
        Particle *q = &r->parts[r->np++];
        double a = i * 2 * M_PI / 10, v = 0.1 + 0.04 * (i % 3);
        q->x = x + cos(a) * 4 - 1; q->y = y - 3 + sin(a) * 2; q->vx = cos(a) * v; q->vy = sin(a) * v * 0.5 - 0.06;
        q->life = 1.0; q->c = i % 2 ? C_WHITE : RGB(200, 200, 214); q->heart = 2;
    }
}

static void sparks(Room *r, Rng *rng, double x, double y, int n) {
    for (int i = 0; i < n && r->np < MAX_PARTS; i++) {
        Particle *q = &r->parts[r->np++];
        q->x = x + rng_uniform(rng, -3, 3); q->y = y; q->vx = rng_uniform(rng, -0.4, 0.4); q->vy = -rng_uniform(rng, 0.3, 1.1);
        q->life = 1.0; q->c = (i % 2) ? C_WHITE : C_YELLOW; q->heart = 0;
    }
}

static void come_home(Room *r, Session *s, const Geo *geo, double now, int side) {
    (void)r;
    if (s->move) s->pack = 3; /* a move nobody took: back with the box, unpacks at its desk again */
    s->has_away = 0; s->move = 0;
    enter(s, side, geo);
    s->next_plan = now + rng_uniform(&s->rng, 1.0, 3.0);
}

/* ---- crossing between windows ---------------------------------------------------------------- */
static void remove_guest(Room *r, Session *g) {
    int i = (int)(g - r->gpool);
    memmove(g, g + 1, sizeof(Session) * (size_t)(r->ngp - i - 1));
    r->ngp--;
}

Session *room_add_guest(Room *r, const char *key, double now) {
    if (r->ngp >= MAX_GUESTS) return NULL;
    Session *g = &r->gpool[r->ngp++];
    session_init(g, key, now);
    g->guest = 1;
    return g;
}

void room_receive(Room *r, const Msg *m) { if (r->nin < MAX_OUT) r->incoming[r->nin++] = *m; }
void room_peer_gone(Room *r, const char *peer) { if (r->ngone < 4) snprintf(r->gone_peers[r->ngone++], 24, "%s", peer); }

static void handle_messages(Room *r, const Geo *geo, double now) {
    for (int i = 0; i < r->nin; i++) {
        Msg *m = &r->incoming[i];
        double y = m->has_y ? m->y : 500;
        (void)y; /* the vertical fraction is not used for placing (doors line up by themselves) */
        if (m->walk && m->side >= 0 && m->sid[0]) {
            if (m->move) { /* moving in: it lives here now (the other room stops showing it when it sees our claim) */
                Session *s = find_session(r, m->sid);
                if (!s) continue; /* (not a session this room knows yet) */
                add_mine(r, m->sid);
                drop_social(s);
                s->has_away = 0; s->leaving = 0; s->returning = 0; s->moving = 0;
                if (s->name[0] == 0) snprintf(s->name, sizeof s->name, "%.32s", m->name);
                enter(s, m->side, geo);
                s->pack = 3; /* with its box */
                s->next_plan = now + rng_uniform(&s->rng, 1.0, 3.0);
            } else if (m->home) { /* one of ours coming back */
                Session *s = find_session(r, m->sid);
                if (s && s->has_away && !strcmp(s->away_peer, m->peer)) come_home(r, s, geo, now, m->side);
            } else { /* a visitor */
                char key[100];
                snprintf(key, sizeof key, "~%s~%s", m->peer, m->sid);
                int dup = 0;
                for (int q = 0; q < r->ngp; q++) if (!strcmp(r->gpool[q].sid, key)) dup = 1;
                if (dup || r->ngp >= MAX_GUESTS_SHOWN) continue;
                Session *g = room_add_guest(r, key, now);
                snprintf(g->origin, sizeof g->origin, "%s", m->peer);
                snprintf(g->origin_sid, sizeof g->origin_sid, "%s", m->sid);
                g->enter_side = m->side;
                snprintf(g->name, sizeof g->name, "%.32s", m->name);
                g->stay_until = now + rng_uniform(&g->rng, 25.0, 45.0);
                g->next_plan = now + rng_uniform(&g->rng, 1.0, 3.0);
                enter(g, m->side, geo);
            }
        } else if (!m->walk) { /* recall */
            char key[100];
            snprintf(key, sizeof key, "~%s~%s", m->peer, m->sid);
            for (int q = 0; q < r->ngp; q++) if (!strcmp(r->gpool[q].sid, key)) r->gpool[q].returning = 1;
        }
    }
    r->nin = 0;
    for (int p = 0; p < r->ngone; p++) { /* a window closed: its Clawds are brought home, its visitors vanish */
        for (int i = 0; i < r->npool; i++)
            if (r->pool[i].has_away && !strcmp(r->pool[i].away_peer, r->gone_peers[p])) come_home(r, &r->pool[i], geo, now, r->pool[i].away_side);
        for (int q = 0; q < r->ngp;) {
            if (!strcmp(r->gpool[q].origin, r->gone_peers[p])) remove_guest(r, &r->gpool[q]);
            else q++;
        }
    }
    r->ngone = 0;
}

static void steer_away(Room *r, Session *s, const Geo *geo, double now) {
    if (s->move) { /* moved out: done when that room claims it, back home if it does not answer */
        if (s->has_left) { s->gone = 1; return; }
        const char *by = claimed_by(r, s->sid);
        if (by && !strcmp(by, s->away_peer)) { s->move = 0; drop_mine(r, s->sid); } /* (still away and packed this frame: room_order lets it go next frame, no blink) */
        else if (now - s->away_since > MOVE_WAIT) { s->move = 0; come_home(r, s, geo, now, s->away_side); }
        return;
    }
    const char *state = session_shown(s, now);
    if ((!strcmp(state, "work") || !strcmp(state, "think") || s->has_left) && !s->away_recalled) {
        s->away_recalled = 1;
        push_out(r, s->away_peer, 0, s->sid, NULL, 0, 0, 0, 0);
    }
    if (s->has_left) s->gone = 1;
    else if (now - s->away_since > AWAY_MAX) come_home(r, s, geo, now, s->away_side); /* nobody answered: come home anyway */
}

static void cross(Room *r, Session *s, const Geo *geo, double now) {
    if (!outside(s, geo)) return;
    if (s->guest) {
        if (s->returning) { /* back through the door it came in by */
            push_out(r, s->origin, 1, s->origin_sid, "", OPPOSITE(s->enter_side), 1, 1, frac_of(s, geo));
            remove_guest(r, s);
        }
    } else if (s->has_travel && !s->has_left) {
        int edge = s->travel_edge, mv = s->move;
        char peer[24];
        snprintf(peer, sizeof peer, "%s", s->travel_peer);
        push_out(r, peer, 1, s->sid, s->name, OPPOSITE(edge), 0, 1, frac_of(s, geo)); /* it enters by the opposite edge there */
        if (mv && r->nout) r->outbox[r->nout - 1].move = 1;
        drop_social(s);
        s->move = mv; /* (a move waits for that room to say it lives there now: steer_away) */
        s->has_travel = 0; s->has_goal = 0; s->moving = 0; s->nroute = 0; s->leaving = 0;
        s->has_away = 1; snprintf(s->away_peer, sizeof s->away_peer, "%s", peer); s->away_side = edge; s->away_since = now; s->away_recalled = 0;
    }
}

/* the b key: call our Clawds that are next door back home (the visit ends, they walk back through the door);
 * with none away, the visitors in this room are sent home instead */
const char *room_recall(Room *r, double now, char *buf, size_t n) {
    (void)now;
    int called = 0, sent = 0;
    for (int i = 0; i < r->npool; i++) {
        Session *s = &r->pool[i];
        if (!s->has_away || s->away_recalled) continue;
        s->away_recalled = 1;
        push_out(r, s->away_peer, 0, s->sid, NULL, 0, 0, 0, 0);
        called++;
    }
    if (!called) for (int q = 0; q < r->ngp; q++) if (!r->gpool[q].returning) { r->gpool[q].returning = 1; sent++; }
    if (called) snprintf(buf, n, "CALLING %d CLAWD%s HOME", called, called == 1 ? "" : "S");
    else if (sent) snprintf(buf, n, "SENDING %d VISITOR%s HOME", sent, sent == 1 ? "" : "S");
    else snprintf(buf, n, "NOBODY AWAY, NO VISITORS");
    return buf;
}

const char *room_send_one(Room *r, const Geo *geo, double now, char *buf, size_t n) { return room_send_to(r, geo, now, -1, buf, n); }

/* the g key: send an idle Clawd through the gate on `side` (-1: the first one) */
const char *room_send_to(Room *r, const Geo *geo, double now, int side, char *buf, size_t n) {
    int any = 0;
    for (int i = 0; i < SIDES; i++) any |= has_nb(r, i);
    if (!any) return "NO WINDOW NEXT TO THIS ONE";
    if (side >= 0 && !has_nb(r, side)) return "NO WINDOW THAT WAY";
    for (int i = 0; i < r->n; i++) {
        Session *s = r->s[i];
        if (s->has_away || s->has_travel || !s->has_pos || s->has_left || s->guest) continue;
        const char *st = session_shown(s, now);
        if (!strcmp(st, "idle") || !strcmp(st, "sleep")) {
            drop_social(s);
            set_state(s, "idle", now);
            s->last = now; /* wake it up */
            int edge = side;
            for (int a = 0; a < SIDES && edge < 0; a++) if (has_nb(r, ALPHA[a])) edge = ALPHA[a];
            s->has_travel = 1; s->travel_edge = edge; snprintf(s->travel_peer, sizeof s->travel_peer, "%s", nb_id(r, edge));
            exit_route(s, edge, geo);
            char nm[16];
            snprintf(nm, sizeof nm, "%.10s", s->name[0] ? s->name : "CLAWD");
            for (char *p = nm; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            char ed[8];
            snprintf(ed, sizeof ed, "%s", SIDE_NAME[edge]);
            for (char *p = ed; *p; p++) *p -= 32;
            snprintf(buf, n, "SENDING %s %s", nm, ed);
            return buf;
        }
    }
    return "NO IDLE CLAWD TO SEND";
}

/* the m key: the Clawd `sid` packs up and moves to the room on `side`: it lives there from now on (while the rooms run) */
const char *room_move_to(Room *r, const Geo *geo, double now, const char *sid, int side, char *buf, size_t n) {
    if (side < 0 || !has_nb(r, side)) return "NO WINDOW THAT WAY";
    Session *s = find_session(r, sid);
    if (!s || !s->has_pos || s->has_left || !owns(r, s)) return "TAB: PICK A CLAWD, THEN M";
    if (s->has_away || (s->has_travel && !s->move)) return "THAT CLAWD IS ON ITS WAY";
    int pack = s->pack;
    if (s->move && s->has_travel && pack >= 1 && pack <= 2) { /* still packing: only where it goes changes */
        s->travel_edge = side; snprintf(s->travel_peer, sizeof s->travel_peer, "%s", nb_id(r, side));
    } else {
        drop_social(s);
        s->has_travel = 1; s->move = 1; s->travel_edge = side; snprintf(s->travel_peer, sizeof s->travel_peer, "%s", nb_id(r, side));
        if (pack == 3 || pack == 4) { s->pack = 3; s->leaving = 0; exit_route(s, side, geo); } /* it has its box (or is unpacking): straight to that gate with it */
        else { s->pack = 1; s->has_goal = 0; s->nroute = 0; } /* first to its desk to pack: steer */
    }
    (void)geo; (void)now;
    char nm[16], ed[8];
    snprintf(nm, sizeof nm, "%.10s", s->name[0] ? s->name : "CLAWD");
    for (char *p = nm; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    snprintf(ed, sizeof ed, "%s", SIDE_NAME[side]);
    for (char *p = ed; *p; p++) *p -= 32;
    snprintf(buf, n, "MOVING %s %s", nm, ed);
    return buf;
}

/* ---- deciding what to do ---------------------------------------------------------------------- */
static int cmp_sid(const void *a, const void *b) { return strcmp((*(Session *const *)a)->sid, (*(Session *const *)b)->sid); }

typedef struct { Session *all[MAX_SESS + MAX_GUESTS]; int n; } Everyone;

static void everyone(Room *r, Everyone *e) {
    e->n = 0;
    for (int i = 0; i < r->npool; i++) e->all[e->n++] = &r->pool[i];
    for (int i = 0; i < r->ngp; i++) e->all[e->n++] = &r->gpool[i];
}

static void plan(Room *r, Session *s, int has_seat, double seat_x, double seat_y, const Geo *geo, double now, int social) {
    Rng *rng = &s->rng;
    Everyone ev;
    everyone(r, &ev);
    Session *pals[MAX_SESS + MAX_GUESTS], *busy[MAX_SESS + MAX_GUESTS];
    int np = 0, nbusy = 0;
    for (int i = 0; i < ev.n; i++) {
        Session *o = ev.all[i];
        if (o == s || o->has_left || !o->has_pos) continue;
        if (!(o->guest || geo_home(geo, r, o, NULL, NULL))) continue;
        const char *st = session_shown(o, now);
        if (!o->chat_with[0] && !strcmp(st, "idle") && o->py >= geo->ymin - 6) pals[np++] = o;
        if (!strcmp(st, "work") || !strcmp(st, "think")) busy[nbusy++] = o;
    }
    qsort(pals, (size_t)np, sizeof(Session *), cmp_sid);
    qsort(busy, (size_t)nbusy, sizeof(Session *), cmp_sid);
    double roll = rng_random(rng);
    int any_nb = 0;
    for (int i = 0; i < SIDES; i++) any_nb |= has_nb(r, i);
    if (social && !s->guest && any_nb && rng_random(rng) < 0.2) { /* go and see the next window */
        int edges[SIDES], ne = 0;
        for (int a = 0; a < SIDES; a++) if (has_nb(r, ALPHA[a])) edges[ne++] = ALPHA[a];
        int edge = edges[rng_choice(rng, ne)];
        s->has_travel = 1; s->travel_edge = edge; snprintf(s->travel_peer, sizeof s->travel_peer, "%s", nb_id(r, edge));
        double y = rng_uniform(rng, geo->ymin, geo->ymax);
        (void)y;
        exit_route(s, edge, geo);
    } else if (social && np && roll < 0.35) { /* visit: chat, wave, hop, high-five, dance or tag */
        Session *o = pals[rng_choice(rng, np)];
        int kind = r->theme->kinds[rng_choice(rng, r->theme->nkinds)];
        if (kind == 6) { /* ordering coffee needs a customer and the barista */
            Session *barista = NULL;
            for (int i = 0; i < np && !barista; i++) if (!strcmp(geo_role(geo, r, pals[i]), "barista")) barista = pals[i];
            if (barista && strcmp(geo_role(geo, r, s), "barista")) o = barista;
            else kind = 0;
        }
        snprintf(s->chat_with, sizeof s->chat_with, "%s", o->sid);
        snprintf(o->chat_with, sizeof o->chat_with, "%s", s->sid);
        snprintf(s->stage, sizeof s->stage, "walk"); snprintf(o->stage, sizeof o->stage, "wait");
        snprintf(s->part, sizeof s->part, "visitor"); snprintf(o->part, sizeof o->part, "host");
        s->chat_kind = o->chat_kind = kind;
        o->has_goal = 0; /* the friend stops where it is */
        o->peek_on = o->peek_has_end = 0; o->emote[0] = 0;
    } else if (social && nbusy && roll < 0.47) {
        Session *o = busy[rng_choice(rng, nbusy)];
        s->peek_on = 1; s->peek_has_end = 0; snprintf(s->peek_sid, sizeof s->peek_sid, "%s", o->sid);
    } else if (roll < 0.67) {
        double x = rng_uniform(rng, 8, geo->w - 8);
        double y = rng_uniform(rng, geo->ymin, geo->ymax);
        set_floor_goal(s, geo, x, y);
    } else if (social && roll < 0.87) {
        Emote em = r->theme->emotes[rng_choice(rng, r->theme->nemotes)];
        if ((!strcmp(em.kind, "swing") && geo->spots.no_swing) || (!strcmp(em.kind, "slide") && geo->spots.no_slide)) em.kind = "wave"; /* (left out: no room) */
        int spot = geo->spots.has && (!strcmp(em.kind, "swing") || !strcmp(em.kind, "slide"));
        if (spot) { /* play equipment: walk there first */
            if (!strcmp(em.kind, "swing")) set_goal(s, geo->spots.swing.x, geo->spots.swing.y);
            else set_goal(s, geo->spots.slide.base[0], geo->spots.slide.base[1]);
            s->has_then = 1; snprintf(s->then_kind, sizeof s->then_kind, "%s", em.kind); s->then_dur = em.dur;
        } else {
            snprintf(s->emote, sizeof s->emote, "%s", em.kind); s->emote_start = now; s->emote_end = now + em.dur;
        }
    } else {
        if (has_seat) set_goal(s, seat_x, seat_y);
        else {
            double x = rng_uniform(rng, 8, geo->w - 8);
            double y = rng_uniform(rng, geo->ymin, geo->ymax);
            set_floor_goal(s, geo, x, y);
        }
    }
}

static void play(Session *s, const Geo *geo, double now) { /* swing back and forth, climb the ladder and slide down */
    double u = now - s->emote_start;
    if (!strcmp(s->emote, "swing")) {
        double ph = u * 3.2;
        s->px = geo->spots.swing.x;              /* the swing set faces us: towards us and back */
        s->py = geo->spots.swing.y + 2 * sin(ph);
    } else {
        double bx = geo->spots.slide.base[0], by = geo->spots.slide.base[1], tx = geo->spots.slide.top[0], ty = geo->spots.slide.top[1];
        double ex = geo->spots.slide.bottom[0], ey = geo->spots.slide.bottom[1];
        double c = fmod(u, 2.2);
        if (c < 1.0) { s->px = tx; s->py = by + (ty - by) * c; } /* climb */
        else if (c < 1.9) { double k = (c - 1.0) / 0.9; s->px = tx + (ex - tx) * k; s->py = ty + (ey - ty) * k * k; } /* slide */
        else { double k = (c - 1.9) / 0.3; s->px = ex + (bx - ex) * k; s->py = ey + (by - ey) * k; } /* walk back to the ladder */
        int down_right = ex >= tx; /* (the slide may run either way) */
        s->face = c < 1.9 ? (down_right ? 1 : -1) : (down_right ? -1 : 1);
    }
    s->has_goal = 0;
    s->moving = 0;
}

static void watch(Room *r, Session *s, const Geo *geo, double now) { /* walk over to a busy friend, look at their screen, wander off */
    Session *o = find_any(r, s->peek_sid);
    if (!o || o->has_left || !o->has_pos || !(!strcmp(session_shown(o, now), "work") || !strcmp(session_shown(o, now), "think"))) {
        s->peek_on = s->peek_has_end = 0;
        s->next_plan = now + 1.0;
        return;
    }
    s->face = o->px >= s->px ? 1 : -1;
    if (!s->peek_has_end) {
        set_floor_goal(s, geo, o->px - 17, clampd(o->py + 3, geo->ymin, geo->ymax));
        if (fabs(s->px - s->gx) + fabs(s->py - s->gy) < 1.0) {
            s->has_goal = 0;
            s->peek_has_end = 1; s->peek_end = now + rng_uniform(&s->rng, 3.0, 5.0);
        }
    } else if (now >= s->peek_end) {
        s->peek_on = s->peek_has_end = 0;
        s->next_plan = now + rng_uniform(&s->rng, 1.0, 3.0);
    }
}

static void chat(Room *r, Session *s, const Geo *geo, double now) { /* one step of a visit: walk over, do something together, part */
    Session *o = find_any(r, s->chat_with);
    if (!o || strcmp(o->chat_with, s->sid) || o->has_left || strcmp(session_shown(o, now), "idle")) {
        drop_social(s); /* the friend got busy or left */
        s->next_plan = now + 1.0;
        return;
    }
    if (strcmp(s->stage, "flee")) s->face = o->px >= s->px ? 1 : -1;
    if (!strcmp(s->stage, "walk")) {
        set_floor_goal(s, geo, o->px + (s->px > o->px ? 18 : -18), clampd(o->py, geo->ymin, geo->ymax));
        if (fabs(s->px - s->gx) + fabs(s->py - s->gy) < 1.0) {
            s->has_goal = 0;
            if (s->chat_kind == 5) { /* tag: this one chases, the friend runs */
                snprintf(s->stage, sizeof s->stage, "chase"); snprintf(o->stage, sizeof o->stage, "flee");
                s->chat_until = o->chat_until = now + 6.0;
                s->catch_ok = o->catch_ok = now + 1.0;
            } else {
                snprintf(s->stage, sizeof s->stage, "talk"); snprintf(o->stage, sizeof o->stage, "talk");
                s->chat_until = o->chat_until = now + rng_uniform(&s->rng, 3.0, 5.0);
            }
        }
    } else if (!strcmp(s->stage, "chase")) {
        s->run = 1.7;
        set_goal(s, o->px, o->py);
        if (now >= s->catch_ok && hypot(o->px - s->px, o->py - s->py) < 9) { /* tagged: swap roles */
            snprintf(s->stage, sizeof s->stage, "flee"); snprintf(o->stage, sizeof o->stage, "chase");
            s->catch_ok = o->catch_ok = now + 1.0;
            sparks(r, &s->rng, o->px, o->py - 1, 6);
        }
    } else if (!strcmp(s->stage, "flee")) {
        s->run = 1.5;
        if (!s->has_goal || now >= s->flee_next) { /* run away from the chaser */
            s->flee_next = now + 0.7;
            double ang = atan2(s->py - o->py, s->px - o->px) + rng_uniform(&s->rng, -0.8, 0.8);
            double dist = rng_uniform(&s->rng, 25, 50);
            set_floor_goal(s, geo, clampd(s->px + cos(ang) * dist, 8, geo->w - 8), clampd(s->py + sin(ang) * dist * 0.5, geo->ymin, geo->ymax));
        }
    }
    int active = !strcmp(s->stage, "talk") || !strcmp(s->stage, "chase") || !strcmp(s->stage, "flee");
    if (active && now > s->chat_until && strcmp(s->sid, o->sid) < 0) { /* one side ends it */
        int kind = s->chat_kind;
        Session *who[2] = {s, o};
        double lo[2] = {2.0, 1.0}, hi[2] = {5.0, 3.0};
        for (int k = 0; k < 2; k++) {
            Session *x = who[k];
            drop_social(x);
            x->has_goal = 0;
            x->next_plan = now + rng_uniform(&x->rng, lo[k], hi[k]);
            int cx, by;
            int has_slot = geo_home(geo, r, x, &cx, &by);
            if (kind == 6 && !strcmp(x->part, "visitor")) { /* got the coffee */
                x->cup_until = now + 25.0;
                if (has_slot) set_goal(x, cx - 5, by + SEAT_DY); /* ... and takes it back to the table */
            }
        }
    }
}

static void idle_step(Room *r, Session *s, int has_seat, double sx, double sy, const Geo *geo, double now) { /* what a free Clawd (idle, or a visitor) does next */
    if (now < s->react_until) { /* applauding a friend who just finished */
        s->has_goal = 0;
        s->peek_on = s->peek_has_end = 0; s->emote[0] = 0;
    } else if (s->chat_with[0]) chat(r, s, geo, now);
    else if (s->peek_on) watch(r, s, geo, now);
    else if (s->emote[0]) {
        if (now >= s->emote_end) {
            s->emote[0] = 0;
            s->next_plan = now + rng_uniform(&s->rng, 1.0, 3.0);
        } else if (!strcmp(s->emote, "swing") || !strcmp(s->emote, "slide")) play(s, geo, now);
    } else if (s->has_then) { /* walking to a play spot */
        if (!s->has_goal) {
            snprintf(s->emote, sizeof s->emote, "%s", s->then_kind);
            s->emote_start = now; s->emote_end = now + s->then_dur;
            s->has_then = 0;
        }
    } else if (!s->has_goal && !s->has_travel && now >= s->next_plan) plan(r, s, has_seat, sx, sy, geo, now, 1);
}

static void steer(Room *r, Session *s, double seat_x, double seat_y, const Geo *geo, double now) {
    const char *state = session_shown(s, now);
    if (s->has_left) { /* leave through the nearest side */
        drop_social(s);
        set_goal(s, s->px < geo->w / 2.0 ? -12.0 : geo->w + 12.0, (double)geo->ymax);
    } else if (s->move && s->has_travel) { /* moving out (busy or not): to its desk, pack it into a box, out by the gate with it */
        if (s->pack == 1) {
            if (fabs(s->px - seat_x) + fabs(s->py - seat_y) > 0.5) { set_goal(s, seat_x, seat_y); s->nroute = 0; }
            else { s->has_goal = 0; s->pack = 2; s->pack_t0 = now; }
        } else if (s->pack == 2 && now - s->pack_t0 >= PACK_T) {
            poof(r, seat_x + 5, seat_y + 7);
            s->pack = 3;
            exit_route(s, s->travel_edge, geo);
        }
    } else if (s->pack == 3 || s->pack == 4) { /* moved in with its box (busy or not): to its new desk, unpack it there */
        s->chat_with[0] = 0; s->peek_on = s->peek_has_end = 0; s->emote[0] = 0; s->has_then = 0;
        if (s->pack == 4) { s->has_goal = 0; if (now - s->pack_t0 >= UNPACK_T) { s->pack = 0; s->next_plan = now + rng_uniform(&s->rng, 1.0, 3.0); } }
        else if (s->has_goal || s->nroute) { /* (still coming in by the gate) */
        } else if (fabs(s->px - seat_x) + fabs(s->py - seat_y) > 0.5) set_goal(s, seat_x, seat_y);
        else { s->pack = 4; s->pack_t0 = now; poof(r, seat_x + 5, seat_y + 7); }
    } else if (!strcmp(state, "work") || !strcmp(state, "think") || !strcmp(state, "sleep")) { /* busy or asleep: be at the desk */
        drop_social(s);
        int far = fabs(s->px - seat_x) + fabs(s->py - seat_y) > 0.5;
        if (far) set_goal(s, seat_x, seat_y); else s->has_goal = 0;
    } else if (strcmp(state, "idle")) { /* needs OK or just done: free to walk around, no visits */
        drop_social(s);
        if (!s->has_goal && now >= s->next_plan) plan(r, s, 1, seat_x, seat_y, geo, now, 0);
    } else idle_step(r, s, 1, seat_x, seat_y, geo, now);
}

static void steer_guest(Room *r, Session *s, const Geo *geo, double now) { /* wanders and socialises for a while, then walks out the way it came in */
    if (s->returning || now >= s->stay_until) {
        s->returning = 1;
        if (!s->leaving) {
            drop_social(s);
            exit_route(s, s->enter_side, geo);
        }
    } else idle_step(r, s, 0, 0, 0, geo, now);
}

/* ---- particles and moving ---------------------------------------------------------------------- */
static void emit(Room *r, Session *s, double now) { /* sparks when a Clawd is done, hearts, notes, high-five sparks */
    if (!s->has_pos) return;
    if (!strcmp(session_shown(s, now), "cheer") && !(s->has_burst && s->burst_t == s->state_t)) {
        s->has_burst = 1; s->burst_t = s->state_t;
        sparks(r, &s->rng, s->px, s->py - 1, 8);
        Everyone ev;
        everyone(r, &ev);
        for (int i = 0; i < ev.n; i++) {
            Session *o = ev.all[i];
            if (o != s && o->has_pos && !o->has_left && !o->chat_with[0] && !strcmp(session_shown(o, now), "idle") && fabs(o->px - s->px) < 90) {
                o->react_until = now + 1.6;
                o->face = s->px >= o->px ? 1 : -1;
            }
        }
    }
    Session *o = s->chat_with[0] ? find_any(r, s->chat_with) : NULL;
    if (!o || strcmp(s->stage, "talk")) return;
    if (s->chat_kind == 1 && now >= s->next_heart) {
        s->next_heart = now + 0.7;
        if (r->np < MAX_PARTS) { Particle *q = &r->parts[r->np++]; *q = (Particle){s->px - 1, s->py - 4, rng_uniform(&s->rng, -0.1, 0.1), -0.25, 1.0, PINK, 1}; }
    } else if (s->chat_kind == 4 && now >= s->next_heart) { /* dancing throws out little notes of colour */
        s->next_heart = now + 0.35;
        double x = s->px + rng_uniform(&s->rng, -5, 5), vx = rng_uniform(&s->rng, -0.2, 0.2);
        Rgb c = (Rgb[]){PINK, C_YELLOW, BLUE}[rng_choice(&s->rng, 3)];
        if (r->np < MAX_PARTS) { Particle *q = &r->parts[r->np++]; *q = (Particle){x, s->py - 1, vx, -0.5, 1.0, c, 0}; }
    } else if (s->chat_kind == 3 && strcmp(s->sid, o->sid) < 0) { /* high-five: sparks where the hands meet */
        int cycle = (int)(now / 1.6);
        if (cycle != s->last_hf && fmod(now, 1.6) > 0.9) {
            s->last_hf = cycle;
            sparks(r, &s->rng, (s->px + o->px) / 2, fmin(s->py, o->py) - 3, 8);
        }
    }
}

static void fly(Room *r, double dt) { /* move the particles: a little gravity, fading out */
    double k = dt * 30;
    int w = 0;
    for (int i = 0; i < r->np; i++) {
        Particle q = r->parts[i];
        q.x += q.vx * k; q.y += q.vy * k;
        q.vy += (q.heart ? 0 : 0.04) * k;
        q.life -= (q.heart == 2 ? 0.04 : q.heart ? 0.02 : 0.025) * k;
        if (q.life > 0) r->parts[w++] = q;
    }
    r->np = w;
}

static void move(Session *s, const Geo *geo, double dt, double now) {
    int was_moving = s->moving;
    s->moving = 0;
    if (!s->has_goal) { s->walk_t = 0.0; return; }
    double dx = s->gx - s->px, dy = s->gy - s->py, d = hypot(dx, dy);
    s->walk_t = was_moving ? s->walk_t + dt : 0.0;
    double step;
    if (s->has_left) step = LEAVE_SPEED * dt;
    else { /* ease in when setting off and ease out when arriving */
        double e = fmin(fmin(1.0, 0.3 + s->walk_t / 0.5), 0.3 + d / 10.0);
        step = WALK_SPEED * s->run * e * dt;
    }
    if (d <= fmax(step, 0.5)) { /* arrived */
        s->px = s->gx; s->py = s->gy;
        if (s->nroute) { /* on to the next point (up the rope, down the stairs) */
            s->gx = s->rx[0]; s->gy = s->ry[0];
            for (int i = 1; i < s->nroute; i++) { s->rx[i - 1] = s->rx[i]; s->ry[i - 1] = s->ry[i]; }
            s->nroute--; s->vclimb = fabs(s->gx - s->px) < 1; /* climbing pose only straight up or down (a ladder); stairs are walked */
            return;
        }
        s->has_goal = 0; s->vclimb = 0;
        s->next_plan = now + rng_uniform(&s->rng, 1.5, 4.0);
        return;
    }
    s->px += dx / d * step;
    s->py += dy / d * step;
    s->moving = 1;
    if (fabs(dx) > 0.5) s->face = dx > 0 ? 1 : -1;
    s->walk_phase += step;
    if (s->has_left && !(-10 < s->px && s->px < geo->w + 10)) s->gone = 1;
}

/* ---- one frame -------------------------------------------------------------------------------- */
void room_step(Room *r, const Geo *geo, double now) {
    double dt = !r->has_last_now ? 0.0 : fmin(0.25, fmax(0.0, now - r->last_now));
    r->last_now = now; r->has_last_now = 1;
    if (!r->has_geo_key || geo->w != r->geo_w || geo->h != r->geo_h) { /* the room changed size: everybody goes back to their desk */
        r->has_geo_key = 1; r->geo_w = geo->w; r->geo_h = geo->h;
        Everyone ev;
        everyone(r, &ev);
        for (int i = 0; i < ev.n; i++) {
            Session *s = ev.all[i];
            if (s->move && s->has_travel) s->move = s->pack = 0; /* (a move not yet through the gate is called off) */
            s->has_pos = 0; s->has_goal = 0; s->chat_with[0] = 0; s->peek_on = s->peek_has_end = 0; s->emote[0] = 0; s->has_travel = 0;
            s->run = 1.0;
        }
        r->np = 0;
    }
    handle_messages(r, geo, now);
    room_order(r); /* (handle_messages may have changed the guests) */
    for (int i = 0; i < r->n; i++) {
        Session *s = r->s[i];
        int cx, by;
        if (!geo_home(geo, r, s, &cx, &by)) continue; /* does not fit in the room */
        if (s->has_away) { steer_away(r, s, geo, now); continue; }
        double seat_x = cx - 5, seat_y = by + SEAT_DY;
        if (s->has_travel && !has_nb(r, s->travel_edge)) { /* the gate is gone (windows moved): stay */
            drop_social(s);
            s->has_goal = 0;
        }
        if (!s->has_pos) {
            s->has_pos = 1;
            if (now - s->created < ARRIVE_WALK) { s->px = -8.0; s->py = (double)geo->ymax; } else { s->px = seat_x; s->py = seat_y; }
            s->next_plan = now + rng_uniform(&s->rng, 1.0, 4.0);
        }
        steer(r, s, seat_x, seat_y, geo, now);
        move(s, geo, dt, now);
        emit(r, s, now);
        if (s->has_travel) cross(r, s, geo, now);
    }
    for (int i = 0; i < r->ngp;) {
        Session *g = &r->gpool[i];
        if (!g->has_pos) enter(g, g->enter_side, geo); /* (re)appear at the door it came through */
        steer_guest(r, g, geo, now);
        move(g, geo, dt, now);
        emit(r, g, now);
        int before = r->ngp;
        cross(r, g, geo, now);
        if (r->ngp == before) i++; /* (a guest that left was removed: the next one is now at i) */
    }
    fly(r, dt);
}
