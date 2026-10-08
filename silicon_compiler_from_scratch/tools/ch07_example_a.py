#!/usr/bin/env python3
"""Chapter 7, running example A: follow a program through the sequencer. A six-instruction program is assembled by hand, run on the circuit (Icarus) with a trace of the sequencer's FETCH, ISSUE and DONE moments, and every instruction's start, end and busy time is compared with the cycle model and the reference simulator. Writes out/ch07_example_a.json for the figures. Usage: ch07_example_a.py"""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G
R = hw.ROOT
SRC = """ld   dst=0   src=0 len=12
mm   dst=100 A=0 B=6 M=2 K=3 N=2 lda=3 ldb=2 ldc=2
rq   dst=110 src=100 len=4 m=8388608 s=23 relu=1
amax dst=120 src=110 len=4
st   src=110 dst=24 len=4
st   src=120 dst=28 len=1
halt"""
prog = I.assemble(SRC); ext = [0] * I.EXT; ext[0:12] = [1, 2, 3, 4, 5, 6, 1, 0, 0, 1, 1, 1]
open(os.path.join(R, "out", "ga2_trace.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in [1] + G.record(prog, ext)) + "\n")
rc, out = hw.sim_icarus(["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_trace_tb.v"], "ga2_trace_tb"); assert rc == 0, out
ev = {"F": {}, "I": {}, "D": {}}; counters = None; memx = {}
for l in out.splitlines():
    p = l.split()
    if not p: continue
    if p[0] in ev: ev[p[0]][int(p[1])] = int(p[2])
    if p[0] == "COUNTERS": counters = list(map(int, p[1:]))
    if p[0] == "X": memx[int(p[1])] = int(p[2])
print("program (assembled by hand):"); print(SRC)
print("\nwhat the circuit's sequencer did (cycle numbers are the circuit's own counter at the start of each state):")
print(" pc  instruction                                       FETCH  ISSUE   DONE   busy(measured)  busy(model)  agree")
rows = []; t_model = 0; ok = True
for pc, ins in enumerate(prog[:-1]):
    f, i_, d = ev["F"][pc], ev["I"][pc], ev["D"][pc]; busy = d - i_; mw = I.wait_cycles(ins); good = busy == mw and f == t_model; ok &= good
    rows.append({"pc": pc, "text": I.disassemble(ins), "op": ins["op"], "fetch": f, "issue": i_, "done": d, "busy": busy, "model": mw}); t_model += 2 + mw
    print(f" {pc:2d}  {I.disassemble(ins):50s}{f:5d}  {i_:5d}  {d:5d}   {busy:10d}   {mw:10d}    {'yes' if good else 'NO'}")
print(f"\nHALT: fetched at cycle {ev['F'][len(prog)-1]}, issued at {ev['I'][len(prog)-1]}; the total counter reads {counters[0]}  (model: {I.cycle_counts(prog)[0]})")
print("counters [total, mm, dma, vec, n_inst]  circuit:", counters, "  model:", I.cycle_counts(prog)); ok &= counters == I.cycle_counts(prog)
m = I.Machine(ext).run(prog); want = {i: I.s32(v) for i, v in enumerate(m.ext) if i >= 16 and v}
print("\nexternal memory after the program (non-zero words at addresses >= 16): circuit", memx, " reference", want, " ->", "identical" if memx == want else "DIFFERENT"); ok &= memx == want
print("\nreading it: each instruction costs 1 (FETCH) + 1 (ISSUE) + its busy time. The matrix multiply (mm) is", rows[1]["busy"], "cycles for a 2x3 by 3x2 product, of which only K+M+N-2 =", 3 + 2 + 2 - 2, "are the array computing:")
print("the rest is staging the tiles in (M*K + K*N =", 2 * 3 + 3 * 2, ") and writing the result out (M*N =", 4, "), plus control. The load (ld) of 12 words costs", rows[0]["busy"], "= 12 + latency 8 + 1: the memory latency is paid once.")
json.dump({"rows": rows, "counters": counters, "total": counters[0]}, open(os.path.join(R, "out", "ch07_example_a.json"), "w")); sys.exit(0 if ok else 1)
