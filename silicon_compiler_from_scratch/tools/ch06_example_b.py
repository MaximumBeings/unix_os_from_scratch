#!/usr/bin/env python3
"""Chapter 6, running example B: two experiments on the limits of an integer softmax. (1) A temperature sweep on the REAL circuit: the same eight logits multiplied by 0.25 .. 4 (scores are clipped to int8), showing how sharp or flat the distribution becomes. (2) The exp table's cut-off: one big score and 63 small ones, as the small ones fall further below the maximum; the integer model (which the circuit matches bit for bit, Chapter 6's run) against the real softmax. Writes out/ch06_example_b.json. Usage: ch06_example_b.py"""
import json, math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import softmax_gold as g
R = hw.ROOT; N = 8
logits = [2.0, 1.0, 0.5, 0.0, -0.5, -1.0, -2.0, -3.0]
subprocess.run(["iverilog", "-g2012", "-DNN=8", "-s", "softmax_one_tb", "-o", "/tmp/s8.vvp", "rtl/exp_lut.v", "rtl/divu.v", "rtl/softmax.v", "tb/softmax_one_tb.v"], cwd=R, capture_output=True, check=True)
print("== 1. temperature on the circuit: scores x = clip(round(16 * k * logit)), logits =", logits)
print("   k (sharpness)   scores                              top p     p of 2nd   p of last    sum(p)   circuit = integer model?")
res = {"ks": [], "p": [], "scores": []}; ok = True
for k in (0.25, 0.5, 1.0, 2.0, 4.0):
    xs = [max(-128, min(127, round(16 * k * v))) for v in logits]; bus = "".join("%02x" % (v & 255) for v in reversed(xs))
    out = subprocess.run(["vvp", "-n", "/tmp/s8.vvp", "+bus=" + bus], cwd=R, capture_output=True, text=True).stdout
    p = [int(l.split()[2]) for l in out.splitlines() if l.startswith("P ")]; same = p == g.softmax_fixed(xs); ok &= same
    res["ks"].append(k); res["p"].append(p); res["scores"].append(xs)
    print(f"   {k:5.2f}         {str(xs):36s}{p[0]/65536:7.3f}    {p[1]/65536:7.3f}    {p[-1]/65536:8.5f}   {sum(p):6d}   {'yes' if same else 'NO'}")
print("   Multiplying the scores by k is dividing the temperature by k. k = 0.25 is nearly flat; k = 4 puts almost everything on the top element.")
print("   (At k = 4 the top two scores clip at 127: the int8 range limits how sharp the distribution can be made. That is a property of Q4.4 scores.)")
print("\n== 2. the cut-off of the exp table: 1 big score (127) and 63 equal small scores (127 - d), N = 64")
print("   d    T[d]    p_big (fixed)   p_big (real)   error of p_big   p_small (fixed)  p_small (real)   sum of all p (fixed)")
res["cut"] = []
for dd in (100, 150, 180, 188, 189, 200):
    xs = [127] + [127 - dd] * 63; pf = g.softmax_fixed(xs); pr = g.softmax_float(xs)
    res["cut"].append({"d": dd, "T": g.T[dd], "big": pf[0] / 65536, "bigr": pr[0], "small": pf[1] / 65536, "smallr": pr[1]})
    print(f"  {dd:3d}  {g.T[dd]:6d}   {pf[0]/65536:12.6f}   {pr[0]:12.6f}   {abs(pf[0]/65536 - pr[0])*65536:9.2f} u    {pf[1]/65536:12.7f}   {pr[1]:12.7f}   {sum(pf):8d}")
print("   (u = units of 2^-16). Up to d = 188 the table still has a non-zero entry; from d = 189 the entry is 0 and the 63 small scores get probability exactly 0,")
print("   so the big one takes everything (65536). The real value is 0.9995 at d = 189: a deliberate rounding of probabilities below 7.4e-6 each, which add up over 63 terms to 4.7e-4.")
print("   The ORDER is never changed (the big score always has the largest p), which is what greedy decoding needs; the SUM of tiny probabilities is what is lost.")
json.dump(res, open(os.path.join(R, "out", "ch06_example_b.json"), "w")); sys.exit(0 if ok else 1)
