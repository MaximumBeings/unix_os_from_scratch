#!/usr/bin/env python3
"""Chapter 3: the quantization arithmetic, in plain Python integers, exactly as the hardware must do it, plus the helpers that choose scales and measure error.

THE SCHEME (symmetric int8, the one most inference chips use): a real number x is stored as an integer q in [-127, 127] with x ~ q * scale. The value -128 is never used, so that negating a value can never overflow and the range is symmetric about zero.
  quantize(x, scale)      q = clamp(round_half_away(x / scale), -127, 127)
  scale from data         scale = max|x| / 127      (per tensor, or per row/channel)
A matrix product of two int8 tensors accumulates in int32:   acc = sum a_q * b_q    and represents the real value  acc * (scale_a * scale_b).
REQUANTIZATION turns that int32 accumulator back into an int8 for the next layer, with output scale s_out:   q_out = round_half_away(acc * M),   M = scale_a * scale_b / s_out
M is a real number, usually between 2^-16 and 1. Hardware does not multiply by a real number; it multiplies by an integer MANTISSA m (24 bits, 2^23 <= m < 2^24) and shifts right by s:   M ~ m / 2^s,  q_out = clamp(round_half_away(acc * m / 2^s), -127, 127), optionally clamped at 0 from below (a fused ReLU).
"""
import math, random
QMIN, QMAX = -127, 127
def round_half_away(num, den):
    """round(num / den) to the nearest integer, ties away from zero, for integers (den > 0), with no floating point."""
    q, r = divmod(abs(num), den); q += (2 * r >= den); return q if num >= 0 else -q
def clamp(v, lo, hi): return max(lo, min(hi, v))
def quantize(x, scale): return clamp(round_half_away_float(x / scale), QMIN, QMAX)
def round_half_away_float(v): return int(math.floor(abs(v) + 0.5)) * (1 if v >= 0 else -1)
def scale_for(values): m = max(abs(v) for v in values); return m / QMAX if m > 0 else 1.0
def mantissa_shift(M):
    """Represent a positive real M as (m, s) with 2^23 <= m < 2^24 and M ~ m / 2^s (s in 16..47 for the ranges we care about)."""
    assert M > 0
    s = 23 - math.floor(math.log2(M)); m = round(M * (1 << s))
    if m >= 1 << 24: m >>= 1; s -= 1
    assert (1 << 23) <= m < (1 << 24); return m, s
def requant(acc, m, s, relu=False):
    """The hardware's function: the int32 accumulator, a 24-bit mantissa and a shift, to an int8 in [-127, 127] (or [0, 127] with relu)."""
    q = round_half_away(acc * m, 1 << s); return clamp(q, 0 if relu else QMIN, QMAX)
def matmul_int8(A, B):
    """A: n x k, B: k x p, int8 -> n x p int32 (exact)."""
    return [[sum(A[i][t] * B[t][j] for t in range(len(B))) for j in range(len(B[0]))] for i in range(len(A))]
