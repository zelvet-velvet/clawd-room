#include "art.h"
#include "theme.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "actions.h"
#include "room.h"

#define BG RGB(44, 44, 56)
#define TRANSPARENT RGB(1, 2, 3)

static void mkdirs(const char *p) {
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char *q = tmp + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
    mkdir(tmp, 0755);
}

/* ---- PNG: filter "up", LZ77 + fixed Huffman (no zlib needed) ----------------------------------------- */
typedef struct { uint8_t *p; size_t n, cap; uint32_t bits; int nb; } Out;

static void out_byte(Out *o, uint8_t b) { if (o->n == o->cap) { o->cap = o->cap ? o->cap * 2 : 1 << 16; o->p = realloc(o->p, o->cap); } o->p[o->n++] = b; }
static void put_bits(Out *o, uint32_t v, int n) { /* LSB first */
    o->bits |= v << o->nb; o->nb += n;
    while (o->nb >= 8) { out_byte(o, (uint8_t)(o->bits & 255)); o->bits >>= 8; o->nb -= 8; }
}
static void put_huff(Out *o, uint32_t code, int n) { /* Huffman codes go MSB first */
    uint32_t r = 0;
    for (int i = 0; i < n; i++) r |= ((code >> i) & 1u) << (n - 1 - i);
    put_bits(o, r, n);
}
static void put_lit(Out *o, int sym) {
    if (sym < 144) put_huff(o, 0x30 + (uint32_t)sym, 8);
    else if (sym < 256) put_huff(o, 0x190 + (uint32_t)(sym - 144), 9);
    else if (sym < 280) put_huff(o, (uint32_t)(sym - 256), 7);
    else put_huff(o, 0xC0 + (uint32_t)(sym - 280), 8);
}
static void put_match(Out *o, int len, int dist) {
    static const int LB[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int LE[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int DB[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int DE[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    int l = 28;
    while (LB[l] > len) l--;
    put_lit(o, 257 + l);
    if (LE[l]) put_bits(o, (uint32_t)(len - LB[l]), LE[l]);
    int d = 29;
    while (DB[d] > dist) d--;
    put_huff(o, (uint32_t)d, 5);
    if (DE[d]) put_bits(o, (uint32_t)(dist - DB[d]), DE[d]);
}

static void deflate(Out *o, const uint8_t *s, size_t n) { /* one final block, fixed Huffman, greedy matches */
    enum { HB = 15, HS = 1 << HB, W = 32768 };
    int32_t *head = malloc(sizeof(int32_t) * HS), *prev = malloc(sizeof(int32_t) * (n ? n : 1));
    for (int i = 0; i < HS; i++) head[i] = -1;
    put_bits(o, 1, 1); put_bits(o, 1, 2); /* final block, fixed Huffman */
    size_t i = 0;
    while (i < n) {
        int best = 0, bd = 0;
        if (i + 3 <= n) {
            uint32_t h = ((uint32_t)s[i] * 506832829u ^ (uint32_t)s[i + 1] * 2654435761u ^ (uint32_t)s[i + 2] * 40503u) >> (32 - HB) & (HS - 1);
            int32_t c = head[h];
            for (int depth = 0; c >= 0 && depth < 24 && i - (size_t)c <= W; depth++, c = prev[c]) {
                int l = 0;
                while (l < 258 && i + (size_t)l < n && s[(size_t)c + (size_t)l] == s[i + (size_t)l]) l++;
                if (l > best) { best = l; bd = (int)(i - (size_t)c); if (l == 258) break; }
            }
        }
        size_t adv = 1;
        if (best >= 3) { put_match(o, best, bd); adv = (size_t)best; } else put_lit(o, s[i]);
        for (size_t k = 0; k < adv; k++, i++) { /* index every position we pass */
            if (i + 3 <= n) {
                uint32_t h = ((uint32_t)s[i] * 506832829u ^ (uint32_t)s[i + 1] * 2654435761u ^ (uint32_t)s[i + 2] * 40503u) >> (32 - HB) & (HS - 1);
                prev[i] = head[h]; head[h] = (int32_t)i;
            }
        }
    }
    put_lit(o, 256);
    if (o->nb) put_bits(o, 0, 8 - o->nb);
    free(head); free(prev);
}

static uint32_t crc_table[256];
static uint32_t crc32_bytes(uint32_t crc, const uint8_t *p, size_t n) {
    if (!crc_table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); crc_table[i] = c; }
    crc = ~crc;
    while (n--) crc = crc_table[(crc ^ *p++) & 255] ^ (crc >> 8);
    return ~crc;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *data, size_t n) {
    uint8_t h[8];
    be32(h, (uint32_t)n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t c = crc32_bytes(0, (const uint8_t *)type, 4);
    c = crc32_bytes(c, data, n);
    uint8_t t[4];
    be32(t, c);
    fwrite(t, 1, 4, f);
}

int png_write(const char *path, const Canvas *cv, int k) {
    int w = cv->w * k, h = cv->h * k;
    size_t stride = (size_t)w * 3 + 1;
    uint8_t *raw = malloc(stride * (size_t)h);
    for (int y = 0; y < h; y++) {
        uint8_t *row = raw + stride * (size_t)y;
        row[0] = y ? 2 : 0; /* filter: up (the repeated rows become zeros) */
        for (int x = 0; x < w; x++) {
            Rgb c = cv->p[(y / k) * cv->w + x / k];
            row[1 + 3 * x] = c.r; row[2 + 3 * x] = c.g; row[3 + 3 * x] = c.b;
        }
    }
    for (int y = h - 1; y >= 1; y--) { /* now subtract the row above */
        uint8_t *row = raw + stride * (size_t)y, *up = raw + stride * (size_t)(y - 1);
        for (size_t i = 1; i < stride; i++) row[i] = (uint8_t)(row[i] - up[i]);
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < stride * (size_t)h; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    Out o = {0};
    out_byte(&o, 0x78); out_byte(&o, 0x01);
    deflate(&o, raw, stride * (size_t)h);
    uint8_t ad[4];
    be32(ad, (b << 16) | a);
    for (int i = 0; i < 4; i++) out_byte(&o, ad[i]);
    free(raw);
    char dir[600];
    snprintf(dir, sizeof dir, "%s", path);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = 0; mkdirs(dir); }
    FILE *f = fopen(path, "wb");
    if (!f) { free(o.p); return 1; }
    static const uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    fwrite(SIG, 1, 8, f);
    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", o.p, o.n);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(o.p);
    return 0;
}

/* ---- GIF (LZW), same encoder as sprites.py ------------------------------------------------------------ */
typedef struct { uint8_t *p; size_t n, cap; uint32_t buf; int nb; } Bits;
static void gb_byte(Bits *b, uint8_t v) { if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 4096; b->p = realloc(b->p, b->cap); } b->p[b->n++] = v; }
static void gb_emit(Bits *b, uint32_t code, int size) {
    b->buf |= code << b->nb; b->nb += size;
    while (b->nb >= 8) { gb_byte(b, (uint8_t)(b->buf & 255)); b->buf >>= 8; b->nb -= 8; }
}

static void lzw(Bits *b, const uint8_t *idx, size_t n) {
    enum { TS = 1 << 16 };
    int32_t *tab = malloc(sizeof(int32_t) * TS), *keys = malloc(sizeof(int32_t) * TS); /* (prefix * 256 + k) -> code, open addressing */
    for (int i = 0; i < TS; i++) keys[i] = -1;
    int nxt = 258, size = 9;
    gb_emit(b, 256, size);
    int prefix = idx[0];
    for (size_t i = 1; i < n; i++) {
        int k = idx[i];
        int32_t key = prefix * 256 + k;
        uint32_t h = ((uint32_t)key * 2654435761u) >> 16;
        int found = -1;
        while (keys[h] != -1) { if (keys[h] == key) { found = tab[h]; break; } h = (h + 1) & (TS - 1); }
        if (found >= 0) { prefix = found; continue; }
        gb_emit(b, (uint32_t)prefix, size);
        keys[h] = key; tab[h] = nxt;
        nxt++;
        if (nxt - 1 == (1 << size) && size < 12) size++;
        if (nxt >= 4095) { /* the table is full: start again */
            gb_emit(b, 256, size);
            for (int q = 0; q < TS; q++) keys[q] = -1;
            nxt = 258; size = 9;
        }
        prefix = k;
    }
    gb_emit(b, (uint32_t)prefix, size);
    gb_emit(b, 257, size);
    if (b->nb) gb_byte(b, (uint8_t)(b->buf & 255));
    free(tab); free(keys);
}

int gif_write(const char *path, const Rgb *const *frames, int nframes, int w, int h, int delay_cs) {
    Rgb pal[256];
    int np = 0;
    uint8_t *idx = malloc((size_t)w * (size_t)h * (size_t)nframes);
    for (int f = 0; f < nframes; f++)
        for (int i = 0; i < w * h; i++) {
            Rgb c = frames[f][i];
            int q = 0;
            while (q < np && !rgb_eq(pal[q], c)) q++;
            if (q == np) { if (np == 256) { free(idx); return 1; } pal[np++] = c; }
            idx[(size_t)f * (size_t)w * (size_t)h + (size_t)i] = (uint8_t)q;
        }
    int bits = 1;
    while ((1 << bits) < np) bits++;
    char dir[600];
    snprintf(dir, sizeof dir, "%s", path);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = 0; mkdirs(dir); }
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(idx); return 1; }
    fwrite("GIF89a", 1, 6, fp);
    uint8_t hd[7] = {(uint8_t)(w & 255), (uint8_t)(w >> 8), (uint8_t)(h & 255), (uint8_t)(h >> 8), (uint8_t)(0x80 | (bits - 1)), 0, 0};
    fwrite(hd, 1, 7, fp);
    for (int i = 0; i < (1 << bits); i++) { uint8_t c[3] = {0, 0, 0}; if (i < np) { c[0] = pal[i].r; c[1] = pal[i].g; c[2] = pal[i].b; } fwrite(c, 1, 3, fp); }
    fwrite("\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00", 1, 19, fp); /* loop forever */
    for (int f = 0; f < nframes; f++) {
        uint8_t gce[8] = {0x21, 0xf9, 0x04, 0x00, (uint8_t)(delay_cs & 255), (uint8_t)(delay_cs >> 8), 0, 0};
        fwrite(gce, 1, 8, fp);
        uint8_t id[10] = {0x2c, 0, 0, 0, 0, (uint8_t)(w & 255), (uint8_t)(w >> 8), (uint8_t)(h & 255), (uint8_t)(h >> 8), 0};
        fwrite(id, 1, 10, fp);
        fputc(8, fp);
        Bits b = {0};
        lzw(&b, idx + (size_t)f * (size_t)w * (size_t)h, (size_t)w * (size_t)h);
        for (size_t i = 0; i < b.n; i += 255) { size_t l = b.n - i < 255 ? b.n - i : 255; fputc((int)l, fp); fwrite(b.p + i, 1, l, fp); }
        fputc(0, fp);
        free(b.p);
    }
    fputc(';', fp);
    fclose(fp);
    free(idx);
    return 0;
}

/* ---- the frames of every action, drawn by the code --------------------------------------------------------- */
static void pose_key_of(const Pose *p, char *key, size_t n) { pose_key(p, key, n); }
static Pose blank(const Pose *p) { Pose q = *p; q.bob = 0; return q; }

typedef struct { char name[64], key[128]; Rgb px[15 * 9]; int w, h; } Frame;
typedef struct { Frame *f; int n, cap; } Frames;

static Frame *add_frame(Frames *fs, const char *name, const char *key, int w, int h) {
    if (fs->n == fs->cap) { fs->cap = fs->cap ? fs->cap * 2 : 64; fs->f = realloc(fs->f, sizeof(Frame) * (size_t)fs->cap); }
    Frame *f = &fs->f[fs->n++];
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "%s", name); snprintf(f->key, sizeof f->key, "%s", key);
    f->w = w; f->h = h;
    return f;
}

