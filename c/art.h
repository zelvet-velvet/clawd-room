/* The sprite files and the pictures made from them: export the code's drawing to the sprite files,
 * and draw the sheet, the items and one GIF per action into the notes folder. */
#ifndef ART_H
#define ART_H
#include "canvas.h"
#include "sprites.h"

/* writes sprites/clawd.txt and items.txt from the code's own drawing; force overwrites, add appends the frames a file lacks. 0 on success */
int art_export(const char *sprites_dir, int force, int add);
/* draws sheet.png, items.png and the GIFs into out_dir from the sprite files in sprites_dir. 0 on success */
int art_build(const char *sprites_dir, const char *out_dir);

int png_write(const char *path, const Canvas *cv, int k); /* the canvas, every pixel k times bigger */
/* a looping GIF; frames are w x h RGB pixels each */
int gif_write(const char *path, const Rgb *const *frames, int nframes, int w, int h, int delay_cs);
int gif_write_fit(const char *path, Rgb **frames, int nframes, int w, int h, int delay_cs); /* the same, the colours rounded (the frames changed) until they are at most 256 */
#endif
