/* Mockup (tools/mock, build.sh): the editor's gate page, where a gate's appearing is edited frame by frame as keyframes
 * of its parts (where each part is, how it is turned, whether it shows, its glow), so one animation fits every room size.
 * A: a dope sheet (a row per group of parts, keyframes as diamonds); B: one track for the picked part, a bigger stage.
 *   gate_editor_mock OUT_DIR */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "art.h"
#include "fixture.h"
#include "theme.h"

Canvas render(int cols, int rows, Room *room, double now, double t, char *note, size_t note_n);
#define BG RGB(18, 18, 24)
#define DIM RGB(110, 110, 130)
#define TXT RGB(200, 200, 215)
#define YEL RGB(250, 210, 70)
#define FLD RGB(34, 34, 44)
static Rgb mix(Rgb a, Rgb b, double t) { return RGB((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t)); }
static void box(Canvas *c, int x, int y, int w, int h, Rgb k) { canvas_rect(c, x - 1, y - 1, w + 2, 1, k); canvas_rect(c, x - 1, y + h, w + 2, 1, k); canvas_rect(c, x - 1, y - 1, 1, h + 2, k); canvas_rect(c, x + w, y - 1, 1, h + 2, k); }
static void button(Canvas *c, int x, int y, const char *t, int on) { int w = text_width(t) + 6; canvas_rect(c, x, y, w, 9, on ? RGB(90, 90, 120) : FLD); if (on) box(c, x, y, w, 9, YEL); canvas_text(c, x + 3, y + 2, t, on ? RGB(235, 235, 245) : RGB(150, 155, 185)); }
static int bw(const char *t) { return text_width(t) + 6; }
static void diamond(Canvas *c, int x, int y, Rgb k) { canvas_px(c, x, y - 2, k); canvas_rect(c, x - 1, y - 1, 3, 1, k); canvas_rect(c, x - 2, y, 5, 1, k); canvas_rect(c, x - 1, y + 1, 3, 1, k); canvas_px(c, x, y + 2, k); }

/* the stage: the coffee shop with its stairs part way in: steps and platform in place, the railing's posts flying in, the
 * picked post outlined, its path dotted from its first keyframe */