/* an item frame has its own size, so the pixel store is dynamic: ItemFrame wraps a Canvas */
typedef struct { char name[64]; Canvas cv; } ItemFrame;

const SpriteFile *draw_frames(void);

static void clawd_frames(Frames *out) {
    draw_set_sprites(NULL, NULL); /* the code's own drawing, not the files */
    char seen[512][128];
    int nseen = 0;
    for (int a = 0; a < N_ACTIONS; a++) {
        Pose poses[ACTION_STEPS];
        action_poses(a, poses);
        int n = 0;
        for (int i = 0; i < ACTION_STEPS; i++) {
            char key[128];
            pose_key_of(&poses[i], key, sizeof key);
            int dup = 0;
            for (int q = 0; q < nseen; q++) if (!strcmp(seen[q], key)) dup = 1;
            if (dup) continue;
            snprintf(seen[nseen++], 128, "%s", key);
            n++;
            char name[64];
            snprintf(name, sizeof name, "%s-%d", ACTIONS[a].name, n);
            Canvas cv = canvas_new(15, 9, TRANSPARENT);
            Pose b = blank(&poses[i]);
            draw_clawd(&cv, 7, 1, &b, NULL, C_CLAWD, NULL, "");
            Frame *f = add_frame(out, name, key, 15, 9);
            memcpy(f->px, cv.p, sizeof f->px);
            canvas_free(&cv);
        }
    }
}

