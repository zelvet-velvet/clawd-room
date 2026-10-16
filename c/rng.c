#include "rng.h"

#define N 624
#define M 397

static void init_genrand(Rng *r, uint32_t s) {
    r->mt[0] = s;
    for (int i = 1; i < N; i++) r->mt[i] = 1812433253u * (r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) + (uint32_t)i;
    r->idx = N;
}

/* random.seed(int) uses init_by_array on the 32-bit pieces of abs(seed): here exactly one piece */
void rng_seed(Rng *r, uint32_t seed) {
    uint32_t key[1] = {seed};
    int i = 1, j = 0, k = N;
    init_genrand(r, 19650218u);
    for (; k; k--) {
        r->mt[i] = (r->mt[i] ^ ((r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) * 1664525u)) + key[j] + (uint32_t)j;
        i++; j++;
        if (i >= N) { r->mt[0] = r->mt[N - 1]; i = 1; }
        if (j >= 1) j = 0;
    }
    for (k = N - 1; k; k--) {
        r->mt[i] = (r->mt[i] ^ ((r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
        i++;
        if (i >= N) { r->mt[0] = r->mt[N - 1]; i = 1; }
    }
    r->mt[0] = 0x80000000u;
    r->idx = N;
}

static uint32_t genrand_uint32(Rng *r) {
    static const uint32_t mag[2] = {0u, 0x9908b0dfu};
    if (r->idx >= N) {
        int kk;
        uint32_t y;
        for (kk = 0; kk < N - M; kk++) {
            y = (r->mt[kk] & 0x80000000u) | (r->mt[kk + 1] & 0x7fffffffu);
            r->mt[kk] = r->mt[kk + M] ^ (y >> 1) ^ mag[y & 1u];
        }
        for (; kk < N - 1; kk++) {
            y = (r->mt[kk] & 0x80000000u) | (r->mt[kk + 1] & 0x7fffffffu);
            r->mt[kk] = r->mt[kk + (M - N)] ^ (y >> 1) ^ mag[y & 1u];
        }
        y = (r->mt[N - 1] & 0x80000000u) | (r->mt[0] & 0x7fffffffu);
        r->mt[N - 1] = r->mt[M - 1] ^ (y >> 1) ^ mag[y & 1u];
        r->idx = 0;
    }
    uint32_t y = r->mt[r->idx++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y;
}

double rng_random(Rng *r) { /* two 32-bit draws: 27 + 26 bits */
    uint32_t a = genrand_uint32(r) >> 5, b = genrand_uint32(r) >> 6;
    return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
}

double rng_uniform(Rng *r, double a, double b) { return a + (b - a) * rng_random(r); }

int rng_choice(Rng *r, int n) { /* _randbelow_with_getrandbits: draw k = bit_length(n) bits until the value is < n */
    int k = 0;
    for (int v = n; v; v >>= 1) k++;
    uint32_t v;
    do v = genrand_uint32(r) >> (32 - k); while (v >= (uint32_t)n);
    return (int)v;
}
