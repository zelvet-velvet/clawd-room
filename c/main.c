/* clawd-room, C port. Stage 1: canvas, font, sprite files, Clawd poses and drawing.
 *   ./clawd-room --golden   prints "action step hash" for every action (compared with tests/expected by tools/test.sh)
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parts.h"
#include "settings.h"
#include "canvas.h"
#include "theme.h"
#include "clawd.h"
#include "term.h"
#include "fixture.h"
#include "rng.h"
#include <time.h>
#include <unistd.h>
#include "actions.h"
#include "app.h"
#include "link.h"
#include "win.h"
#include "script.h"
#include "tail.h"
#include "ui.h"

int gallery_run(const char *dir); /* gallery.c: --check, --gallery DIR */
static int golden(const SpriteFile *frames) {
    for (int a = 0; a < N_ACTIONS; a++) {
        Session s;
        session_init(&s, "x", 1000.0);
        s.seed = 0.0;
        s.state_t = 1000.0;
        ACTIONS[a].set(&s);
        for (int i = 0; i < 41; i++) {
            double t = 0.2 + i / 12.0;
            s.walk_phase = t * 12;
            Pose p = pose_for(&s, ACTIONS[a].state, t, 1000.0 + t);
            Canvas cv = canvas_new(26, 18, RGB(0, 0, 0));
            draw_clawd(&cv, 13, 8, &p, frames, C_CLAWD, NULL, "");
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
    for (int a = 0; a < N_ACTIONS; a++) {
        Session s;
        session_init(&s, "x", 1000.0);
        s.seed = 0.0;
        s.state_t = 1000.0;
        ACTIONS[a].set(&s);
        Cells prev = {0, 0, NULL};
        for (int i = 0; i < 41; i++) {
            double t = 0.2 + i / 12.0;
            s.walk_phase = t * 12;
            Pose p = pose_for(&s, ACTIONS[a].state, t, 1000.0 + t);
            Canvas cv = canvas_new(26, 18, RGB(0, 0, 0));
            draw_clawd(&cv, 13, 8, &p, frames, C_CLAWD, NULL, "");
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

/* --fixture FILE...: one line per scene: name, hash of the terminal canvas, note */
static int run_fixtures(int n, char **files, const SpriteFile *frames, const SpriteFile *items) {
    draw_set_sprites(frames, items);
    for (int i = 0; i < n; i++) {
        static Fixture f;
        if (fixture_load(&f, files[i])) { fprintf(stderr, "cannot read %s\n", files[i]); return 1; }
        char note[32];
        Canvas cv = render(f.cols, f.rows, &f.room, f.now, f.t, note, sizeof note);
        const char *base = strrchr(files[i], '/');
        printf("fixture %s %016" PRIx64 " %s\n", base ? base + 1 : files[i], canvas_hash(&cv), note);
        canvas_free(&cv);
    }
    return 0;
}

/* the random numbers a few seeds give (the snapshot was made with Python's random.Random) */
static int golden_rng(void) {
    static const char *SIDS[] = {"x", "aaa", "619c9f42-0144-479e-bc6c-ca68e665dc6f", ""};
    for (int i = 0; i < 4; i++) {
        Rng r;
        rng_seed(&r, crc32_str(SIDS[i]));
        printf("rng %d", i);
        for (int k = 0; k < 6; k++) printf(" %.17g", rng_random(&r));
        for (int k = 0; k < 4; k++) printf(" %.17g", rng_uniform(&r, 1.0, 4.0));
        for (int k = 0; k < 8; k++) printf(" %d", rng_choice(&r, 1 + k));
        for (int k = 0; k < 2000; k++) rng_random(&r); /* past one refill of the 624-word state */
        printf(" %.17g\n", rng_random(&r));
    }
    return 0;
}

/* the pure helpers: the events reader, status line, auto scale and menu (tools/golden_ui.py prints the same) */
static int golden_ui(const char *path) {
    Tail t;
    tail_init(&t, path);
    static Event ev[64];
    int n = tail_history(&t, 1000.0, ev, 64);
    for (int i = 0; i < n; i++) printf("tail %.17g %s %s %s %s\n", ev[i].t, ev[i].sid, ev[i].ev, ev[i].cwd, ev[i].tool);
    tail_free(&t);
    char buf[4096];
    for (int paused = 0; paused < 2; paused++) for (int help = 0; help < 2; help++) {
        static const char *NF[4][2] = {{"", ""}, {"+3 MORE", ""}, {"", "SCALE 2X"}, {"+1 MORE", "FLASH"}};
        for (int k = 0; k < 4; k++) { overlay_text(buf, sizeof buf, paused, help, NF[k][0], NF[k][1]); printf("overlay %d %d [%s]\n", paused, help, buf); }
    }
    static const int SM[4][2] = {{30, 10}, {44, 19}, {10, 5}, {200, 80}};
    for (int i = 0; i < 4; i++) {
        too_small(buf, sizeof buf, SM[i][0], SM[i][1]);
        printf("small %d %d '", SM[i][0], SM[i][1]);
        for (char *p = buf; *p; p++) { if (*p == 27) printf("\\x1b"); else putchar(*p); }
        printf("'\n");
    }
    static const int PW[4] = {110, 160, 220, 300}, PH[4] = {64, 100, 140, 200}, NN[6] = {0, 1, 3, 6, 12, 40};
    for (int a = 0; a < 4; a++) for (int b = 0; b < 4; b++) {
        printf("auto %d %d", PW[a], PH[b]);
        for (int k = 0; k < 6; k++) printf(" %d", auto_scale(PW[a], PH[b], NN[k]));
        printf("\n");
    }
    static const int MS[6][2] = {{144, 84}, {110, 64}, {60, 40}, {200, 120}, {44, 30}, {46, 56}};
    for (int s = 0; s < 6; s++) for (int pick = 0; pick < 3; pick++) for (int solo = 0; solo < 2; solo++) for (int ti = 0; ti < 2; ti++) {
        double tt = ti ? 0.4 : 0.0;
        Canvas cv = canvas_new(MS[s][0], MS[s][1], RGB(14, 14, 18));
        draw_menu(&cv, pick, solo, tt);
        printf("menu %d %d %d %d %.1f %016" PRIx64 "\n", MS[s][0], MS[s][1], pick, solo, tt, canvas_hash(&cv));
        canvas_free(&cv);
    }
    static const int TS[4][2] = {{110, 32}, {200, 60}, {300, 100}, {90, 30}};
    for (int i = 0; i < 4; i++) {
        Canvas cv = render_menu(TS[i][0], TS[i][1], 1, 0, 0.4);
        printf("menuterm %d %d %016" PRIx64 "\n", TS[i][0], TS[i][1], canvas_hash(&cv));
        canvas_free(&cv);
    }
    return 0;
}

/* `--link-demo NAME [SECONDS]`: ping every new room, answer pings, print what happens */
static int link_demo(const char *name, double seconds) {
    Link l;
    link_init(&l, name, NULL);
    link_start(&l);
    printf("room %s is up (id %s); looking for other rooms...\n", l.name, l.id);
    fflush(stdout);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double end = ts.tv_sec + ts.tv_nsec / 1e9 + seconds;
    for (;;) {
        clock_gettime(CLOCK_REALTIME, &ts);
        double now = ts.tv_sec + ts.tv_nsec / 1e9;
        if (seconds && now >= end) break;
        LinkEvent ev[32];
        int n = link_update(&l, now, 0, 0, ev, 32);
        for (int i = 0; i < n; i++) {
            if (ev[i].kind == 0) { printf("joined: %s\n", ev[i].peer.name); link_send_num(&l, ev[i].peer.id, "ping", "sent", now); }
            else if (ev[i].kind == 1) printf("left: %s\n", ev[i].peer.name);
            else if (!strcmp(ev[i].type, "ping")) link_send_num(&l, ev[i].msg.peer, "pong", "sent", now);
            else if (!strcmp(ev[i].type, "pong")) printf("pong from %s\n", ev[i].msg.peer);
            else printf("got %s from %s\n", ev[i].type, ev[i].msg.peer);
        }
        fflush(stdout);
        usleep(100000);
    }
    link_close(&l);
    return 0;
}

/* the Windows geometry on synthetic inputs */
static int golden_win(void) {
    /* neighbour sides */
    static const int R[][4] = {{0, 0, 800, 600}, {830, 0, 1600, 600}, {800, 100, 1600, 700}, {-790, 0, 10, 600}, {0, 620, 800, 1200}, {0, -590, 800, 10}, {850, 0, 1650, 600}, {400, 300, 1200, 900}, {810, 560, 1700, 1100}};
    for (int a = 0; a < 9; a++) for (int b = 0; b < 9; b++) {
        printf("side %d %d", a, b);
        for (int slack = 0; slack <= 15; slack += 15) printf(" %d", neighbor_side(R[a], R[b], 30, slack));
        printf(" %d\n", neighbor_side(R[a], R[b], 60, 0));
    }
    /* snapping two grids */
    static const double G[][2][2] = {{{10.5, 8}, {3.25, 6}}, {{0, 8.5}, {4, 5}}, {{7.3, 13}, {1.1, 13}}, {{100.25, 20}, {200.5, 12}}};
    for (int i = 0; i < 4; i++) {
        int idx[2];
        snap_both(100.5, 230.25, G[i], 2, idx);
        int idy[2];
        snap_both(55, 56, G[i], 2, idy);
        printf("snap %d %d %d %d %d\n", i, idx[0], idx[1], idy[0], idy[1]);
    }
    /* cell size from the area or the terminal's report */
    static const int AR[2][4] = {{10, 40, 890, 650}, {0, 0, 1720, 880}};
    for (int a = 0; a < 2; a++) for (int rep = 0; rep < 3; rep++) for (int dpi = 96; dpi <= 120; dpi += 24) {
        Reports rp = {0};
        if (rep == 1) { rp.has_cell = 1; rp.cell[0] = 10; rp.cell[1] = 20; }
        if (rep == 2) { rp.has_cell = 1; rp.cell[0] = 8; rp.cell[1] = 16; }
        double cw, ch;
        cell_px(AR[a], dpi, &rp, a ? 215 : 110, a ? 55 : 32, &cw, &ch);
        printf("cell %d %d %d %.6f %.6f\n", a, rep, dpi, cw, ch);
    }
    /* the window size that holds 110x32 cells */
    {
        Reports rp = {0};
        OpenSetup o;
        Tool t = {0};
        opensetup_init(&o, &t, "x", 8, "110x32", &rp, 1, 14);
        double px[2];
        int rect[4] = {183, 192, 1922, 1082}, area[4] = {192, 243, 1913, 1073};
        int ok = opensetup_window_px(&o, rect, area, 215, 55, px) != 0;
        printf("px %d %.6f %.6f\n", ok, px[0], px[1]);
        opensetup_init(&o, &t, "x", 11, "128X36", &rp, 1, 14);
        ok = opensetup_window_px(&o, rect, area, 215, 55, px) != 0;
        printf("px %d %.6f %.6f\n", ok, px[0], px[1]);
        printf("px %d\n", opensetup_window_px(&o, rect, NULL, 215, 55, px) != 0);
    }
    /* the helper's lines */
    {
        Watcher w;
        memset(&w, 0, sizeof w);
        static const char *L[] = {"clawd-room alpha-1|10|20|800|600|18|60|792|592|120|40", "clawd-room we|ird|-5|0|300|200|5|30|295|195|96|30", "clawd-room y|1|2|3|4|5|6|7|8|9|x|11", "--"};
        for (int i = 0; i < 4; i++) watcher_feed(&w, L[i], 1000.0);
        for (int i = 0; i < w.ncur; i++) printf("win %s %d %d %d %d %d %d %d %d %d\n", w.cur[i].title, w.cur[i].rect[0], w.cur[i].rect[1], w.cur[i].rect[2], w.cur[i].rect[3], w.cur[i].area[0], w.cur[i].area[1], w.cur[i].area[2], w.cur[i].area[3], w.cur[i].dpi);
    }
    /* a Windows Terminal window split into panes: which pane is whose, and whether the window may be resized */
    {
        static Watcher w;
        static Link link;
        memset(&w, 0, sizeof w); memset(&link, 0, sizeof link);
        double now = (double)time(NULL);
        static const char *SPLIT[] = { /* (work pane on top with the focus, two rooms below; then a plain room window; rounds 1-3 our pane has the focus) */
            "-|0|0|1200|1300|8|40|1192|1290|96|40", "@42.9.4.8|1|0|10|50|1190|900", "@42.9.4.9|0|1|10|910|598|1290", "@42.9.4.10|0|1|604|910|1190|1290",
            "clawd-room solo-3|1300|0|2000|700|1308|40|1992|692|96|40", "@bad|x|1|2|3|4|5", "--"};
        const char *ids[3] = {"left-1", "right-2", "solo-3"};
        for (int round = 0; round < 6; round++) {
            for (size_t i = 0; i < sizeof SPLIT / sizeof SPLIT[0]; i++) {
                char line[128];
                snprintf(line, sizeof line, "%s", SPLIT[i]);
                if (round >= 1 && round < 4 && i == 0) snprintf(line, sizeof line, "clawd-room left-1|0|0|1200|1300|8|40|1192|1290|96|40"); /* our pane gets the focus */
                if (round >= 1 && round < 4 && i == 1) snprintf(line, sizeof line, "@42.9.4.8|0|0|10|50|1190|900");
                if (round >= 1 && round < 4 && i == 2) snprintf(line, sizeof line, "@42.9.4.9|1|1|10|910|598|1290");
                watcher_feed(&w, line, now + round);
            }
            if (round == 3) { link.npeers = 1; snprintf(link.peers[0].id, 40, "right-2"); snprintf(link.peers[0].pane, 40, "42.9.4.10"); }
            watcher_resolve(&w, "left-1", &link);
            printf("split round %d mine=%s", round, w.my_rid[0] ? w.my_rid : "?");
            for (int q = 0; q < 3; q++) {
                const int *a = watcher_area(&w, ids[q]);
                printf(" %s:%d", ids[q], watcher_in_pane(&w, ids[q]));
                if (a) printf("@%d,%d,%d,%d", a[0], a[1], a[2], a[3]);
            }
            printf(" n=%d\n", w.ncur);
        }
        /* opening in a pane: wait for the watcher, then leave the window and the font alone */
        Tool t = {0, -1};
        Reports rp = {0};
        OpenSetup o;
        opensetup_init(&o, &t, "x", 8, "110x32", &rp, 1, 5);
        int a1 = opensetup_update(&o, 100.0, 80, 24, NULL, NULL), st1 = o.stage;
        o.pane = 1;
        int a2 = opensetup_update(&o, 101.0, 80, 24, NULL, NULL);
        printf("split setup %d %d %d skipped=%d\n", a1, st1, a2, o.skipped);
        /* never told: after the wait the room leaves the window and the font alone as well */
        OpenSetup u;
        opensetup_init(&u, &t, "x", 8, "110x32", &rp, 1, 5);
        int b1 = opensetup_update(&u, 100.0, 80, 24, NULL, NULL);
        int b2 = opensetup_update(&u, 100.0 + OPEN_PANE_WAIT + 1, 80, 24, NULL, NULL);
        printf("split setup unknown %d %d skipped=%d\n", b1, b2, u.skipped);
    }
    /* gates open together: a room's "ready" times go through its heartbeat to the other room */
    {
        char dir[] = "/tmp/clawd-ready-XXXXXX";
        if (mkdtemp(dir)) {
            static Link la, lb;
            link_init(&la, "alpha", dir); link_init(&lb, "beta", dir);
            link_start(&la); link_start(&lb);
            snprintf(la.ready_id[0], 40, "%s", lb.id); la.ready_t[0] = 1000.25; la.nready = 1; la.ready_changed = 1;
            LinkEvent ev[8];
            link_update(&la, 2000.0, 80, 24, ev, 8);
            link_update(&lb, 2000.0, 80, 24, ev, 8);
            int found = 0;
            for (int p = 0; p < lb.npeers; p++)
                for (int k = 0; k < lb.peers[p].nready; k++)
                    if (!strcmp(lb.peers[p].id, la.id) && !strcmp(lb.peers[p].ready_id[k], lb.id)) { printf("gate ready heard %.2f changed=%d\n", lb.peers[p].ready_t[k], la.ready_changed); found = 1; }
            if (!found) printf("gate ready not heard\n");
            link_close(&la); link_close(&lb);
            rmdir(dir);
        }
    }
    /* two room panes opened quickly: ours never had the focus while the window showed our title (the other room's pane
     * has it), so ours is the one Clawd Room pane the other room does not claim */
    {
        static Watcher w;
        static Link link;
        memset(&w, 0, sizeof w); memset(&link, 0, sizeof link);
        double now = (double)time(NULL);
        static const char *QUICK[] = {"clawd-room right-2|0|0|1200|1300|8|40|1192|1290|96|40", "@7.1|0|0|10|50|1190|900",
                                      "@7.2|0|1|10|910|598|1290", "@7.3|1|1|604|910|1190|1290", "--"};
        for (int round = 0; round < 3; round++) {
            for (size_t i = 0; i < sizeof QUICK / sizeof QUICK[0]; i++) watcher_feed(&w, QUICK[i], now + round);
            if (round == 1) { link.npeers = 1; snprintf(link.peers[0].id, 40, "right-2"); snprintf(link.peers[0].pane, 40, "7.3"); }
            watcher_resolve(&w, "left-1", &link);
            const int *a = watcher_area(&w, "left-1");
            printf("split quick round %d mine=%s left-1:%d", round, w.my_rid[0] ? w.my_rid : "?", watcher_in_pane(&w, "left-1"));
            if (a) printf("@%d,%d,%d,%d", a[0], a[1], a[2], a[3]);
            printf("\n");
        }
    }
    /* the gates between two windows */
    for (int variant = 0; variant < 4; variant++) {
        static Room room;
        room_init(&room);
        room.has_geo = 1; room.geo.ymin = 50; room.geo.ymax = 120 - 8 * (variant & 1);
        room.view_x = 4 * (variant & 2); room.view_y = 2 * (variant & 2); room.view_k = 1 + (variant >> 1);
        static const char *SIDES_[4] = {"left", "right", "up", "down"};
        room.nnb = 4;
        for (int s = 0; s < 4; s++) { room.nb[s].side = s; snprintf(room.nb[s].id, sizeof room.nb[s].id, "peer-%s", SIDES_[s]); }
        static Watcher w;
        memset(&w, 0, sizeof w);
        char buf[200];
        double now = (double)time(NULL);
        snprintf(buf, sizeof buf, "clawd-room me|100|100|1000|700|108|150|992|692|96|40"); watcher_feed(&w, buf, now);
        snprintf(buf, sizeof buf, "clawd-room peer-left|-700|110|90|690|-692|160|82|682|96|40"); watcher_feed(&w, buf, now);
        snprintf(buf, sizeof buf, "clawd-room peer-right|1010|140|1900|760|1018|190|1892|752|144|40"); watcher_feed(&w, buf, now);
        snprintf(buf, sizeof buf, "clawd-room peer-up|150|-500|900|90|158|-450|892|82|96|40"); watcher_feed(&w, buf, now);
        snprintf(buf, sizeof buf, "clawd-room peer-down|120|710|1050|1300|128|760|1042|1292|96|40"); watcher_feed(&w, buf, now);
        watcher_feed(&w, "--", now);
        static Link link;
        memset(&link, 0, sizeof link);
        link.npeers = 4;
        for (int s = 0; s < 4; s++) {
            snprintf(link.peers[s].id, sizeof link.peers[s].id, "peer-%s", SIDES_[s]);
            if (variant >= 1 && s != 2) { link.peers[s].has_grid = 1; double g[7] = {1018, 190, 9.5, 19, 2, 3, 4}; memcpy(link.peers[s].grid, g, sizeof g); }
            if (variant >= 2 && s < 2) { link.peers[s].has_band = 1; link.peers[s].band[0] = 200; link.peers[s].band[1] = 600; }
        }
        Reports rp = {0};
        int area[4] = {108, 150, 992, 692};
        Grid g = grid_params(&room, area, 96, &rp, 110, 32);
        Passages ps;
        plan_passages(&room, &w, "me", &g, 1, &link, &ps);
        printf("gates %d %.6f %.6f %.6f %.6f", variant, g.cw, g.ch, g.k, g.y0);
        for (int s = 0; s < 4; s++) printf(" | %d %.1f %d %d %.6f", ps.has_pos[s], ps.pos[s], ps.closed[s], ps.has_scale[s], ps.scale[s]);
        printf("\n");
    }
    return 0;
}

/* where each part of the part animations in FILES is at some times (parts.c); a file that does not load says so */
static int golden_parts(int nfiles, char **files) {
    static const double T[] = {-0.1, 0, 0.2, 0.4, 0.8, 1.2, 1.6, 1.7, 2.5};
    for (int f = 0; f < nfiles; f++) {
        static PartAnim a;
        const char *base = strrchr(files[f], '/') ? strrchr(files[f], '/') + 1 : files[f];
        if (parts_load(&a, files[f])) { printf("%s: error\n", base); continue; }
        printf("%s: %s length %.2f %s parts %d\n", base, a.name, a.length, a.loop ? "loop" : "once", a.npart);
        for (int p = 0; p < a.npart; p++) for (size_t i = 0; i < sizeof T / sizeof *T; i++) {
            PartPose q = parts_pose(&a, p, T[i]);
            printf("  %-8s %-8s t=%5.2f x=%6.2f y=%6.2f turn=%7.2f glow=%6.2f show=%d grow=%6.2f\n", a.part[p].name, a.part[p].group, T[i], q.x + 0.0, q.y + 0.0, q.turn + 0.0, q.glow + 0.0, q.show, q.grow + 0.0); /* (+ 0.0: no "-0.00") */
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "--golden-parts")) return golden_parts(argc - 2, argv + 2);
    if (argc > 2 && !strcmp(argv[1], "--link-demo")) return link_demo(argv[2], argc > 3 ? atof(argv[3]) : 0);
    if (!(argc > 1 && (!strncmp(argv[1], "--golden", 8) || !strcmp(argv[1], "--fixture") || !strcmp(argv[1], "--script") || !strcmp(argv[1], "--check") || !strcmp(argv[1], "--gallery")))) return app_main(argc, argv);
    const char *dir = getenv("CLAWD_SPRITES") ? getenv("CLAWD_SPRITES") : "../sprites";
    char path[512];
    snprintf(path, sizeof path, "%s/clawd.txt", dir);
    SpriteFile frames = sprites_load(path), items;
    snprintf(path, sizeof path, "%s/items.txt", dir);
    items = sprites_load(path);
    snprintf(path, sizeof path, "%s/props.txt", dir);
    SpriteFile props = sprites_load(path);
    theme_set_props(&props);
    { char rd[600]; snprintf(rd, sizeof rd, "%s", dir); char *sl = strrchr(rd, '/'); if (sl) strcpy(sl, "/rooms"); else snprintf(rd, sizeof rd, "rooms"); theme_set_rooms_dir(getenv("CLAWD_ROOMS") ? getenv("CLAWD_ROOMS") : rd); }
    { char ad[600]; snprintf(ad, sizeof ad, "%s", dir); char *sl = strrchr(ad, '/'); if (sl) strcpy(sl, "/anims"); else snprintf(ad, sizeof ad, "anims"); parts_set_dir(getenv("CLAWD_ANIMS") ? getenv("CLAWD_ANIMS") : ad); parts_set_mine_dir(getenv("CLAWD_MY_ANIMS")); settings_init(getenv("CLAWD_SETTINGS")); settings_apply(NULL); } /* (the tests use none of yours) */
    int rc = 0;
    if (argc > 1 && !strcmp(argv[1], "--golden")) rc = golden(argc > 2 && !strcmp(argv[2], "--code") ? NULL : &frames);
    else if (argc > 2 && !strcmp(argv[1], "--fixture")) rc = run_fixtures(argc - 2, argv + 2, &frames, &items);
    else if (argc > 2 && !strcmp(argv[1], "--script")) { draw_set_sprites(&frames, &items); for (int i = 2; i < argc && !rc; i++) { printf("script %s\n", argv[i]); rc = script_run(argv[i]); } }
    else if (argc > 1 && !strcmp(argv[1], "--check")) { draw_set_sprites(&frames, &items); rc = gallery_run(NULL); }
    else if (argc > 2 && !strcmp(argv[1], "--gallery")) { draw_set_sprites(&frames, &items); rc = gallery_run(argv[2]); }
    else if (argc > 2 && !strcmp(argv[1], "--golden-ui")) rc = golden_ui(argv[2]);
    else if (argc > 1 && !strcmp(argv[1], "--golden-win")) rc = golden_win();
    else if (argc > 1 && !strcmp(argv[1], "--golden-rng")) rc = golden_rng();
    else if (argc > 1 && !strcmp(argv[1], "--golden-term")) rc = golden_term(&frames);
    else { fprintf(stderr, "usage: clawd-room --golden [--code] | --golden-term\n"); rc = 1; }
    sprites_free(&frames);
    sprites_free(&items);
    sprites_free(&props);
    return rc;
}
