#!/usr/bin/env python3
"""Chapter 7: GA-2 through the flow: an assembled example, the reference simulator's answer, the program suites in both simulators, and what the pieces cost. Usage: ch07_run.py"""
import os, subprocess, sys, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
print("== 1. an example program, assembled")
SRC = """ld   dst=0   src=0 len=12            ; A (2x3) and B (3x2) from external memory
mm   dst=100 A=0 B=6 M=2 K=3 N=2 lda=3 ldb=2 ldc=2     ; C = A x B, int32
rq   dst=110 src=100 len=4 m=8388608 s=23 relu=1       ; requantize: 2^23 / 2^23 = 1.0, so int8 = clamp(C), relu
amax dst=120 src=110 len=4                  ; which of the four results is largest
st   src=100 dst=20 len=4                   ; C
st   src=110 dst=24 len=4                   ; int8 results
st   src=120 dst=28 len=1                   ; the index
halt"""
prog = I.assemble(SRC); print(SRC); print("\nmachine code (128 bits each):"); [print(f"  {I.encode(p):032x}  {I.disassemble(p)}") for p in prog]
ext = [0] * I.EXT; ext[0:12] = [1, 2, 3, 4, 5, 6, 1, 0, 0, 1, 1, 1]
m = I.Machine(ext).run(prog)
print("\nthe reference simulator says: C =", [I.s32(v) for v in m.ext[20:24]], " int8 =", [I.s32(v) for v in m.ext[24:28]], " argmax =", m.ext[28], " cycles [total, mm, dma, vec, n]:", I.cycle_counts(prog))
words = [1] + G.record(prog, ext); open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)): rc, out = fn(F, "ga2_tb"); print(f"  the circuit, {name:10s}{line(out)} (exit {rc})")
print("\n== 2. the program suite: 18 directed programs + 60 random ones (seed 1)")
print(subprocess.run([sys.executable, "model/ga2_progs.py", "out/ga2_programs.hex", "60", "1"], cwd=hw.ROOT, capture_output=True, text=True).stdout.strip())
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)): rc, out = fn(F, "ga2_tb"); print(f"  {name:10s}{line(out)} (exit {rc})")
print("\n== 3. a larger run: 200 fresh random programs (seed 7), Icarus")
print(subprocess.run([sys.executable, "model/ga2_progs.py", "out/ga2_programs.hex", "200", "7"], cwd=hw.ROOT, capture_output=True, text=True).stdout.strip())
rc, out = hw.sim_icarus(F, "ga2_tb"); print(f"  {line(out)} (exit {rc})")
print("\n== 4. a different memory latency (LAT=20 instead of 8): the cycle model changes by one constant (LD costs len + LAT + 1)")
I.LAT = 20; subprocess.run([sys.executable, "-c", "import sys; sys.path.insert(0,'model'); import ga2_isa as I; I.LAT=20; import ga2_progs as G; print(G.write('out/ga2_programs.hex', 30, 3))"], cwd=hw.ROOT, capture_output=True)
rc, out = hw.sim_icarus(F, "ga2_tb", defines=("LAT=20",)); print(f"  {line(out)} (exit {rc})")
print("\n== 5. Yosys: what the pieces cost")
GATES = "abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; stat"
RT = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v"]
print(f"{'unit':10s} {'generic gates':>14s} {'iCE40 cells':>12s} {'flip-flops':>11s} {'block RAMs':>11s}")
for top in ("ga2_st", "ga2_rq", "ga2_vadd", "ga2_amax", "ga2_sm", "ga2_mm", "ga2"):
    g = hw.synth_stats(RT, top, f"synth -flatten -top {top}; {GATES}") if top != "ga2" else None
    l = hw.synth_stats(RT, top, f"synth_ice40 -flatten -top {top}; stat")
    ff = sum(v for k, v in l["cells"].items() if k.startswith("SB_DFF")); ram = l["cells"].get("SB_RAM40_4K", 0)
    print(f"{top:10s} {(str(g['total']) if g else 'n/a (scratchpad)'):>14s} {l['total']:12d} {ff:11d} {ram:11d}")
