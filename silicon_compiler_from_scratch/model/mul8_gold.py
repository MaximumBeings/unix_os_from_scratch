#!/usr/bin/env python3
"""Chapter 2: golden vectors for the 8x8 signed multipliers: every one of the 65,536 input pairs, with the 16-bit two's-complement product, as 8 hex digits per line: {a[7:0], b[7:0], p[15:0]}."""
import sys
def s8(x): return x - 256 if x >= 128 else x
lines = []
for a in range(256):
    for b in range(256):
        p = (s8(a) * s8(b)) & 0xFFFF; lines.append("%02x%02x%04x" % (a, b, p))
open(sys.argv[1], "w").write("\n".join(lines) + "\n"); print(len(lines), "pairs written to", sys.argv[1])
