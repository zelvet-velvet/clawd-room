/* A picture of the editor's stage page (c/stage.inc), drawn by its own stage_draw(): the swing and the barista, at a frame.
 *   stage_shot OUT_DIR SPRITES_DIR   (writes stage-swing.png, stage-barista.png) */
#include "editor.c"
#include "art.h"

static void shot(const char *dir, const char *name, int frame, int pick, int tool, int help, int asking, const char *out) {
    static Stage s;
    if (stage_open(&s, name)) { fprintf(stderr, "no %s\n", name); return; }
    s.frame = frame; s.pick = pick; s.tool = tool; s.help = help; s.asking = asking;
    if (asking == 2) { s.asking = 0; s.a.part[3].key[0].pose.turn = 25; s.a.part[4].key[0].pose.turn = -15; }
    if (asking == 2) snprintf(s.msg, sizeof s.msg, "KEY ADDED: SEAT AT FRAME 1");
    Canvas cv = canvas_new(300, 215, ST_BG);
    stage_draw(&s, &cv, 0);
    png_write(out, &cv, 3);
    fprintf(stderr, "wrote %s\n", out);
    canvas_free(&cv);
    (void)dir;
}
int main(int argc, char **argv) {
    const char *o = argc > 1 ? argv[1] : "out/18-stage", *dir = argc > 2 ? argv[2] : "sprites";
    char p[700];
    snprintf(p, sizeof p, "%s/clawd.txt", dir); static SpriteFile frames, items, props;
    frames = sprites_load(p);
    snprintf(p, sizeof p, "%s/items.txt", dir); items = sprites_load(p);
    snprintf(p, sizeof p, "%s/props.txt", dir); props = sprites_load(p);
    theme_set_props(&props);
    draw_set_sprites(&frames, &items);
    char rd[700]; snprintf(rd, sizeof rd, "%s/../rooms", dir); theme_set_rooms_dir(rd);
    snprintf(rd, sizeof rd, "%s/../anims", dir); parts_set_dir(rd); parts_set_mine_dir("");
    snprintf(p, sizeof p, "%s/stage-swing.png", o); shot(dir, "swing", 14, 3, 0, 0, 0, p);
    snprintf(p, sizeof p, "%s/stage-barista.png", o); shot(dir, "brew", 9, 1, 0, 0, 0, p);
    snprintf(p, sizeof p, "%s/stage-help.png", o); shot(dir, "swing", 6, 4, 1, 1, 0, p);
    snprintf(p, sizeof p, "%s/stage-asking.png", o); shot(dir, "swing", 0, 3, 1, 0, 1, p);
    snprintf(p, sizeof p, "%s/stage-turn.png", o); shot(dir, "swing", 0, 3, 2, 0, 2, p);
    return 0;
}
