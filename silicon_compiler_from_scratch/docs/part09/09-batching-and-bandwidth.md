# 9. Batching and Bandwidth: Why Decoding Is Slow, and What Helps

![ch-09](../assets/art/ch-09.svg)

--8<-- "docs/assets/art/ch-09.md"


**What you will understand:** the one idea that explains most of the economics of running a language model: **decoding is limited by memory traffic, not by arithmetic.** You will measure it on the chip, build the standard remedy (batching several requests so they share the weights), see exactly how much of the problem it fixes and what it cannot touch, state the limit with a **roofline**, and test the batched program builder with the oracle that batching makes possible: *a request must get the same answer whether or not other requests ride along.*

**What you need to know first:** Chapter 7 (the chip and its counters) and Chapter 8 (the decode step).

## Arithmetic intensity: how much work per byte moved

Every program does some number of multiply-accumulates (MACs) and moves some number of bytes to and from memory. The ratio is its **arithmetic intensity**, in MACs per byte. A chip has two ceilings:

- a **compute ceiling**: the most MACs per cycle it can do (here 16, a 4 x 4 array, at best);
- a **memory ceiling**: bytes per cycle it can move times the program's intensity.

The attainable speed is the *lower* of the two. This is the **roofline model**. The *ridge point* is the intensity at which the two ceilings meet: below it a program is **memory-bound** (faster arithmetic would not help), above it **compute-bound**. For GA-2 the memory moves one word per cycle (one int8 element per word in the design's logical accounting) and the array peaks at 16 MACs per cycle, so the ridge point is 16 MACs per word. A program needs to do 16 MACs for every word it moves just to keep the array busy.

Now look at what a decode step does. Each weight is used **once** per token: a matrix-vector product reads every element of the matrix and uses it in one multiply. Intensity is about 1 MAC per word, 16 times below the ridge. That is true of every chip and every model: for a single sequence, a decode step is memory-bound by a wide margin.

## Batching: reuse the weights

If `B` requests are decoded together, their `B` new tokens can be stacked as the rows of one matrix, and **one** matrix product with `M = B` rows does the projection for all of them. The weights are read once for the whole batch, so intensity rises by up to `B`. On a 4 x 4 array `M = 4` costs almost the same as `M = 1`, so a batch of four is nearly free in the projections.

What batching **cannot** share is attention: each request has its own cache, and its scores and weighted sum read only that cache. Those costs add up per request.

## The batched program builder: `model/batch.py`

```python
--8<-- "model/batch.py"
```

`program(seqs, ts, P)` builds one decode step for a list of sequences, with `ts[i]` tokens each (the lengths may differ: a server's requests arrive at different times). The projections are three loops of four `MM` instructions with `M = n`; the caches are per sequence; attention is a loop over sequences. `check_batching` is the test, explained below.

## Running it on the chip

```python
--8<-- "tools/ch09_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch09_run_out.txt"
```

### Reading it

- **The batched programs run correctly on the chip** (five programs, both simulators, memory and cycle counters exactly as the reference), including a ragged batch where four requests are at tokens 3, 9, 1 and 14 in the same step.
- **Batching helps, by less than you might hope.** Cycles per token fall from 3,827 (batch of 1) to 2,419 (batch of 4), a 1.58x gain, and the MACs per word rise from 0.98 to 1.74. The projections are shared, but the attention work, which does not shrink, becomes the bulk of the step.
- **The words moved per step follow a formula, and the measured programs match it exactly**: `768 + 544 B` (768 words of weights, loaded once; 544 words of cache and input/output per sequence at L = 16): 1,312 for one request, 2,944 for four, as measured. Section 4 extends the formula: with an unlimited batch the intensity tends to 2.35 MACs per word at L = 16, and **to 1 as the context grows**, because each cached key and value is used for exactly one multiply per step. No batch size gets attention across the ridge point.
- **The achieved speed is about a third of the memory ceiling and rises no higher.** `achieved` (MACs per cycle) is 0.33 to 0.53; the `ceiling` is the intensity itself, 0.98 to 1.74. About 30% of it is reached, whatever the batch. The reasons are in Chapter 7: instructions run one at a time (a load never overlaps a compute), and the matrix unit stages operands before streaming them. The last column estimates what overlapping would give if every load ran in the shadow of compute: **1.4x to 1.55x faster**. That is derived from the counters (total divided by the larger of the DMA and non-DMA times), not built, and the gain would be the double buffering of Chapter 5.

## What it means for a real model (derived, with an assumed chip)

The last table is arithmetic, not a measurement, and it assumes a chip with 1 TB/s of memory bandwidth that is limited only by memory traffic. Tokens per second per chip is `B x BW / (weight bytes + B x cache bytes)` for a 7B-class model with an int8 cache of 256 KiB per token and a 4,096-token context (1.07 GB per request):

- At batch 1, the speed is set by the weights alone: 66, 124 and 219 tokens/s for fp16, int8 and int4 weights. **Halving the bytes per weight doubles the speed**, which is why inference chips push to 8 and 4 bits.
- As the batch grows, throughput rises and then **flattens at 931 tokens/s per chip**, whatever the weights: the cache of each request must be read once per token, and that traffic does not amortize.
- The **cache equals the weights in size at a batch of 13 (fp16), 6.5 (int8) and 3.3 (int4)**. Quantizing the weights more aggressively makes batching run out of road sooner. This is why long-context serving is limited by the size and bandwidth of the KV cache, and why techniques that shrink the cache (fewer key-value heads shared by many query heads, a smaller cache dtype, evicting old tokens) matter as much as ones that shrink the weights.

## Testing the tests: batch invariance

A batched program is easy to get subtly wrong: a row index off, a cache in the wrong slot, a length taken from the wrong request. The test that finds all of these has a beautiful form that needs no knowledge of the arithmetic: **a request must produce the same output and the same cache rows whether it is decoded alone or in a batch with others.** `check_batching` runs batches of 1 to 4 and a ragged batch, compares every sequence with the same sequence decoded alone at every step, and also compares each sequence decoded alone with the integer reference.

```python
--8<-- "tools/mut_ch09.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch09_mutation_out.txt"
```

All 16 broken builders are caught, and the second column shows what each part of the test is worth:

- Without the **ragged batch** (sequences of different lengths in one step) three mutants survive: *"attention length of every sequence is the first sequence's"*, *"the new key is appended at the first sequence's position"* and *"a sequence's cache is loaded with the first sequence's length"*. In a batch where every sequence has the same length, the first sequence's length is everybody's length, so these bugs are invisible. Ragged batches are what real servers have.
- An earlier version of the test checked the integer reference only for a batch of one, i.e. only for sequence 0. The mutant *"every sequence reads the hidden state of sequence 0"* then passed in lock-step batches: the batched run and the single run **shared the bug**, so they agreed, and only sequence 0 was compared with the reference. Checking every sequence against the reference closed the hole (it is the version in `model/batch.py` above). Batch invariance is a strong oracle, but like any comparison between two things built from the same code it is blind to a bug that both contain.

## What this chapter does and does not establish

- **Measured on the chip**: batched decode steps for 1 to 4 sequences (L = 16) and a ragged batch, with exact agreement with the reference simulator; cycles per token, words moved, and MACs per word.
- **Derived, not built**: the estimate of 1.4x to 1.55x from overlapping loads with compute; the limit of 2.35 MACs per word (and 1 for long contexts); the tokens-per-second table for a 7B-class model on an assumed 1 TB/s chip.
- **Not covered**: more than four sequences (the scratchpad layout holds four), contexts above 16 tokens in the batched layout, compute-bound phases (prefill, with long matrix products, is the opposite case and sits near the ridge), and real DRAM behaviour.

## Chapter summary

A decode step uses each weight once, so its arithmetic intensity is about 1 MAC per byte against a ridge point of 16: memory-bound, on this chip and any other. Batching stacks requests into one matrix product so the weights are read once, which raised the speed per token by 1.58x at a batch of 4; it cannot touch attention, whose traffic is each request's own KV cache. The cache, not the weights, is what bounds throughput at large batch and long context. A batched program is tested by the invariance of each request to its company, with a ragged batch, plus an exact reference for each sequence alone.

## Self-check questions

1. What is GA-2's ridge point, and which side of it is a decode step on?
2. The words moved per step are `768 + 544 B`. Where do 768 and 544 come from?
3. Why does the batch's MAC-per-word figure approach 2.35 but not 16?
4. Using the table, at what batch size does the KV cache equal the weights for int8 weights, and what does that say about adding more batch beyond it?
5. Why did the first version of the batch test miss "every sequence reads the hidden state of sequence 0", and what fixed it?
