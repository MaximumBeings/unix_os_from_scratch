#!/usr/bin/env python3
"""Chapter 2, running example B: overflow. (1) How many worst-case products fit in 32 bits? (2) The real circuits, wrapping and saturating, fed that many, compared with arithmetic done in Python. (3) How likely is overflow for a REAL dot product? A measured experiment on random int8 vectors. Usage: ch02_example_b.py"""
import math, os, random, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
R = hw.ROOT
def wrap(x): x &= (1 << 32) - 1; return x - (1 << 32) if x >= 1 << 31 else x
def sat(x): return max(-(1 << 31), min((1 << 31) - 1, x))
print("== 1. derived: the width of a sum of K products, worst case |p| = 128*128 = 16384 = 2^14")
print("   K products need ceil(log2(K * 2^14 + 1)) + 1 bits (the +1 is the sign). 32 bits hold 2^31 - 1 = 2147483647.")
for K in (1, 16, 256, 4096, 65536, 131071, 131072):
    need = (K * 16384).bit_length() + 1; print(f"   K = {K:7d}: worst-case sum {K*16384:11d}  needs {need:2d} bits  {'fits in 32' if K*16384 <= 2**31-1 else 'DOES NOT FIT'}")
print(f"   so the largest K that always fits is floor((2^31 - 1) / 16384) = {(2**31-1)//16384}; the 131072nd worst-case product is the first that overflows")
print("\n== 2. the circuits: Icarus on rtl/mac.v (SAT=0, wrap) and mac_sat (SAT=1), against Python")
subprocess.run(["iverilog", "-g2012", "-s", "mac_overflow_tb", "-o", "/tmp/macov.vvp", "rtl/mac.v", "tb/mac_overflow_tb.v"], cwd=R, capture_output=True, check=True)
out = subprocess.run(["vvp", "-n", "/tmp/macov.vvp"], cwd=R, capture_output=True, text=True).stdout
print("   products added | wrap acc (circuit) | sat acc (circuit) | wrap (Python) | sat (Python) | agree")
ok = True
for l in out.splitlines():
    if not l.startswith("O "): continue
    n, w, s = map(int, l.split()[1:]); ew, es = wrap(n * 16384), sat(n * 16384); g = (w == ew and s == es); ok &= g
    print(f"   {n:14d} | {w:18d} | {s:17d} | {ew:13d} | {es:12d} | {'yes' if g else 'NO'}")
print("   look at row 131072: the wrapping unit has jumped from +2147467264 to -2147483648 (a huge positive sum became a huge NEGATIVE one);")
print("   the saturating unit sits at +2147483647. Both are wrong in the strict sense (the true sum is 2147483648), but one is wrong by 1 and the other by 4.3 billion.")
print("\n== 3. measured: a REAL dot product of random int8 vectors, seed 5, 2000 trials per length (200 for the two longest)")
rng = random.Random(5); print("   length K | largest |sum| seen | worst case K*16384 | ratio    | any overflow of 32 bits?")
for K in (64, 256, 1024, 4096, 16384, 65536):
    worst = 0
    for _ in range(2000 if K <= 4096 else 200):
        s = sum(rng.randrange(-128, 128) * rng.randrange(-128, 128) for _ in range(K)); worst = max(worst, abs(s))
    print(f"   {K:8d} | {worst:17d} | {K*16384:18d} | {worst/(K*16384):.5f} | {'yes' if worst >= 2**31 else 'no'}")
print("   Random data grows like sqrt(K), the worst case like K: the typical sum stays about 1/sqrt(K) of the bound. Overflow needs ADVERSARIAL or very structured data;")
print("   this is why 32-bit accumulators are the industry norm for int8, and why the MAC still has to define what happens (a rare event in a safety-critical system is still an event).")
sys.exit(0 if ok else 1)
