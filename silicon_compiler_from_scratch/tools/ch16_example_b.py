#!/usr/bin/env python3
"""Chapter 16, example B: when does speculation pay? (1) measured sweep over draft quality and k (cycles from the reference model, which the RTL reproduces exactly); (2) a simple model of the speedup compared with the measurement; (3) derived serving arithmetic for a 7B target and a small draft. Usage: ch16_example_b.py. Writes out/ch16_example_b.json"""
import json, os, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import ga2_isa as I, tiny_lm as T, spec_lm as S
N = 32; m = T.make_model("structured"); res = {}
chip = T.Chip(m, N); tb, rec = chip.generate(0, N); base = sum(I.cycle_counts(r["code"])[0] for r in rec)
print(f"== 1. measured: 32 tokens of the structured model; plain decoding = {base:,} cycles. Cells: acceptance rate (accepted guesses / guesses) and speedup over plain decoding. Draft quality is the noise added to the draft's logits (0 = perfect).")
ks = (1, 2, 3, 4, 5); noises = (0, 3, 4, 5, 6)
print(f"{'noise':>6s} " + " ".join(f"{'k=' + str(k):>14s}" for k in ks)); res["grid"] = {}
for nz in noises:
    row = []
    for k in ks:
        Wd = S.make_draft(m, nz, 1); sc = S.SpecChip(m, Wd, N, k); toks, st, log = sc.generate(0, N); cyc = S.cycles_of(log); assert toks[:N + 1] == tb[:N + 1]
        alpha = st["accepted"] / st["proposed"]; row.append((alpha, base / cyc, st["blocks"], cyc, len(log))); res["grid"][f"{nz}/{k}"] = row[-1]
    print(f"{nz:6.0f} " + " ".join(f"{100*a:4.0f}% {s:5.2f}x    " for a, s, *_ in row))
print("   every run produced exactly the plain decoder's 32 tokens")
print("\n== 2. a model of the speedup: cost per block = k * t_draft + t_verify(k+1); tokens per block = 1 + accepted. Measured unit costs (cycles, from the programs):")
Wd = S.make_draft(m, 0, 1); sc = S.SpecChip(m, Wd, N, 3); sc.generate(0, N); td = I.cycle_counts(sc.dprog.code)[0]
print(f"   one draft step: {td} cycles;  one plain target step: {base // N} cycles on average ({T.__name__} programs); verifier of m rows against the cache sizes seen:")
tv = {}
for (c, mm), P in sorted(sc.vprogs.items()):
    if c in (4, 12, 24): tv[(c, mm)] = I.cycle_counts(P.code)[0]
for (c, mm), v in tv.items(): print(f"      cache {c:2d}, m = {mm}: {v} cycles")
print("   the geometric model: with an independent acceptance chance a per guess, the expected tokens per block is (1 - a^(k+1)) / (1 - a); speedup = tokens per block * t_plain / (k * t_draft + t_verify). Prediction against measurement (cache around 12 for t_plain and t_verify):")
tp = I.cycle_counts(chip.program(13).code)[0]; res["model"] = []
print(f"{'noise':>6s} {'k':>3s} {'alpha':>6s} {'predicted':>10s} {'measured':>9s}")
for nz in (0, 3, 4):
    for k in (1, 3, 5):
        a, s_meas, *_ = res["grid"][f"{nz}/{k}"]; E = (k + 1) if a >= 0.999 else (1 - a ** (k + 1)) / (1 - a); P = sc._vprog(12, k + 1) if (12, k + 1) in sc.vprogs else None
        if P is None: continue
        tvv = I.cycle_counts(P.code)[0]; pred = E * tp / (k * td + tvv); res["model"].append([nz, k, a, pred, s_meas]); print(f"{nz:6d} {k:3d} {a:6.2f} {pred:9.2f}x {s_meas:8.2f}x")
print("   the model is good when acceptance is high (1.92 predicted, 1.93 measured) and rough when it is not: at noise 4 it under-predicts (0.78 against 1.01) because acceptance is not independent from guess to guess: some positions are easy for the draft and some are hard, and speculation does better on the easy ones than the average suggests. It also ignores the shorter first blocks and the overshoot at the end of the run")
print("\n== 3. serving arithmetic (DERIVED, ASSUMED): 7B target at 7 GB (int8), draft with c = t_draft / t_target of its weight traffic; at batch 1 both are bandwidth-bound, so t is proportional to weight bytes; the verifier reads the target's weights once for k+1 rows (the extra rows' cache reads and compute are neglected)")
print(f"{'accept a':>9s} {'k':>3s} " + " ".join(f"{'c=' + str(c):>10s}" for c in (0.01, 0.05, 0.15))); res["serve"] = {}
for a in (0.6, 0.8, 0.9):
    for k in (2, 4, 8):
        E = (1 - a ** (k + 1)) / (1 - a); cells = []
        for c in (0.01, 0.05, 0.15): cells.append(E / (1 + k * c)); res["serve"][f"{a}/{k}/{c}"] = E / (1 + k * c)
        print(f"{a:9.1f} {k:3d} " + " ".join(f"{x:9.2f}x" for x in cells))
print("   speedup = E[tokens per block] / (1 + k c). For a given acceptance there is a best k: more guesses give more tokens per block but cost k draft steps and lose to a wrong guess (a rejected guess wastes everything after it)")
json.dump({str(k): v for k, v in res.items()}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch16_example_b.json"), "w"))
