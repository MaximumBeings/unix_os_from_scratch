#!/usr/bin/env python3
"""Chapter 12 (capstone): a tiny transformer decodes 24 tokens on GA-2, with every program produced by the compiler of Chapter 11. Usage: ch12_run.py"""
import math, os, random, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G, compiler as C, tiny_lm as T
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
N = 24; m = T.make_model("structured")
print("== 1. the same token sequence three ways (start token 0, 24 steps)")
exp = [0]
for _ in range(N): exp.append(T.f_next(exp[-1]))
fl = T.float_generate(m, 0, N); chip = T.Chip(m, N); toks, rec = chip.generate(0, N)
print("closed form f(t) = (5t+3) mod 16 :", exp); print("floating point                   :", fl); print("GA-2 (compiled programs)         :", toks)
print("all three identical:", exp == fl == toks)
print("\n== 2. is the attention/feed-forward path real? (floating point, along this generation)")
kc, vc, tok = [], [], 0; ro, rf, gaps, shift = [], [], [], []
m0 = dict(m); m0["Wo"] = [[0.0] * T.D for _ in range(T.D)]; m0["W2"] = [[0.0] * T.D for _ in range(T.HID)]
for _ in range(N):
    lg, k, v, aux = T.float_step(m, tok, kc, vc); lg0, _, _, _ = T.float_step(m0, tok, kc, vc); nx = lambda a: math.sqrt(sum(t * t for t in a))
    ro.append(nx(aux["o"]) / nx(aux["x"])); rf.append(nx(aux["f"]) / nx(aux["x"])); s = sorted(lg, reverse=True); gaps.append((s[0] - s[1]) / (s[0] - s[-1]))
    shift.append(nx([a - b for a, b in zip(lg, lg0)]) / nx(lg)); kc.append(k); vc.append(v); tok = lg.index(max(lg))
print(f"attention output / embedding (norm ratio): mean {sum(ro)/N:.2f};  feed-forward output / embedding: mean {sum(rf)/N:.2f}")
print(f"switching attention and feed-forward OFF changes the logits by {100*sum(shift)/N:.1f}% on average (but not the chosen tokens: they decide the logits' detail, the embeddings decide the token)")
print(f"gap between best and second-best logit, as a fraction of the logit range: min {min(gaps):.2f} mean {sum(gaps)/N:.2f}  (a chip with ~1% quantization noise needs this well above 0.01)")
print("\n== 3. the 24 decode steps replayed on the RTL (the programs the compiler produced, with the external memory the host prepared)")
words = [len(rec)]
for r in rec: words += G.record(r["code"], r["ext"])
open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
res = {}
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {name:10s}{line(out)} (exit {rc})")
    for mm in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out): res[int(mm.group(1))] = [int(q) for q in mm.groups()[1:]]
print(f"\n{'step':>4s} {'context':>8s} {'instr':>6s} {'cycles':>7s} {'matrix':>7s} {'DMA':>6s} {'vector':>7s}  token")
for i, r in enumerate(rec):
    tot, mm_, dma, vec, n = res[i]
    if i < 4 or i >= N - 2 or i % 6 == 5: print(f"{i+1:4d} {r['L']:8d} {n:6d} {tot:7d} {mm_:7d} {dma:6d} {vec:7d}  {toks[i]} -> {toks[i+1]}")
total = sum(res[i][0] for i in range(N))
print(f"\n24 tokens: {total} cycles in all, {total/N:.0f} per token on average (first step {res[0][0]}, last {res[N-1][0]}).")
print(f"At the 10.74 ns critical path of Chapter 10's toy-library timing (93 MHz) that would be {93e6/(total/N):.0f} tokens per second -- a toy-library, no-memory-timing figure, quoted only to show the order of magnitude.")
print("\n== 4. a generic model: random weights, random embeddings (so the answer is not designed). Teacher-forced: both systems see the same tokens; how often does the chip pick the token floating point picks?")
agree = tot_steps = 0; errs = []
for seed in (1, 2, 3):
    mg = T.make_model("random", seed=seed); ch = T.Chip(mg, 12)
    for s in range(T.V):
        ft = T.float_generate(mg, s, 12); ctoks, crec = ch.generate(s, 12, force=ft)
        kc, vc = [], []
        for L in range(1, 13):
            lg, k, v, _ = T.float_step(mg, ft[L - 1], kc, vc); kc.append(k); vc.append(v); agree += (crec[L - 1]["tok"] == lg.index(max(lg))); tot_steps += 1
            num = math.sqrt(sum((a - b) ** 2 for a, b in zip(crec[L - 1]["logits"], lg))); den = math.sqrt(sum(b * b for b in lg)); errs.append(num / den)
print(f"3 models x 16 start tokens x 12 steps = {tot_steps} decisions: the chip agrees with floating point on {agree} ({100*agree/tot_steps:.1f}%); relative logit error: mean {100*sum(errs)/len(errs):.1f}%, worst {100*max(errs):.1f}%")