static Canvas stage(int cols, int rows, int *sx, int *sy) {
    FILE *f = fopen("/tmp/clawd-mock-gate-editor.fx", "w");
    fprintf(f, "theme coffee\ntitle coffee\nterminal %d %d\ntime 1000.0 3.4\nscale 1\nneighbor up office\nsession s00 name=api created=900 state=work tool=Bash\nsession s01 name=web created=910 state=think tool=Edit hold=1000.4\n", cols, rows);
    fclose(f);
    static Fixture fx;
    fixture_load(&fx, "/tmp/clawd-mock-gate-editor.fx");
    unlink("/tmp/clawd-mock-gate-editor.fx");
    for (int i = 0; i < fx.room.npool; i++) { fx.room.pool[i].has_pos = 0; fx.room.pool[i].created = fx.now - 100; fx.room.pool[i].last = fx.now - 1; }
    fx.room.freeze = 0;
    char note[64];
    Canvas after = {0};
    for (int k = 0; k < 40; k++) { if (after.p) canvas_free(&after); after = render(fx.cols, fx.rows, &fx.room, fx.now + k * 0.0625, fx.t + k * 0.0625, note, sizeof note); } /* (the Clawds at their desks) */
    const Geo *g = &fx.room.geo;
    Canvas rail = canvas_new(after.w, after.h, RGB(255, 0, 255)); /* the railing alone */
    theme_stair_rail(fx.room.theme, &rail, g, 1.0);
    Canvas plain = canvas_new(after.w, after.h, RGB(255, 0, 255)); /* the room without the railing: the stairs, before it */
    theme_gate(fx.room.theme, &plain, g, SIDE_UP, 1.0, 0, 0);
    theme_gate(fx.room.theme, &plain, g, SIDE_UP, 1.0, 0, 1);
    Canvas c = canvas_new(after.w, after.h, BG);
    memcpy(c.p, after.p, sizeof(Rgb) * (size_t)(c.w * c.h));
    int w = c.w;
    /* take the railing out (put back what is behind it: the plain stairs, or the room's wall) */
    for (int i = 0; i < w * c.h; i++) if (!rgb_eq(rail.p[i], RGB(255, 0, 255))) {
        int x = i % w, y = i / w; Rgb k = rgb_eq(plain.p[i], RGB(255, 0, 255)) ? after.p[(y > 3 ? y - 3 : y) * w + x] : plain.p[i];
        c.p[i] = k;
    }
    /* the posts (columns of the rail below its top line), each a part: some in place, some on the way */
    int x0 = w, x1 = 0;
    for (int i = 0; i < w * c.h; i++) if (!rgb_eq(rail.p[i], RGB(255, 0, 255))) { int x = i % w; if (x < x0) x0 = x; if (x > x1) x1 = x; }
    int post = 0, picked_x = -1, picked_y0 = 0, picked_y1 = 0;
    for (int x = x0; x <= x1; x++) {
        int top = -1, bot = -1;
        for (int y = 0; y < c.h; y++) if (!rgb_eq(rail.p[y * w + x], RGB(255, 0, 255))) { if (top < 0) top = y; bot = y; }
        if (top < 0 || bot - top < 3) continue; /* (the hand rail alone: not a post) */
        int k = post++;
        double q = k < 6 ? 1.0 : k < 11 ? 0.8 - (k - 6) * 0.15 : 0; /* (the first posts are in, the next ones on the way) */
        if (q <= 0) continue;
        int dx = (int)lround((1 - q) * -10), dy = (int)lround((1 - q) * -14);
        for (int y = top + 2; y <= bot; y++) { int X = x + dx, Y = y + dy; if (X >= 0 && X < w && Y >= 0 && Y < c.h) c.p[Y * w + X] = q < 1 ? mix(rail.p[y * w + x], RGB(170, 225, 255), 0.35) : rail.p[y * w + x]; }
        if (k == 7) { picked_x = x + dx; picked_y0 = top + 2 + dy; picked_y1 = bot + dy; }
    }
    if (picked_x >= 0) { /* the picked post: outlined; its path dotted back to where it starts (its first keyframe) */
        box(&c, picked_x, picked_y0, 1, picked_y1 - picked_y0 + 1, YEL);
        for (int s = 1; s <= 10; s += 2) canvas_px(&c, picked_x - s, picked_y0 - s * 14 / 10, YEL);
        box(&c, picked_x - 11, picked_y0 - 15, 1, picked_y1 - picked_y0 + 1, RGB(160, 140, 60)); /* (where it starts) */
    }
    *sx = picked_x; *sy = picked_y0;
    canvas_free(&after); canvas_free(&rail); canvas_free(&plain);
    return c;
}

