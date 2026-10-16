#include "clawd.h"
#include "theme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

unsigned crc32_str(const char *s) { /* zlib.crc32 */
    unsigned crc = 0xFFFFFFFFu;
    for (; *s; s++) {
        crc ^= (unsigned char)*s;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void session_init(Session *s, const char *sid) {
    memset(s, 0, sizeof *s);
    snprintf(s->sid, sizeof s->sid, "%s", sid);
    snprintf(s->state, sizeof s->state, "idle");
    s->face = 1;
    s->seed = (crc32_str(sid) % 1000) / 1000.0;
}

const char *session_shown(const Session *s, double now) {
    if (s->guest) return "idle";
    if (!strcmp(s->state, "think") && now < s->hold_work_until) return "work"; /* no flicker between two tool calls */
    if (!strcmp(s->state, "cheer") && now - s->state_t > CHEER_FOR) return now - s->state_t > SLEEP_AFTER ? "sleep" : "idle";
    if (!strcmp(s->state, "idle") && now - s->last > SLEEP_AFTER) return "sleep";
    return s->state;
}

/* Python's int(x) % n for x >= 0 */
#define IMOD(x, n) (((int)(x)) % (n))

Pose pose_for(const Session *s, const char *state, double t, double now) {
    Pose p = {0, 0, {0, 0}, 0, "open", {0, 0}};
    static const int LEAN4[4] = {-1, 0, 1, 0};
    double ph = t + s->seed * 10; /* each Clawd has its own rhythm */
    int talk = s->chat_with[0] && !strcmp(s->stage, "talk");
    if (s->moving && s->vclimb) { /* up the rope or the stairs */
        int k = IMOD(s->walk_phase / 2, 2);
        p.arms[0] = k ? 1 : 2; p.arms[1] = k ? 2 : 1; p.legs = 1 + k; p.look[1] = -1;
    } else if (s->moving) {
        int k = IMOD(s->walk_phase / 3, 2);
        p.arms[0] = k ? 2 : 0; p.arms[1] = k ? 0 : 2;
        if (!strcmp(s->stage, "chase")) { p.arms[0] = p.arms[1] = 1; }
        else if (!strcmp(s->stage, "flee")) { p.arms[0] = p.arms[1] = 2; }
        p.bob = -k; p.lean = s->face; p.legs = 1 + k; p.look[0] = s->face;
        if (!strcmp(s->stage, "chase")) p.eyes = "happy";
    } else if (talk) {
        int kind = s->chat_kind, side = strcmp(s->sid, s->chat_with) < 0 ? 1 : -1;
        if (kind == 0) { /* chatting: little nods */
            p.bob = IMOD(ph * 3, 2) ? -1 : 0; p.look[0] = s->face;
        } else if (kind == 1) { /* waving at each other */
            if (IMOD(ph * 4, 2)) { p.arms[0] = 0; p.arms[1] = 1; } else { p.arms[0] = 1; p.arms[1] = 0; }
            p.look[0] = s->face; p.eyes = "happy";
        } else if (kind == 2) { /* hopping together */
            p.bob = -(int)(fabs(sin(now * 6)) * 3); p.arms[0] = p.arms[1] = 1; p.eyes = "happy";
        } else if (kind == 3) { /* high-five: a jump with the near arm out, every 1.6 s */
            double u = fmod(now, 1.6);
            if (u > 0.75 && u < 1.05) {
                p.bob = -(int)(sin((u - 0.75) / 0.3 * M_PI) * 3);
                if (s->face > 0) { p.arms[0] = 0; p.arms[1] = 1; } else { p.arms[0] = 1; p.arms[1] = 0; }
                p.eyes = "happy"; p.lean = s->face;
            } else p.look[0] = s->face;
        } else if (kind == 6) { /* ordering coffee: the host works the machine, the visitor waits */
            if (!strcmp(s->part, "host")) {
                if (IMOD(now * 6, 2)) { p.arms[0] = 2; p.arms[1] = 0; } else { p.arms[0] = 0; p.arms[1] = 2; }
                p.look[0] = s->face; p.eyes = "happy";
            } else { p.look[0] = s->face; p.bob = IMOD(ph * 2, 2) ? -1 : 0; }
        } else { /* dancing together */
            int step = IMOD(now * 2.5, 4);
            p.lean = LEAN4[step] * side;
            if (step % 2) { p.arms[0] = 1; p.arms[1] = 2; } else { p.arms[0] = 2; p.arms[1] = 1; }
            p.bob = -(step % 2); p.eyes = "happy";
        }
    } else if (!strcmp(state, "work")) { /* typing: lean to the screen, look down at the keys, arms tap */
        p.lean = 1;
        if (IMOD(t * 12, 2)) { p.arms[0] = 2; p.arms[1] = 0; } else { p.arms[0] = 0; p.arms[1] = 2; }
        p.look[1] = 1;
    } else if (!strcmp(state, "think")) { /* sways, scratches its head, looks from side to side */
        static const int LOOK4[4] = {-1, 0, 1, 0};
        p.lean = LEAN4[IMOD(ph * 1.5, 4)];
        p.arms[1] = IMOD(ph * 0.7, 2) ? 2 : 0;
        p.bob = IMOD(ph * 2, 2) ? -1 : 0;
        p.look[0] = LOOK4[IMOD(ph * 1.2, 4)];
    } else if (!strcmp(state, "wait")) { /* waves an arm for attention */
        p.arms[1] = IMOD(t * 3, 2) ? 1 : 0; p.look[0] = 1;
    } else if (!strcmp(state, "cheer")) {
        p.bob = -(int)(fabs(sin((now - s->state_t) * 8)) * 4); p.arms[0] = p.arms[1] = 1; p.eyes = "happy";
        p.lean = IMOD(ph * 3, 2) ? 1 : -1;
    } else if (!strcmp(state, "sleep")) {
        p.bob = IMOD(t * 0.8, 2) ? 1 : 0; p.eyes = "shut";
    } else if (now < s->react_until) { /* applauding a friend who just finished */
        p.bob = -(int)(fabs(sin(now * 9)) * 3); p.arms[0] = p.arms[1] = 1; p.eyes = "happy"; p.look[0] = s->face;
    } else if (s->peeking) { /* watching a friend work */
        p.lean = s->face; p.look[0] = s->face; p.arms[0] = 0; p.arms[1] = 2; p.bob = IMOD(ph * 2, 2) ? -1 : 0;
    } else if (s->emote[0]) { /* a little solo act */
        const char *kind = s->emote;
        int step = IMOD(ph * 2.5, 4);
        if (!strcmp(kind, "stretch")) {
            int up = IMOD(ph * 1.5, 2);
            p.arms[0] = p.arms[1] = up ? 1 : 2; p.bob = -up; p.eyes = "shut";
        } else if (!strcmp(kind, "dance")) {
            p.lean = LEAN4[step];
            if (step % 2) { p.arms[0] = 1; p.arms[1] = 2; } else { p.arms[0] = 2; p.arms[1] = 1; }
            p.bob = -(step % 2); p.eyes = "happy";
        } else if (!strcmp(kind, "swing")) { /* holding the ropes, legs up at the top of each swing */
            p.arms[0] = p.arms[1] = 1; p.eyes = "happy"; p.legs = 1 + IMOD((now - s->emote_start) * 3.2 / 3, 2);
        } else if (!strcmp(kind, "slide")) {
            double u = fmod(now - s->emote_start, 2.2);
            if (u >= 1.0 && u < 1.9) { p.arms[0] = p.arms[1] = 1; p.eyes = "happy"; } /* sliding down */
            else { /* climbing or walking back */
                if (IMOD(now * 6, 2)) { p.arms[0] = 2; p.arms[1] = 0; } else { p.arms[0] = 0; p.arms[1] = 2; }
                p.legs = 1 + IMOD(now * 6, 2);
            }
        } else if (!strcmp(kind, "jump")) {
            p.bob = -(int)(fabs(sin((now - s->emote_start) * 6)) * 4); p.arms[0] = p.arms[1] = 1; p.eyes = "happy";
        } else { /* wave at nobody in particular */
            p.arms[1] = IMOD(ph * 4, 2) ? 1 : 0; p.eyes = "happy";
        }
    } else { /* idle: looks around, shifts its weight */
        static const int LOOKI[4] = {0, -1, 0, 1};
        p.look[0] = LOOKI[IMOD(ph * 0.8, 4)];
        p.bob = IMOD(ph, 3) == 0 ? -1 : 0;
    }
    if (!strcmp(p.eyes, "open") && fmod(t + s->seed * 7, 3.2) < 0.12) p.eyes = "shut"; /* blink */
    return p;
}

void pose_key(const Pose *p, char *out, size_t n) {
    snprintf(out, n, "arms=%d,%d legs=%d eyes=%s look=%d,%d lean=%d", p->arms[0], p->arms[1], p->legs, p->eyes, p->look[0], p->look[1], p->lean);
}

/* the code's own drawing: an 8-wide torso, two 2x2 stub arms, four 1-pixel legs, two 1x1 eyes, no mouth */
static void draw_by_code(Canvas *cv, int x, int y, const Pose *p, Rgb body, const Theme *theme, const char *role) {
    static const int LEGX[4] = {3, 5, 8, 10};
    int lean = p->lean;
    for (int i = 0; i < 4; i++) { /* legs; while walking one pair is lifted */
        int lifted = (p->legs == 1 && i % 2 == 0) || (p->legs == 2 && i % 2 == 1);
        canvas_rect(cv, x + LEGX[i], y + 6, 1, lifted ? 1 : 2, body);
    }
    canvas_rect(cv, x + 3 + lean, y, 8, 4, body);
    canvas_rect(cv, x + 3, y + 4, 8, 2, body);
    for (int k = 0; k < 2; k++) { /* arms: 0 down, 1 raised beside the head, 2 half raised */
        int ax = k ? x + 11 : x + 1, mode = p->arms[k];
        canvas_rect(cv, ax + lean, y + (mode == 0 ? 2 : mode == 1 ? -1 : 1), 2, 2, body);
    }
    if (theme) theme_accessory(theme, cv, x + lean, y, role);
    int ex = x + lean + p->look[0], ey = y + 1 + p->look[1];
    if (strcmp(p->eyes, "shut")) { canvas_px(cv, ex + 4, ey, C_INK); canvas_px(cv, ex + 9, ey, C_INK); }
    if (!strcmp(p->eyes, "happy")) { /* the eye grows to 1x2 (pale peach top), plus blush */
        canvas_px(cv, x + lean + 4, y, C_GLINT); canvas_px(cv, x + lean + 9, y, C_GLINT);
        canvas_px(cv, x + lean + 3, y + 2, C_BLUSH); canvas_px(cv, x + lean + 10, y + 2, C_BLUSH);
    }
}

void draw_clawd(Canvas *cv, int cx, int y, const Pose *p, const SpriteFile *frames, Rgb body, const Theme *theme, const char *role) {
    int x = cx - 7;
    y += p->bob;
    char key[128];
    pose_key(p, key, sizeof key);
    const Sprite *sp = frames ? sprites_by_key(frames, key) : NULL;
    if (sp) { /* an edited / exported frame (15 wide, from one row above the head) */
        sprite_blit(cv, sp, x, y - 1, body);
        if (theme) theme_accessory(theme, cv, x + p->lean, y, role);
    } else draw_by_code(cv, x, y, p, body, theme, role);
}
