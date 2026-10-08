#!/usr/bin/env python3
"""Chapter 13, running example A: int4 by hand. Eight real weights are quantized to signed 4-bit integers (range -7..7), packed into ONE 32-bit word, loaded and expanded by the UNPACK instruction on the real circuit (Icarus), and used in a small matrix product against int8 activations; the result is compared with the same product using int8 weights and with floating point. Writes out/ch13_example_a.json. Usage: ch13_example_a.py"""
import json, os, sys
os.environ["GA2_EXT"] = "2048"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import ga2_isa as I, ga2_progs as G
from quant import quantize, round_half_away_float
w = [0.62, -0.31, 0.05, 0.93, -0.88, 0.27, -0.12, 0.44]; x = [3, -2, 5, 1, -4, 2, 6, -1]
print("== 1. quantize eight weights to int4 (symmetric, range -7..7, scale = max|w| / 7)")
sc4 = max(abs(v) for v in w) / 7; q4 = [quantize(v, sc4) for v in w]; sc8 = max(abs(v) for v in w) / 127; q8 = [quantize(v, sc8) for v in w]
print(f"scale int4 = {max(abs(v) for v in w):.2f} / 7 = {sc4:.5f} (one step); scale int8 = {sc8:.5f}")
print("   w        w/scale   int4  w4=q*scale   err4      int8   err8")
for a, b, c, d in zip(w, q4, q8, range(8)): print(f"  {a:+.2f}    {a/sc4:+7.3f}    {b:+3d}   {b*sc4:+.4f}   {abs(b*sc4-a):.4f}    {c:+4d}   {abs(c*sc8-a):.4f}")
print(f"largest error: int4 {max(abs(b*sc4-a) for a, b in zip(w, q4)):.4f} (half a step = {sc4/2:.4f}), int8 {max(abs(c*sc8-a) for a, c in zip(w, q8)):.4f} (half a step = {sc8/2:.5f})")
word = sum((v & 15) << (4 * j) for j, v in enumerate(q4))
print("\n== 2. packing: element j goes to bits 4j+3..4j of one 32-bit word")
print("   nibbles (two's complement):", " ".join(f"{v & 15:X}" for v in q4), "-> word, most significant nibble first:", " ".join(f"{(word >> (4*j)) & 15:X}" for j in reversed(range(8))), f"= 0x{word:08X}")
print("   eight weights in one word: 32 bits instead of 8 x 32 (this chip stores one int8 per word) or 8 x 8 = 64 bits (packed int8): the logical storage is half of int8's")
print("\n== 3. on the circuit: LD the packed word, UNPACK it, multiply a 1x8 row of int8 activations by the 8x1 unpacked weights (MM), store both")
src = f"""ld     dst=0 src=0 len=1
unpack dst=8 src=0 len=1
ld     dst=100 src=16 len=8
mm     dst=200 A=100 B=8 M=1 K=8 N=1 lda=8 ldb=1 ldc=1
st     src=8 dst=32 len=8
st     src=200 dst=48 len=1
halt"""
prog = I.assemble(src); ext = [0] * I.EXT; ext[0] = word
for i, v in enumerate(x): ext[16 + i] = v & 0xFFFFFFFF
open(os.path.join(hw.ROOT, "out", "ga2_trace.hex"), "w").write("\n".join("%08x" % (v & 0xFFFFFFFF) for v in [1] + G.record(prog, ext)) + "\n")
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_trace_tb.v"]
rc, out = hw.sim_icarus(F, "ga2_trace_tb", defines=("EXTW=2048",)); assert rc == 0, out
memx = {int(l.split()[1]): int(l.split()[2]) for l in out.splitlines() if l.startswith("X ")}; cnt = [l for l in out.splitlines() if l.startswith("COUNTERS")][0].split()[1:]
unp = [memx.get(32 + i, 0) for i in range(8)]; dot = memx.get(48, 0)
print("   program:"); [print("     ", l) for l in src.splitlines()]
print(f"   UNPACK result read back from the circuit: {unp}")
print(f"   the same, unpacked by hand from the nibbles: {q4}  -> identical: {unp == q4}")
want = sum(a * b for a, b in zip(x, q4)); print(f"   dot product on the circuit: {dot};  by hand with the int4 integers: {want}  -> identical: {dot == want}")
print(f"   cycles [total, mm, dma, vec, n]: {list(map(int, cnt))}  (UNPACK of one packed word costs 10 cycles: 1 read, 1 latch, 8 writes; model: {I.cycle_counts(prog)})")
ok = unp == q4 and dot == want and list(map(int, cnt)) == I.cycle_counts(prog)
print("\n== 4. what the product means in real numbers: x . w with int4 weights, int8 weights, and floating point (activations x are integers here)")
real = sum(a * b for a, b in zip(x, w)); r4 = want * sc4; r8 = sum(a * c for a, c in zip(x, q8)) * sc8
print(f"   floating point {real:+.4f}   int8 weights {r8:+.4f} (error {abs(r8-real):.4f})   int4 weights {r4:+.4f} (error {abs(r4-real):.4f})")
print("   one dot product is a single sample: the rounding errors of the eight weights partly cancel or add. Running example B measures the effect over many.")
json.dump({"w": w, "q4": q4, "q8": q8, "sc4": sc4, "sc8": sc8, "word": word}, open(os.path.join(hw.ROOT, "out", "ch13_example_a.json"), "w")); sys.exit(0 if ok else 1)
