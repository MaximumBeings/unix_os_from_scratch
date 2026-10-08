#!/usr/bin/env python3
"""Chapter 13, example B: how much accuracy does int4 cost, how much bandwidth does it save, and when does it pay?  Usage: ch13_example_b.py"""
import math, os, random, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_progs as G, capra as C, tiny_lm as T
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
# ------------------------------------------------------------------ 1. weight-quantization error, by scale granularity (a study of dequantized weights; the chip implements per-tensor only)
print("== 1. error of a quantized weight matrix, by scale granularity (K=64 inputs, N=32 outputs, Gaussian weights, 200 random inputs)")
def quant(W, bits, groups):
    """groups: list of lists of (row, col) cells sharing one scale. Returns the dequantized matrix."""
    qm = 2 ** (bits - 1) - 1; D = [row[:] for row in W]
    for g in groups:
        mx = max(abs(W[r][c]) for r, c in g) or 1.0; s = mx / qm
        for r, c in g: D[r][c] = max(-qm, min(qm, round(W[r][c] / s))) * s
    return D
def schemes(K, N):
    allc = [(r, c) for r in range(K) for c in range(N)]
    out = {"per-tensor": [allc], "per-column": [[(r, c) for r in range(K)] for c in range(N)]}
    for g in (64, 32, 16, 8): out[f"per-group {g}"] = [[(r, c) for r in range(b, b + g)] for c in range(N) for b in range(0, K, g)]
    return out
def rel_err(W, D, X):
    num = den = 0.0
    for x in X:
        for c in range(len(W[0])):
            a = sum(x[r] * W[r][c] for r in range(len(W))); b = sum(x[r] * D[r][c] for r in range(len(W))); num += (a - b) ** 2; den += a * a
    return math.sqrt(num / den)
K, N = 64, 32; R = random.Random(7); X = [[R.gauss(0, 1) for _ in range(K)] for _ in range(200)]
W = [[R.gauss(0, 1) for _ in range(N)] for _ in range(K)]
print(f"{'scheme':14s} {'scales':>7s} {'int8 error':>11s} {'int4 error':>11s}")
for nm, gr in schemes(K, N).items():
    print(f"{nm:14s} {len(gr):7d} {100*rel_err(W, quant(W, 8, gr), X):10.2f}% {100*rel_err(W, quant(W, 4, gr), X):10.2f}%")
print("\nthe same, when one column of W is 8 times larger than the rest (an 'outlier column')")
Wo = [row[:] for row in W]
for r in range(K): Wo[r][5] *= 8
print(f"{'scheme':14s} {'int8 error':>11s} {'int4 error':>11s}")
for nm, gr in schemes(K, N).items():
    print(f"{nm:14s} {100*rel_err(Wo, quant(Wo, 8, gr), X):10.2f}% {100*rel_err(Wo, quant(Wo, 4, gr), X):10.2f}%")
# ------------------------------------------------------------------ 2. the tiny language model, int8 against int4
print("\n== 2. the tiny language model of Chapter 12 with int4 weights (24 decode steps, start token 0)")
N_ = 24; m = T.make_model("structured"); exp = [0]
for _ in range(N_): exp.append(T.f_next(exp[-1]))
res = {}
for label, wb in (("int8 (all weights)", 8), ("int4 (all weights)", 4), ("int4 except Wout", {n: 4 for n in ("Wq", "Wk", "Wv", "Wo", "W1", "W2")})):
    chip = T.Chip(m, N_, wbits=wb); toks, rec = chip.generate(0, N_)
    wordsx = chip.program(1).ext_words; words24 = chip.program(N_).ext_words; res[label] = (toks, rec, chip)
    print(f"{label:20s} tokens correct: {sum(x == y for x, y in zip(toks, exp))}/{N_+1}   external words, step 1 / step 24: {wordsx} / {words24}")
# ------------------------------------------------------------------ 3. replay on the RTL
print("\n== 3. the decodes replayed on the RTL (Icarus and Verilator)")
tot = {}
for label in ("int8 (all weights)", "int4 (all weights)"):
    toks, rec, chip = res[label]; words = [len(rec)]
    for r in rec: words += G.record(r["code"], r["ext"])
    open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n"); cnt = {}
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {label:20s}{name:10s}{line(out)} (exit {rc})")
        cnt[name] = {int(mm.group(1)): [int(q) for q in mm.groups()[1:]] for mm in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out)}
    tot[label] = cnt["Icarus"]; print(f"  {label:20s}counters identical in both simulators: {cnt['Icarus'] == cnt['Verilator']}")
print(f"\n{'':20s} {'step 1':>8s} {'step 24':>8s} {'24 steps':>9s} {'per token':>10s}   (total / DMA / vector cycles at step 24)")
for label, c in tot.items():
    t = sum(c[i][0] for i in range(N_)); print(f"{label:20s} {c[0][0]:8d} {c[N_-1][0]:8d} {t:9d} {t/N_:10.0f}   {c[N_-1][0]} / {c[N_-1][2]} / {c[N_-1][3]}")
# ------------------------------------------------------------------ 4. a generic model, teacher-forced
print("\n== 4. a generic random model: agreement with floating point, int8 against int4 (3 models x 16 starts x 12 steps)")
for label, wb in (("int8", 8), ("int4", 4)):
    agree = tot = 0; errs = []
    for seed in (1, 2, 3):
        mg = T.make_model("random", seed=seed); ch = T.Chip(mg, 12, wbits=wb)
        for s in range(T.V):
            ft = T.float_generate(mg, s, 12); _, crec = ch.generate(s, 12, force=ft); kc, vc = [], []
            for L in range(1, 13):
                lg, k, v, _ = T.float_step(mg, ft[L - 1], kc, vc); kc.append(k); vc.append(v); agree += crec[L - 1]["tok"] == lg.index(max(lg)); tot += 1
                errs.append(math.sqrt(sum((a - b) ** 2 for a, b in zip(crec[L - 1]["logits"], lg)) / sum(b * b for b in lg)))
    print(f"{label}: agree {agree}/{tot} ({100*agree/tot:.1f}%), relative logit error mean {100*sum(errs)/len(errs):.1f}%, worst {100*max(errs):.1f}%")
# ------------------------------------------------------------------ 5. when does int4 pay? (derived)
print("\n== 5. break-even external bandwidth (derived from the cycle formulas, not measured)")
print("n weights. int8: n words over the link, n/b cycles (b = link bandwidth in words per cycle; one int8 per word in this model).")
print("int4: n/8 words over the link (n/(8b) cycles) plus n/8 UNPACK words of u cycles each (n*u/8 cycles). int4 is faster when  n/b > n/(8b) + n*u/8,  i.e.  b < 7/u.")
print(f"{'u (cycles per packed word)':28s} {'break-even b* = 7/u':>20s}")
for u, why in ((10, "as built"), (8, "one write per output, no overhead"), (4, "two write ports"), (1, "dequantize in the operand path")):
    print(f"{u:4d}  {why:23s} {7/u:20.2f} words/cycle")
print("A chip at b = 1 word/cycle (this model's link) therefore never gains from the UNPACK as built; the gain exists only where the link is slower than 0.7 words per cycle, or after u falls below 7.")
print("Chapter 9's roofline: at batch 1 the speed is set by weight bytes alone (66 / 124 / 219 tokens/s for fp16 / int8 / int4 on a 7B model at 1 TB/s) -- that holds when dequantization is free, the case u -> 0.")