void draw_bubble_code(Canvas *cv, int cx, int y, const char *kind, double ph);

/* write a sprite file: legend (body, the fixed letters, new colours in order of appearance) and the grids */
static int save_file(const char *path, const char *note, Canvas *const *cvs, const char (*names)[64], const char (*keys)[128], int n) {
    static const Rgb KNOWN[6] = {{20, 20, 30}, {250, 200, 216}, {242, 190, 168}, {235, 235, 245}, {250, 210, 70}, {136, 136, 136}}; /* K P G W Y E */
    static const char KL[6] = {'K', 'P', 'G', 'W', 'Y', 'E'};
    char pool[32] = "";
    { size_t k = 0; for (const char *c = "KPGWYEcstmzdyhvxqrnajkuo"; *c; c++) if (!strchr("KPGWYE", *c)) pool[k++] = *c; pool[k] = 0; }
    Rgb col[64];
    char ch[64];
    int nc = 0, pi = 0;
    col[nc] = C_CLAWD; ch[nc++] = 'B';
    for (int i = 0; i < 6; i++) { col[nc] = KNOWN[i]; ch[nc++] = KL[i]; }
    for (int f = 0; f < n; f++)
        for (int i = 0; i < cvs[f]->w * cvs[f]->h; i++) {
            Rgb c = cvs[f]->p[i];
            if (rgb_eq(c, TRANSPARENT)) continue;
            int q = 0;
            while (q < nc && !rgb_eq(col[q], c)) q++;
            if (q == nc && nc < 64 && pool[pi]) { col[nc] = c; ch[nc++] = pool[pi++]; }
        }
    FILE *fp = fopen(path, "w");
    if (!fp) return 1;
    for (const char *s = note; *s;) { const char *e = strchr(s, '\n'); fprintf(fp, "# %.*s\n", (int)(e ? e - s : (long)strlen(s)), s); if (!e) break; s = e + 1; }
    fprintf(fp, ": B = body\n");
    for (int q = 1; q < nc; q++) { /* only the colours that are used (the body is always listed) */
        int used = 0;
        for (int f = 0; f < n && !used; f++) for (int i = 0; i < cvs[f]->w * cvs[f]->h; i++) if (rgb_eq(cvs[f]->p[i], col[q])) { used = 1; break; }
        if (used) fprintf(fp, ": %c = %d,%d,%d\n", ch[q], col[q].r, col[q].g, col[q].b);
    }
    for (int f = 0; f < n; f++) {
        fprintf(fp, "\n@ %s | %s\n", names[f], keys[f]);
        for (int y = 0; y < cvs[f]->h; y++) {
            for (int x = 0; x < cvs[f]->w; x++) {
                Rgb c = cvs[f]->p[y * cvs[f]->w + x];
                char o = '.';
                if (!rgb_eq(c, TRANSPARENT)) for (int q = 0; q < nc; q++) if (rgb_eq(col[q], c)) { o = ch[q]; break; }
                fputc(o, fp);
            }
            fputc('\n', fp);
        }
    }
    fclose(fp);
    return 0;
}

