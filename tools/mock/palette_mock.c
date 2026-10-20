/* Mockups of the pixel editor's colour panel (tools/mock, built and run by build.sh): three directions after familiar apps.
   build and run: tools/mock/build.sh */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "canvas.h"
#include "art.h"

#define BG RGB(18, 18, 24)
#define LABEL RGB(110, 110, 130)
#define FIELD RGB(34, 34, 44)
#define CAP RGB(200, 200, 215)
static const Rgb FILEC[] = {{205,110,88},{235,235,245},{240,244,250},{150,200,236},{90,150,90},{170,110,80},{110,84,60},{40,60,46},{235,235,210},{150,150,140},{80,60,48},{230,190,110},{255,230,150},{120,80,50},{120,92,66}};
static const char FILEL[] = "BWcstmzdyhvxqrn";
#define NF 15
static void frame(Canvas *cv, int x, int y, int w, int h, Rgb c) { canvas_rect(cv, x-1, y-1, w+2, 1, c); canvas_rect(cv, x-1, y+h, w+2, 1, c); canvas_rect(cv, x-1, y-1, 1, h+2, c); canvas_rect(cv, x+w, y-1, 1, h+2, c); }
static void checker(Canvas *cv, int x, int y, int w, int h) { for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) canvas_px(cv, x+dx, y+dy, ((dx/2)+(dy/2))%2 ? RGB(70,70,80) : RGB(52,52,60)); }
static Rgb mix(Rgb a, Rgb b, double t) { return RGB((int)(a.r+(b.r-a.r)*t), (int)(a.g+(b.g-a.g)*t), (int)(a.b+(b.b-a.b)*t)); }
static Rgb hsv(double h, double s, double v) { double c = v*s, x = c*(1-fabs(fmod(h/60,2)-1)), m = v-c, r=0,g=0,b=0;
    if (h<60){r=c;g=x;} else if(h<120){r=x;g=c;} else if(h<180){g=c;b=x;} else if(h<240){g=x;b=c;} else if(h<300){r=x;b=c;} else {r=c;b=x;}
    return RGB((int)((r+m)*255),(int)((g+m)*255),(int)((b+m)*255)); }
static void button(Canvas *cv, int x, int y, const char *t, int on) { int w = text_width(t)+6; canvas_rect(cv, x, y, w, 9, on ? RGB(90,90,120) : FIELD); if (on) frame(cv, x, y, w, 9, C_YELLOW); canvas_text(cv, x+3, y+2, t, on ? C_WHITE : C_GREY); }
static void hexfield(Canvas *cv, int x, int y, const char *t) { int w = text_width("#DDDDDD")+4; canvas_rect(cv, x, y, w, 7, FIELD); canvas_text(cv, x+2, y+1, t, C_WHITE); }

/* the left side, the same in all three: title, the window sprite at zoom, the shortcut line */
static int base(Canvas *cv, const char *opt) {
    canvas_rect(cv, 0, 0, cv->w, cv->h, BG);
    canvas_text(cv, 4, 3, "EDIT WINDOW  PROPS.TXT  14X9", C_YELLOW);
    canvas_text(cv, cv->w - text_width(opt) - 4, 3, opt, C_GREY);
    int z = 9, gx = 4, gy = 12;
    for (int y = 0; y < 9; y++) for (int x = 0; x < 14; x++) {
        int glass = (x >= 1 && x <= 5 && y >= 1 && y <= 7) || (x >= 8 && x <= 12 && y >= 1 && y <= 7);
        canvas_rect(cv, gx + x*z, gy + y*z, z, z, glass ? RGB(150,200,236) : RGB(240,244,250));
    }
    frame(cv, gx, gy, 14*z, 9*z, RGB(90,90,110));
    canvas_rect(cv, 30, cv->h - 9, text_width("?")+4, 7, CAP); canvas_text(cv, 32, cv->h - 8, "?", RGB(20,20,28)); canvas_text(cv, 30+text_width("?")+7, cv->h - 8, "SHORTCUTS", RGB(150,150,170));
    return gx + 14*z + 10;
}

