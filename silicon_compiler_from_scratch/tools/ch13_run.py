#!/usr/bin/env python3
"""Chapter 13: the tests of int4 weight-only quantization. (1) UNPACK on the RTL: directed + 40 random programs against the Python reference, in both simulators. (2) The compiler's int4 battery: 60 random graphs with int4 weights, checks (a)-(d), error bound 0.72; the int8 battery for comparison (bound 0.16). Usage: ch13_run.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
print("== 1. UNPACK on the RTL: directed programs and 40 random ones (seed 1), compared with the Python reference")
subprocess.run([sys.executable, "model/unpack_progs.py", "out/ga2_programs.hex", "40", "1"], cwd=hw.ROOT, check=True)
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "ga2_tb"); print(f"  {name:10s}{line(out)} (exit {rc})")
import capra_tests as T
print("\n== 2. the compiler's battery: 60 random graphs, checks (a)-(d): compiled program == plain-Python meaning, cancellation-free buffers, close to floating point")
p8, w8 = T.check_many(range(60)); print(f"  int8 weights: worst relative error {w8:.4f} (bound 0.16); problems: {p8 if p8 else 'none'}")
p4, w4 = T.check_many(range(60), bound=0.72, wbits=4); print(f"  int4 weights: worst relative error {w4:.4f} (bound 0.72, about twice the worst seen); problems: {p4 if p4 else 'none'}")
