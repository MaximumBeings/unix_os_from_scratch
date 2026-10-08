# 8. Attention and the KV Cache: One Decode Step, From Hidden State to Output

**What you will understand:** what a language model actually computes to produce one token, why it keeps a **KV cache**, and what the cache costs and saves, measured in cycles on the chip of Chapter 7. You will write the decode step as a GA-2 program (the hand-written forerunner of the compiler of Chapter 11), run it on the RTL, compare its answer with floating-point attention, see where the quantization error comes from, and then test the *program builder* by mutation, where three different kinds of check turn out to have three different blind spots.

**What you need to know first:** Chapters 3 (quantization), 6 (softmax) and 7 (the GA-2 instruction set).

## What a decode step computes

A transformer generates text one token at a time. For each new token, every attention head does this (one head shown; `d` is its width):

```text
q = x Wq       k = x Wk       v = x Wv        x: the hidden state of the newest token
scores  = K q / sqrt(d)                        K: the keys of ALL tokens so far, one row each
weights = softmax(scores)
out     = weights V                            V: the values of all tokens so far
```

The matrices `K` and `V` have one row per token. The key insight is that **the rows for old tokens never change**: token 5's key is `x_5 Wk` whatever token 6 is. So instead of recomputing `K` and `V` for the whole prefix at every step, the model **stores** them: that is the **KV cache**. Each step computes `k` and `v` only for the new token, appends them, and attends over the stored rows.

Because each step attends to the new token and all earlier ones (never later ones), the **causal mask** is automatic in decoding: there is nothing "later" in the cache to hide. (Masks matter in *prefill*, where a whole prompt is processed at once.)

## The same step as GA-2 programs: `model/attn.py`

```python
--8<-- "model/attn.py"
```

The file builds two programs for the same step:

- **cached**: load the new token's hidden state, the weights and the L-1 cached rows of K and V; project q, k, v for the new token; requantize them to int8; *append* k and v to the caches in external memory (two `ST`s); then attend.
- **recompute**: no cache. Project K and V for *all* `L` tokens from their hidden states, in 4 x 4 tiles, then attend.

Attention itself (`tail`) is the same in both, and each line is an instruction of Chapter 7: scores for four positions at a time (`MM` with `tb=1` so the key rows are read as columns, straight out of the cache with no copy), `RQ` into the softmax's Q4.4 input range, `SM`, `RQ` to int8 weights, `MM` of weights by V four output columns at a time, `RQ` to int8, `ST`. Below is the cached program for the third token, as assembled:

```text
--8<-- "out/ch08_run_out.txt:3:34"
```

Every requantizer in it has its own scale, and the scales have to be right. `rq_params` derives them from the six tensor scales: for example the scores' multiplier is `scale_q * scale_k / sqrt(d) * 16` (the `16` because the softmax unit's input is the score times 16). This is the kind of arithmetic a compiler must do for the user, and a place where bugs hide.

## Running it on the chip

```python
--8<-- "tools/ch08_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch08_run_out.txt"
```

### Reading it

- **All ten programs (cached and recompute, for L = 2, 4, 8, 16, 32) pass in both simulators**: the RTL produces exactly the memory contents and cycle counts of the reference simulator, and the two programs give identical outputs at every step of a decode loop.
- **The cache saves little at the start and a lot later.** At L = 2 the cached step is 1.05x faster; at L = 32 it is 3.04x (5,251 cycles against 15,989). Between L = 16 and L = 32 the recompute step costs about 455 more cycles per extra token of context ((15,989 - 8,709) / 16) and the cached step about 89 ((5,251 - 3,827) / 16): recomputation redoes the K and V projections of every token every step, so over a whole generation of `n` tokens its total work grows with `n` squared, while the cached total grows far more slowly.
- **Even the cached step is dominated by the matrix unit (58.5% of cycles at L = 32), and not because of arithmetic.** Of its 24 matrix instructions, 12 project the new token and cost the same at any context length, and in every one `M = 1`: a decode step multiplies a *vector* by a matrix, so only one row of the 4 x 4 array has anything to do, and the staging of operands (Chapter 7) dominates. This is the central inefficiency of decoding, and the reason Chapter 9 looks at batching: with four requests in flight, `M = 4` costs almost the same as `M = 1`.
- **Loading the caches is 34.7% of the cycles** (4 `LD` instructions: 1,820 cycles) and grows linearly with `L`: every step re-reads the whole cache from DRAM.

## How big is a cache?

