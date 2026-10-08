#!/usr/bin/env python3
"""Chapter 1, running example B: the answer key for the 4-bit counter. It steps a Python integer through the same cycle-by-cycle rules and writes, for every clock cycle, the inputs (rst, en) and what the circuit must show:
the wrap flag BEFORE the edge and the value of q AFTER it. File: the first line is the number of cycles; each following line is 4 hex digits {rst, en, wrap, q[3:0]} (bit 6 rst, bit 5 en, bit 4 wrap, bits 3..0 q). Usage: counter4_gold.py OUTFILE [CYCLES]"""
import random, sys
def step(q, rst, en):
    """One clock edge. Returns (wrap flag seen before the edge, q after the edge)."""
    wrap = int(bool(en) and q == 15)
    if rst: return wrap, 0
    return wrap, ((q + 1) % 16 if en else q)
def scenario(cycles):
    R = random.Random(4)
    seq = [(1, 0)] * 2 + [(0, 1)] * 40 + [(0, 0)] * 20 + [(0, 1)] * 5 + [(1, 1)] * 3 + [(0, 1)] * 18        # reset; count past a wrap; hold; reset while enabled; count again
    while len(seq) < cycles: seq.append((int(R.random() < .03), int(R.random() < .7)))
    return seq
if __name__ == "__main__":
    out = sys.argv[1]; n = int(sys.argv[2]) if len(sys.argv) > 2 else 4000; q = 7; rows = []      # the model may start anywhere: the first cycles reset it
    for rst, en in scenario(n):
        wrap, q2 = step(q, rst, en); rows.append("%04x" % ((rst << 6) | (en << 5) | (wrap << 4) | q2)); q = q2
    open(out, "w").write("%04x\n" % len(rows) + "\n".join(rows) + "\n"); print(len(rows), "cycles written to", out)
