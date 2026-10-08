#!/usr/bin/env python3
"""Chapter 8: where does the accuracy go? Starting from floating point, switch on one quantization step at a time and measure the error of the attention output (relative RMS against the all-float result).
Then the error of the real int8 GA-2 pipeline against floating point, by context length. Fixed seeds, Gaussian random data. Usage: ch08_study.py"""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import attn, softmax_gold as sg
from quant import quantize
D = attn.D
def mm(A, B): return [[sum(A[i][t] * B[t][j] for t in range(len(B))) for j in range(len(B[0]))] for i in range(len(A))]
def deq(M, sc): return [[quantize(v, sc) * sc for v in r] for r in M]
def pipeline(ex, stage):
    X, W, s = ex["X"], ex["W"], ex["scales"]
    if stage >= 1: X = deq(X, s["x"]); W = [deq(W[0], s["wq"]), deq(W[1], s["wk"]), deq(W[2], s["wv"])]
    Q, K, V = (mm(X, w) for w in W)
    if stage >= 2: Q, K, V = deq(Q, s["q"]), deq(K, s["k"]), deq(V, s["v"])
    q = Q[-1]; sc = [sum(q[i] * K[p][i] for i in range(D)) / math.sqrt(D) for p in range(len(K))]
    if stage >= 3: sc = [max(-127, min(127, int(math.floor(abs(v) * 16 + 0.5)) * (1 if v >= 0 else -1))) / 16 for v in sc]       # Q4.4 softmax input
    if stage >= 4: p = [v / 65536 for v in sg.softmax_fixed([int(round(v * 16)) for v in sc])]                                      # the circuit's integer softmax
    else:
        mx = max(sc); e = [math.exp(v - mx) for v in sc]; z = sum(e); p = [v / z for v in e]
    if stage >= 5: p = [int(math.floor(v * 127 + 0.5)) / 127 for v in p]                                                           # int8 weights
    out = [sum(p[i] * V[i][c] for i in range(len(V))) for c in range(D)]
    if stage >= 6: out = [quantize(v, s["o"]) * s["o"] for v in out]
    return out
def err(a, b): return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / D) / math.sqrt(sum(y * y for y in b) / D)
NAMES = ["all floating point", "+ int8 inputs and weights", "+ int8 q, k, v", "+ scores rounded to 1/16 (Q4.4)", "+ integer softmax (table, divider)", "+ int8 attention weights", "+ int8 output"]
print("== 1. error added by each quantization step (cumulative), L = 16, 60 random examples")
print(f"{'stage':38s} {'mean error':>11s} {'worst':>8s}")
exs = [attn.make_example(16, 5000 + i) for i in range(60)]
for st in range(7):
    e = [err(pipeline(ex, st), ex["out_float"]) for ex in exs]; print(f"{NAMES[st]:38s} {sum(e)/len(e)*100:10.2f}% {max(e)*100:7.2f}%")
print("\n== 2. the real int8 GA-2 pipeline (reference simulator) against floating point, by context length (30 examples each)")
print(f"{'L':>3s} {'mean error':>11s} {'worst':>8s}")
for L in (1, 2, 4, 8, 16, 24, 32):
    e = []
    for i in range(30):
        ex = attn.make_example(L, 9000 + 100 * L + i); st = attn.sequence(ex)[-1]; e.append(attn.rel_error(ex, st["cached"][2]))
    print(f"{L:3d} {sum(e)/len(e)*100:10.2f}% {max(e)*100:7.2f}%")
