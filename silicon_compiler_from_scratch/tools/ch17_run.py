#!/usr/bin/env python3
"""Chapter 17: mixture of experts. (1) the software checks; (2) 24 decode steps of the structured model as plain (one expert's worth of feed-forward), routed (4 experts, one used per token) and dense-all (4 experts, all used); every program replayed on the RTL in both simulators. Usage: ch17_run.py"""
import os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_progs as G_, ga2_isa as I, tiny_lm as T, moe_lm as M, moe_tests as MT
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
N = 24
print("== 1. software checks: the router sends each token to its group's expert; any other expert breaks the rule for every token; the chip's route equals the float route; programs equal the interpreter; the dense (no routing) model agrees")
p = MT.check_all(); print("  problems:", p if p else "none")
def replay(label, progs):
    words = [len(progs)]
    for r in progs: words += G_.record(r["code"], r["ext"])
    open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n"); cnt = {}
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {label:28s}{name:10s}{line(out)} (exit {rc})")
        cnt[name] = [[int(q) for q in mm.groups()[1:]] for mm in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out)]
    assert cnt["Icarus"] == cnt["Verilator"]; return cnt["Icarus"]
print("\n== 2. 24 decode steps of the structured model")
m = M.make_moe("structured"); exp = [0]
for _ in range(N): exp.append(T.f_next(exp[-1]))
chip = M.MoEChip(m, N); toks, rec = chip.generate(0, N); print(f"  routed decode follows the rule: {toks == exp}; routes: {[r['route'] for r in rec]}")
dt, drec = M.DenseChip(chip).generate(0, N); print(f"  dense-all decode follows the rule: {dt == exp}")
cm = replay("routed (A then B_e)", [p for r in rec for p in r["progs"]]); cd = replay("dense-all (every expert)", [p for r in drec for p in r["progs"]])
tm = T.make_model("structured"); c12 = T.Chip(tm, N); t12, r12 = c12.generate(0, N); c1 = replay("Chapter 12 model (1 FFN)", [{"code": r["code"], "ext": r["ext"]} for r in r12])
tot = lambda c: sum(x[0] for x in c)
print(f"\n{'decoder':36s} {'feed-forward weights':>21s} {'programs':>9s} {'step 1':>8s} {'step 24':>8s} {'24 steps':>9s} {'vs 1 FFN':>9s}")
ffn1 = 2 * 16 * 32; print(f"{'one feed-forward block (Chapter 12)':36s} {ffn1:21d} {len(c1):9d} {c1[0][0]:8d} {c1[-1][0]:8d} {tot(c1):9d} {1.0:8.2f}x")
print(f"{'4 experts, routed (one used)':36s} {4*ffn1:21d} {len(cm):9d} {cm[0][0]+cm[1][0]:8d} {cm[-2][0]+cm[-1][0]:8d} {tot(cm):9d} {tot(cm)/tot(c1):8.2f}x")
print(f"{'4 experts, dense (all used)':36s} {4*ffn1:21d} {len(cd):9d} {cd[0][0]:8d} {cd[-1][0]:8d} {tot(cd):9d} {tot(cd)/tot(c1):8.2f}x")
a = [cm[2 * i] for i in range(N)]; b = [cm[2 * i + 1] for i in range(N)]
print(f"\n  routed step 24: program A {a[-1][0]} cycles (matrix {a[-1][1]}, DMA {a[-1][2]}, vector {a[-1][3]}); program B {b[-1][0]} cycles (matrix {b[-1][1]}, DMA {b[-1][2]}, vector {b[-1][3]})")
print(f"  the price of the second program: B's {b[-1][2]} DMA cycles include loading h again and the output matrix; the dispatch itself costs no chip cycles (the host picks the program)")