static Canvas *sprite_canvas(const Sprite *sp) { /* a sprite file frame back to a canvas (body = the Clawd colour) */
    Canvas *c = malloc(sizeof *c);
    *c = canvas_new(sp->w, sp->h, TRANSPARENT);
    for (int y = 0; y < sp->h; y++) for (int x = 0; x < sp->w; x++) {
        SpritePx p = sp->px[y * sp->w + x];
        if (p.kind == PX_BODY) c->p[y * sp->w + x] = C_CLAWD; else if (p.kind == PX_RGB) c->p[y * sp->w + x] = p.c;
    }
    return c;
}

static int export_one(const char *path, const char *note, Canvas **new_cvs, char (*new_names)[64], char (*new_keys)[128], int nnew, int force, int add) {
    FILE *ex = fopen(path, "r");
    if (ex) fclose(ex);
    if (ex && !(force || add)) { printf("kept %s (exists; --add appends new frames, --force rewrites)\n", path); return 0; }
    int total = nnew;
    static Canvas *cvs[1024];
    static char names[1024][64], keys[1024][128];
    int n = 0;
    if (add && ex) {
        SpriteFile have = sprites_load(path);
        for (int i = 0; i < have.n && n < 1024; i++) { cvs[n] = sprite_canvas(&have.s[i]); snprintf(names[n], 64, "%s", have.s[i].name); snprintf(keys[n], 128, "%s", have.s[i].key); n++; }
        for (int i = 0; i < nnew && n < 1024; i++) {
            if (sprites_by_key(&have, new_keys[i]) || sprites_by_name(&have, new_names[i])) continue;
            cvs[n] = new_cvs[i]; snprintf(names[n], 64, "%s", new_names[i]); snprintf(keys[n], 128, "%s", new_keys[i]); n++;
        }
        sprites_free(&have);
    } else for (int i = 0; i < nnew && n < 1024; i++) { cvs[n] = new_cvs[i]; snprintf(names[n], 64, "%s", new_names[i]); snprintf(keys[n], 128, "%s", new_keys[i]); n++; }
    (void)total;
    int rc = save_file(path, note, cvs, (const char (*)[64])names, (const char (*)[128])keys, n);
    printf("wrote %s %d frames\n", path, n);
    return rc;
}

