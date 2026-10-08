#!/usr/bin/env python3
"""Chapter 10, running example B: pipelining the requantizer, and learning where to cut. Chapter 10's chip-level timing report names one unit, the requantizer, as the one that sets the clock. Here it is improved in two steps, each MEASURED in the toy library: (1) cut in two by a register after the big multiplier (rtl/requant_p2.v); (2) the same cut, plus a parallel-prefix adder in stage 2 in place of the ripple chain the synthesis tool builds for "+" (Example A's lesson). Both are proved correct on the same 201,808 golden vectors in both simulators, with a one-cycle latency. Writes out/ch10_example_b.json. Usage: ch10_example_b.py"""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, synth
R = hw.ROOT
subprocess.run([sys.executable, "model/requant_gold.py", "out/requant_vectors.hex"], cwd=R, capture_output=True)
print("== 1. correctness: both two-stage requantizers against the golden vectors used for the combinational one in Chapter 3 (one-cycle latency)")
for tb, files in (("requant_p2_tb", ["rtl/requant_p2.v", "tb/requant_p2_tb.v"]), ("requant_p2k_tb", ["rtl/demo_add.v", "rtl/requant_p2.v", "tb/requant_p2k_tb.v"])):
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(files, tb); print(f"  {tb:15s} {name:10s}", [l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))][0], f"(exit {rc})")
print("\n== 2. timing and area, measured in the toy library (areas in NAND2 equivalents; delays in ns; every critical path includes the library's 0.30 ns clock-to-q and 0.10 ns setup where registers are involved)")
res = {}; F = ["rtl/demo_add.v", "rtl/requant.v", "rtl/requant_p2.v"]
print(f"{'design':36s} {'cells':>6s} {'area (GE)':>10s} {'flip-flops':>11s} {'critical path':>14s} {'gate depth':>11s} {'max clock':>10s}")
for label, top in (("combinational requant (Ch. 3)", "requant"), ("  stage 1 alone: abs + multiply", "rq_stage1"), ("  stage 2 alone: round, shift, clamp", "rq_stage2"), ("  stage 2 with prefix adder", "rq_stage2k"), ("two stages, cut after multiply", "requant_p2"), ("two stages + prefix adder", "requant_p2k")):
    s = synth.synth(F, top, "b_" + top); t = synth.sta(os.path.join(R, "out", f"b_{top}_net.json"), top); ff = s["cells"].get("DFF", 0)
    res[top] = {"ns": t["critical_ns"], "area": s["area"], "ff": ff, "depth": t["depth"], "cells": s["ncells"]}
    print(f"{label:36s} {s['ncells']:6d} {s['area']:10.0f} {ff:11d} {t['critical_ns']:11.2f} ns {t['depth']:11d} {1000/t['critical_ns']:7.0f} MHz")
a, b, c, ck, d, dk = (res[k] for k in ("requant", "rq_stage1", "rq_stage2", "rq_stage2k", "requant_p2", "requant_p2k"))
print(f"\nstep 1, cut after the multiplier: {a['ns']:.2f} -> {d['ns']:.2f} ns, only {a['ns']/d['ns']:.2f}x. The cut is unbalanced: stage 1 (the multiplier) is {b['ns']:.2f} ns but stage 2 is {c['ns']:.2f} ns, 72 gates deep for what looks like 'an add, a shift and a clamp'.")
print(f"        Why: stage 2's 57-bit addition is a RIPPLE chain (Example A: Yosys maps '+' to one), and the carry passes through ~57 gates.")
print(f"step 2, replace that adder by a prefix adder: stage 2 falls from {c['ns']:.2f} to {ck['ns']:.2f} ns; the whole two-stage design from {d['ns']:.2f} to {dk['ns']:.2f} ns, {a['ns']/dk['ns']:.2f}x faster than the original")
print(f"        for {dk['ff']} flip-flops and {dk['area']-a['area']:+.0f} GE ({100*(dk['area']/a['area']-1):+.1f}% area) and one cycle of latency. Now stage 1 ({b['ns']:.2f} ns, the multiplier) is the limit: to go further the multiplier itself must be pipelined or restructured.")
print("\nNOTE: ABC maps every synthesis run separately, so a block's delay differs by a few percent between standalone and inside a larger design (stage 1 reads 8.23 ns alone and about 7.7 ns inside requant_p2); read differences of several percent as noise and the large steps as real.")
print("\n== 3. what this would do to the whole chip (an ESTIMATE from Chapter 10's table of unit timings, not a re-synthesis of the chip)")
print("the chip's critical path was 10.74 ns through the requantizer (scratchpad read -> requant -> scratchpad write). The next slowest units: the matrix unit at 5.94 ns, the softmax at 5.03 ns.")
print(f"With the requantizer at {dk['ns']:.2f} ns it is STILL the slowest block in the chip and sets the clock: about {10.74*dk['ns']/a['ns']:.2f} ns (scaling the chip's path by the unit's improvement), {10.74/(10.74*dk['ns']/a['ns']):.2f}x faster.")
print("Reaching the next unit (about 6 ns) would need a third stage inside the multiplier. Chapter 7's cycle model gives RQ n + 2 cycles; each added stage adds one cycle per RQ instruction.")
json.dump(res, open(os.path.join(R, "out", "ch10_example_b.json"), "w"))
