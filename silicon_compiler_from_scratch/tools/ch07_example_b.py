#!/usr/bin/env python3
"""Chapter 7, running example B: a small neural network written by hand in GA-2 assembly. Four inputs -> 4 hidden units (relu) -> 2 classes, for a batch of four samples. The weights are quantized with Chapter 3's rules, the program is written out with every address chosen by hand, run on the circuit and on the reference simulator, and the classes are compared with the floating-point network. Writes out/ch07_example_b.json. Usage: ch07_example_b.py"""
import json, math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G
from quant import quantize, scale_for, mantissa_shift
R = hw.ROOT
X = [[0.9, 0.7, 0.1, -0.2], [-0.5, 0.1, 0.8, 0.6], [0.3, -0.2, 0.4, 0.0], [0.6, 0.8, 0.5, 0.4]]                       # four samples, four features
W1 = [[1, -1, 1, 0], [1, -1, -1, 0], [-1, 1, 0, 1], [-1, 1, 0, -1]]                                                       # 4 x 4: s = x0+x1-x2-x3, -s, x0-x1, x2-x3
W2 = [[1, 0], [0, 1], [0.5, -0.5], [-0.5, 0.5]]                                                                          # 4 x 2: class scores
relu = lambda v: max(0.0, v); mm = lambda A, B: [[sum(A[i][k] * B[k][j] for k in range(len(B))) for j in range(len(B[0]))] for i in range(len(A))]
H = [[relu(v) for v in r] for r in mm(X, W1)]; Y = mm(H, W2); real_cls = [r.index(max(r)) for r in Y]
print("floating-point network: hidden =", [[round(v, 2) for v in r] for r in H]); print("                        scores =", [[round(v, 3) for v in r] for r in Y], " classes =", real_cls)
sx, sw1, sw2 = scale_for([v for r in X for v in r]), scale_for([v for r in W1 for v in r]), scale_for([v for r in W2 for v in r])
sh = scale_for([v for r in H for v in r]); sy = scale_for([v for r in Y for v in r])
q = lambda M, s: [[quantize(v, s) for v in r] for r in M]; Xq, W1q, W2q = q(X, sx), q(W1, sw1), q(W2, sw2)
m1, s1 = mantissa_shift(sx * sw1 / sh); m2, s2 = mantissa_shift(sh * sw2 / sy)
print(f"\nscales: x {sx:.5f}  W1 {sw1:.5f}  hidden {sh:.5f}  W2 {sw2:.5f}  scores {sy:.5f}")
print(f"layer 1 requantizer: M = sx*sw1/sh = {sx*sw1/sh:.6f} -> m={m1}, s={s1};   layer 2: M = {sh*sw2/sy:.6f} -> m={m2}, s={s2}")
# memory plan, chosen by hand. External memory: ext[0..15] X (4x4), ext[16..31] W1, ext[32..39] W2; results at ext[64..].  Scratchpad: X at 0, W1 at 16, W2 at 32, acc1 at 100, h at 120, acc2 at 140, scores at 150, argmax at 160..163
src = f"""ld   dst=0   src=0  len=40                 ; X, W1 and W2 in one load (ext[0..39] -> spad[0..39])
mm   dst=100 A=0  B=16 M=4 K=4 N=4 lda=4 ldb=4 ldc=4      ; layer 1: acc1 = X W1 (int32), 16 results
rq   dst=120 src=100 len=16 m={m1} s={s1} relu=1       ; hidden = relu(requant(acc1)), int8
mm   dst=140 A=120 B=32 M=4 K=4 N=2 lda=4 ldb=2 ldc=2     ; layer 2: acc2 = hidden W2 (4 x 2)
rq   dst=150 src=140 len=8  m={m2} s={s2} relu=0       ; scores, int8
amax dst=160 src=150 len=2                              ; class of sample 0 (one amax per row of two scores)
amax dst=161 src=152 len=2
amax dst=162 src=154 len=2
amax dst=163 src=156 len=2
st   src=160 dst=64  len=4                              ; the four classes
halt"""
prog = I.assemble(src); ext = [0] * I.EXT
for k, v in enumerate([v for r in Xq for v in r] + [v for r in W1q for v in r] + [v for r in W2q for v in r]): ext[k] = v & 0xFFFFFFFF
print("\nthe program (every address chosen by hand):"); print(src)
mach = I.Machine(ext).run(prog); chip_cls = [mach.ext[64 + i] for i in range(4)]
open(os.path.join(R, "out", "ga2_trace.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in [1] + G.record(prog, ext)) + "\n")
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v"]
rc, out = hw.sim_icarus(F + ["tb/ga2_trace_tb.v"], "ga2_trace_tb"); assert rc == 0, out
memx = {int(l.split()[1]): int(l.split()[2]) for l in out.splitlines() if l.startswith("X ")}; counters = list([l for l in out.splitlines() if l.startswith("COUNTERS")][0].split()[1:])
circ_cls = [memx.get(64 + i, 0) for i in range(4)]
print(f"\nclasses: floating point {real_cls}   reference simulator {chip_cls}   circuit {circ_cls}")
cnt = I.cycle_counts(prog); print("cycle counters [total, mm, dma, vec, n_inst]  circuit:", list(map(int, counters)), " model:", cnt)
ok = circ_cls == chip_cls and list(map(int, counters)) == cnt
print(f"\nthe 16 instruction slots hold {len(prog)-1} instructions; the program took {cnt[0]} cycles for 4 samples: {cnt[0]/4:.1f} per sample.")
print("matrix-unit cycles:", cnt[1], f"({cnt[1]/cnt[0]*100:.0f}% of the total); DMA:", cnt[2], f"({cnt[2]/cnt[0]*100:.0f}%); vector units:", cnt[3], f"({cnt[3]/cnt[0]*100:.0f}%)")
print("agreement with floating point:", "all four classes match" if circ_cls == real_cls else f"classes differ at samples {[i for i in range(4) if circ_cls[i] != real_cls[i]]}")
print("\nwhat this cost the programmer: 3 address maps (external memory, scratchpad, strides) kept in your head, two quantizer scale pairs worked out on paper, and every")
print("instruction's operands typed by hand. A mistake in any of them gives a plausible-looking wrong answer. That is the job Chapter 11's compiler (Capra) does automatically.")
json.dump({"classes_real": real_cls, "classes_chip": circ_cls, "counters": cnt, "prog": src, "m1": m1, "s1": s1, "m2": m2, "s2": s2}, open(os.path.join(R, "out", "ch07_example_b.json"), "w")); sys.exit(0 if ok else 1)
