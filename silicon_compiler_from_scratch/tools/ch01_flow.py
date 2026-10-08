#!/usr/bin/env python3
"""Chapter 1: one circuit through the whole flow, printing what each tool says. Usage: ch01_flow.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
F = ["rtl/adder4.v"]; TB = ["tb/adder4_tb.v"]
print("== 1. the golden model writes the 512 expected answers"); print(subprocess.run([sys.executable, "model/adder4_gold.py", "out/adder4_vectors.hex"], cwd=hw.ROOT, capture_output=True, text=True).stdout.strip())
print("\n== 2. Icarus Verilog"); rc, out = hw.sim_icarus(F + TB, "adder4_tb"); print(out.strip(), f"\n(exit status {rc})")
print("\n== 3. Verilator, a second, independent simulator"); rc, out = hw.sim_verilator(F + TB, "adder4_tb"); print(out.strip(), f"\n(exit status {rc})")
print("\n== 4. Yosys, generic gates"); r = hw.synth_stats(F, "adder4", "synth -flatten -top adder4; abc -g AND,NAND,OR,NOR,XOR,XNOR; opt_clean; stat")
print("cells after mapping to simple gates:", dict(sorted(r["cells"].items())), "total", r["total"])
print("\n== 5. Yosys, for an FPGA (the Lattice iCE40 family's 4-input lookup tables)"); r = hw.synth_stats(F, "adder4", "synth_ice40 -flatten -top adder4; stat")
print("cells:", dict(sorted(r["cells"].items())), "total", r["total"])
