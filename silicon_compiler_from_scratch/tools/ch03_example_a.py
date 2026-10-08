#!/usr/bin/env python3
"""Chapter 3, running example A: quantize by hand. (1) one vector of real numbers to int8 and back; (2) a complete tiny layer: real X and W -> int8 -> int32 accumulators -> requantizer (mantissa and shift, checked on the real circuit in Icarus) -> int8 -> back to real, against the real answer. Usage: ch03_example_a.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); from quant import quantize, scale_for, mantissa_shift, requant, matmul_int8
R = hw.ROOT
x = [0.42, -1.30, 2.75, -0.08, 1.91, -2.20, 0.00, 0.66]
sc = scale_for(x)
print("== 1. one vector. scale = max|x| / 127 = %.2f / 127 = %.6f   (one int8 step is worth %.6f)" % (max(abs(v) for v in x), sc, sc))
print("      x       x/scale   q (rounded)  q*scale   error")
tot = 0
for v in x:
    q = quantize(v, sc); b = q * sc; tot += abs(b - v)
    print(f"   {v:6.2f}   {v/sc:9.3f}   {q:6d}      {b:7.4f}  {b-v:+.5f}")
print(f"   largest possible error is half a step = {sc/2:.6f}; the largest error here is {max(abs(quantize(v, sc)*sc - v) for v in x):.6f}.  The value 2.75 (the maximum) is exact by construction.")
print("\n== 2. a complete tiny layer: Y = X W, X is 2x3, W is 3x2")
X = [[0.5, -1.0, 1.5], [2.0, 0.25, -0.75]]; W = [[1.2, -0.4], [-0.6, 0.9], [0.3, -1.1]]
Yreal = [[sum(X[i][t] * W[t][j] for t in range(3)) for j in range(2)] for i in range(2)]
sx = scale_for([v for r in X for v in r]); sw = scale_for([v for r in W for v in r])
Xq = [[quantize(v, sx) for v in r] for r in X]; Wq = [[quantize(v, sw) for v in r] for r in W]
print(f"   scale_x = 2.0/127 = {sx:.6f},  scale_w = 1.2/127 = {sw:.6f}")
print("   Xq =", Xq); print("   Wq =", Wq)
acc = matmul_int8(Xq, Wq); print("   int32 accumulators  acc = Xq Wq =", acc, "  (exact integer arithmetic, no rounding yet)")
print("   real value of an accumulator = acc * scale_x * scale_w:")
for i in range(2): print("     row", i, [f"{acc[i][j]*sx*sw:.4f}" for j in range(2)], " real answer", [f"{v:.4f}" for v in Yreal[i]])
so = scale_for([v for r in Yreal for v in r]); M = sx * sw / so; m, s = mantissa_shift(M)
print(f"\n   the output will be int8 too: scale_out = max|Y| / 127 = {max(abs(v) for r in Yreal for v in r):.4f} / 127 = {so:.6f}")
print(f"   so each accumulator must be multiplied by M = scale_x * scale_w / scale_out = {M:.8f}")
print(f"   as a mantissa and shift: M ~ m / 2^s with m = {m} (a 24-bit number, between 2^23 = 8388608 and 2^24 = 16777216) and s = {s};  m/2^s = {m/2**s:.8f}")
print("\n   requantizer, one accumulator at a time: software reference vs the real circuit (Icarus)")
subprocess.run(["iverilog", "-g2012", "-s", "requant_one_tb", "-o", "/tmp/rq1.vvp", "rtl/requant.v", "tb/requant_one_tb.v"], cwd=R, capture_output=True, check=True)
ok = True; Yq = []
print("   acc    acc*M      software q   circuit q   real value q*scale_out   real answer")
for i in range(2):
    row = []
    for j in range(2):
        a = acc[i][j]; sw_q = requant(a, m, s, False)
        out = subprocess.run(["vvp", "-n", "/tmp/rq1.vvp", f"+acc={a}", f"+m={m}", f"+s={s}", "+relu=0"], cwd=R, capture_output=True, text=True).stdout
        hw_q = int([l for l in out.splitlines() if l.startswith("Q ")][0].split()[1]); ok &= hw_q == sw_q; row.append(hw_q)
        print(f"   {a:5d}  {a*M:9.3f}  {sw_q:8d}    {hw_q:8d}      {hw_q*so:9.4f}              {Yreal[i][j]:9.4f}")
    Yq.append(row)
err = max(abs(Yq[i][j] * so - Yreal[i][j]) for i in range(2) for j in range(2))
print(f"\n   circuit and software agree on all four: {ok}.  Largest error against the real answer: {err:.4f} (about {err/so:.2f} output steps). Most of it was already present in the accumulators, which came from rounded INPUTS (compare the two lines above); the requantizer itself adds at most half an output step.")
print("\n== 3. ties and signs: what 'round half away from zero' does (M = 1/2, so acc*M = acc/2; m = 2^23, s = 24)")
print("   acc   acc*M   away from zero   to nearest even   circuit")
subprocess.run(["iverilog", "-g2012", "-s", "requant_one_tb", "-o", "/tmp/rq1.vvp", "rtl/requant.v", "tb/requant_one_tb.v"], cwd=R, capture_output=True, check=True)
for a in (-5, -3, -1, 1, 3, 5):
    hwq = int([l for l in subprocess.run(["vvp", "-n", "/tmp/rq1.vvp", f"+acc={a}", f"+m={1<<23}", "+s=24", "+relu=0"], cwd=R, capture_output=True, text=True).stdout.splitlines() if l.startswith("Q ")][0].split()[1])
    ok &= hwq == requant(a, 1 << 23, 24); print(f"   {a:3d}   {a/2:5.1f}   {requant(a, 1<<23, 24):10d}       {round(a/2):10d}        {hwq:6d}")
print("   (Python's round() goes to the nearest EVEN number: 0.5 -> 0, 2.5 -> 2. The chip goes AWAY from zero, symmetrically: 0.5 -> 1, -0.5 -> -1, 2.5 -> 3. Both are fine; they must simply be the same everywhere.)")
sys.exit(0 if ok else 1)
