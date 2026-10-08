# 11. Capra, the Compiler: From a Tensor Graph to GA-2 Instructions

![ch-11](../assets/art/ch-11.svg)

--8<-- "docs/assets/art/ch-11.md"


**What you will understand:** how **Capra** (named for *Capra*, the genus of the wild goats and ibexes) turns a model, written as a graph of tensor operations in floating point, becomes a program for the chip of Chapter 7 that no human wrote: how the compiler **chooses an int8 scale for every tensor**, **cuts big matrix products into the 4 x 4 tiles** the hardware can do, **places tensors in the 4,096-word scratchpad and reuses the space** as soon as they are dead, and **emits** instructions. You will also see how a compiler is tested: against a plain-Python interpreter, against the meaning of each number, against its own allocator, and against graphs it must refuse.

**What you need to know first:** Chapters 3 (quantization), 7 (the ISA) and 8-9 (programs written by hand, which is what this chapter automates).

## Where the compiler sits

```text
 graph of tensor ops (float)
        |  calibrate: run on sample data, record each tensor's range
        v
 scale planning   -> an int8 scale per tensor; a multiplier + shift per requantizer
        v
 tiling           -> each matmul becomes 4 x 4 output tiles (MM instructions)
        v
 allocation       -> a scratchpad address for every tensor, reusing dead space
        v
 emission         -> LD / MM / RQ / SM / VADD / AMAX / ST ... HALT
```

The input is a small **graph IR**: tensors are 2-D, and the operations are `input`, `weight`, `matmul` (optionally with the second operand transposed and a constant factor), `softmax` (per row), `add`, `relu`, `concat_rows`, `argmax` (per row) and `output`. That is exactly what a transformer decode step needs; Chapter 12 uses it for one.

## Capra: `model/capra.py`

```python
--8<-- "model/capra.py"
```

What it decides, in order:

1. **Calibration** (`plan_scales`). The graph is evaluated in floating point on sample inputs; each tensor's largest absolute value gives its int8 scale (`max / 127`). Tensors that must share a scale form a group: the parts of a concatenation and its result, or the input and output of a `relu`. Two scales are **pinned** by the hardware: a softmax's input must be in Q4.4 (1/16) and its output is a probability (1/127). An `add` does *not* force a shared scale: the compiler **rescales** an operand whose scale differs from the sum's, with an `RQ` from int8 to int8.
2. **Requantizers.** For `matmul` the multiplier is `factor x scale_a x scale_b / scale_out`, turned into the 24-bit mantissa and shift of Chapter 3.
3. **Tiling.** A product of an `M x K` and a `K x N` matrix becomes `ceil(M/4) x ceil(N/4)` `MM` instructions, each with the right strides (`lda`, `ldb`, `ldc`) and the transpose flag. The matrix unit's limit `K <= 64` is a compile error, not a surprise.
4. **Allocation.** Each tensor gets a buffer; a concatenation's inputs become **views** of its output buffer (so the concatenation costs no instructions: the producers write straight into place), and a fused `relu` shares the buffer of its producer. Buffers are allocated first-fit when first needed and **released after their last consumer**, with coalescing.
5. **Emission.** Weights and inputs are loaded just before first use, outputs stored at the end, softmax and argmax emitted row by row.

Two interpreters accompany it (`interpret` and `stagewise_problems`), written for testing and discussed below.

## An example, compiled

