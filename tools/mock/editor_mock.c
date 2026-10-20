/* Mockup (tools/mock, build.sh): pictures of the pixel editor drawn by its own draw() (editor.c is included). The first
 * mockup of the animation editor (a timeline or a filmstrip, 2026-10-20) is kept in the vault; since the editor has the
 * timeline, play, onion skin and fill, this draws the real one in a few states:
 *   editor_mock OUT_DIR SPRITES_DIR */
#include "editor.c"
#include "art.h"

static Ed E;
static void shot(const char *dir, const char *name, int W, int H, void (*set)(Ed *), const char *out) {
    memset(&E, 0, sizeof E);
    E.pk = 1; E.pi = 0; E.hx = E.hy = -1; E.drag = -1; E.speed = 1; E.onion = 1; E.del_ask = -1;
    int which = 0;
    for (int f = 0; f < NFILES; f++) { /* (the file that has NAME) */
        static File tf;
        memset(&tf, 0, sizeof tf);
        char p[700];
        snprintf(p, sizeof p, "%s/%s", dir, FILES[f]);
        int has = !file_load(&tf, p) && find_sprite(&tf, name) >= 0;
        file_free(&tf);
        if (has) { which = f; break; }
    }
    file_open(&E, dir, which, name);
    if (set) set(&E);
    Canvas cv = canvas_new(W, H, RGB(18, 18, 24));
    draw(&E, &cv);
    png_write(out, &cv, 3);
    canvas_free(&cv);
    fprintf(stderr, "wrote %s\n", out);
}
static void fill_on(Ed *e) { e->filling = 1; e->hx = 7; e->hy = 3; }      /* FILL: the area under the pointer outlined */
static void fill_all(Ed *e) { e->filling = 2; e->hx = 3; e->hy = 2; }     /* FILL ALL */
static void playing(Ed *e) { e->playing = 1; e->play_t0 = wall_now() - 0.35; } /* PLAY: the frame playing, its bounce */
int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/09-editor-animation", *dir = argc > 2 ? argv[2] : "sprites";
    char p[400];
    snprintf(p, sizeof p, "%s/editor-walk-onion-fill.png", o); shot(dir, "walk-2", 300, 200, fill_on, p);
    snprintf(p, sizeof p, "%s/editor-cheer-playing.png", o); shot(dir, "cheer", 300, 200, playing, p);
    snprintf(p, sizeof p, "%s/editor-think-nine-frames.png", o); shot(dir, "think-7", 220, 180, NULL, p);
    snprintf(p, sizeof p, "%s/editor-window-fill-all.png", o); shot(dir, "window", 300, 200, fill_all, p);
    return 0;
}
