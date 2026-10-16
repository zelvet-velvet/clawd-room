#include "actions.h"
#include <string.h>

static void a_none(Session *s) { (void)s; }
static void a_walk(Session *s) { s->moving = 1; strcpy(s->stage, "walk"); }
static void a_climb(Session *s) { s->moving = 1; s->vclimb = 1; }
static void a_chase(Session *s) { s->moving = 1; strcpy(s->stage, "chase"); }
static void a_flee(Session *s) { s->moving = 1; strcpy(s->stage, "flee"); }
static void talk(Session *s, int kind) { strcpy(s->chat_with, "y"); strcpy(s->stage, "talk"); s->chat_kind = kind; }
static void a_nod(Session *s) { talk(s, 0); }
static void a_cwave(Session *s) { talk(s, 1); }
static void a_hop(Session *s) { talk(s, 2); }
static void a_hi5(Session *s) { talk(s, 3); }
static void a_cdance(Session *s) { talk(s, 4); }
static void a_barista(Session *s) { talk(s, 6); strcpy(s->part, "host"); }
static void a_visitor(Session *s) { talk(s, 6); strcpy(s->part, "visitor"); }
#define EMOTE(fn, kind) static void fn(Session *s) { strcpy(s->emote, kind); s->emote_start = 1000.0; }
EMOTE(a_stretch, "stretch") EMOTE(a_sdance, "dance") EMOTE(a_swing, "swing") EMOTE(a_slide, "slide") EMOTE(a_jump, "jump") EMOTE(a_wave, "wave")
static void a_applaud(Session *s) { s->react_until = 9e9; }
static void a_peek(Session *s) { s->peek_on = s->peek_has_end = 1; }

const Action ACTIONS[] = {
    {"idle", "idle", a_none}, {"work", "work", a_none}, {"think", "think", a_none}, {"wait", "wait", a_none},
    {"cheer", "cheer", a_none}, {"sleep", "sleep", a_none}, {"walk", "idle", a_walk}, {"climb", "idle", a_climb},
    {"chase", "idle", a_chase}, {"flee", "idle", a_flee}, {"chat-nod", "idle", a_nod}, {"chat-wave", "idle", a_cwave},
    {"chat-hop", "idle", a_hop}, {"chat-hi5", "idle", a_hi5}, {"chat-dance", "idle", a_cdance}, {"barista", "idle", a_barista},
    {"visitor", "idle", a_visitor}, {"stretch", "idle", a_stretch}, {"solo-dance", "idle", a_sdance}, {"swing", "idle", a_swing},
    {"slide", "idle", a_slide}, {"jump", "idle", a_jump}, {"wave", "idle", a_wave}, {"applaud", "idle", a_applaud}, {"peek", "idle", a_peek},
};
const int N_ACTIONS = (int)(sizeof ACTIONS / sizeof ACTIONS[0]);

void action_poses(int i, Pose out[ACTION_STEPS]) {
    Session s;
    session_init(&s, "x", 1000.0);
    s.seed = 0.0;
    s.state_t = 1000.0;
    ACTIONS[i].set(&s);
    for (int k = 0; k < ACTION_STEPS; k++) {
        double t = 0.2 + k / (double)ACTION_FPS;
        s.walk_phase = t * 12;
        out[k] = pose_for(&s, ACTIONS[i].state, t, 1000.0 + t);
    }
}
