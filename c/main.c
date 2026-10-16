/* clawd-room, C port. Stage 1: canvas, font, sprite files, Clawd poses and drawing.
 *   ./clawd-room --golden   prints "action step hash" for every action (tools/golden_clawd.py prints the same from Python)
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas.h"
#include "clawd.h"
#include "term.h"

typedef struct { const char *name, *state; void (*set)(Session *); } Action;

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
static void a_peek(Session *s) { s->peeking = 1; }

/* same order and attributes as ACTIONS in sprite_tool.py */
static const Action ACTIONS[] = {
    {"idle", "idle", a_none}, {"work", "work", a_none}, {"think", "think", a_none}, {"wait", "wait", a_none},
    {"cheer", "cheer", a_none}, {"sleep", "sleep", a_none}, {"walk", "idle", a_walk}, {"climb", "idle", a_climb},
    {"chase", "idle", a_chase}, {"flee", "idle", a_flee}, {"chat-nod", "idle", a_nod}, {"chat-wave", "idle", a_cwave},
    {"chat-hop", "idle", a_hop}, {"chat-hi5", "idle", a_hi5}, {"chat-dance", "idle", a_cdance}, {"barista", "idle", a_barista},
    {"visitor", "idle", a_visitor}, {"stretch", "idle", a_stretch}, {"solo-dance", "idle", a_sdance}, {"swing", "idle", a_swing},
    {"slide", "idle", a_slide}, {"jump", "idle", a_jump}, {"wave", "idle", a_wave}, {"applaud", "idle", a_applaud}, {"peek", "idle", a_peek},
};

static int golden(const SpriteFile *frames) {
    for (size_t a = 0; a < sizeof ACTIONS / sizeof ACTIONS[0]; a++) {
        Session s;
        session_init(&s, "x");
        s.seed = 0.0;
        s.state_t = 1000.0;
        ACTIONS[a].set(&s);
        for (int i = 0; i < 41; i++) {
            double t = 0.2 + i / 12.0;
            s.walk_phase = t * 12;
            Pose p = pose_for(&s, ACTIONS[a].state, t, 1000.0 + t);
            Canvas cv = canvas_new(26, 18, RGB(0, 0, 0));
            draw_clawd(&cv, 13, 8, &p, frames, C_CLAWD);
            printf("%s %d %016" PRIx64 "\n", ACTIONS[a].name, i, canvas_hash(&cv));
            canvas_free(&cv);
        }
    }
    return 0;
}

static uint64_t fnv_bytes(const char *s, size_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) h = (h ^ (unsigned char)s[i]) * 1099511628211ULL;
    return h;
}

/* the terminal update strings: first frame of an action in full, then each step against the one before */
static int golden_term(const SpriteFile *frames) {
    for (size_t a = 0; a < sizeof ACTIONS / sizeof ACTIONS[0]; a++) {
        Session s;
        session_init(&s, "x");
        s.state_t = 1000.0;
        ACTIONS[a].set(&s);
        Cells prev = {0, 0, NULL};
        for (int i = 0; i < 41; i++) {
            double t = 0.2 + i / 12.0;
            s.walk_phase = t * 12;
            Pose p = pose_for(&s, ACTIONS[a].state, t, 1000.0 + t);
            Canvas cv = canvas_new(26, 18, RGB(0, 0, 0));
            draw_clawd(&cv, 13, 8, &p, frames, C_CLAWD);
            Cells cur = cells_from_canvas(&cv, ACTIONS[a].name);
            size_t n1, n2;
            char *full = diff_frame(&cur, NULL, &n1), *d = diff_frame(&cur, prev.c ? &prev : NULL, &n2);
            printf("term %s %d %016" PRIx64 " %016" PRIx64 "\n", ACTIONS[a].name, i, fnv_bytes(full, n1), fnv_bytes(d, n2));
            free(full); free(d); cells_free(&prev); prev = cur; canvas_free(&cv);
        }
        cells_free(&prev);
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *dir = getenv("CLAWD_SPRITES") ? getenv("CLAWD_SPRITES") : "../sprites";
    char path[512];
    snprintf(path, sizeof path, "%s/clawd.txt", dir);
    SpriteFile frames = sprites_load(path);
    int rc = 0;
    if (argc > 1 && !strcmp(argv[1], "--golden")) rc = golden(argc > 2 && !strcmp(argv[2], "--code") ? NULL : &frames);
    else if (argc > 1 && !strcmp(argv[1], "--golden-term")) rc = golden_term(&frames);
    else { fprintf(stderr, "usage: clawd-room --golden [--code] | --golden-term\n"); rc = 1; }
    sprites_free(&frames);
    return rc;
}
