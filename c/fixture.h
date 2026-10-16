/* A fixed room scene from a text file, so the C and Python drawings can be compared (see tools/fixtures/). */
#ifndef FIXTURE_H
#define FIXTURE_H
#include "room.h"

typedef struct {
    Room room;
    Session s[64], g[16];
    Particle parts[64];
    int cols, rows;
    double now, t;
} Fixture;

int fixture_load(Fixture *f, const char *path); /* 0 on success */
#endif
