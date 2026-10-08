#!/usr/bin/env python3
"""Chapter 1, running example A: watch a carry ripple. Simulates the delayed 4-bit adder, draws the waveform as text, and checks the transient values against a hand calculation. Usage: ch01_example_a.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, vcd_ascii
R = hw.ROOT
r = subprocess.run(["iverilog", "-g2012", "-s", "ripple_tb", "-o", "/tmp/ripple.vvp", "rtl/ripple.v", "tb/ripple_tb.v"], cwd=R, capture_output=True, text=True); assert r.returncode == 0, r.stderr
subprocess.run(["vvp", "-n", "/tmp/ripple.vvp"], cwd=R, capture_output=True)
print("== the waveform of ripple_tb (time in ns; each column is 1 ns; '=' means 'unchanged'; # is 1, _ is 0)\n")
print(vcd_ascii.draw(os.path.join(R, "out", "ripple.vcd"), 0, 30, ["ripple_tb.a", "ripple_tb.b", "ripple_tb.cin", "ripple_tb.sum", "ripple_tb.cout", "ripple_tb.c"]))
sig = vcd_ascii.parse(os.path.join(R, "out", "ripple.vcd")); ev = sig["ripple_tb.sum"]["ev"]
print("\nevery change of `sum`:", [(t, int(v, 2)) for t, v in ev if set(v) <= set("01")])
print("\n== the same thing worked out by hand: 0111 + 0001, each full adder takes 1 ns (the carry out of a stage and its sum bit both appear 1 ns after its inputs settle)")
print("  t=10: the inputs change to a=0111, b=0001. Every stage computes at once with the carries it sees NOW (all still 0):")
print("        stage 0: 1+1+0 -> sum 0, carry 1;  stage 1: 1+0+0 -> sum 1;  stage 2: 1+0+0 -> sum 1;  stage 3: 0+0+0 -> sum 0")
print("  t=11: those results appear: sum = 0110 (6) -- a WRONG intermediate value. Carry c1 = 1 is now visible to stage 1.")
print("  t=12: stage 1 redoes its sum with carry-in 1: 1+0+1 -> sum 0, carry 1. sum = 0100 (4). c2 = 1 reaches stage 2.")
print("  t=13: stage 2: 1+0+1 -> sum 0, carry 1.  sum = 0000 (0). c3 = 1 reaches stage 3.")
print("  t=14: stage 3: 0+0+1 -> sum 1.  sum = 1000 (8), the correct answer, FOUR nanoseconds after the inputs changed.")
print("  For 4 ns the output held wrong values: 6, 4, 0, then 8. A circuit whose output is sampled before t=14 (a clock period shorter than the carry chain) would read a wrong one of them.")
print("  That is what 'timing' means; Chapter 10 measures it for the whole chip.")
