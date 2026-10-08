#!/usr/bin/env python3
"""Chapter 1: the golden model of the 4-bit adder, written in Python and sharing nothing with the Verilog. It writes every possible input (2^9 = 512 of them) with the answer, one line per case, as hexadecimal, for the testbench to read with $readmemh.
Line format: a b cin -> sum cout packed as one 17-bit word: {a[3:0], b[3:0], cin, sum[3:0], cout} = 4+4+1+4+1 = 14 bits."""
import sys
def gold(a, b, cin): t = a + b + cin; return t & 15, t >> 4
out = []
for a in range(16):
    for b in range(16):
        for cin in range(2):
            s, c = gold(a, b, cin); out.append((a << 10) | (b << 6) | (cin << 5) | (s << 1) | c)
open(sys.argv[1], "w").write("\n".join("%04x" % v for v in out) + "\n"); print(len(out), "cases written to", sys.argv[1])
