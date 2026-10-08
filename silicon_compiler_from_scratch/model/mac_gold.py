#!/usr/bin/env python3
"""Chapter 2: golden vectors for the MAC unit. A fixed pseudo-random program of clock cycles: directed sequences that reach every corner (the largest product, -128 * -128 = +16384 which does not fit in int8 arithmetic, long runs that overflow 32 bits both upward and downward, clear with and
without enable, enable off), then random ones. The FIRST line is the number of rows that follow. Each following line is {repeat, clr, en, a, b, acc_wrap, acc_sat} = 20 + 1 + 1 + 8 + 8 + 32 + 32 = 102 bits, written as 26 hex digits: the same clock cycle applied `repeat` times in a row, followed by the values both units must hold AFTER the last of them (long runs are checked every 32,768 cycles).
Usage: mac_gold.py OUTFILE"""
import random, sys
M = 1 << 32
def wrap(x): x &= M - 1; return x - M if x >= M // 2 else x
def sat(x): return max(-(1 << 31), min((1 << 31) - 1, x))
R = random.Random(2); rows = []; aw = asat = 0
def clock(clr, en, a, b, rep=1):
    global aw, asat
    p = a * b
    for k in range(rep):
        if clr and k == 0: aw = asat = (p if en else 0)
        elif en: aw = wrap(aw + p); asat = sat(asat + p)
    rows.append((rep, clr, en, a & 255, b & 255, aw & (M - 1), asat & (M - 1)))
clock(0, 0, 0, 0)                                                                # right after reset: the accumulator must be 0 (a unit that resets to the wrong value is caught here)
clock(1, 0, 0, 0)
for a, b in ((-128, -128), (-128, 127), (127, 127), (127, -128), (0, 0), (1, -1), (-1, -1)):
    clock(1, 1, a, b); clock(0, 1, a, b); clock(0, 0, 5, 5)                     # a fresh sum, then one more product, then an idle cycle
for a, b in ((-128, -128), (127, 127)):                                          # overflow upward: 2^31 / 16384 = 131072 products of the largest value
    clock(1, 0, 0, 0)
    for _ in range(4): clock(0, 1, a, b, 32768)
clock(1, 0, 0, 0)
for _ in range(5): clock(0, 1, -128, 127, 32768)                                # overflow downward (-16256 each: it takes 132,105 of them to pass -2^31)
for k in range(3000):
    clock(R.random() < .1, R.random() < .85, R.randrange(-128, 128), R.randrange(-128, 128))
open(sys.argv[1], "w").write("%026x\n" % len(rows) + "\n".join("%026x" % ((r << 82) | (c << 81) | (e << 80) | (a << 72) | (b << 64) | (w << 32) | s) for r, c, e, a, b, w, s in rows) + "\n"); print(len(rows), "clock cycles written to", sys.argv[1])
