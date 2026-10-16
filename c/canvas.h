/* A grid of RGB pixels with the few drawing calls the room needs (port of Canvas in room_tui.py). */
#ifndef CANVAS_H
#define CANVAS_H
#include <stdint.h>

typedef struct { uint8_t r, g, b; } Rgb;
#define RGB(r, g, b) ((Rgb){(r), (g), (b)})

typedef struct { int w, h; Rgb *p; } Canvas;

static inline int rgb_eq(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

Canvas canvas_new(int w, int h, Rgb bg);
void canvas_free(Canvas *c);
void canvas_px(Canvas *c, int x, int y, Rgb col);
void canvas_rect(Canvas *c, int x, int y, int w, int h, Rgb col);
void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, Rgb col);
void canvas_text(Canvas *c, int x, int y, const char *s, Rgb col);
int text_width(const char *s);
uint64_t canvas_hash(const Canvas *c); /* FNV-1a over the RGB bytes (the tests compare it with Python) */

/* palette (same values as room_tui.py) */
#define C_CLAWD RGB(205, 110, 88)
#define C_BLUSH RGB(250, 200, 216)
#define C_BUBBLE_EDGE RGB(136, 136, 136)
#define C_INK RGB(20, 20, 30)
#define C_WHITE RGB(235, 235, 245)
#define C_GLINT RGB(242, 190, 168)
#define C_YELLOW RGB(250, 210, 70)
#define C_GREY RGB(150, 155, 185)
#endif
