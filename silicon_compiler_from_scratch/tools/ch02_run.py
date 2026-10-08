#!/usr/bin/env python3
"""Chapter 2: the multipliers and the MAC unit through the whole flow: golden vectors, two simulators, and Yosys area for the two multiplier styles and the MAC. Usage: ch02_run.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
def gold(script, out): return subprocess.run([sys.executable, script, out], cwd=hw.ROOT, capture_output=True, text=True).stdout.strip()
print("== 1. the golden models write their vectors"); print(gold("model/mul8_gold.py", "out/mul8_vectors.hex")); print(gold("model/mac_gold.py", "out/mac_vectors.hex"))
F = ["rtl/mul8.v", "rtl/mac.v"]
for tb, top in (("tb/mul8_tb.v", "mul8_tb"), ("tb/mac_tb.v", "mac_tb")):
    for name, fn in (("Icarus Verilog", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F + [tb], top); print(f"\n== 2. {top} in {name}"); print([l for l in out.strip().splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH", "mul8", "wrap", "sat"))][0], f"(exit status {rc})")
print("\n== 3. Yosys: what each multiplier costs")
GATES = "abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; stat"
for top in ("mul8_beh", "mul8_sa"):
    g = hw.synth_stats(F, top, f"synth -flatten -top {top}; {GATES}"); l = hw.synth_stats(F, top, f"synth_ice40 -flatten -top {top}; stat")
    print(f"{top:9s} generic gates {g['total']:5d} {dict(sorted(g['cells'].items()))}\n{'':9s} iCE40 cells  {l['total']:5d} {dict(sorted(l['cells'].items()))}")
for top, params in (("mac", ""), ("mac_sat", "")):
    g = hw.synth_stats(F, top, f"synth -flatten -top {top}; {GATES}"); l = hw.synth_stats(F, top, f"synth_ice40 -flatten -top {top}; stat")
    print(f"{top:9s} generic gates {g['total']:5d} {dict(sorted(g['cells'].items()))}\n{'':9s} iCE40 cells  {l['total']:5d} {dict(sorted(l['cells'].items()))}")
