#!/usr/bin/env python3
"""Chapter 6, running example A: one softmax, every step. Scores [32, 16, 0, -16] (real values 2, 1, 0, -1 times 16). The script works the algorithm out step by step in integers, compares with the real softmax in floating point, and then runs the same scores through the softmax circuit (Icarus) and checks that it produces the same integers and the cycle count 3N + 43. Writes out/ch06_example_a.json. Usage: ch06_example_a.py"""
import json, math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import softmax_gold as g
R = hw.ROOT; xs = [32, 16, 0, -16]; N = len(xs)
print("scores x (int8, real value = x/16):", xs, " real:", [x / 16 for x in xs])
m = max(xs); d = [m - x for x in xs]; e = [g.T[v] for v in d]; s = sum(e); r = (1 << 38) // s; p = [(ei * r + (1 << 21)) >> 22 for ei in e]
print(f"\nstep 1  maximum  m = {m}")
print(f"step 2  distance below the maximum  d = m - x = {d}   (an integer 0..255, the table index)")
print(f"        table lookup  e = T[d] = round(65535 * exp(-d/16)) = {e}")
print("        check one entry by hand: T[16] = 65535 * exp(-1) = %.2f -> %d;  T[32] = 65535 * exp(-2) = %.2f -> %d" % (65535 * math.exp(-1), g.T[16], 65535 * math.exp(-2), g.T[32]))
print(f"step 3  sum  s = {' + '.join(map(str, e))} = {s}")
print(f"step 4  ONE division  r = floor(2^38 / s) = floor({1<<38} / {s}) = {r}")
print(f"step 5  p = (e * r + 2^21) >> 22  =  {p}     (Q0.16: 65536 means 1.0)")
print(f"        sum of the p = {sum(p)} (an exact softmax sums to 65536; rounding can make it differ by a few units: here by {sum(p) - 65536})")
pf = g.softmax_float(xs)
print("\nagainst the real softmax (floating point):")
print("   i    x    exp(x/16 - m/16)   real p      fixed p / 65536   error (units of 2^-16)")
for i in range(N): print(f"  {i}  {xs[i]:4d}   {math.exp((xs[i]-m)/16):10.6f}      {pf[i]:.6f}    {p[i]/65536:.6f}        {abs(p[i]/65536 - pf[i])*65536:5.2f}")
bus = "".join("%02x" % (v & 255) for v in reversed(xs))
subprocess.run(["iverilog", "-g2012", "-DNN=%d" % N, "-s", "softmax_one_tb", "-o", "/tmp/s1.vvp", "rtl/exp_lut.v", "rtl/divu.v", "rtl/softmax.v", "tb/softmax_one_tb.v"], cwd=R, capture_output=True, check=True)
out = subprocess.run(["vvp", "-n", "/tmp/s1.vvp", "+bus=" + bus], cwd=R, capture_output=True, text=True).stdout
hwp = [int(l.split()[2]) for l in out.splitlines() if l.startswith("P ")]; cyc = int([l for l in out.splitlines() if l.startswith("CYCLES")][0].split()[1])
ok = hwp == p and cyc == 3 * N + 43
print(f"\nthe circuit (softmax.v, N={N}) gives {hwp}  -> {'identical to the integer steps above' if hwp == p else 'DIFFERENT'}")
print(f"cycles: {cyc}; formula 3N + 43 = {3*N+43}: {'ok' if cyc == 3*N+43 else 'DIFFERENT'}   (4 + 4 + 40 + 4 + 3 = {4+4+40+4+3}: max pass, exp+sum pass, divider, scale pass, control)")
json.dump({"xs": xs, "d": d, "e": e, "s": s, "r": r, "p": p, "pf": pf, "cycles": cyc}, open(os.path.join(R, "out", "ch06_example_a.json"), "w"))
sys.exit(0 if ok else 1)
