#!/usr/bin/env python3
"""Chapter 3: the requantizer through the flow: golden vectors, two simulators, Yosys cost. Usage: ch03_run.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
print("== 1. the golden model writes its vectors")
print(subprocess.run([sys.executable, "model/requant_gold.py", "out/requant_vectors.hex"], cwd=hw.ROOT, capture_output=True, text=True).stdout.strip())
F = ["rtl/requant.v", "tb/requant_tb.v"]
for name, fn in (("Icarus Verilog", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "requant_tb"); print(f"\n== 2. requant_tb in {name}")
    print(([l for l in out.strip().splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-3:])[0], f"(exit status {rc})")
print("\n== 3. Yosys: what the requantizer costs")
GATES = "abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; stat"
g = hw.synth_stats(["rtl/requant.v"], "requant", f"synth -flatten -top requant; {GATES}"); l = hw.synth_stats(["rtl/requant.v"], "requant", "synth_ice40 -flatten -top requant; stat")
print(f"requant   generic gates {g['total']:5d}\n          iCE40 cells   {l['total']:5d} {dict(sorted(l['cells'].items()))}")
