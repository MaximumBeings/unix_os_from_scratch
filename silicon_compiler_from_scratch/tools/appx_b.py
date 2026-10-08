#!/usr/bin/env python3
"""Appendix B (Verilog): compile and run the primer's demonstration modules in both simulators, and show what Yosys makes of a forgotten case. Usage: appx_b.py"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
F = ["rtl/appx_b.v", "tb/appx_b_tb.v"]
print("== 1. the testbench in two simulators (Icarus Verilog and Verilator)")
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "appx_b_tb"); print(f"-- {name} (exit {rc})"); [print("   " + l) for l in out.splitlines() if l.startswith(("gray", "after", "x =", "PASS", "FAIL", "MISMATCH", "COMPILE"))]
print("\n== 2. what a forgotten case turns into: Yosys on mux_latch and on the repaired mux_ok (generic cells after 'proc')")
for top in ("mux_latch", "mux_ok"):
    r = hw.synth_stats(["rtl/appx_b.v"], top, script=f"proc; opt; stat"); cells = {k: v for k, v in r["cells"].items()}
    print(f"  {top:10s}: {cells}")
    if "$dlatch" in r["log"] or "$_DLATCH" in r["log"]: print(f"  {'':10s}  -> contains a latch cell: the output remembers a value when sel == 3")
print("  the mux with a default has no latch; the one without has one. A latch in a clocked design is almost always a bug: the tool says so with a warning in the log:")
rc, out = hw._run(["yosys", "-p", "read_verilog -sv rtl/appx_b.v; proc; check"], cwd=hw.ROOT); print("  " + "\n  ".join(l.strip() for l in out.splitlines() if "latch" in l.lower() and "Latch inferred" in l)[:300])