```python
--8<-- "tools/ch11_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020)**

```text
--8<-- "out/ch11_run_out.txt"
```

### Reading it

- **Section 1** shows the whole pipeline on a two-layer network with a residual connection and a softmax head. The compiler chose a scale for every tensor, fused the `relu` into the first matmul's requantizer, inserted two rescales (instructions 17 and 18) for the residual `add`, whose operands (scales 0.0376 and 0.0325) differ from the sum's (0.0438), emitted 38 instructions in all, and needed 280 of the 4,096 scratchpad words. The class decision agrees with floating point on all five rows, and the probabilities are 2.0% off.
- **Section 2** compiles twelve random graphs the test battery has never seen, puts them on the RTL, and compares with the reference simulator: **identical memory and cycle counters, in both simulators**. Their error against floating point is 0-3.7%.
- **Allocation matters.** On those graphs, reuse cuts the scratchpad footprint to 43-85% of what keeping every buffer would need. In section 3 a six-layer network of 32 x 32 layers compiles to 111 instructions in **1,792 words** with reuse, and **fails** without it ("scratchpad is full"): its weights alone (6 x 1,024 words) exceed the memory.

## How a compiler is tested

A compiler's output is a program, and "does the program run" is not the question; the question is whether it computes the right thing for *every* graph. The battery in `model/capra_tests.py` generates random graphs (random shapes, matmul chains, fused relus, self-products, softmax heads, residual adds, concatenations, argmax outputs), compiles each twice (with and without buffer reuse), and applies five checks:

```python
--8<-- "model/capra_tests.py"
```

- **(a) The allocator.** The program with buffer reuse and the one that keeps every buffer alive must give identical outputs. A buffer freed too early, or two live buffers given overlapping space, makes them differ.
- **(b) Meaning.** The simulator's outputs must equal the integer **interpreter's**, which runs the graph in plain Python with no instructions, tiles or addresses.
- **(c) Every tensor.** With reuse off, every tensor read back from the scratchpad must equal the interpreter's, so a bug in the middle of the graph cannot hide behind a correct-looking output.
- **(d) Scales.** Every tensor must be within one level of the *real-number meaning* of its operation applied to the dequantized inputs (`stagewise_problems`). This check uses the planned scales but not the requantizer parameters, so it sees a wrong multiplier that (b) cannot (the interpreter shares the parameters with the compiler).
- **(e) Accuracy.** The dequantized outputs must stay within 0.16 of the floating-point result (relative error) on inputs **not** used for calibration; 0.16 is twice the worst error the correct compiler has on the 60 battery graphs (0.079).

Beyond random graphs, `check_errors` lists **five graphs the compiler must refuse** (inner dimension 65, a relu that cannot be fused, a tensor feeding two concatenations, buffers larger than the scratchpad, a relu on a tensor with another consumer) and `check_cancellation` is a directed case described below.

### What the tests found while the compiler was being written

None of these is an invented example; each was a failing check:

1. **A fused relu lost its place.** In a graph that concatenated the output of a relu with another tensor, the placement code gave the relu's output a view inside the concatenation buffer and then *overwrote* that placement with its producer's separate buffer. Check (a) failed, (b) failed, and (c) pointed at the exact tensor. The fix: the matmul under a relu takes the *relu's* location.
2. **Forced scale sharing saturates.** The first version made both operands and the result of an `add` share one scale. When one operand was a softmax output (pinned to 1/127, a range of 1.0) and the other an ordinary tensor reaching 3, the ordinary tensor clipped at 1.0 and the error was 50-100%. The principled fix is the rescale instruction described above. A second fix followed: the sum's scale must hold both **operands** (not just the sum) because two large operands that cancel would otherwise be clipped when rescaled.
3. **The generator was wrong too.** Concatenating a unit-variance input onto a tensor of magnitude 0.01 forces one scale on both and ruins the small one. That is not a compiler bug: real concatenations (a key cache and a new key) have matching magnitudes. The generator now creates partner inputs at the magnitude of what they join.
4. **A limit of per-tensor static scales.** Repeated squaring (`x x^T`, three times) spans orders of magnitude between samples; no single scale serves it. The generator allows one self-product per graph. This is a property of the technique, not a defect to fix.
5. **Cancellation.** In `add(x, y)` with `y` close to `-x`, int8 cannot hold the tiny sum however the compiler behaves: the correct compiler gets this wrong by 100% in relative terms. The accuracy check therefore cannot judge it. But a **compiler bug** that chooses the sum's scale from the sum alone (ignoring the operands) makes the rescaled operands clip, which the real-number check (d) sees. So the battery includes this one graph, with checks (a)-(d) only. Random graphs almost never contain a cancellation, and an experiment showed the mutant surviving until this directed case was added.

## Testing the tests

```python
--8<-- "tools/mut_ch11.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch11_mutation_out.txt"
```

All 27 broken compilers are caught: tiling mistakes (edge tiles, offsets, strides, the transpose flag), quantization mistakes (a dropped constant factor, a wrong operand's scale, a lost relu flag, a wrong probability scale, a wrong pinned scale, calibration keeping the smallest range), lowering mistakes (rows reading the wrong row, wrong destinations, a missing rescale, concatenation parts overlapping, a truncated load or store, overlapping external regions), allocator mistakes (releasing a buffer a step early, merging free blocks across a gap) and removed safety checks (the inner-dimension limit, relu fusion with another consumer, a tensor feeding two concatenations). The removed checks are caught only because the battery contains graphs the compiler must refuse; without `check_errors` those mutants survive.

One change survives and is **equivalent in effect**: an allocator that takes the next larger hole instead of an exact fit only differs when memory is nearly full.

## What this chapter does and does not establish

- **Verified**: on 60 battery graphs and 12 fresh ones, the compiled programs match the interpreter exactly, every tensor is consistent with its real-number meaning, the allocator is correct under reuse, and the programs run on the RTL with exactly the reference's memory and cycle counters.
- **Not covered**: a matrix product with `K > 64` (it needs an int32 accumulate instruction the ISA does not have; the compiler refuses it); scratchpad spilling; scheduling that overlaps loads with compute (the machine of Chapter 7 runs one instruction at a time); other operations (layer normalization, GELU); dynamic shapes; and anything about calibration data beyond random Gaussians. The accuracy numbers say nothing about a trained model.

## Chapter summary

The compiler turns a float graph into a GA-2 program in five steps: calibrate, plan scales, tile, allocate with reuse, emit. It was tested against an interpreter and the real-number meaning of every tensor, against itself with and without reuse, and against graphs it must refuse; 27 of 27 broken compilers were caught, and the process found a placement bug, a scale-sharing flaw in `add`, and a blind spot (cancellation) that needed a directed case.

## Self-check questions

1. Why does the compiler reject a matmul with `K = 65`, and what would the hardware need to support it?
2. Why was forcing both operands of an `add` to share a scale wrong, and what does the compiler do instead?
3. Why is check (c) run with buffer reuse switched off?
4. Which check catches "buffers are released one step too early", and why do the others not?
5. Why is the cancellation graph tested without an accuracy bound, and what bug does it exist to catch?
