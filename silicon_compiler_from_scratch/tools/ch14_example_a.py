#!/usr/bin/env python3
"""Chapter 14, example A: one decode step by hand, and what the compiler emits for MHA, GQA and MQA. Usage: ch14_example_a.py. Writes out/ch14_example_a.json."""
import json, math, os, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import capra as C, gqa_lm as Q, tiny_lm as T
print("== 1. who reads whose cache?  H = 4 query heads, G key/value groups; head h uses group h // (H/G)")
for G in (4, 2, 1): print(f"  G={G} ({'MHA' if G == 4 else 'MQA' if G == 1 else 'GQA'}): " + ", ".join(f"head {h} -> group {h // (4 // G)}" for h in range(4)) + f"   cache per token: {Q.cache_words_per_token(G)} words")
print("\n== 2. one decode step with 3 tokens of context, GQA with G = 2, floating point (random model, seed 5, with Wq and Wk multiplied by 6 for this display only: the untouched model's attention is nearly uniform at three tokens)")
m0 = Q.make_mha("random", 5); m0["Wq"] = [[6 * a for a in r] for r in m0["Wq"]]; m0["Wk"] = [[6 * a for a in r] for r in m0["Wk"]]; m = Q.to_groups(m0, 2); kc, vc = [], []; toks = [3, 7, 1]
for t in toks[:-1]:
    lg, k, v = Q.float_step(m, t, kc, vc); kc.append(k); vc.append(v)
x = m["E"][toks[-1]]; q = T.mv(x, m["Wq"]); k = T.mv(x, m["Wk"]); v = T.mv(x, m["Wv"]); K = kc + [k]
print(f"  token {toks[-1]}: q has 16 numbers (4 heads x 4), k and v have 8 numbers (2 groups x 4)")
rows = []
for h in range(4):
    g = h // 2; qh = q[4 * h:4 * h + 4]; sc = [sum(qh[i] * Kp[4 * g + i] for i in range(4)) / 2 for Kp in K]; mx = max(sc); e = [math.exp(s - mx) for s in sc]; p = [a / sum(e) for a in e]
    rows.append({"head": h, "group": g, "scores": sc, "probs": p}); print(f"  head {h} reads group {g}: scores {[round(s, 3) for s in sc]} -> softmax {[round(a, 3) for a in p]}")
print("  heads 0 and 1 look at the SAME three keys with different queries and so attend differently; that is all grouped-query attention changes.")
print("\n== 3. what the compiler emits for one decode step at context 8 (instruction counts)")
res = {}
for G in (4, 2, 1):
    mm = Q.to_groups(Q.make_mha("structured"), G) if G != 4 else Q.make_mha("structured"); chip = Q.Chip(mm, 8); P = chip.program(8); h = {}
    for ins in P.code: h[ins["op"]] = h.get(ins["op"], 0) + 1
    ldw = sum(ins["c"] for ins in P.code if ins["op"] == "LD"); res[G] = {"hist": h, "ld_words": ldw, "high": P.spad_high}
    print(f"  G={G}: " + "  ".join(f"{k}={h[k]}" for k in sorted(h)) + f"   | words loaded from external memory: {ldw}; scratchpad high-water: {P.spad_high}")
print("  The query/score/context/output matmuls of the four heads do not depend on G; the K and V projections (and their cache loads, stores and requantizers) do: MM falls 56 -> 52 -> 50.")
json.dump({"heads": rows, "res": {str(g): r for g, r in res.items()}}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch14_example_a.json"), "w"))
