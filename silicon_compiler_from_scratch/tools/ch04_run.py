#!/usr/bin/env python3
"""Chapter 4: the systolic array through the flow: a printed wavefront, golden vectors for several array sizes, both simulators, and Yosys cost against N. Usage: ch04_run.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
def run(*a): return subprocess.run([sys.executable] + list(a), cwd=hw.ROOT, capture_output=True, text=True).stdout.strip()
print("== 1. the wavefront for a 3 x 3 array multiplying (3 x 4) by (4 x 3)"); print(run("model/systolic_model.py", "--wave", "3", "4"))
CONFIGS = [(2, 2), (3, 5), (4, 7), (4, 1), (8, 8), (8, 33)]
F = ["rtl/systolic.v", "tb/systolic_tb.v"]
print("\n== 2. golden vectors and both simulators, for several sizes (N = array side, K = inner dimension)")
for N, K in CONFIGS:
    print(run("model/systolic_model.py", "out/systolic_vectors.hex", str(N), str(K), "100"))
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F, "systolic_tb", defines=(f"NN={N}", f"KK={K}"))
        print(f"  {name:10s}", ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0], f"(exit {rc})")
print("\n== 3. Yosys: cost against array size")
GATES = "abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; stat"
print(f"{'N':>3s} {'PEs':>4s} {'generic gates':>14s} {'gates/PE':>9s} {'iCE40 cells':>12s} {'flip-flops':>11s}")
for N in (1, 2, 4, 8):
    top = f"systolic_n{N}"; wrap = f"module {top}(input clk, input clr, input [{8*N-1}:0] a_edge, input [{8*N-1}:0] b_edge, output [{32*N*N-1}:0] c); systolic #(.N({N})) u(.*); endmodule\n"
    open(os.path.join(hw.ROOT, "out", top + ".v"), "w").write(wrap)
    fs = ["rtl/systolic.v", f"out/{top}.v"]
    g = hw.synth_stats(fs, top, f"synth -flatten -top {top}; {GATES}"); l = hw.synth_stats(fs, top, f"synth_ice40 -flatten -top {top}; stat")
    ff = sum(v for k, v in l["cells"].items() if k.startswith("SB_DFF"))
    print(f"{N:3d} {N*N:4d} {g['total']:14d} {g['total']/(N*N):9.1f} {l['total']:12d} {ff:11d}")
