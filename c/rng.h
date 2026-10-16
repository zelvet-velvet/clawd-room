/* Python's random.Random, bit for bit: Mersenne Twister (MT19937), seeded the way random.Random(int) seeds it.
 * Only the calls room_tui.py makes: random(), uniform(a, b) and choice(n) (the index it would pick). */
#ifndef RNG_H
#define RNG_H
#include <stdint.h>

typedef struct { uint32_t mt[624]; int idx; } Rng;

void rng_seed(Rng *r, uint32_t seed);  /* random.Random(seed) for 0 <= seed < 2**32 (a crc32 is) */
double rng_random(Rng *r);             /* [0, 1) with 53 bits, like random.random() */
double rng_uniform(Rng *r, double a, double b);
int rng_choice(Rng *r, int n);         /* index random.choice(seq) takes from a sequence of n items */
#endif
