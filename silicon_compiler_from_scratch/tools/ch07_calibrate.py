#!/usr/bin/env python3
"""Chapter 7: read the cycle model's constants off the circuit. Each instruction type is run alone (followed by HALT) with several sizes; the busy time is the matching counter. The structural part of the formula
(the part that depends on the operands) is subtracted and what is left must be one constant per instruction type. Prints the table; the constants in model/ga2_isa.py (dict K) are these. Usage: ch07_calibrate.py"""
import os, re, sys, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G
B = I.build; H = B("HALT")
CASES = [
 ("LD", "len=1", B("LD", dst=0, src=0, len=1), "dma", lambda ins: 1 + I.LAT), ("LD", "len=10", B("LD", dst=0, src=0, len=10), "dma", lambda ins: 10 + I.LAT), ("LD", "len=50", B("LD", dst=0, src=0, len=50), "dma", lambda ins: 50 + I.LAT),
 ("ST", "len=1", B("ST", src=0, dst=1024, len=1), "dma", lambda ins: 1), ("ST", "len=10", B("ST", src=0, dst=1024, len=10), "dma", lambda ins: 10),
 ("RQ", "len=1", B("RQ", dst=100, src=0, len=1, m=1 << 23, s=24), "vec", lambda ins: 1), ("RQ", "len=10", B("RQ", dst=100, src=0, len=10, m=1 << 23, s=24), "vec", lambda ins: 10),
 ("SM", "len=1", B("SM", dst=100, src=0, len=1), "vec", lambda ins: 3 + 41), ("SM", "len=10", B("SM", dst=100, src=0, len=10), "vec", lambda ins: 30 + 41), ("SM", "len=30", B("SM", dst=100, src=0, len=30), "vec", lambda ins: 90 + 41),
 ("VADD", "len=1", B("VADD", dst=100, src1=0, src2=10, len=1), "vec", lambda ins: 2), ("VADD", "len=10", B("VADD", dst=100, src1=0, src2=10, len=10), "vec", lambda ins: 20),
 ("AMAX", "len=1", B("AMAX", dst=100, src=0, len=1), "vec", lambda ins: 1), ("AMAX", "len=10", B("AMAX", dst=100, src=0, len=10), "vec", lambda ins: 10),
 ("MM", "M1 K1 N1", B("MM", dst=100, A=0, B=10, M=1, K=1, N=1), "mm", lambda ins: 1 + 1 + 1 + 1), ("MM", "M1 K4 N4", B("MM", dst=100, A=0, B=10, M=1, K=4, N=4, ldb=4), "mm", lambda ins: 4 + 16 + 7 + 4),
 ("MM", "M4 K8 N4", B("MM", dst=100, A=0, B=64, M=4, K=8, N=4, lda=8, ldb=4, ldc=4), "mm", lambda ins: 32 + 32 + 14 + 16), ("MM", "M2 K5 N3 tb", B("MM", dst=100, A=0, B=64, M=2, K=5, N=3, tb=1, lda=5, ldb=5, ldc=3), "mm", lambda ins: 10 + 15 + 8 + 6),
]
R = random.Random(1); words = [len(CASES)]
for c in CASES: words += G.record([c[2], H], G.ext_image(R))
open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
rc, out = hw.sim_icarus(F, "ga2_tb", defines=("DUMP", "MAXPRINT=0"))
got = {int(m.group(1)): [int(x) for x in m.groups()[1:]] for m in (re.match(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", l) for l in out.splitlines()) if m}
print(f"{'op':5s} {'operands':12s} {'measured busy':>14s} {'operand-dependent part':>23s} {'left over':>10s}")
left = {}
for i, (op, desc, ins, unit, f) in enumerate(CASES):
    tot, mm, dma, vec, n = got[i]; busy = {"mm": mm, "dma": dma, "vec": vec}[unit]; rest = busy - f(ins); left.setdefault(op, set()).add(rest)
    print(f"{op:5s} {desc:12s} {busy:14d} {f(ins):23d} {rest:10d}")
print("\nconstant per instruction type (must be a single value each):", {op: sorted(v) for op, v in left.items()})
print("model/ga2_isa.py K =", I.K, "  consistent:", all(len(v) == 1 and list(v)[0] == I.K[op] for op, v in left.items()))