static int title(Canvas *c, const char *opt) {
    canvas_text(c, 4, 3, "EDIT GATE  CONSTRUCT  APPEARING", YEL);
    canvas_text(c, c->w - text_width(opt) - 4, 3, opt, DIM);
    return 12;
}
/* the panel: the room and gate shown, the style, the picked part and its values at this frame */
static void panel(Canvas *c, int x, int y) {
    canvas_text(c, x, y, "SHOW", DIM); y += 7;
    int bx = x; button(c, bx, y, "OFFICE", 0); bx += bw("OFFICE") + 2; button(c, bx, y, "COFFEE", 1); bx += bw("COFFEE") + 2; button(c, bx, y, "PARK", 0); y += 11;
    bx = x; button(c, bx, y, "DOOR", 0); bx += bw("DOOR") + 2; button(c, bx, y, "STAIRS", 1); bx += bw("STAIRS") + 2; button(c, bx, y, "WELL", 0); y += 11;
    bx = x; button(c, bx, y, "APPEAR", 1); bx += bw("APPEAR") + 2; button(c, bx, y, "LEAVE", 0); y += 14;
    canvas_text(c, x, y, "PART", DIM); y += 7;
    canvas_text(c, x, y, "BALUSTER 7", TXT); canvas_text(c, x + 46, y, "IN BALUSTERS", DIM); y += 9;
    static const char *K[] = {"MOVE X", "MOVE Y", "TURN", "SHOW", "GLOW"}, *V[] = {"-4", "-6", "0", "YES", "35"};
    for (int i = 0; i < 5; i++) {
        canvas_text(c, x + 3, y, K[i], RGB(170, 170, 190));
        int vx = x + 70 - text_width(V[i]);
        if (i == 1) { canvas_rect(c, x + 44, y - 1, 30, 7, RGB(60, 60, 80)); box(c, x + 44, y - 1, 30, 7, YEL); canvas_text(c, x + 46, y, "<", YEL); canvas_text(c, x + 70, y, ">", YEL); vx -= 6; }
        canvas_text(c, vx, y, V[i], i == 1 ? YEL : RGB(230, 200, 120));
        y += 8;
    }
    y += 3;
    button(c, x, y, "+ KEY", 0); button(c, x + bw("+ KEY") + 2, y, "- KEY", 0); button(c, x + bw("+ KEY") + bw("- KEY") + 4, y, "EASE OUT", 0); y += 13;
    canvas_text(c, x, y, "CLICK A PART ON THE STAGE", DIM); y += 7;
    canvas_text(c, x, y, "G: ITS WHOLE GROUP", DIM);
}
/* the timeline: the time ruler (16 frames a second), the playhead, keyframes; A has a row per group of parts */
static void ruler(Canvas *c, int x, int y, int w, double at, int frames) {
    canvas_rect(c, x, y + 6, w, 1, RGB(60, 60, 76));
    for (int f = 0; f <= frames; f++) {
        int fx = x + f * (w - 1) / frames;
        canvas_rect(c, fx, y + (f % 4 == 0 ? 3 : 5), 1, f % 4 == 0 ? 3 : 1, RGB(90, 90, 110));
        if (f % 8 == 0) { char t[8]; snprintf(t, sizeof t, "%.1f", f / 16.0); canvas_text(c, fx - text_width(t) / 2, y - 3, t, DIM); }
    }
    int px = x + (int)lround(at * (w - 1));
    canvas_rect(c, px, y + 2, 1, 6, YEL);
}
static void dope_sheet(Canvas *c, int y, int H) {
    static const char *ROWS[] = {"STEPS", "PLATFORM", "BALUSTERS", "BALUSTER 7", "HAND RAIL", "OPENING"};
    static const double KEYS[6][4] = {{0, 0.3, -1}, {0.2, 0.5, -1}, {0.35, 0.75, -1}, {0.45, 0.62, 0.7, -1}, {0.6, 0.85, -1}, {0.8, 1.0, -1}};
    int lx = 4, tx = 56, tw = c->w - tx - 6, frames = 22; /* (1.4 s at 16 a second) */
    double at = 0.55;
    canvas_rect(c, 0, y - 3, c->w, 1, RGB(44, 44, 56));
    int bx = lx; button(c, bx, y, "PLAY", 0); bx += bw("PLAY") + 2; button(c, bx, y, "1X", 0); bx += bw("1X") + 2;
    canvas_text(c, bx + 4, y + 2, "FRAME 12 OF 23   0.69 S   LENGTH 1.4 S", TXT);
    canvas_text(c, c->w - text_width("< > FRAME  K KEY") - 4, y + 2, "< > FRAME  K KEY", DIM);
    y += 16;
    ruler(c, tx, y, tw, at, frames);
    y += 10;
    for (int r = 0; r < 6 && y + 7 <= H; r++) {
        int on = r == 3;
        if (on) canvas_rect(c, 0, y - 1, c->w, 8, RGB(32, 32, 44));
        canvas_text(c, lx + (r == 3 ? 4 : 0), y, ROWS[r], on ? YEL : r == 2 ? TXT : RGB(170, 170, 190));
        canvas_rect(c, tx, y + 3, tw, 1, RGB(40, 40, 52));
        double k0 = KEYS[r][0];
        for (int k = 0; k < 4 && KEYS[r][k] >= 0; k++) {
            int kx = tx + (int)lround(KEYS[r][k] * (tw - 1));
            if (k > 0) canvas_rect(c, tx + (int)lround(k0 * (tw - 1)), y + 3, kx - (tx + (int)lround(k0 * (tw - 1))), 1, on ? RGB(150, 130, 60) : RGB(80, 80, 100)); /* (moving between keys) */
            diamond(c, kx, y + 3, on ? YEL : RGB(200, 200, 215));
            k0 = KEYS[r][k];
        }
        y += 9;
    }
    int px = tx + (int)lround(at * (tw - 1));
    canvas_rect(c, px, y - 9 * 6 - 1, 1, 9 * 6, RGB(250, 210, 70)); /* (the playhead down through the rows) */
}
static void one_track(Canvas *c, int y) {
    int tx = 4, tw = c->w - 8;
    canvas_rect(c, 0, y - 3, c->w, 1, RGB(44, 44, 56));
    int bx = tx; button(c, bx, y, "PLAY", 0); bx += bw("PLAY") + 2; button(c, bx, y, "1X", 0); bx += bw("1X") + 2;
    canvas_text(c, bx + 4, y + 2, "BALUSTER 7   FRAME 12 OF 23   0.69 S", TXT);
    y += 16;
    ruler(c, tx, y, tw, 0.55, 22);
    y += 10;
    canvas_rect(c, tx, y + 3, tw, 1, RGB(40, 40, 52));
    static const double K[] = {0.45, 0.62, 0.7};
    canvas_rect(c, tx + (int)lround(0.45 * (tw - 1)), y + 3, (int)lround(0.25 * (tw - 1)), 1, RGB(150, 130, 60));
    for (int k = 0; k < 3; k++) diamond(c, tx + (int)lround(K[k] * (tw - 1)), y + 3, YEL);
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "out/10-gate-editor";
    int W = 330, H = 262;
    /* A: the stage at the left top, the panel on its right, the dope sheet along the bottom */
    Canvas a = canvas_new(W, H, BG);
    int y = title(&a, "A  DOPE SHEET"), sx, sy;
    Canvas st = stage(150, 50, &sx, &sy);
    int cw = st.w < 200 ? st.w : 200, ch = st.h < 130 ? st.h : 130, cx = sx - 100, cy = sy - 40;
    if (cx + cw > st.w) cx = st.w - cw; if (cx < 0) cx = 0; if (cy + ch > st.h) cy = st.h - ch; if (cy < 0) cy = 0;
    Canvas crop = canvas_new(cw, ch, BG);
    for (int yy = 0; yy < ch; yy++) for (int xx = 0; xx < cw; xx++) if (cx + xx < st.w && cy + yy < st.h) crop.p[yy * cw + xx] = st.p[(cy + yy) * st.w + cx + xx];
    canvas_blit(&a, &crop, 4, y); box(&a, 4, y, cw, ch, RGB(90, 90, 110));
    panel(&a, 4 + cw + 10, y);
    dope_sheet(&a, y + ch + 8, H);
    /* B: a bigger stage (twice), one track for the picked part */
    Canvas b = canvas_new(W, H, BG);
    y = title(&b, "B  ONE TRACK");
    int cw2 = 104, ch2 = 84, cx2 = sx - 50, cy2 = sy - 20;
    if (cx2 + cw2 > st.w) cx2 = st.w - cw2; if (cx2 < 0) cx2 = 0; if (cy2 + ch2 > st.h) cy2 = st.h - ch2; if (cy2 < 0) cy2 = 0;
    Canvas c2 = canvas_new(cw2, ch2, BG);
    for (int yy = 0; yy < ch2; yy++) for (int xx = 0; xx < cw2; xx++) if (cx2 + xx < st.w && cy2 + yy < st.h) c2.p[yy * cw2 + xx] = st.p[(cy2 + yy) * st.w + cx2 + xx];
    Canvas c2s = canvas_scaled(&c2, 2);
    canvas_blit(&b, &c2s, 4, y); box(&b, 4, y, c2s.w, c2s.h, RGB(90, 90, 110));
    panel(&b, 4 + c2s.w + 10, y);
    one_track(&b, y + c2s.h + 8);
    Canvas sheet = canvas_new(2 * W + 12, H + 4, RGB(24, 24, 30));
    canvas_blit(&sheet, &a, 4, 2); canvas_blit(&sheet, &b, W + 8, 2);
    char p[400];
    snprintf(p, sizeof p, "%s/gate-editor.png", dir);
    png_write(p, &sheet, 3);
    fprintf(stderr, "wrote %s\n", p);
    return 0;
}
