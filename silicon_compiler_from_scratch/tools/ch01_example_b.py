#!/usr/bin/env python3
"""Chapter 1, running example B: a first clocked circuit through the whole loop: golden model, two simulators, a waveform, Yosys, and nine mutants. Usage: ch01_example_b.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, vcd_ascii
R = hw.ROOT; F = ["rtl/counter4.v"]; TB = ["tb/counter4_tb.v"]
print("== 1. golden model and vectors"); print(subprocess.run([sys.executable, "model/counter4_gold.py", "out/counter4_vectors.hex", "4000"], cwd=R, capture_output=True, text=True).stdout.strip())
print("\n== 2. two simulators")
for name, fn in (("Icarus Verilog", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F + TB, "counter4_tb"); print(f"  {name:15s} {[l for l in out.splitlines() if l.startswith(('PASS', 'FAIL', 'MISMATCH'))][0]} (exit {rc})")
print("\n== 3. a short run as a waveform (clk rising edges at 5, 15, 25, ...; q changes just after an edge)")
subprocess.run(["iverilog", "-g2012", "-s", "counter4_wave_tb", "-o", "/tmp/c4.vvp", "rtl/counter4.v", "tb/counter4_wave_tb.v"], cwd=R, capture_output=True); subprocess.run(["vvp", "-n", "/tmp/c4.vvp"], cwd=R, capture_output=True)
SIG = ["counter4_wave_tb.clk", "counter4_wave_tb.rst", "counter4_wave_tb.en", "counter4_wave_tb.q", "counter4_wave_tb.wrap"]
print("time 0-120: reset for two edges, then counting:"); print(vcd_ascii.draw(os.path.join(R, "out", "counter4.vcd"), 0, 120, SIG))
print("\ntime 120-220: the wrap from 15 to 0 (the wrap flag is high during the cycle just BEFORE it), then a hold when enable goes low:"); print(vcd_ascii.draw(os.path.join(R, "out", "counter4.vcd"), 120, 220, SIG))
print("\ntime 215-300: still holding, counting again, then a reset while enabled (reset wins):"); print(vcd_ascii.draw(os.path.join(R, "out", "counter4.vcd"), 215, 300, SIG))
print("\n== 4. what Yosys makes of it")
g = hw.synth_stats(F, "counter4"); l = hw.synth_stats(F, "counter4", "synth_ice40 -flatten -top counter4; stat")
print("  generic gates:", g["total"], dict(sorted(g["cells"].items()))); print("  iCE40 cells:", l["total"], dict(sorted(l["cells"].items())))
print("\n== 5. testing the test: nine broken counters")
V = "rtl/counter4.v"
M = [(V, "reset is ignored", "if (rst) q <= 4'd0;\n        else if (en) q <= q + 4'd1;", "if (en) q <= q + 4'd1;"),
     (V, "enable is ignored (always counts)", "else if (en) q <= q + 4'd1;", "else q <= q + 4'd1;"),
     (V, "counts down", "q <= q + 4'd1;", "q <= q - 4'd1;"),
     (V, "saturates at 15 instead of wrapping", "else if (en) q <= q + 4'd1;", "else if (en && q != 4'd15) q <= q + 4'd1;"),
     (V, "reset loads 1", "if (rst) q <= 4'd0;", "if (rst) q <= 4'd1;"),
     (V, "counts by two", "q <= q + 4'd1;", "q <= q + 4'd2;"),
     (V, "enable beats reset", "if (rst) q <= 4'd0;\n        else if (en) q <= q + 4'd1;", "if (en) q <= q + 4'd1;\n        else if (rst) q <= 4'd0;"),
     (V, "the wrap flag ignores enable", "assign wrap = en && (q == 4'd15);", "assign wrap = (q == 4'd15);"),
     (V, "the reset is asynchronous", "always @(posedge clk) begin", "always @(posedge clk or posedge rst) begin")]
c, n, lines = hw.mutate(M, F, "counter4_tb", TB); print("\n".join("  " + x for x in lines)); print(f"\n  {c} of {n} broken counters caught")
