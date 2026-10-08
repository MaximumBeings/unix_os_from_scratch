#!/usr/bin/env python3
"""Chapter 12: how good a hardware test is the capstone workload? Run the 49 hardware mutants of Chapter 7 against the 24 decode steps of the tiny transformer (programs from the compiler, replayed on the RTL) instead of
against the 78 purpose-built test programs. 'caught' = the testbench's comparison of memory and cycle counters fails."""
import os, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_progs as G, tiny_lm as T, mut_ch07 as M7
def workload(kind):
    m = T.make_model(kind, seed=2); chip = T.Chip(m, 24); toks, rec = chip.generate(0, 24); words = [len(rec)]
    for r in rec: words += G.record(r["code"], r["ext"])
    open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' is EXPECTED: it shows the workload notices the mistake.")
caught = {}
for kind in ("structured", "random"):
    workload(kind); c, n, lines = hw.mutate(M7.MUT, M7.F, "ga2_tb", M7.TB, workers=6, defines=("EXTW=8192",))
    caught[kind] = {l.rsplit(": ", 1)[0] for l in lines if l.endswith(": caught")}
    print(f"\n{kind} model, 24 decode steps: {c} of {n} hardware mutants caught (the 78 purpose-built programs of Chapter 7 caught all {n})")
    print("  missed: " + "; ".join(sorted(l.rsplit(": ", 1)[0] for l in lines if l.endswith("NOT CAUGHT"))))
both = caught["structured"] | caught["random"]; print(f"\nby either workload: {len(both)} of {len(M7.MUT)}")
print("missed by both: " + ("; ".join(sorted(m[1] for m in M7.MUT if m[1] not in both)) or "none"))
