/* A fixed room scene from a text file, for tests (see tools/fixtures/ and tools/test.sh). */
#ifndef FIXTURE_H
#define FIXTURE_H
#include "room.h"

typedef struct {
    Room room;
    int cols, rows;
    double now, t;
} Fixture;

int fixture_load(Fixture *f, const char *path); /* 0 on success */
int fixture_load_text(Fixture *f, const char *text); /* ... the same from lines in memory (the pixel editor's stage) */
#endif
