#!/usr/bin/env python3
"""Chapter 3, running example B: the outlier experiment, taken apart. A 32 x 256 activation matrix times a 256 x 32 weight matrix, one weight column made `f` times larger than the others. For a sweep of f, the relative RMS error on the NORMAL columns with one scale for the whole tensor and with one scale per column; plus the histogram of how many of the 255 int8 levels a normal column actually uses. Writes out/ch03_example_b.json for the figures. Usage: ch03_example_b.py"""
import json, math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); from quant import quantize, scale_for, matmul_int8
R = random.Random(21); N, K, P = 32, 256, 32
def rms(v): return math.sqrt(sum(x * x for x in v) / len(v))
X = [[R.gauss(0, 1) for _ in range(K)] for _ in range(N)]; W0 = [[R.gauss(0, 1) for _ in range(P)] for _ in range(K)]
sx = scale_for([v for r in X for v in r]); Xq = [[quantize(v, sx) for v in r] for r in X]
def run(f, per_col):
    W = [[v * (f if j == 0 else 1) for j, v in enumerate(r)] for r in W0]; cols = list(zip(*W))
    sw = [scale_for(c) for c in cols] if per_col else [scale_for([v for r in W for v in r])] * P
    Wq = [[quantize(W[t][j], sw[j]) for j in range(P)] for t in range(K)]; acc = matmul_int8(Xq, Wq)
    err = []; ref = []
    for i in range(N):
        for j in range(1, P):
            e = sum(X[i][t] * W[t][j] for t in range(K)); err.append(acc[i][j] * sx * sw[j] - e); ref.append(e)
    used = len({Wq[t][1] for t in range(K)})                 # how many distinct int8 values does column 1 (a NORMAL column) use?
    return rms(err) / rms(ref), used, Wq
FS = [1, 2, 4, 8, 16, 32, 64, 128]; res = {"f": FS, "tensor": [], "column": [], "levels_tensor": [], "levels_column": []}
print(f"X is {N} x {K}, W is {K} x {P}, Gaussian, seed 21; column 0 of W is multiplied by f. Error is measured on columns 1..{P-1} only (the normal ones).\n")
print("   f | per-tensor error | per-column error | distinct int8 levels used by a normal column: per-tensor / per-column")
for f in FS:
    et, ut, _ = run(f, False); ec, uc, _ = run(f, True)
    res["tensor"].append(et); res["column"].append(ec); res["levels_tensor"].append(ut); res["levels_column"].append(uc)
    print(f"{f:4d} | {et*100:15.2f}% | {ec*100:15.2f}% | {ut:4d} / {uc:4d}")
_, _, Wq = run(32, False)
hist = {}
for t in range(K): hist[Wq[t][1]] = hist.get(Wq[t][1], 0) + 1
res["hist32"] = sorted(hist.items()); _, _, Wq1 = run(1, False); h1 = {}
for t in range(K): h1[Wq1[t][1]] = h1.get(Wq1[t][1], 0) + 1
res["hist1"] = sorted(h1.items())
print(f"\nfor f = 32, per-tensor: column 1 has {K} weights and they land on only {len(hist)} distinct int8 values, between {min(hist)} and {max(hist)}; with f = 1 they spread over {len(h1)} values, {min(h1)}..{max(h1)}.")
print("each weight's rounding error is at most half a step; the step is set by the OUTLIER, so it is f times larger than the normal column needs.")
print("\nreading the sweep: the per-column error stays near 1.2% whatever f is (each column is scaled to itself). The per-tensor error grows about linearly with f once f is large,")
print("because the step size is about f times coarser than the normal columns need: that is exactly the cost of using one scale for a tensor whose columns differ in size.")
json.dump(res, open(os.path.join(hw.ROOT, "out", "ch03_example_b.json"), "w"))
