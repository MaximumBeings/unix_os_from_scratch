#!/usr/bin/env python3
"""Chapter 14 (grouped-query attention): the tests. (1) the float GQA step equals a plain MHA step whose heads in a group carry identical K/V weights; (2) for every decode step of every G the compiled program equals the plain-Python meaning of the graph (checks as in Chapter 11) and is close to float; (3) the decodes replayed on the RTL in both simulators, with cycle counts; (4) structured model: the rule f(t)=5t+3 mod 16 under MHA, GQA and MQA. Usage: ch14_run.py"""
import math, os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_progs as G_, capra as C, tiny_lm as T, gqa_lm as Q
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
N = 24; exp = [0]
for _ in range(N): exp.append(T.f_next(exp[-1]))
print("== 1. the grouped float step equals an ordinary MHA step with replicated K/V heads (independent form: 4 full heads, no grouping logic)")
worst = 0.0
for seed in (1, 2, 3):
    m = Q.make_mha("random", seed); mg = Q.to_groups(m, 2); rep = dict(m)                       # rebuild an MHA model whose heads 0,1 and 2,3 share K/V weights
    for nm in ("Wk", "Wv"):
        cols = []
        for h in range(4):
            g = h // 2
            for c in range(Q.DH): cols.append([mg[nm][i][g * Q.DH + c] for i in range(Q.D)])
        rep[nm] = [[cols[c][i] for c in range(Q.D)] for i in range(Q.D)]
    kc = vc = []; kg = vg = []; kc, vc, kg, vg = [], [], [], []; tok = 3
    for _ in range(6):
        l1, k1, v1 = Q.float_step(rep, tok, kc, vc); l2, k2, v2 = Q.float_step(mg, tok, kg, vg); worst = max(worst, max(abs(a - b) for a, b in zip(l1, l2)))
        kc.append(k1); vc.append(v1); kg.append(k2); vg.append(v2); tok = l1.index(max(l1))
print(f"  3 random models x 6 steps: largest logit difference {worst:.2e} (should be rounding only)")
print("\n== 2. compiled program == plain-Python meaning of the graph, for every step of every G (structured model); and distance from floating point")
res = {}
for G in (4, 2, 1):
    m0 = Q.make_mha("structured"); m = Q.to_groups(m0, G) if G != 4 else m0; chip = Q.Chip(m, N); toks, rec = chip.generate(0, N); res[G] = (toks, rec, chip)
    bad = 0; kc, vc = [], []; rel = []
    for L in range(1, N + 1):
        P = chip.program(L); inp = {"x": [m["E"][toks[L - 1]]]}
        if L > 1:
            for gi in range(G): inp[f"kc{gi}"] = [r[gi * Q.DH:(gi + 1) * Q.DH] for r in kc]; inp[f"vc{gi}"] = [r[gi * Q.DH:(gi + 1) * Q.DH] for r in vc]
        out = C.run(P, inp); ref = C.interpret(P, inp); bad += any(out[nm][0] != ref[e['node']] for nm, e in P.ext.items() if e['kind'] == 'output')
        lg, k, v = Q.float_step(m, toks[L - 1], kc, vc); ch = out["logits"][1][0]; rel.append(math.sqrt(sum((a - b) ** 2 for a, b in zip(ch, lg)) / sum(b * b for b in lg)))
        kc.append(k); vc.append(v)
    print(f"  G={G}: steps where compiled != interpreted: {bad}/{N}; relative logit error vs float: mean {100*sum(rel)/N:.1f}%, worst {100*max(rel):.1f}%; scratchpad high-water {max(r['high'] for r in rec)} words")
print("\n== 3. the rule f(t) = 5t+3 mod 16 under MHA, GQA, MQA (24 tokens from token 0)")
for G in (4, 2, 1): print(f"  G={G}: float {Q.float_generate(Q.to_groups(Q.make_mha('structured'), G) if G != 4 else Q.make_mha('structured'), 0, N) == exp}   chip {res[G][0] == exp}")
print("\n== 4. the decodes replayed on the RTL")
tot = {}
for G in (4, 2, 1):
    toks, rec, chip = res[G]; words = [len(rec)]
    for r in rec: words += G_.record(r["code"], r["ext"])
    open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n"); cnt = {}
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  G={G} {name:10s}{line(out)} (exit {rc})")
        cnt[name] = {int(mm.group(1)): [int(q) for q in mm.groups()[1:]] for mm in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out)}
    assert cnt["Icarus"] == cnt["Verilator"]; tot[G] = cnt["Icarus"]
print(f"\n{'G':>3s} {'kv words/token':>15s} {'instr step 24':>14s} {'cycles step 1':>14s} {'cycles step 24':>15s} {'24 steps':>9s}   (matrix / DMA / vector at step 24)")
for G in (4, 2, 1):
    c = tot[G]; print(f"{G:3d} {Q.cache_words_per_token(G):15d} {c[N-1][4]:14d} {c[0][0]:14d} {c[N-1][0]:15d} {sum(c[i][0] for i in range(N)):9d}   {c[N-1][1]} / {c[N-1][2]} / {c[N-1][3]}")
