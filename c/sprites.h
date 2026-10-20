/* Editable sprites from the sprites folder (same format as sprites.py). A frame not in the file is drawn by the code. */
#ifndef SPRITES_H
#define SPRITES_H
#include "canvas.h"

enum { PX_NONE = 0, PX_BODY = 1, PX_RGB = 2 };
typedef struct { uint8_t kind; Rgb c; } SpritePx;
typedef struct { char name[64], key[128]; int w, h; SpritePx *px; } Sprite;
typedef struct { Sprite *s; int n; } SpriteFile;

SpriteFile sprites_load(const char *path); /* a missing file gives an empty set */
SpriteFile sprites_load_layered(const char *def_path, const char *mine_path); /* the defaults with your own over them */
const char *sprites_mine_dir(void); /* your own sprites: ~/.clawd-room/sprites (CLAWD_MY_SPRITES) */
void sprites_free(SpriteFile *f);
const Sprite *sprites_by_key(const SpriteFile *f, const char *key);
const Sprite *sprites_by_name(const SpriteFile *f, const char *name);
void sprite_blit(Canvas *cv, const Sprite *sp, int x, int y, Rgb body);
#endif
