#!/usr/bin/env python3
"""Chapter 14, example B: what converting multi-head attention to grouped-query attention costs in accuracy, and what it buys in cache and serving capacity. Usage: ch14_example_b.py. Writes out/ch14_example_b.json."""
import json, math, os, random, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import gqa_lm as Q, tiny_lm as T
STEPS = 12
def agree_vs_mha(m_mha, m_other, seeds_starts=range(16)):
    """Teacher-forced on the MHA float model's own tokens: how often does `m_other` (float) pick the same next token, and the relative logit error."""
    ag = tot = 0; errs = []
    for s in seeds_starts:
        ft = Q.float_generate(m_mha, s, STEPS); k1, v1, k2, v2 = [], [], [], []
        for L in range(1, STEPS + 1):
            l1, a, b = Q.float_step(m_mha, ft[L - 1], k1, v1); l2, c, d = Q.float_step(m_other, ft[L - 1], k2, v2); k1.append(a); v1.append(b); k2.append(c); v2.append(d)
            ag += l1.index(max(l1)) == l2.index(max(l2)); tot += 1; errs.append(math.sqrt(sum((x - y) ** 2 for x, y in zip(l1, l2)) / sum(x * x for x in l1)))
    return ag, tot, sum(errs) / len(errs)
print("== 1. converting a model that was NOT trained for grouping: mean-pool the K and V heads of each group (floating point against the original MHA; 3 random models x 16 starts x 12 steps)")
res = {"pool": {}}
print(f"{'G':>3s} {'agreement':>16s} {'mean logit error':>17s}")
for G in (4, 2, 1):
    ag = tot = 0; er = []
    for seed in (1, 2, 3):
        m = Q.make_mha("random", seed); a, t, e = agree_vs_mha(m, Q.to_groups(m, G) if G != 4 else m); ag += a; tot += t; er.append(e)
    res["pool"][G] = [ag, tot, sum(er) / 3]; print(f"{G:3d} {ag:6d}/{tot} ({100*ag/tot:5.1f}%) {100*sum(er)/3:16.1f}%")
print("\n== 2. the same conversion when the heads of a group were already similar (a model 'trained for sharing'): each head's K/V weights = a shared group weight + noise of relative size eps")
print(f"{'eps':>6s} {'G=2 agreement':>14s} {'G=2 logit error':>16s} {'G=1 agreement':>14s} {'G=1 logit error':>16s}")
res["eps"] = {}
for eps in (1.0, 0.5, 0.25, 0.1, 0.0):
    row = []
    for G in (2, 1):
        ag = tot = 0; er = []
        for seed in (1, 2, 3):
            m = Q.make_mha("random", seed); R = random.Random(seed * 31); r = 4 // G
            for nm in ("Wk", "Wv"):
                W = m[nm]; base = [[R.gauss(0, 1 / math.sqrt(16)) for _ in range(4)] for _ in range(16)]; base_g = {}
                for h in range(4):
                    g = h // r
                    if g not in base_g: base_g[g] = [[R.gauss(0, 1 / math.sqrt(16)) for _ in range(4)] for _ in range(16)]
                    for i in range(16):
                        for c in range(4): W[i][h * 4 + c] = (1 - eps) * base_g[g][i][c] + eps * W[i][h * 4 + c]
            a, t, e = agree_vs_mha(m, Q.to_groups(m, G)); ag += a; tot += t; er.append(e)
        row += [100 * ag / tot, 100 * sum(er) / 3]
    res["eps"][eps] = row; print(f"{eps:6.2f} {row[0]:13.1f}% {row[1]:15.1f}% {row[2]:13.1f}% {row[3]:15.1f}%")
print("  (eps = 1 is the untouched random model of part 1 with G = 2 and 1; eps = 0 makes the heads of each group identical, so pooling changes nothing and the conversion is exact)")
print("\n== 3. what sharing buys in serving (DERIVED arithmetic for an ASSUMED chip: 80 GB, 2 TB/s; 7B-class model: 32 layers, 32 query heads, head width 128, int8 weights and cache; context 8192)")
layers, heads, hd, wb, cap, bw, ctx = 32, 32, 128, 7e9, 80e9, 2e12, 8192
print(f"{'variant':16s} {'KV heads':>9s} {'KV KiB/token':>13s} {'KV GiB/request':>15s} {'max batch':>10s} {'tokens/s at B=1':>16s} {'tokens/s at max batch':>22s}")
res["serve"] = {}
for name, kvh in (("MHA", 32), ("GQA (8 groups)", 8), ("GQA (4 groups)", 4), ("MQA", 1)):
    kv = 2 * layers * kvh * hd; req = kv * ctx; maxb = int((cap - wb) // req); tps = lambda b: b * bw / (wb + b * req)
    res["serve"][name] = [kv, req, maxb, tps(1), tps(maxb)]; print(f"{name:16s} {kvh:9d} {kv/1024:13.0f} {req/2**30:15.2f} {maxb:10d} {tps(1):16.0f} {tps(maxb):22.0f}")
print("  At B = 1 the weights dominate and sharing changes little; at the largest batch the cache dominates and MQA/GQA move the limit from capacity-bound to bandwidth-bound much later.")
json.dump({str(k): v for k, v in res.items()}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch14_example_b.json"), "w"))