The printed arithmetic at the end of the output is derived, not measured: one GA-2 head stores `2 x 16 x 1 byte = 32` bytes per token. For a 7B-class transformer (32 layers x 32 heads of width 128) in fp16 it is `2 x 32 x 32 x 128 x 2 = 524,288` bytes per token: **512 KiB per token, 2 GiB for a 4,096-token context** (1 GiB with an int8 cache), per sequence being served. The cache, not the weights, is what limits how many conversations fit in a chip's memory.

## How accurate is it?

```python
--8<-- "tools/ch08_study.py"
```

```text
--8<-- "out/ch08_study_out.txt"
```

These are measured on random Gaussian data with scales calibrated on the same example (a real system calibrates on a separate data set, so these numbers are optimistic).

- **int8 inputs and weights cost the most** (1.7% mean error), then the int8 attention weights (+1.1 points). Rounding the scores to 1/16 costs about half a point.
- **Replacing the softmax by Chapter 6's integer softmax adds nothing visible** (the stage "+ integer softmax" is the same as the one before to two decimals). That is the result of Chapter 6's accuracy study showing up in context.
- **The whole int8 step is 1% to 5% from floating point on average**, with outliers of 8-11% at larger `L`. Error grows a little with context length (more terms rounded), and these figures say nothing about a trained model's tolerance.

## Testing the tests: the program builder

The chip has been tested since Chapter 7. What is new here is a *program*, written by hand, with tiling, strides, scales and a cache protocol, and a hand-written program is exactly where a compiler would be wrong. So this chapter mutates the **builder** (`model/attn.py`) one line at a time and asks which checks notice:

- **(a)** the cached and recompute programs give identical outputs at every step of a decode loop (always applied);
- **(b)** the error against floating point stays under 12% (twice the worst error the correct builder has on these examples);
- **(c)** the output equals an *integer reference* exactly, a plain-Python implementation of the maths with no instructions, tiles or addresses (`int_reference`);
- **(d)** every stage's int8 values are within one level of the real-number arithmetic its scale stands for (`stagewise_problems`): the int8 `q` must equal `round(x Wq * scale_x * scale_w / scale_q)` computed in floating point from the quantized inputs, and so on for k, v, scores, weights and output.

```python
--8<-- "tools/mut_ch08.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch08_mutation_out.txt"
```

Every mutant is caught by at least one check, and **no single check catches all of them**:

- **(c) the exact integer reference misses 8 of 23.** All eight are scale errors. The reference shares `rq_params` with the builder, so a wrong scale is wrong in both and the two agree perfectly. An exact comparison is only as independent as what it does not share.
- **(b) the accuracy bound misses two small scale errors** (3% and 2% too large): they are below the quantization noise (1-5%) and the 12% bound cannot see them.
- **(d) the stage-wise real-number check catches those**, because it checks each stage against what its scale *means* and shares nothing with the builder. Its one miss is a plumbing bug (the output stored from the wrong buffer), which it does not look at, and which (b) and (c) catch.
- Everything about tiling, strides, the cache protocol and which weights are used is caught by (a) already: for instance *"the new key is never appended"* is caught only because the test runs a **whole decode loop** with the cache persisting from step to step. A single step cannot see it, since the append matters only on the next step.

Two further changes survive and are **equivalent**: always computing 4 scores in the last tile, or 4 rows in the last projection block, only writes extra values that nothing reads.

## What this chapter does and does not establish

- **Verified**: the cached and recompute programs agree with each other at every step of a decode loop (L = 1 to 24, several seeds) and with an integer reference; the RTL matches the reference simulator on ten programs up to L = 32; each stage is consistent with its real-number meaning.
- **Not covered**: more than one head or layer (they repeat the same program), layer normalization and the feed-forward block (Chapter 12 builds a complete, if tiny, transformer), prefill of long prompts (needs `K > 64` tiles), and trained weights. The scratchpad layout of this builder supports up to 32 tokens.

## Chapter summary

A decode step is: project the new token, append its key and value to the cache, score the new query against every cached key, softmax, and average the cached values. The cache turns work that grows with the square of the context into work that grows linearly, but every step still re-reads it all from memory, and the matrix-vector products leave three quarters of the array idle. The step runs correctly on the chip, within 1-5% of floating point. A hand-written program needs its own tests, and the three kinds used here, whole-loop equality, exact reference, and real-number meaning of each scale, each have a blind spot the others cover.

## Self-check questions

1. Why does a decode step need no causal mask?
2. A model has 24 layers, 16 heads of width 64 and an fp16 cache. How many bytes per token and for a 2,048-token context?
3. In the cached program at L = 32, 12 of the 24 `MM` instructions do not depend on the context length. Which ones, and what is their total cycle cost?
4. Why does the exact integer reference miss "q requantization scale is 3% too large" while the stage-wise check catches it?
5. Why can no single-step test catch "the new key is never appended to the cache"?
