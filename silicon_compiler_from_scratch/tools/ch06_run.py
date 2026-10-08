#!/usr/bin/env python3
"""Chapter 6: the exp table, the divider and the softmax unit through the flow: golden files, both simulators, the cycle formula, Yosys cost. Usage: ch06_run.py"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
def gold(*a): return subprocess.run([sys.executable, "model/softmax_gold.py"] + list(a), cwd=hw.ROOT, capture_output=True, text=True).stdout.strip()
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
SIMS = (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator))
print("== 1. the exp table and the divider"); print(gold("table", "out/exp_table.hex")); print(gold("div", "out/divu_vectors.hex", "20000"))
for top, files in (("exp_lut_tb", ["rtl/exp_lut.v", "tb/exp_lut_tb.v"]), ("divu_tb", ["rtl/divu.v", "tb/divu_tb.v"])):
    for name, fn in SIMS: rc, out = fn(files, top); print(f"  {top:11s}{name:10s}{line(out)} (exit {rc})")
print("\n== 2. the softmax unit, for several vector lengths N")
cycles = {}
for N in (1, 2, 5, 8, 16, 64):
    print(gold("soft", "out/softmax_vectors.hex", str(N), "600"))
    for name, fn in SIMS:
        rc, out = fn(["rtl/exp_lut.v", "rtl/divu.v", "rtl/softmax.v", "tb/softmax_tb.v"], "softmax_tb", defines=(f"NN={N}",)); print(f"  {name:10s}{line(out)} (exit {rc})")
        m = re.search(r"took (\d+) cycles", out)
        if m: cycles[N] = int(m.group(1))
print("\n== 3. the cycle count: measured against the formula 3N + 43 (3 passes over N elements + the 40-cycle division + 3 cycles of control)")
for N, c in cycles.items(): print(f"N={N:3d}  measured {c:4d}  formula {3*N+43:4d}  {'ok' if c == 3*N+43 else 'DIFFERENT'}")
print("\n== 4. Yosys: what the pieces cost")
GATES = "abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; stat"
for top, files in (("exp_lut", ["rtl/exp_lut.v"]), ("divu", ["rtl/divu.v"]), ("softmax", ["rtl/exp_lut.v", "rtl/divu.v", "rtl/softmax.v"])):
    g = hw.synth_stats(files, top, f"synth -flatten -top {top}; {GATES}"); l = hw.synth_stats(files, top, f"synth_ice40 -flatten -top {top}; stat")
    ff = sum(v for k, v in l["cells"].items() if k.startswith("SB_DFF")); print(f"{top:8s} generic gates {g['total']:6d}   iCE40 cells {l['total']:5d} (flip-flops {ff})")
