#!/usr/bin/env python3
"""Prints the random numbers Python's random.Random gives for a few seeds, like `c/clawd-room --golden-rng`."""
import random
import zlib

for i, sid in enumerate(["x", "aaa", "619c9f42-0144-479e-bc6c-ca68e665dc6f", ""]):
    r = random.Random(zlib.crc32(sid.encode()))
    out = ["rng %d" % i]
    out += ["%.17g" % r.random() for _ in range(6)]
    out += ["%.17g" % r.uniform(1.0, 4.0) for _ in range(4)]
    out += [str(r.choice(range(3 + k))) for k in range(8)]
    for _ in range(2000):
        r.random()
    out.append("%.17g" % r.random())
    print(" ".join(out))
