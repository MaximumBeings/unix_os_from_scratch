#!/usr/bin/env python3
"""Chapter 15: the software checks, then 32 decode steps of the tiny model with no window, a window of 8 and a window of 4, each replayed on the RTL in both simulators. Usage: ch15_run.py"""
import os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_progs as G_, tiny_lm as T, paged_lm as PL, paged_tests as PT
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
N = 32
print("== 1. software checks (page table vs a shadow model under 2,400 random operations with invariants after every one; exhaustion; exact page counts; paged host == plain host; window semantics)")
p = PT.check_all(); print("  problems:", p if p else "none")
print("\n== 2. the rule f(t) = 5t+3 mod 16 for 32 tokens, and the pages in use (page = 4 rows)")
m = T.make_model("structured"); exp = [0]
for _ in range(N): exp.append(T.f_next(exp[-1]))
res = {}
for W in (None, 8, 4):
    chip = PL.PagedChip(m, N, W, 4, 64); toks, rec = chip.generate(0, N); res[W] = (toks, rec)
    print(f"  window {str(W):>4s}: rule ok {toks == exp}; distinct programs compiled {len(chip.progs)}; pages in use: after step 1 {rec[1]['pages']}, 8 {rec[8]['pages']}, 16 {rec[16]['pages']}, 32 {rec[-1]['pages']}; peak {chip.peak}")
print("\n== 3. the decodes replayed on the RTL")
tot = {}
for W in (None, 8, 4):
    toks, rec = res[W]; words = [len(rec)]
    for r in rec: words += G_.record(r["code"], r["ext"])
    open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n"); cnt = {}
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  window {str(W):>4s} {name:10s}{line(out)} (exit {rc})")
        cnt[name] = {int(mm.group(1)): [int(q) for q in mm.groups()[1:]] for mm in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out)}
    assert cnt["Icarus"] == cnt["Verilator"]; tot[W] = cnt["Icarus"]
print(f"\n{'step':>5s} " + " ".join(f"{'cycles W='+str(W):>14s}" for W in (None, 8, 4)))
for i in (0, 3, 7, 8, 11, 15, 23, 31): print(f"{i+1:5d} " + " ".join(f"{tot[W][i][0]:14d}" for W in (None, 8, 4)))
print(f"{'total':>5s} " + " ".join(f"{sum(tot[W][i][0] for i in range(N)):14d}" for W in (None, 8, 4)))
