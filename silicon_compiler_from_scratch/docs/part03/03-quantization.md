# 3. Quantization: int8 Numbers, the Requantizer, and What Accuracy It Costs

![ch-03](../assets/art/ch-03.svg)

--8<-- "docs/assets/art/ch-03.md"


**What you will understand:** why inference chips compute in 8-bit integers, how a real number becomes an int8 (and why the range is -127..127), how the 32-bit accumulator of a matrix product is brought back to 8 bits by a circuit called the **requantizer**, and how much accuracy all this costs, measured, including the case where one outlier ruins everything else.

**What you need to know first:** Chapter 2 (two's complement, the MAC, overflow).

## Why 8 bits

Every weight of a model has to be read from memory for every token generated, and memory bandwidth, not arithmetic, usually limits inference (Chapter 9 measures this). An int8 weight is one byte: a quarter of fp32, half of fp16. An int8 multiplier is also far smaller than a floating-point one (Chapter 2: about 400 gates). So the chip stores and multiplies small integers, accumulates in 32 bits, and converts back to 8 bits between layers.

## The scheme: symmetric int8

A real number `x` is stored as an integer `q` with `x ~ q * scale`.

- `scale = max|x| / 127`, so the largest value maps to +-127.
- `q = clamp(round(x / scale), -127, 127)`. The value -128 is **never used** (Chapter 2): the range is symmetric, so negating a quantized value cannot overflow and a zero-centred distribution loses nothing.
- Rounding is **to nearest, ties away from zero**, done on integers (no floating point in the reference), so every implementation agrees on the half-way cases.

A matrix product of two int8 tensors accumulates exactly in int32 (`acc = sum a_q * b_q`), and `acc` represents the real value `acc * scale_a * scale_b`. To feed the next layer it must become an int8 with an output scale `s_out`:

```text
q_out = round( acc * M ),   M = scale_a * scale_b / s_out     (a real number, typically 2^-16 .. 1)
```

Hardware does not multiply by a real number. It uses an integer **mantissa** `m` (24 bits, 2^23 <= m < 2^24) and a **shift** `s`: `M ~ m / 2^s`, so `q_out = clamp(round(acc * m / 2^s))`. The relative error of that approximation is at most 2^-24 (measured below).

## The reference: `model/quant.py`

```python
--8<-- "model/quant.py"
```

`round_half_away` works on integers only (`divmod`, then compare twice the remainder with the denominator). `requant` is the function the circuit has to match, bit for bit.

## The circuit: `rtl/requant.v`

```verilog
--8<-- "rtl/requant.v"
```

It is combinational (no clock): take the magnitude of `acc`, multiply by `m` (32 x 24 bits gives up to 56 bits), add the rounding constant `2^(s-1)`, shift right by `s`, clamp the magnitude to 127, restore the sign, and optionally clamp negatives to zero (a fused ReLU). Rounding the **magnitude** gives ties-away-from-zero for both signs, and makes `requant(-x) = -requant(x)`. A `2^31` magnitude fits in 32 unsigned bits, so the most negative accumulator is not special.

## The answer key and the testbench

```python
--8<-- "model/requant_gold.py"
```

```verilog
--8<-- "tb/requant_tb.v"
```

The key has 201,808 cases: the accumulator at both ends of int32, mantissas at both ends, shifts 0 and 63, **exact ties** at every shift (built so that `acc * m` is exactly `(2k+1) * 2^(s-1)`), results sitting exactly at the clamp limit, and 200,000 random ones, with two thirds of them having small accumulators (below about 2^20 or a few hundred times a power of two) so that the interesting rounding region is exercised.

## Running it

```python
--8<-- "tools/ch03_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch03_run_out.txt"
```

Both simulators agree with the key on all 201,808 cases. The requantizer is **5,322 generic gates / 2,777 iCE40 cells** (measured), about thirteen times a Chapter 2 multiplier. It is a large block for what it does, mostly because of the 32 x 24 multiply; a real design would share one requantizer per row of outputs, or pipeline it. (Not measured here: speed.)

## Testing the tests

```python
--8<-- "tools/mut_ch03.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch03_mutation_out.txt"
```

All 15 real mutants are caught, including the subtle ones: rounding the signed value instead of the magnitude, a shift of 5 bits instead of 6, a product cut to 48 bits, the rounding constant minus one (ties toward zero), and the 31-bit magnitude of the most negative accumulator.

Three mutants survive and each is **equivalent**; none is a gap:

- *"Shift of 0 adds a half."* With `s = 0`, `1 << (s-1)` would be `1 << 63`, but the `half` wire is 56 bits wide, so the 1 falls off the end and the constant is 0 anyway. The special case in the source is documentation, not logic.
- *"Clamp compares against 126."* A magnitude of exactly 127 is returned as 127 either way (clamped or not).
- *"Compare `>= 128` instead of `> 127`."* Same condition on integers.

This is worth knowing for a reason beyond the test: the first two are **dead logic** a synthesis tool will remove, and an honest mutation report finds it.

## How much accuracy does int8 cost?

All figures below are **measured** with `tools/ch03_study.py` (pure Python, fixed seeds, Gaussian random data, relative RMS error against an fp64 reference). They show the *shape* of the effect on random data; real model weights have their own distributions.

```python
--8<-- "tools/ch03_study.py"
```

```text
--8<-- "out/ch03_study_out.txt"
```

### Reading it

- **Plain data: about 1-1.5% error**, and it does *not* grow with the length K of the dot product (1.1% at K=64, 1.4% at K=1024). Rounding errors are random and average out relative to the sum.
- **Per-column scales help a little on plain data** (1.2% vs 1.4%), because each column gets its own `max/127`.
- **One outlier column breaks per-tensor quantization for everyone else.** With one column 30x larger, the single scale is set by the outlier, the other 31 columns use only a few of the 255 levels, and their error is **19.5%** (per-tensor) vs **1.3%** (per-column). This is the reason real systems use per-channel scales and special handling of outliers.
- **The requantizer's own approximation is negligible.** The worst relative error of `m / 2^s` against the exact multiplier over 100,000 random values is 5.9e-8, at the 2^-24 bound.
- **The 32-bit accumulator is generous on this data** (about 40,000 times the largest value seen), consistent with Chapter 2: overflow needs 2^16 worst-case products. Real models with large activations and K in the thousands come closer; this test does not claim they do not.
- **Bytes per weight** are derived, not measured: fp32 4, fp16 2, int8 1, int4 0.5, so a 7-billion-parameter model occupies 28, 14, 7 and 3.5 GB. Chapter 9 turns this into tokens per second.

## What this chapter does and does not establish

- The requantizer **matches its reference exactly** on 201,808 cases including all the tie and clamp corners; it is not exhaustive (2^63 combinations).
- The accuracy numbers are for random Gaussian data. Whether int8 is accurate enough for a given model is an experiment on that model.
- The gate counts are Yosys generic gates and iCE40 cells, not area or speed.

## Chapter summary

int8 symmetric quantization stores `x` as `q` in -127..127 with `scale = max|x|/127`. The int32 accumulator returns to int8 through a mantissa-and-shift requantizer with round-half-away and a clamp; the circuit matches the integer reference on all 201,808 cases and all 15 real mutants are caught (3 equivalents explained). The accuracy cost is about 1-1.5% on plain data, but a single outlier ruins per-tensor quantization for the rest, which per-column scales fix.

## Self-check questions

1. Why does the scheme avoid -128, and what would break if it were allowed?
2. The requantizer rounds the magnitude, not the signed value. What would change for an accumulator of -3 with M = 1/2?
3. Why does the error not grow with K, and why does an outlier column hurt the other columns?
4. The mutant "shift of 0 adds a half" survives. Why is that not a test gap?
5. A 7B-parameter model in int8 needs 7 GB of weights. If a chip can read 100 GB/s, what is the upper bound on tokens per second, and what are you assuming?
