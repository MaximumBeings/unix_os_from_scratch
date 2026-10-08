#!/usr/bin/env python3
"""Chapter 8, running example A: attention on four tokens, by hand. Vectors of width 2 so every number can be checked on paper. (1) The decode step in real numbers: scores, softmax, weighted average of the values. (2) The same step in the chip's integers (Q4.4 scores, Chapter 6's integer softmax, int8 weights). (3) The same four tokens as a PREFILL, with the causal mask: the last row of the weight matrix is exactly the decode step. Writes out/ch08_example_a.json. Usage: ch08_example_a.py"""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); from softmax_gold import softmax_fixed, softmax_float
K = [[1.0, 0.0], [0.0, 1.0], [1.0, 1.0], [-1.0, 0.0]]; V = [[1.0, 0.0], [0.0, 2.0], [2.0, 2.0], [-1.0, 1.0]]
Q = [[1.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.5]]; d = 2; L = 4
print("keys   K =", K); print("values V =", V); print("the newest token's query q =", Q[-1], "   (width d = 2, so scores are divided by sqrt(2) = %.4f)" % math.sqrt(2))
q = Q[-1]
sc = [sum(q[i] * K[p][i] for i in range(d)) / math.sqrt(d) for p in range(L)]
print("\n== 1. real numbers")
print("scores   = K q / sqrt(d) =", [round(x, 4) for x in sc], "   (position 2 is most similar to q: K[2] = [1,1])")
mx = max(sc); e = [math.exp(x - mx) for x in sc]; z = sum(e); w = [x / z for x in e]
print("weights  = softmax(scores) =", [round(x, 4), ] if False else [round(x, 4) for x in w], "  sum =", round(sum(w), 6))
out = [sum(w[p] * V[p][c] for p in range(L)) for c in range(d)]
print("output   = weights V =", [round(x, 4) for x in out], "  <- a weighted average of the four value vectors")
print("check one component by hand: out[0] = %.4f*1 + %.4f*0 + %.4f*2 + %.4f*(-1) = %.4f" % (w[0], w[1], w[2], w[3], out[0]))
print("\n== 2. the chip's integers: score x 16 as an int8 (Q4.4), integer softmax, weights as int8 in 0..127")
s8 = [max(-128, min(127, round(x * 16))) for x in sc]; p16 = softmax_fixed(s8); w8 = [round(v * 127 / 65536) for v in p16]
print("scores x 16, rounded      =", s8)
print("integer softmax (Q0.16)   =", p16, "  = ", [round(v / 65536, 4) for v in p16], " sum =", sum(p16))
print("weights as int8 (x127)    =", w8, "  sum =", sum(w8))
print("error of the weights: ", [round(abs(a / 65536 - b), 5) for a, b in zip(p16, w)])
oi = [sum(w8[p] / 127 * V[p][c] for p in range(L)) for c in range(d)]
print("output from int8 weights  =", [round(x, 4) for x in oi], "  vs real", [round(x, 4) for x in out])
print("\n== 3. the same four tokens as a PREFILL: every token attends at once, with the causal mask (a token may not see later ones)")
W = []
for t in range(L):
    row = [sum(Q[t][i] * K[p][i] for i in range(d)) / math.sqrt(d) if p <= t else -1e9 for p in range(L)]
    mxr = max(row); ee = [math.exp(x - mxr) if x > -1e8 else 0.0 for x in row]; zz = sum(ee); W.append([x / zz for x in ee])
for t in range(L): print(f"  token {t} attends with weights", [round(x, 3) for x in W[t]])
print("the last row equals the decode weights of part 1:", all(abs(a - b) < 1e-12 for a, b in zip(W[-1], w)))
print("so decoding the 4th token needs only the last row; with a KV cache, the other three rows are never recomputed.")
json.dump({"scores": sc, "w": w, "p16": p16, "w8": w8, "out": out, "prefill": W, "s8": s8}, open(os.path.join(hw.ROOT, "out", "ch08_example_a.json"), "w"))