/* A: like the colour menu in Word / PowerPoint */
static void option_a(Canvas *cv) {
    int x = base(cv, "A  OFFICE"), y = 12, s = 8, st = 10;
    canvas_text(cv, x, y, "COLOUR", LABEL);
    canvas_rect(cv, x, y + 7, 14, 14, FILEC[0]); frame(cv, x, y + 7, 14, 14, CAP);
    canvas_text(cv, x + 18, y + 8, "BODY", C_WHITE); hexfield(cv, x + 18, y + 15, "#CD6E58");
    button(cv, x + 52, y + 13, "ERASE", 0);
    y += 25;
    canvas_text(cv, x, y, "IN THIS FILE", LABEL); y += 7;
    for (int i = 0; i < NF; i++) { int sx = x + (i % 10) * st, sy = y + (i / 10) * st; canvas_rect(cv, sx, sy, s, s, FILEC[i]); if (i == 0) frame(cv, sx, sy, s, s, C_WHITE); }
    y += 2 * st + 3;
    canvas_text(cv, x, y, "THEME COLOURS", LABEL); y += 7;
    static const Rgb TH[10] = {{255,255,255},{0,0,0},{231,230,230},{68,84,106},{68,114,196},{237,125,49},{165,165,165},{255,192,0},{91,155,213},{112,173,71}};
    for (int c = 0; c < 10; c++) {
        canvas_rect(cv, x + c*st, y, s, s, TH[c]);
        static const double T[5] = {0.8, 0.6, 0.4, -0.25, -0.5};
        for (int r = 0; r < 5; r++) { Rgb k = T[r] > 0 ? mix(TH[c], RGB(255,255,255), T[r]) : mix(TH[c], RGB(0,0,0), -T[r]); canvas_rect(cv, x + c*st, y + 10 + r*6, s, 5, k); }
    }
    y += 10 + 5*6 + 3;
    canvas_text(cv, x, y, "STANDARD", LABEL); y += 7;
    static const Rgb SD[10] = {{192,0,0},{255,0,0},{255,192,0},{255,255,0},{146,208,80},{0,176,80},{0,176,240},{0,112,192},{0,32,96},{112,48,160}};
    for (int c = 0; c < 10; c++) canvas_rect(cv, x + c*st, y, s, s, SD[c]);
    y += st + 3;
    canvas_text(cv, x, y, "RECENT", LABEL); y += 7;
    static const Rgb RC[4] = {{150,200,236},{230,190,110},{90,150,90},{205,110,88}};
    for (int c = 0; c < 4; c++) canvas_rect(cv, x + c*st, y, s, s, RC[c]);
    button(cv, x + 44, y - 1, "MORE COLOURS...", 0);
}

