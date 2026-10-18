/* The gallery and the overlap check (Engine Design, step 1): every room x gates x empty / some / crowded x three window
 * sizes, each run until the Clawds have settled, then measured.
 *   --check            one line per item painting over another where it must not (tools/test.sh keeps a snapshot)
 *   --gallery DIR      the same, plus a contact sheet per room: DIR/gallery-office.png, -coffee, -park */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "art.h"
#include "fixture.h"
#include "scene.h"

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
void draw_set_measure(int on);
const Scene *draw_last_scene(void);

static const char *THEMES[] = {"office", "coffee", "park"};
static const struct { const char *name, *lines; } GATES[] = {
    {"none", ""},
    {"all", "neighbor left office\nneighbor right office\nneighbor up office\nneighbor down office\n"},
    {"left+up", "neighbor left office\nneighbor up office\n"},
    {"right+down", "neighbor right office\nneighbor down office\n"},
};
static const int PEOPLE[] = {0, 3, 8};
static const int SIZES[][2] = {{110, 32}, {181, 53}, {240, 70}};
static const char *SESSIONS[] = {
    "session s00 name=clawd-room created=900 state=work tool=Bash",
    "session s01 name=park created=910 state=think tool=Edit hold=1000.4",
    "session s02 name=api created=920 state=wait tool=mcp__x__y",
    "session s03 name=web created=930 state=idle tool=Read",
    "session s04 name=docs created=940 state=cheer tool=Foo state_t=999.0",
    "session s05 name=infra created=950 state=sleep tool=Task state=idle last=800.0",
    "session s06 name=tests created=960 state=work tool=Bash",
    "session s07 name=notes created=970 state=think tool=Edit hold=1000.4",
};
#define N(a) ((int)(sizeof a / sizeof a[0]))

/* which kinds must not paint over each other: gates, wall items, bushes, the playground, desks, the railing */
static char cat(const char *kind) {
    static const char *const K[] = {"gate-left", "gate-right", "gate-up", "gate-down", "opening", "platform", "building", "streetlamp", "bench", "sign", "signpost", "window",
                                    "plant", "menu", "lamp", "shelf", "bush", "swing", "slide", "desk", "rail"};
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) if (!strcmp(kind, K[i])) return 1;
    return 0;
}
static int allowed(const char *a, const char *b) {
    static const char *const OK[][2] = {
        {"rail", "gate-up"}, {"rail", "platform"}, {"rail", "opening"}, {"platform", "gate-up"}, {"opening", "platform"}, /* one structure */
        {"gate-left", "building"}, {"gate-right", "building"}, /* its door */
        {"desk", "platform"}, {"desk", "building"}, {"swing", "building"}, {"slide", "building"}, {"gate-up", "building"}, {"platform", "building"}, {"rail", "building"}, {"opening", "building"}, /* (the park's stairs, terrace and tower against the building) */
        {"opening", "gate-up"}, {"gate-right", "gate-up"}, {"gate-left", "gate-up"}, /* (the park: the door of the building behind the steps; the side doors stand in front of it) */ /* standing in front of the platform's face or a plain fence */
    };
    for (size_t i = 0; i < sizeof OK / sizeof OK[0]; i++)
        if ((!strcmp(a, OK[i][0]) && !strcmp(b, OK[i][1])) || (!strcmp(b, OK[i][0]) && !strcmp(a, OK[i][1]))) return 1;
    return 0;
}

static int one(const char *theme, int g, int people, int cols, int rows, Canvas *out, int *problems) {
    char path[256], label[64];
    snprintf(path, sizeof path, "%s/clawd-gallery-%d.fx", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "theme %s\ntitle %s\nterminal %d %d\ntime 1000.0 3.4\n%s", theme, theme, cols, rows, GATES[g].lines);
    for (int i = 0; i < people; i++) fprintf(f, "%s\n", SESSIONS[i]);
    fclose(f);
    static Fixture fx;
    int rc = fixture_load(&fx, path);
    unlink(path);
    if (rc) return 1;
    for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; }
    fx.room.freeze = 0;
    char note[32];
    for (int k = 0; k < 40; k++) { /* the Clawds walk to their places */
        draw_set_measure(k == 39);
        Canvas cv = render(fx.cols, fx.rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note);
        if (k == 39 && out) *out = cv;
        else canvas_free(&cv);
    }
    draw_set_measure(0);
    const Scene *sc = draw_last_scene();
    snprintf(label, sizeof label, "%s %s %d %dx%d", theme, GATES[g].name, people, cols, rows);
    for (int h = 0; h < sc->nhit; h++) {
        const SceneItem *a = &sc->it[sc->hit[h].a], *b = &sc->it[sc->hit[h].b];
        if (!cat(a->kind) || !cat(b->kind) || allowed(a->kind, b->kind)) continue;
        printf("%-34s %s over %s (%d px)\n", label, b->kind, a->kind, sc->hit[h].n);
        (*problems)++;
    }
    return 0;
}

int gallery_run(const char *dir) {
    int problems = 0;
    for (int th = 0; th < N(THEMES); th++) {
        int cw = SIZES[N(SIZES) - 1][0] + 4, ch = SIZES[N(SIZES) - 1][1] * 2 + 10;
        int ncol = N(PEOPLE), nrow = N(GATES) * N(SIZES); /* a column each for empty / some / crowded; a row each for gates x size */
        Canvas sheet = {0, 0, NULL};
        if (dir) sheet = canvas_new(ncol * cw + 4, nrow * ch + 4, RGB(24, 24, 30));
        for (int g = 0; g < N(GATES); g++)
            for (int p = 0; p < N(PEOPLE); p++)
                for (int s = 0; s < N(SIZES); s++) {
                    Canvas cv = {0, 0, NULL};
                    if (one(THEMES[th], g, PEOPLE[p], SIZES[s][0], SIZES[s][1], dir ? &cv : NULL, &problems)) { fprintf(stderr, "gallery: could not build a scene\n"); return 1; }
                    if (dir && cv.p) {
                        int x = 4 + p * cw, y = 4 + (g * N(SIZES) + s) * ch;
                        char label[48];
                        snprintf(label, sizeof label, "%s %d %dX%d", GATES[g].name, PEOPLE[p], SIZES[s][0], SIZES[s][1]);
                        for (char *q = label; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
                        canvas_text(&sheet, x, y, label, RGB(200, 200, 220));
                        canvas_blit(&sheet, &cv, x, y + 7);
                        canvas_free(&cv);
                    }
                }
        if (dir) {
            char path[512];
            snprintf(path, sizeof path, "%s/gallery-%s.png", dir, THEMES[th]);
            if (png_write(path, &sheet, 2)) fprintf(stderr, "gallery: cannot write %s\n", path);
            else fprintf(stderr, "wrote %s\n", path);
            canvas_free(&sheet);
        }
    }
    printf("%d overlap problem%s\n", problems, problems == 1 ? "" : "s");
    return 0;
}
