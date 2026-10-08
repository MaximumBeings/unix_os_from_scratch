#!/usr/bin/env python3
"""Chapter 2, running example A: follow the MAC cycle by cycle. A short program is run on the real circuit (Icarus) and, separately, worked out in plain Python with no reference to the circuit; the two tables must agree. Usage: ch02_example_a.py"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
R = hw.ROOT
P = [(1, 1, 3, 4), (0, 1, -5, 6), (0, 1, 100, -2), (0, 0, 99, 99), (0, 1, -128, -128), (0, 1, 7, -9), (0, 1, -1, 127), (1, 1, 2, 2), (0, 1, 10, 10), (1, 0, 55, 55)]
acc = 0; hand = []
for clr, en, a, b in P:                                    # the rules of the MAC, written out by hand
    if clr: acc = a * b if en else 0
    elif en: acc += a * b
    hand.append(acc)
subprocess.run(["iverilog", "-g2012", "-s", "mac_trace_tb", "-o", "/tmp/mact.vvp", "rtl/mac.v", "tb/mac_trace_tb.v"], cwd=R, capture_output=True, check=True)
out = subprocess.run(["vvp", "-n", "/tmp/mact.vvp"], cwd=R, capture_output=True, text=True).stdout
rows = [tuple(map(int, l.split()[1:])) for l in out.splitlines() if l.startswith("T ")]
print("cycle  clr en    a    b | product | hand-worked acc | circuit acc | agree")
ok = True
for i, ((clr, en, a, b), h, r) in enumerate(zip(P, hand, rows), 1):
    good = r[4] == h; ok &= good
    print(f"{i:5d}  {clr:3d} {en:2d} {a:4d} {b:4d} | {a*b:7d} | {h:15d} | {r[4]:11d} | {'yes' if good else 'NO'}")
print("\nall 10 cycles agree" if ok and len(rows) == len(P) else "\nMISMATCH")
print("\nread the table: cycle 4 has en = 0, so the product 99*99 is ignored and the accumulator keeps the value from cycle 3 (-218).")
print("cycle 5 adds the largest possible product, +16384: -218 + 16384 = 16166.  Cycle 8 (clr with en) throws the old sum away and starts again at 2*2 = 4.  Cycle 10 (clr without en) forces 0.")
sys.exit(0 if ok else 1)