int art_export(const char *dir, int force, int add) {
    Frames fs = {0};
    clawd_frames(&fs);
    static Canvas *cv[1024];
    static char nm[1024][64], ky[1024][128];
    for (int i = 0; i < fs.n; i++) {
        cv[i] = malloc(sizeof(Canvas));
        *cv[i] = canvas_new(15, 9, TRANSPARENT);
        memcpy(cv[i]->p, fs.f[i].px, sizeof fs.f[i].px);
        snprintf(nm[i], 64, "%s", fs.f[i].name); snprintf(ky[i], 128, "%s", fs.f[i].key);
    }
    char path[700];
    snprintf(path, sizeof path, "%s/clawd.txt", dir);
    mkdirs(dir);
    int rc = export_one(path, "Clawd frames (15 x 9, the row above the head is row 0).\nEdit the grids; `clawd-room --build-art` shows them.", cv, nm, ky, fs.n, force, add);
    /* the items */
    static Canvas *ic[16];
    static char inm[16][64], iky[16][128];
    int ni = 0;
    draw_set_sprites(NULL, NULL);
    for (int n = 0; n < 4; n++) {
        ic[ni] = malloc(sizeof(Canvas)); *ic[ni] = canvas_new(9, 8, TRANSPARENT);
        draw_bubble_code(ic[ni], 4, 0, "dots", n / 3.0 + 0.01);
        snprintf(inm[ni], 64, "bubble-dots-%d", n); snprintf(iky[ni], 128, "%s", inm[ni]); ni++;
    }
    static const char *K3[3][2] = {{"bubble-excl", "!"}, {"bubble-ques", "?"}, {"bubble-cup", "cup"}};
    for (int q = 0; q < 3; q++) {
        ic[ni] = malloc(sizeof(Canvas)); *ic[ni] = canvas_new(9, 8, TRANSPARENT);
        draw_bubble_code(ic[ni], 4, 0, K3[q][1], 0.0);
        snprintf(inm[ni], 64, "%s", K3[q][0]); snprintf(iky[ni], 128, "%s", K3[q][0]); ni++;
    }
    ic[ni] = malloc(sizeof(Canvas)); *ic[ni] = canvas_new(4, 2, TRANSPARENT); /* a coffee cup carried at the side (the steam is drawn by the code) */
    canvas_rect(ic[ni], 0, 0, 3, 2, C_WHITE); canvas_px(ic[ni], 3, 0, C_WHITE);
    snprintf(inm[ni], 64, "cup-held"); snprintf(iky[ni], 128, "cup-held"); ni++;
    ic[ni] = malloc(sizeof(Canvas)); *ic[ni] = canvas_new(9, 10, TRANSPARENT); /* the "need your OK" sign */
    canvas_rect(ic[ni], 0, 0, 9, 9, C_YELLOW); canvas_rect(ic[ni], 1, 1, 7, 7, RGB(250, 235, 150));
    canvas_text(ic[ni], 3, 2, "!", C_INK); canvas_rect(ic[ni], 4, 9, 1, 1, RGB(120, 90, 40));
    snprintf(inm[ni], 64, "sign-wait"); snprintf(iky[ni], 128, "sign-wait"); ni++;
    snprintf(path, sizeof path, "%s/items.txt", dir);
    rc |= export_one(path, "Items Clawd carries or shows (bubbles, cup, sign). Top-left corner = where the code puts it.", ic, inm, iky, ni, force, add);
    /* the props on the back wall (the placer finds them a place; the sign and the signpost have words, so stay code) */
    static const struct { const char *kind, *theme; int w, h; } P[] = {{"window", "office", 14, 9}, {"plant", "office", 6, 12}, {"menu", "coffee", 16, 11}, {"lamp", "coffee", 5, 7}, {"shelf", "coffee", 24, 4}};
    static Canvas *pc[8];
    static char pnm[8][64], pky[8][128];
    int np = 0;
    theme_set_props(NULL);
    for (size_t q = 0; q < sizeof P / sizeof P[0]; q++) {
        pc[np] = malloc(sizeof(Canvas)); *pc[np] = canvas_new(P[q].w, P[q].h, TRANSPARENT);
        Placed pl = {P[q].kind, {0, 0, P[q].w, P[q].h}};
        theme_wall_item(theme_get(P[q].theme), pc[np], &pl, "", 0);
        snprintf(pnm[np], 64, "%s", P[q].kind); snprintf(pky[np], 128, "%s", P[q].kind); np++;
    }
    snprintf(path, sizeof path, "%s/props.txt", dir);
    rc |= export_one(path, "Props on the back wall (window, plant, menu, lamp, shelf). Edit with: ./clawd-room.sh --edit NAME.\nA prop's size is its grid's size; the placer finds it a place (rooms/*.txt).", pc, pnm, pky, np, force, add);
    return rc;
}

