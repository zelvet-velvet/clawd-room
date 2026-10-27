#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "panel.h"
#include "parts.h"
#include "settings.h"
#include "ui.h"

#define LINE 7
#define N_GATE 16
enum { SPOTLIGHT, APPEAR, SIZE, FRAMES, N_ROWS };
static const struct { const char *group, *label, *key; } ROW[N_ROWS] = {
    {"PICKED CLAWD", "SPOTLIGHT", "spotlight"}, {"GATES", "APPEAR", "gate_theme"}, {"ROOM", "SIZE", "room_size"}, {NULL, "FRAMES", "fps"},
};
static const int SPOT[] = {0, 20, 40, 60, 80, 100}, FPSV[] = {8, 12, 16, 24, 30};
#define N_SPOT ((int)(sizeof SPOT / sizeof *SPOT))
#define N_FPS ((int)(sizeof FPSV / sizeof *FPSV))

static int themes(char out[][24]) { /* the gate themes: the folders of anims/gates/, simple first */
    char names[256][PARTS_PATH_NAME];
    int n = parts_names(names, 256), k = 0;
    snprintf(out[k++], 24, "simple");
    for (int i = 0; i < n; i++) {
        char th[24];
        if (sscanf(names[i], "gates/%23[^/]/", th) != 1) continue;
        int have = 0;
        for (int j = 0; j < k; j++) if (!strcmp(out[j], th)) have = 1;
        if (!have && k < N_GATE) snprintf(out[k++], 24, "%s", th);
    }
    return k;
}
static int near_index(const int *v, int n, int x) { int b = 0; for (int i = 1; i < n; i++) if (abs(v[i] - x) < abs(v[b] - x)) b = i; return b; }

/* the current value as the index into the row's choices, how many there are, and the choice's text */
static int choices(int row, int *cur, char *text, size_t tn, int step) {
    char th[N_GATE][24];
    int n = 0, c = 0;
    if (row == SPOTLIGHT) { n = N_SPOT; c = near_index(SPOT, n, settings_int("spotlight", 60)); }
    else if (row == FRAMES) { n = N_FPS; c = near_index(FPSV, n, settings_int("fps", 16)); }
    else if (row == SIZE) {
        n = N_SIZES; const char *v = settings_get("room_size");
        for (int i = 0; i < n; i++) if (v && !strcmp(v, SIZE_KEY[i])) c = i;
    } else {
        n = themes(th); const char *v = parts_gate_theme();
        for (int i = 0; i < n; i++) if (!strcmp(th[i], v)) c = i;
        if (step) c = (c + n + step) % n;
        snprintf(text, tn, "%s", th[c]);
        for (char *q = text; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        *cur = c;
        return n;
    }
    if (step) c = (c + n + step) % n;
    if (row == SPOTLIGHT) snprintf(text, tn, "%d", SPOT[c]);
    else if (row == FRAMES) snprintf(text, tn, "%d FPS", FPSV[c]);
    else snprintf(text, tn, "%.*s", (int)strcspn(SIZE_LABEL[c], " "), SIZE_LABEL[c]);
    *cur = c;
    return n;
}

static void change(int row, int step) {
    int c; char text[32], val[24];
    int n = choices(row, &c, text, sizeof text, step);
    (void)n;
    if (row == SPOTLIGHT) snprintf(val, sizeof val, "%d", SPOT[c]);
    else if (row == FRAMES) snprintf(val, sizeof val, "%d", FPSV[c]);
    else if (row == SIZE) snprintf(val, sizeof val, "%s", SIZE_KEY[c]);
    else { for (char *q = text; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; snprintf(val, sizeof val, "%s", text); }
    settings_set(ROW[row].key, val);
}

int panel_key(Panel *p, char k) {
    if (!p->open) return 0;
    if (k == '.' || k == 27) { p->open = 0; return 1; }
    if (k == 'w') p->sel = (p->sel + N_ROWS - 1) % N_ROWS;
    else if (k == 's') p->sel = (p->sel + 1) % N_ROWS;
    else if (k == 'a') change(p->sel, -1);
    else if (k == 'd') change(p->sel, 1);
    else return 0;
    return 1;
}

void draw_panel(Canvas *cv, int x, const Panel *p) {
    const Rgb BG = RGB(22, 24, 36), EDGE = RGB(84, 88, 110), TITLE = RGB(255, 208, 64), GROUP = RGB(150, 150, 168), LABEL = RGB(214, 214, 226), VAL = RGB(255, 208, 64);
    canvas_rect(cv, x, 0, PANEL_W, cv->h, BG);
    canvas_rect(cv, x, 0, 1, cv->h, EDGE);
    int y = 3, heads = cv->h >= 90; /* a short window drops the group names to fit */
    canvas_text(cv, x + 6, y, "SETTINGS", TITLE);
    y += LINE + 2;
    for (int r = 0; r < N_ROWS; r++) {
        if (heads && ROW[r].group) { if (r) y += 3; canvas_text(cv, x + 6, y, ROW[r].group, GROUP); y += LINE; }
        int c; char text[32];
        choices(r, &c, text, sizeof text, 0);
        int sel = r == p->sel;
        if (sel) canvas_rect(cv, x + 3, y - 1, PANEL_W - 5, LINE, RGB(46, 50, 76));
        canvas_text(cv, x + 9, y, ROW[r].label, sel ? RGB(255, 255, 255) : LABEL);
        char v[48];
        snprintf(v, sizeof v, sel ? "< %s >" : "%s", text);
        canvas_text(cv, x + PANEL_W - 4 - text_width(v), y, v, VAL);
        y += LINE;
    }
}
