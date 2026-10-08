#!/usr/bin/env python3
"""Chapter 3: golden vectors for the requantizer. Directed cases (the accumulator at both ends of int32, exact ties at .5 in both signs, mantissas at both ends, shifts at both ends, results exactly at the clamp limits, zero) and random ones.
The FIRST line is the number of cases that follow. Each following line: {acc[31:0], m[23:0], s[5:0], relu, q[7:0]} = 32 + 24 + 6 + 1 + 8 = 71 bits as 18 hex digits (the top digit holds 3 bits). Usage: requant_gold.py OUTFILE [COUNT]"""
import os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from quant import requant
count = int(sys.argv[2]) if len(sys.argv) > 2 else 200000; R = random.Random(3); rows = []
def add(acc, m, s, relu):
    q = requant(acc, m, s, bool(relu)); rows.append(((acc & 0xFFFFFFFF) << 39) | (m << 15) | (s << 9) | (relu << 8) | (q & 255))
EX = [0, 1, -1, 2, -2, 127, -127, 128, -128, 2**31 - 1, -2**31, 2**30, -2**30, 12345, -12345, 99999999, -99999999]
for acc in EX:
    for m in (1 << 23, (1 << 24) - 1, 12345678, 0xAAAAAA):
        for s in (16, 23, 24, 31, 39, 47, 63):
            for relu in (0, 1): add(acc, m, s, relu)
for s in range(0, 64):                                    # exact ties: acc * m = (2k + 1) * 2^(s-1), so the true quotient is k + 1/2
    for k in (0, 1, 2, 5, 63, 126, 127, 200):
        if s >= 1:
            m = 1 << 23; prod = (2 * k + 1) << (s - 1)
            if prod % m == 0:
                acc = prod // m
                if -2**31 <= acc < 2**31:
                    for sg in (1, -1):
                        for relu in (0, 1): add(sg * acc, m, s, relu)
for _ in range(count):
    s = R.randrange(0, 64); m = R.randrange(1 << 23, 1 << 24); acc = R.choice([R.randrange(-2**31, 2**31), R.randrange(-2**20, 2**20), R.randrange(-300, 300) * (1 << R.randrange(0, 16))])
    add(max(-2**31, min(2**31 - 1, acc)), m, s, R.randrange(2))
open(sys.argv[1], "w").write("%018x\n" % len(rows) + "\n".join("%018x" % r for r in rows) + "\n"); print(len(rows), "requantization cases written to", sys.argv[1])
