#!/usr/bin/env python3
"""Appendix C (number formats): runnable checks. Pure Python (struct only for fp32/fp16 reference encodings). Usage: appx_c.py"""
import itertools, math, struct
print("== 1. two's complement: n bits hold -2^(n-1) .. 2^(n-1)-1; the same bit pattern read two ways (4 bits)")
print("  pattern unsigned signed | pattern unsigned signed")
for i in range(8): print(f"  {i:04b}   {i:5d}  {i:5d}  | {i + 8:04b}   {i + 8:5d}  {i + 8 - 16:5d}")
print("  ranges: " + "; ".join(f"int{n}: {-2**(n-1)}..{2**(n-1)-1}" for n in (4, 8, 16, 32)))
neg = lambda x, n: (~x + 1) & ((1 << n) - 1)
print(f"  negate = invert and add one: -5 in 8 bits is {neg(5, 8):08b} = {neg(5, 8)} unsigned; -(-128) = {neg(128, 8)} in 8 bits: the one value that cannot be negated (it is its own negative)")
print("\n== 2. overflow: wrap-around against saturation (int8)")
wrap8 = lambda v: ((v + 128) & 255) - 128; sat8 = lambda v: max(-128, min(127, v))
for a, b in ((100, 27), (100, 28), (-100, -29), (127, 127)): print(f"  {a:5d} + {b:5d} = {a + b:5d}: wrap {wrap8(a + b):5d}, saturate {sat8(a + b):5d}")
print("\n== 3. fixed point: a real number stored as an integer times a power of two (Qm.n: m integer bits, n fraction bits); quantization error is at most half a step")
for name, bits, frac in (("Q1.7 (int8)", 8, 7), ("Q4.4 (int8)", 8, 4), ("Q8.8 (int16)", 16, 8)):
    step = 2.0 ** -frac; lo, hi = -2 ** (bits - 1) * step, (2 ** (bits - 1) - 1) * step; x = 0.7071; q = max(-2 ** (bits - 1), min(2 ** (bits - 1) - 1, round(x / step)))
    print(f"  {name:13s} step {step:<10g} range {lo:g} .. {hi:.6g}; 0.7071 -> integer {q} -> {q * step:.6f} (error {abs(q * step - x):.6f} <= {step / 2:g})")
print("\n== 4. rounding: which integer does a .5 go to?  (the choice matters for bias)")
def half_up(x): return math.floor(x + 0.5)
away = lambda x: int(math.copysign(math.floor(abs(x) + 0.5), x))
print(f"  {'x':>5s} {'truncate':>9s} {'half up':>8s} {'half away':>10s} {'half even':>10s}")
for x in (0.5, 1.5, 2.5, 3.5, -0.5, -1.5, -2.5): print(f"  {x:5.1f} {math.trunc(x):9d} {half_up(x):8d} {away(x):10d} {round(x):10d}")
xs = [k + 0.5 for k in range(-50, 50)]; print(f"  the average error of rounding the 100 half-way values -50.5..49.5: half up {sum(half_up(x) - x for x in xs) / 100:+.2f}, half away from zero {sum(away(x) - x for x in xs) / 100:+.2f}, half even {sum(round(x) - x for x in xs) / 100:+.2f}  (half even has no bias)")
print("\n== 5. floating point: sign, exponent, mantissa. Decode any format with e exponent bits and m mantissa bits")
def decode(bits, e, m):
    s = bits >> (e + m); ex = (bits >> m) & ((1 << e) - 1); fr = bits & ((1 << m) - 1); bias = 2 ** (e - 1) - 1
    if ex == (1 << e) - 1: return float("nan") if fr else (-1) ** s * float("inf")
    if ex == 0: return (-1) ** s * fr / 2 ** m * 2.0 ** (1 - bias)
    return (-1) ** s * (1 + fr / 2 ** m) * 2.0 ** (ex - bias)
fmts = {"fp32": (8, 23), "fp16": (5, 10), "bf16": (8, 7), "fp8 e4m3": (4, 3), "fp8 e5m2": (5, 2)}
print(f"  {'format':9s} {'bits':>4s} {'exp':>4s} {'mant':>5s} {'largest finite':>16s} {'smallest normal':>16s} {'epsilon (1 to next)':>20s}")
for n, (e, m) in fmts.items():
    top = ((1 << e) - 2) << m | ((1 << m) - 1); print(f"  {n:9s} {1 + e + m:4d} {e:4d} {m:5d} {decode(top, e, m):16.6g} {decode(1 << m, e, m):16.6g} {2.0 ** -m:20.6g}")
print("  (fp8 e4m3 as used in practice reserves only one code for NaN and so reaches 448; this generic decoder follows the IEEE pattern and gives 240)")
w = struct.unpack(">I", struct.pack(">f", 0.1))[0]; h = struct.unpack(">H", struct.pack(">e", 0.1))[0]
print(f"  0.1 in fp32 is 0x{w:08X} = {decode(w, 8, 23)!r}; in fp16 0x{h:04X} = {decode(h, 5, 10)!r}; in bf16 (the top 16 bits of fp32) 0x{w >> 16:04X} = {decode(w >> 16, 8, 7)!r}")
print(f"  decoder check against the hardware encodings: fp32 and fp16 of 1000 values agree: {all(decode(struct.unpack('>I', struct.pack('>f', v))[0], 8, 23) == struct.unpack('>f', struct.pack('>f', v))[0] for v in [i * 0.37 - 180 for i in range(1000)])}, {all(decode(struct.unpack('>H', struct.pack('>e', v))[0], 5, 10) == struct.unpack('>e', struct.pack('>e', v))[0] for v in [i * 0.037 - 18 for i in range(1000)])}")
print("  float spacing grows with magnitude: the gap between neighbours near 1, 100 and 10000 in fp16:")
for v in (1.0, 100.0, 10000.0): b = struct.unpack(">H", struct.pack(">e", v))[0]; print(f"    near {v:>8g}: {decode(b + 1, 5, 10) - decode(b, 5, 10):g}")
print("\n== 6. accumulating products: why the MAC has a wide accumulator (Chapter 2)")
print("  an int8 x int8 product needs 16 bits; a sum of n of them needs 16 + ceil(log2 n) bits. The worst case is every product equal to (-128) x (-128) = 16384:")
for n in (1, 2, 4, 16, 64, 256):
    worst = n * 128 * 128; bits = math.ceil(math.log2(worst + 1)) + 1; print(f"    n = {n:4d}: worst-case sum {worst:9d} (all (-128)x(-128)), needs {bits} signed bits; formula 16 + ceil(log2 n) = {16 + math.ceil(math.log2(n))}; match: {bits == 16 + math.ceil(math.log2(n))}")
print("\n== 7. symmetric int8 quantization in three lines (Chapter 3): scale = max|x| / 127; q = round(x / scale)")
x = [0.62, -0.31, 0.05, 0.93, -0.88]; sc = max(abs(v) for v in x) / 127; q = [round(v / sc) for v in x]; print(f"  x = {x}\n  scale = {sc:.6f}; q = {q}; dequantized = {[round(v * sc, 4) for v in q]}; largest error {max(abs(v * sc - o) for v, o in zip(q, x)):.5f} <= {sc / 2:.5f}")