/* B: like Figma's colour picker */
static void option_b(Canvas *cv) {
    int x = base(cv, "B  FIGMA"), y = 12;
    button(cv, x, y, "PAINT", 1); button(cv, x + 26, y, "ERASE", 0); button(cv, x + 52, y, "PICK", 0);
    y += 13;
    double H = 12, S = 0.57, V = 0.80; /* #CD6E58 */
    int fw = 96, fh = 44;
    for (int dy = 0; dy < fh; dy++) for (int dx = 0; dx < fw; dx++) canvas_px(cv, x + dx, y + dy, hsv(H, (double)dx/(fw-1), 1 - (double)dy/(fh-1)));
    int cx = x + (int)(S*(fw-1)), cy = y + (int)((1-V)*(fh-1));
    frame(cv, cx - 1, cy - 1, 3, 3, C_WHITE);
    y += fh + 3;
    for (int dx = 0; dx < fw; dx++) canvas_rect(cv, x + dx, y, 1, 4, hsv(dx * 359.0 / (fw-1), 1, 1));
    canvas_rect(cv, x + (int)(H/359*(fw-1)), y - 1, 1, 6, C_WHITE);
    y += 8;
    canvas_rect(cv, x, y, 9, 7, FILEC[0]); frame(cv, x, y, 9, 7, CAP);
    hexfield(cv, x + 12, y, "#CD6E58");
    canvas_text(cv, x + 46, y + 1, "205 110 88", C_GREY);
    y += 11;
    canvas_text(cv, x, y, "IN THIS FILE", LABEL); canvas_text(cv, x + 74, y, "+ ADD", C_GREY); y += 7;
    for (int i = 0; i < NF; i++) { int sx = x + (i % 10) * 10, sy = y + (i / 10) * 10; canvas_rect(cv, sx, sy, 8, 8, FILEC[i]); if (i == 0) frame(cv, sx, sy, 8, 8, C_WHITE); }
    y += 23;
    canvas_text(cv, x, y, "SHADES", LABEL); y += 7;
    for (int k = -3; k <= 3; k++) { Rgb c = k < 0 ? mix(FILEC[0], RGB(0,0,0), -k*0.17) : mix(FILEC[0], RGB(255,255,255), k*0.17); canvas_rect(cv, x + (k+3)*10, y, 8, 8, c); if (!k) frame(cv, x + 30, y, 8, 8, CAP); }
}

/* C: like Photoshop / Procreate swatches */
static void option_c(Canvas *cv) {
    int x = base(cv, "C  PHOTOSHOP"), y = 12;
    button(cv, x, y, "PAINT", 1); button(cv, x + 26, y, "ERASE", 0); button(cv, x + 52, y, "PICK", 0);
    y += 13;
    canvas_rect(cv, x + 8, y + 8, 18, 18, RGB(150,200,236)); frame(cv, x + 8, y + 8, 18, 18, CAP);
    canvas_rect(cv, x, y, 18, 18, FILEC[0]); frame(cv, x, y, 18, 18, C_WHITE);
    canvas_text(cv, x + 32, y + 1, "BODY", C_WHITE);
    hexfield(cv, x + 32, y + 8, "#CD6E58");
    canvas_text(cv, x + 32, y + 18, "BEFORE: GLASS", C_GREY);
    y += 31;
    button(cv, x, y, "FILE", 1); button(cv, x + 22, y, "ALL COLOURS", 0); button(cv, x + 76, y, "SHADES", 0);
    y += 12;
    for (int i = 0; i < NF; i++) {
        int sx = x + (i % 6) * 17, sy = y + (i / 6) * 19;
        canvas_rect(cv, sx, sy, 14, 12, FILEC[i]);
        if (i == 0) frame(cv, sx, sy, 14, 12, C_WHITE);
        char l[2] = {FILEL[i], 0};
        canvas_text(cv, sx + 5, sy + 13, i == 0 ? "B" : l, i < 4 ? C_WHITE : RGB(90,90,105));
    }
    checker(cv, x + (NF % 6) * 17, y + (NF / 6) * 19, 14, 12);
    canvas_text(cv, x + (NF % 6) * 17 + 2, y + (NF / 6) * 19 + 13, "SEE", RGB(90,90,105));
    y += 3 * 19 + 2;
    canvas_text(cv, x, y, "BRIGHT: USED HERE", LABEL);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    void (*f[3])(Canvas *) = {option_a, option_b, option_c};
    const char *n[3] = {"a-office", "b-figma", "c-photoshop"};
    for (int i = 0; i < 3; i++) {
        Canvas cv = canvas_new(266, 168, BG);
        f[i](&cv);
        char p[600]; snprintf(p, sizeof p, "%s/palette-%s.png", dir, n[i]);
        png_write(p, &cv, 3);
        canvas_free(&cv);
    }
    return 0;
}
