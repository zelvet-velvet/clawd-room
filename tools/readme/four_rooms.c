/* The README's room pictures, drawn by the current program with no window frame: a room each (office, coffee, park) and the four rooms
 * together. Made-up session names only.   readme_shots OUT_DIR */
#include "../../c/editor.c"
#include "../../c/art.h"

static Canvas room_shot(const char *text) {
    static Fixture fx;
    char note[64];
    if (fixture_load_text(&fx, text)) return canvas_new(8, 8, RGB(0, 0, 0));
    fx.room.freeze = 0; /* (a fixture is frozen: here the room runs) */
    double now = fx.now, t = fx.t; /* the room runs for a while (16 frames a second): everyone walks to their desk */
    for (int k = 0; k < 320; k++) { Canvas c = render(fx.cols, fx.rows, &fx.room, now, t, note, sizeof note); canvas_free(&c); now += 1.0 / 16; t += 1.0 / 16; }
    return render(fx.cols, fx.rows, &fx.room, now, t, note, sizeof note);
}
static void room_text(char *out, size_t n, const char *theme, const char *extra) {
    snprintf(out, n, "theme %s\ntitle %s\nterminal 168 50\ntime 1000.0 3.4\nscale 1\n%s"
        "session s00 name=web-shop created=900 state=work tool=Bash\nsession s01 name=api-server created=910 state=wait tool=mcp__x__y\nsession s02 name=docs-site created=920 state=think tool=Edit hold=1000.4\n", theme, theme, extra);
}

int main(int argc, char **argv) {
    const char *dir = "sprites", *out = argc > 1 ? argv[1] : "four-rooms.png";
    char p[700]; static SpriteFile frames, items, props;
    snprintf(p, sizeof p, "%s/clawd.txt", dir); frames = sprites_load(p);
    snprintf(p, sizeof p, "%s/items.txt", dir); items = sprites_load(p);
    snprintf(p, sizeof p, "%s/props.txt", dir); props = sprites_load(p);
    theme_set_props(&props); ui_watch(dir); draw_set_sprites(&frames, &items);
    theme_set_rooms_dir("rooms"); parts_set_dir("anims"); parts_set_mine_dir("");
    static const char *const G[4] = { /* (each room with the gates to its neighbours) */
        "peer beta right\npeer gamma down\nneighbor right coffee\nneighbor down park\nlinkpos right 33\nlinkpos down 25.5\ngate right 1.0\ngate down 1.0\n",
        "peer alpha left\nneighbor left office\nlinkpos left 40.5\ngate left 1.0\n",
        "peer alpha up\nneighbor up office\nlinkpos up 70\ngate up 1.0\n",
        "peer delta left\nneighbor left park\nlinkpos left 40.5\ngate left 1.0\n"};
    static const char *const TH[4] = {"office", "coffee", "park", "office"};
    char text[2000];
    Canvas parts[4];
    int cw = 0, ch = 0;
    for (int i = 0; i < 4; i++) {
        room_text(text, sizeof text, TH[i], G[i]);
        parts[i] = room_shot(text);
        cw = parts[i].w; ch = parts[i].h;
    }
    Canvas sheet = canvas_new(cw * 2, ch * 2, RGB(24, 24, 32));
    for (int i = 0; i < 4; i++) { canvas_blit(&sheet, &parts[i], (i % 2) * cw, (i / 2) * ch); canvas_free(&parts[i]); }
    png_write(out, &sheet, 6);
    return 0;
}
