/* Following the events file (Tail in room_tui.py): never writes to it; tolerates missing, truncated, bad lines. */
#ifndef TAIL_H
#define TAIL_H
#include <stddef.h>

typedef struct { double t; char sid[72], ev[32], cwd[48], tool[40]; } Event;

typedef struct { char path[512]; long pos; char *buf; size_t buflen; } Tail;

void tail_init(Tail *t, const char *path);
void tail_free(Tail *t);
/* events newer than `since`; afterwards tail_poll continues from the end of the file. Returns the number written to out */
int tail_history(Tail *t, double since, Event *out, int max);
int tail_poll(Tail *t, Event *out, int max);
/* one line of the file: 1 and the five fields if it is a valid record, else 0 */
int tail_parse(const char *line, size_t n, Event *e);
#endif
