# 12. Capstone: A Tiny Transformer Decodes on the Chip

![ch-12](../assets/art/ch-12.svg)

--8<-- "docs/assets/art/ch-12.md"


**What you will see:** everything in this book working together. A one-layer transformer written as a floating-point graph is turned by the compiler of Chapter 11 into GA-2 programs; a host loop feeds each chosen token back and keeps the KV cache of Chapter 8; the programs run on the Verilog of Chapter 7 in two simulators; and the 24 tokens that come out are the ones floating point produces and the ones a formula predicts. Then the capstone is turned on the hardware as a *test*, and it shows what an end-to-end workload can and cannot tell you about a chip.

**What you need to know first:** Chapters 7-11. There is little new machinery here: this chapter is the assembly and the audit.

## The model, and what it is not

```python
--8<-- "model/tiny_lm.py"
```

A decode step is: embed the token (the **host** picks the row; GA-2 has no gather instruction), project `q, k, v`, append `k` and `v` to the cache, attention over the whole cache, an output projection and a residual add, a two-layer feed-forward block and another residual add, a final projection to 16 logits, and `argmax`. That is a complete, if small, transformer block; it lacks layer normalization and GELU (the compiler has no such operations), uses one head, and has a 16-token vocabulary.

**The weights are constructed, not trained.** Training a model is outside this book. To give the run a checkable answer, the 16 embeddings are the orthogonal rows of a Hadamard matrix and the output projection is arranged so that the model's next token is `f(t) = (5 t + 3) mod 16`, a permutation with a single cycle of length 16. Attention and the feed-forward block have random weights and really run, but they are small: measured in floating point along this generation, the attention output has 7% of the norm of the embedding and the feed-forward output 14%. They perturb the logits by about 16% (switching both off changes the logits by that much, but not the chosen tokens); the embeddings decide the token. So **this tests the pipeline, not the model**: whether the compiler, the scales, the cache, the instructions and the chip compute what the graph says. A second, generic model with random weights (below) measures how faithful the int8 chip is when the answer is not designed.

## Running it

```python
--8<-- "tools/ch12_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020)**

```text
--8<-- "out/ch12_run_out.txt"
```

### Reading it

- **Three ways to the same 24 tokens.** The formula, the floating-point model (written without the graph IR) and the compiled programs on the reference simulator all produce `0, 3, 2, 13, 4, 7, 6, 1, 8, 11, 10, 5, 12, 15, 14, 9, 0, 3, ...`: the cycle of the permutation, twice around.
- **The 24 programs run on the RTL**, in both simulators, with exactly the memory contents and cycle counters of the reference. The chip decodes the tokens the host chose.
- **The cost of a token.** A step takes 7,000 cycles with a one-token context and 9,039 with 24. Most of the cost does **not** depend on the context: 7,000 of 9,039 cycles are the projections and feed-forward matrices, whose weights (2,304 words in all) are re-read from external memory on every token. Only about 2,000 cycles are the cache and the attention. This is Chapter 9's lesson in a running model: at batch 1 a decode step is dominated by reading the weights again.
- **The margin is wide in this model**, which is why it works: the best logit exceeds the second best by 81% or more of the full logit range, against quantization noise of 1-3%.
- **A generic model.** With random weights and random embeddings (so there is no designed answer), the chip picks the same token as floating point on **559 of 576 decisions (97.0%)** when both are given the same tokens, with a relative logit error of 2.3% on average and 6.1% at worst. Roughly 3% of decisions flip where two logits are within the noise. That is the honest accuracy of per-tensor int8 on this architecture and these data; a trained model's tolerance would have to be measured.
- **The clock-rate caveat.** At the toy-library timing of Chapter 10 (93 MHz) the average cycle count would be about 11,600 tokens per second. That number is the arithmetic of two toy quantities (a made-up library and no memory timing), and should not be quoted as speed. It is here to say that the whole stack, from a graph to cycles, is connected.

## The capstone as a test of the hardware

Chapter 7's 78 purpose-built programs caught all 49 hardware mutants. How many does the capstone catch? The 24 decode steps (programs from the compiler) were replayed on each broken chip, for the designed model and for a random-weights model:

```python
--8<-- "tools/mut_ch12.py"
```

```text
--8<-- "out/ch12_mutation_out.txt"
```

**A realistic workload catches 38-39 of 49, not 49.** Both models miss the same ten, and they have a pattern: they are **features the compiler never emits**.

- `MM` with `lda` and `ldc` different from `K` and `N`: the compiler always uses dense matrices, so "A's row step uses K instead of lda" is not a bug *for programs the compiler writes*. The same holds for the field-width mutants `K is a 6-bit field` (no product has `K = 64`) and `M is a 2-bit field` (a decode step has `M = 1`).
- `VADD` clamps (three mutants) and `VADD`'s 8-bit length: the residual sums here never reach ±127, and the vectors are 16 words long.
- Two `SM` mutants: here every softmax input has a non-negative maximum, so a maximum that starts at 0 is no different, and writing the output in every pass is harmless when destination and source differ.

None of this makes the capstone a poor *workload*; it makes it a poor *test of the chip*. A compiler uses a subset of an ISA, and a workload exercises the subset the compiler uses. Chapter 7's directed and random programs exist to cover the whole ISA, including the instructions and operand ranges no compiler may ever produce, and **are the right test of the hardware**; the capstone is the right test of the *stack*. The first number to quote about a chip is how it does on the suite designed to break it.

## What the whole book established, and did not

**Established, with the tests that show it:** each circuit (multiplier, MAC, requantizer, systolic array, DMA and double buffering, divider, softmax) matches an independent Python model, bit for bit or cycle for cycle; the instruction set and chip match a reference simulator on 344 programs and, after synthesis, on the gate-level netlist; a compiler turns a graph into programs that match an integer interpreter and the real-number meaning of each tensor; and a tiny transformer decodes on the chip as floating point does. Each claim was put through mutation or fault tests, and each chapter reports what its tests found and which survivors are equivalent.

**Not established:** anything about a real process (area, speed, power: the library and timing are toys), real memory systems (one fixed-latency model), training or accuracy of trained models, models larger than a few thousand words per tensor, floating-point formats, multiple chips, and a complete operation set (no layer normalization, GELU, rotary embeddings, grouped-query attention). A chip is a model here, not a chip.

## Where to go from here

- **The ISA first.** Add an `MM` accumulate flag so `K > 64` compiles; add a fused gather so the host is not needed for embeddings; add a layer-norm unit (a reciprocal square root is the divider of Chapter 6 with a table).
- **The memory system.** Replace the one-instruction-at-a-time machine with the double buffering of Chapter 5: the DMA queue and the roofline analysis of Chapter 9 say that is where the next factor of 1.5 lives.
- **The tools.** Run the same RTL through a place-and-route flow with a real open PDK (OpenROAD with SkyWater's 130 nm library is the natural next step), where the toy numbers of Chapter 10 become real ones.

## Self-check questions

1. The model's next token is `f(t) = (5t + 3) mod 16` although attention and the feed-forward block are running. Why does the output not depend on them, and what does that imply for what this test can show?
2. A decode step costs 7,000 cycles at context 1 and 9,039 at context 24. What does the 7,000 consist of, and what would batching change?
3. The chip agrees with floating point on 97% of the random model's decisions. Why is the agreement not 100%, and what would you measure on a trained model?
4. The capstone catches 38-39 of the 49 hardware mutants and Chapter 7's suite catches all of them. Name two mutants the capstone misses and say why they are not bugs for programs the compiler writes.
5. Which tests in this book would you rerun first after changing the compiler's tiling, and which after changing the requantizer's RTL?