/* ---- the sheet and the GIFs ---------------------------------------------------------------------------------- */
int art_build(const char *sprites_dir, const char *out_dir) {
    char path[700];
    snprintf(path, sizeof path, "%s/clawd.txt", sprites_dir);
    SpriteFile frames = sprites_load(path);
    snprintf(path, sizeof path, "%s/items.txt", sprites_dir);
    SpriteFile items = sprites_load(path);
    draw_set_sprites(&frames, &items);
    const int W = 26, H = 18, K = 8, cell_w = 50, cell_h = 28, label_w = 48;
    static Pose poses[32][ACTION_STEPS];
    static char keys[32][ACTION_STEPS][128];
    int cols = 0;
    for (int a = 0; a < N_ACTIONS; a++) {
        action_poses(a, poses[a]);
        int uniq = 0;
        for (int i = 0; i < ACTION_STEPS; i++) {
            pose_key(&poses[a][i], keys[a][i], 128);
            int dup = 0;
            for (int q = 0; q < i; q++) if (!strcmp(keys[a][q], keys[a][i])) dup = 1;
            if (!dup) uniq++;
        }
        if (uniq > cols) cols = uniq;
    }
    Canvas sheet = canvas_new(label_w + cols * cell_w, N_ACTIONS * cell_h + 4, BG);
    for (int a = 0; a < N_ACTIONS; a++) {
        int y0 = 2 + a * cell_h;
        char up[16];
        snprintf(up, sizeof up, "%.11s", ACTIONS[a].name);
        for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        canvas_text(&sheet, 2, y0 + 12, up, C_WHITE);
        int seen = 0;
        for (int i = 0; i < ACTION_STEPS; i++) {
            int dup = 0;
            for (int q = 0; q < i; q++) if (!strcmp(keys[a][q], keys[a][i])) dup = 1;
            if (dup) continue;
            int cx = label_w + seen * cell_w + cell_w / 2;
            seen++;
            Pose b = blank(&poses[a][i]);
            draw_clawd(&sheet, cx, y0 + 8, &b, &frames, C_CLAWD, NULL, "");
            const Sprite *sp = sprites_by_key(&frames, keys[a][i]);
            char label[128];
            if (sp) snprintf(label, sizeof label, "%s", sp->name); else { snprintf(label, sizeof label, "%s", keys[a][i]); char *sp2 = strchr(label, ' '); if (sp2) *sp2 = 0; }
            for (char *p = label; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            canvas_text(&sheet, cx - text_width(label) / 2, y0 + 21, label, RGB(170, 170, 190));
        }
        Rgb *fr[ACTION_STEPS];
        for (int i = 0; i < ACTION_STEPS; i++) {
            Canvas cv = canvas_new(W, H, BG);
            draw_clawd(&cv, W / 2, 8, &poses[a][i], &frames, C_CLAWD, NULL, "");
            fr[i] = malloc(sizeof(Rgb) * (size_t)(W * K) * (size_t)(H * K));
            for (int y = 0; y < H * K; y++) for (int x = 0; x < W * K; x++) fr[i][y * W * K + x] = cv.p[(y / K) * W + x / K];
            canvas_free(&cv);
        }
        snprintf(path, sizeof path, "%s/%s.gif", out_dir, ACTIONS[a].name);
        int rc = gif_write(path, (const Rgb *const *)fr, ACTION_STEPS, W * K, H * K, 100 / ACTION_FPS);
        for (int i = 0; i < ACTION_STEPS; i++) free(fr[i]);
        if (rc) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    }
    snprintf(path, sizeof path, "%s/sheet.png", out_dir);
    int rc = png_write(path, &sheet, 6);
    canvas_free(&sheet);
    if (items.n) {
        Canvas ic = canvas_new(items.n * 14 + 4, 30, BG);
        for (int i = 0; i < items.n; i++) {
            sprite_blit(&ic, &items.s[i], 4 + i * 14, 4, C_CLAWD);
            const char *dash = strrchr(items.s[i].name, '-');
            char lab[8];
            snprintf(lab, sizeof lab, "%.3s", dash ? dash + 1 : items.s[i].name);
            for (char *p = lab; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            canvas_text(&ic, 4 + i * 14, 20, lab, RGB(170, 170, 190));
        }
        snprintf(path, sizeof path, "%s/items.png", out_dir);
        rc |= png_write(path, &ic, 8);
        canvas_free(&ic);
    }
    sprites_free(&frames); sprites_free(&items);
    printf("built %s\n", out_dir);
    return rc;
}
