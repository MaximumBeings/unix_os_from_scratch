#!/usr/bin/env python3
"""Chapter 3: how much does int8 cost in accuracy? Pure Python, fixed seeds. Compares per-tensor and per-row weight scales, with and without an outlier row, and shows error against the length K of the dot product."""
import math, os, random, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
from quant import quantize, scale_for, mantissa_shift, requant, matmul_int8
R = random.Random(11)
def gauss_matrix(n, k, sd=1.0): return [[R.gauss(0, sd) for _ in range(k)] for _ in range(n)]
def rms(v): return math.sqrt(sum(x * x for x in v) / len(v))
def fmatmul(A, B): return [[sum(A[i][t] * B[t][j] for t in range(len(B))) for j in range(len(B[0]))] for i in range(len(A))]
def transpose(M): return [list(r) for r in zip(*M)]

def int8_layer(X, W, per_row):
    """X: n x k activations (one scale), W: k x p weights (one scale, or one per output column). Returns the dequantized int32 result and the final int8 output with its scale."""
    sx = scale_for([v for r in X for v in r]); Xq = [[quantize(v, sx) for v in r] for r in X]
    cols = transpose(W); sw = [scale_for(c) for c in cols] if per_row else [scale_for([v for c in cols for v in c])] * len(cols)
    Wq = transpose([[quantize(v, sw[j]) for v in c] for j, c in enumerate(cols)])
    acc = matmul_int8(Xq, Wq)
    deq = [[acc[i][j] * sx * sw[j] for j in range(len(sw))] for i in range(len(acc))]
    return deq

def rel_err(approx, exact):
    a = [v for r in approx for v in r]; e = [v for r in exact for v in r]
    return rms([x - y for x, y in zip(a, e)]) / rms(e)

print("== 1. relative RMS error of an int8 layer (X: 32 x K, W: K x 32, fp64 is the reference)")
print(f"{'case':44s} {'K=64':>8s} {'K=256':>8s} {'K=1024':>8s}")
for name, outlier, per_row in (("per-tensor scale, plain weights", False, False), ("per-column scale, plain weights", False, True),
                               ("per-tensor scale, one column x30", True, False), ("per-column scale, one column x30", True, True)):
    row = []
    for K in (64, 256, 1024):
        X = gauss_matrix(32, K); W = gauss_matrix(K, 32)
        if outlier:
            for t in range(K): W[t][0] *= 30
        row.append(rel_err(int8_layer(X, W, per_row), fmatmul(X, W)))
    print(f"{name:44s} " + " ".join(f"{e*100:7.3f}%" for e in row))

print("\n== 2. the outlier hurts the OTHER columns: error measured on columns 1..31 only (K=256)")
for per_row in (False, True):
    X = gauss_matrix(32, 256); W = gauss_matrix(256, 32)
    for t in range(256): W[t][0] *= 30
    d = int8_layer(X, W, per_row); e = fmatmul(X, W)
    print(f"{'per-column' if per_row else 'per-tensor':12s} error on the normal columns: {rel_err([r[1:] for r in d], [r[1:] for r in e])*100:.3f}%")

print("\n== 3. the mantissa/shift form of a real multiplier")
worst = 0
for _ in range(100000):
    M = 2 ** R.uniform(-16, 0); m, s = mantissa_shift(M); worst = max(worst, abs(m / 2 ** s - M) / M)
print(f"worst relative error of M ~ m/2^s over 100000 random M in [2^-16, 1]: {worst:.3e}   (bound 2^-24 = {2**-24:.3e})")

print("\n== 4. end to end: int8 layer, requantized to int8, against the fp64 result (K=256)")
X = gauss_matrix(32, 256); W = gauss_matrix(256, 32); exact = fmatmul(X, W)
sx = scale_for([v for r in X for v in r]); sw = scale_for([v for c in W for v in c]); so = scale_for([v for r in exact for v in r])
Xq = [[quantize(v, sx) for v in r] for r in X]; Wq = [[quantize(v, sw) for v in r] for r in W]
acc = matmul_int8(Xq, Wq); m, s = mantissa_shift(sx * sw / so)
out = [[requant(a, m, s) for a in r] for r in acc]
print(f"m = {m} (24 bits), s = {s}; int8 output error relative to fp64: {rel_err([[q * so for q in r] for r in out], exact)*100:.3f}%")
print(f"max |acc| seen {max(abs(a) for r in acc for a in r)} of 2^31 = {2**31}: the 32-bit accumulator is {2**31 // max(abs(a) for r in acc for a in r)}x larger than needed here")

print("\n== 5. bytes per weight, and what a 7-billion-parameter model occupies")
for name, b in (("fp32", 4.0), ("fp16/bf16", 2.0), ("int8", 1.0), ("int4", 0.5)):
    print(f"{name:10s} {b:4.1f} bytes/weight   {7e9 * b / 1e9:5.1f} GB   (derived: 7e9 x bytes)")
