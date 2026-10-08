# Answers to the Self-Check Questions

Worked answers, chapter by chapter.

---

## Chapter 1: The Flow

**1. Why does the testbench compare with `!==` and not `!=`, and what would a plain `!=` let through?**

Worked answer: `!=` returns *unknown* (`x`) when either side contains an `x` or `z` bit, and an `if` on an unknown condition is treated as false, so the mismatch branch is silently skipped. A wire that nothing drives (for instance the output of a stage that was never built) is `x`, so with `!=` a broken circuit whose outputs are `x` would be reported as correct. `!==` compares the four-state values literally, so `x` never equals a known bit and the mismatch is counted.

**2. The golden model enumerates all 512 inputs. Roughly how many inputs does a circuit with two 32-bit operands have, and what does that do to the idea of "exhaustive"?**

Worked answer: 2^64 (about 1.8 x 10^19) operand pairs, times 2 for a carry-in. At a billion tests per second that is about 600 years, so exhaustive testing is impossible. Testing then relies on directed corner cases, random inputs, structural arguments (a 32-bit adder is built from the same stage repeated, so testing small widths exhaustively says a lot) and, in this book, mutation counts that show how much a pass is worth.

**3. Yosys reports 20 gates for the adder and 9 lookup tables for the same design. Why is neither number "wrong", and which would you quote for a chip?**

Worked answer: they count different things. A standard-cell chip is built from fixed small gates, so a gate count (and, with a library, an area) is the relevant figure; an FPGA is built from programmable lookup tables, so a LUT count is. The same logic packs differently into each. For a chip you would quote the gates or cell area after mapping to the process's real cell library, which this book does not have, so it quotes the generic-gate count and labels it as such.

**4. A testbench that tried only `cin = 0` would miss exactly one of the ten mutants. Which one, and why does that make exhaustiveness valuable?**

Worked answer: the mutant that ties the carry-in to zero (`assign c[0] = 1'b0;`). With `cin` always 0 the tied-off circuit behaves identically, so a test that never sets `cin` to 1 cannot tell them apart; the other nine mutants change behaviour even with `cin = 0`. This is the typical way a weak test fails: not by missing the big mistakes but by never exercising the input that distinguishes one particular mistake. Exhaustive testing leaves no such input unexercised.

**5. A mutant survives. What is the first question to ask, and what are the two possible answers?**

Worked answer: "Is this mutant equivalent to the original?", that is, does it behave identically for every possible input? If it does (like writing `a | b` for `a ^ b` in the carry's propagate term), the survivor is harmless and no test could ever catch it; it should be reported and set aside. If it does not, there is an input on which the circuit differs and the test never tried it: a gap in the test, which the fix is to close by adding that input.

---

## Chapter 2

**1. Why is -128 x -128 the worst case for the accumulator, and how many such products does it take to overflow 32 bits signed?**

Worked answer: the product is +16384 = 2^14, larger in magnitude than any other int8 product (127 x 127 = 16129; -128 x 127 = -16256). The signed 32-bit maximum is 2^31 - 1, so 2^31 / 2^14 = 131,072 products overflow it: the 131,072th takes the sum to exactly 2^31, which is one past the maximum. The test uses 4 x 32,768 = 131,072 cycles.

**2. In `mul8_sa`, why must the row for bit 7 of `b` be subtracted?**

Worked answer: in two's complement the top bit has weight -128, not +128. The product `a * b` therefore contains `a * (-128)` when that bit is set, so its shifted row of `a` has to be subtracted. Adding it instead gives a result that is wrong by 2 x 128 x a for every negative `b`, which the mutant "the sign row is added instead of subtracted" and the exhaustive test show.

**3. Why does the MAC compute its sum in 33 bits instead of 32?**

Worked answer: the sum of a 32-bit value and a 16-bit value can need 33 bits. Computing in 32 would already have discarded the carry, so the overflow could not be detected (and the saturating version could not know which limit was crossed). The mutant "the sum is only 32 bits wide" is caught by the overflow runs.

**4. The two multiplier styles cost 401 and 398 gates. What can you conclude, and what can you not?**

Worked answer: that for this flow (Yosys 0.33, default synthesis, generic gate library) hand-built shift-and-add buys nothing over writing `*`: the tool already finds a comparable structure. Not concluded: anything about speed (no timing was measured), anything about other tools, or that structure never matters (a hard multiplier block in a real process, or a different architecture such as a Booth multiplier, can change the cost).

**5. A testbench ends its loop when the expected value is `x`. Why does it work in Icarus and hang in Verilator?**

Worked answer: Icarus is four-state, so reading past the end of the vector memory yields `x` and the loop test sees it. Verilator is two-state: the same read yields 0 (a valid value), so the end marker is never seen and the loop never ends. The fix is to put the row count in word 0 and loop to it.

**6. The mutant "the upper clamp triggers at the limit itself" survives. Is the test weak?**

Worked answer: no. At the limit the clamped value and the unclamped value are the same number, so the two circuits agree on every input: the mutant is equivalent and no test could separate them. It is reported and set aside, as opposed to the three real gaps in the first version, where an input existed that distinguished the mutant and the test had not tried it.

---

## Chapter 3

**1. Why does the scheme avoid -128, and what would break if it were allowed?**

Worked answer: the range -127..127 is symmetric about zero, so negating any value is again in range and `requant(-x) = -requant(x)` holds. With -128 allowed, `-(-128)` = 128 does not fit in int8, the magnitude-based hardware would need an extra case, and -128 x -128 = 16384 would be reachable (it is the worst case for the accumulator in Chapter 2). The price is one unused code out of 256.

**2. The requantizer rounds the magnitude, not the signed value. What would change for an accumulator of -3 with M = 1/2?**

Worked answer: -3 x 1/2 = -1.5, an exact tie. Rounding the magnitude (3/2 = 1.5 rounds away to 2) and restoring the sign gives -2. Rounding the signed value with "add a half then shift" (floor of -1.0) would give -1, so negative ties would round toward +infinity and the result would no longer be symmetric. The mutant "rounding done on the signed value" is caught by the tie cases in the key.

**3. Why does the error not grow with K, and why does an outlier column hurt the other columns?**

Worked answer: each product has a small independent rounding error; the sum of K errors grows like sqrt(K), but so does the signal for random data, so the *relative* error stays near 1-1.5%. An outlier column raises the single per-tensor scale (`max|w|/127`), so ordinary values near 1 map to just a few integer levels and their rounding error becomes large (19.5% measured). Per-column scales give each column its own range (1.3% measured).

**4. The mutant "shift of 0 adds a half" survives. Why is that not a test gap?**

Worked answer: with `s = 0` the expression `1 << (s - 1)` evaluates `1 << 63` in a 56-bit wire, which is zero. The mutant therefore computes the same value as the original for every input; no test can distinguish them. It is an equivalent mutant and shows the special case is redundant (dead logic).

**5. A 7B-parameter model in int8 needs 7 GB of weights. If a chip can read 100 GB/s, what is the upper bound on tokens per second, and what are you assuming?**

Worked answer: 100 GB/s / 7 GB = about 14 tokens per second, assuming every weight is read once per token (batch size 1), nothing is cached on chip, the KV cache and activations are negligible, and the memory system sustains its peak. Batching several sequences reuses each weight read, which is the subject of Chapter 9.

---

## Chapter 4

**1. At which cycle does PE(2,1) multiply `A[2][3]` by `B[3][1]`?**

Worked answer: `A[i][k]` and `B[k][j]` meet at PE(i,j) on cycle `k + i + j` = 3 + 2 + 1 = 6. The wavefront for N=3, K=4 shows PE(2,1) working on `A[2][3]*B[3][1]` in cycle 6.

**2. Derive the number of cycles for a K-long product on an N x N array, and the utilization for N = 4, K = 4.**

Worked answer: the last PE, PE(N-1,N-1), starts at cycle (N-1)+(N-1) = 2N-2 and needs K cycles, so the last multiply is in cycle K+2N-3 and the product takes K+2N-2 cycles. Each of the N*N PEs does K useful MACs, so utilization = N*N*K / ((K+2N-2)*N*N) = K/(K+2N-2). For N=4, K=4 that is 4/10 = 0.4 (the model prints 0.400).

**3. Why does `clr` also clear the pass-through registers? Which mutants show it?**

Worked answer: after a product the last values are still in the registers. If they stay, they are multiplied into the first cycles of the next product. The mutants "clr does not clear the pass-through a register" and "...b register" are caught, but only on the second and later products.

**4. A tester runs one matrix product per simulation, starting from reset. Which of the 16 mutants would it miss?**

Worked answer: the three `clr` mutants (accumulator not cleared, either pass-through register not cleared). Registers start at zero in a two-state simulator such as Verilator, so there is nothing stale to clear and these mutants behave like the original on a first product. (In Icarus the registers start as `x`, so removing the clear would show up as `x` even on a first product, by reasoning rather than by a run; the safe test is the one the book uses, many products back to back.)

**5. A 128 x 128 array multiplies matrices with K = 64. What is the utilization, and what does that suggest about small batches?**

Worked answer: 64 / (64 + 254) = 0.201, about 20%. The array spends most of its time filling and draining. It suggests keeping the array busy by making K long, or by streaming several products back to back (without draining between them), or by batching requests, which is Chapter 9.

---

## Chapter 5

**1. For `TILE = 16`, `LAT = 4`, `CPW = 2` the model gives L = 23 and C = 35. Compute the serial and double-buffered cycles for T = 6 tiles (with S0 = D0 = 0), and check them against the run.**

Worked answer: serial = 6 x (23 + 35) = 348. Double-buffered = 23 + 5 x max(23, 35) + 35 = 23 + 175 + 35 = 233. The run prints exactly `serial=348 double-buffered=233`.

**2. Why can double buffering never give a speedup of more than 2x?**

Worked answer: the serial time is about T(L + C) and the double-buffered time about T x max(L, C) (for large T). The ratio (L + C) / max(L, C) is at most 2, reached when L = C. If one side is ten times the other the ratio is 1.1: there is little to hide. (Three-way overlap of load, compute and store can reach 3x, but that is a different design.)

**3. Why must a bank be marked empty when the compute *finishes* and not when it starts?**

Worked answer: while the compute is running it is still reading words from that bank. If the bank were marked empty at the start, the DMA could begin writing the next tile into it and overwrite words not yet read, so sums would be wrong. The mutant "a load may overwrite a bank that is still full" is the hazard in a simpler form; it is caught by the compute-bound shape.

**4. Which of the two bugs that only one shape caught would also be missed by a testbench that runs only the serial mode?**

Worked answer: the overwrite bug. In serial mode a load starts only when nothing is full and nothing is computing, so the `!full[nb]` guard is redundant there and removing it changes nothing. The bug is visible only in double-buffered mode, and only where the compute is slower than the load. (The "first cycle of the slot" bug shows in serial mode too, as a cycle-count difference, when `CPW > 1`.)

**5. A real DRAM has a latency of about 100 cycles and returns 64 bytes per request. What would you change in the DMA engine so that a tile of 1 KB is loaded efficiently?**

Worked answer: request in bursts of 64 bytes (16 requests for 1 KB, each returning many words) instead of one request per 4-byte word; make the engine accept wide data (64 bytes = 16 words) and write it to the scratchpad with a wide write port or several narrow ones; keep enough requests in flight to cover the 100-cycle latency (bandwidth x latency, the *bandwidth-delay product*); and align tiles to 64-byte boundaries. The engine here issues one single-word request per cycle, which is the right idea at the wrong granularity.

---

## Chapter 6

**1. Why is subtracting the maximum necessary, and what would the table need to hold without it?**

Worked answer: without it the exponent `x/16` can be as large as 127/16 = 7.9, so `exp` reaches about 2,800, and the sum of N such terms needs even more bits; the table would need to hold both large values and small ones (exp of -128/16 = -8 is 3.4e-4), i.e. a wide range at fixed precision. After subtracting the maximum every exponent is in (0, 1], so a 16-bit fraction (65535 = 1.0) is enough, and the largest element contributes exactly 1.0, which keeps the sum at least 65535 (so the division never meets a tiny divisor).

**2. How many cycles does a 128-element softmax take on this unit, and what part of that is the division?**

Worked answer: 3N + 43 = 3 x 128 + 43 = 427 cycles, by extending the formula, which follows from the structure and was measured only up to N = 64 (the run did not try 128). The division is 40 of the 427 cycles, about 9%; for short rows the division dominates (N=8: 40 of 67).

**3. The sum of the probabilities is not exactly 65536. Why, and by how much at most in the study?**

Worked answer: each probability is rounded to an integer (Q0.16), and each table entry was already rounded to 16 bits, so the individual errors do not cancel exactly. The worst deviation of the sum from 1.0 in the study was 3.1e-05 at N=8 and 2.1e-04 at N=64 (about 14 units of 2^-16).

**4. The divider testbench pulses `start` in the middle of a division on odd cases. What bug does that catch, and why did the original testbench not?**

Worked answer: the bug where `start` is not ignored while the unit is busy, so a second start restarts the division with new operands (mutant "a start during a division restarts it"). The original testbench only pulsed `start` while the unit was idle, which is the flow's normal case, so the restart path was never visited; the author confirmed by running the mutant against the old testbench and seeing it pass.

**5. The table is zero from `d = 189`. What does that do to a vector with 200 equal low scores and one high score?**

Worked answer: the high score gets `e = 65535`, and each of the 200 low scores (if more than 11.8 below it) gets 0, so the sum is 65535, `r = 2^38 / 65535`, and the high score's probability is 65536 (1.0) and every other element's is 0. The true softmax would give the 200 together a mass of up to `200 * 7.4e-6 = 1.5e-3`; the unit loses it. That is the intended trade for a 16-bit table, and it only matters if many tiny terms add up to something comparable to the large one. The Chapter 6 study shows the same effect in its largest error (N=64, one big score, 63 small ones).

---

## Chapter 7

**1. `MM dst=100 A=0 B=64 M=2 K=5 N=3 tb=1 lda=5 ldb=5 ldc=3`: where in the scratchpad is `B[k][n]` and where is `C[m][n]`?**

Worked answer: with `tb = 1`, B is stored as N rows of K elements, so `B[k][n]` is at `B + n*ldb + k = 64 + 5n + k`. `C[m][n]` is at `dst + m*ldc + n = 100 + 3m + n`. (`A[m][k]` is at `0 + 5m + k`.)

**2. How many busy cycles does the model give for that instruction? Which part is arithmetic and which is data movement?**

Worked answer: `M*K + K*N + (K+M+N-2) + M*N + 7 = 10 + 15 + 8 + 6 + 7 = 46` (the calibration table shows 46 measured). Only the streaming term (8 cycles, the `K+M+N-2` of Chapter 4) is arithmetic in the array; staging the operands (25 cycles), writing the result (6) and control (7) are data movement and overhead: arithmetic is 8 of 46 cycles, about 17%.

**3. Why does the ISA forbid partially overlapping operand regions for `SM`?**

Worked answer: the unit is pipelined: it writes output `j` while it is still reading later inputs. With identical regions that is safe (output `j` goes where input `j` was, already read) and with disjoint regions it cannot interfere, but with a partial overlap an output can overwrite an input not yet read, so the result depends on the pipeline. The reference simulator reads everything first, so the two disagree. The first random-program generator produced such a case and the test failed because of it, not because of a chip bug.

**4. A mutant changes the load's source-address field from 16 bits to 12. Why did no test catch it, and what would catch it?**

Worked answer: the test's external memory has 2048 words, so every address fits in 12 bits; the upper four bits are always zero in every test. Only a program that addresses external memory above 4095 can tell the two apart, which needs a bigger memory model (and a golden model for it). The mutant is equivalent within the tested range, not in general.

**5. Why is the cycle model's constant for `MM` (7) bigger than the ones for the vector units (2 to 6)? Name two things in `ga2_mm.v` that it contains.**

Worked answer: the matrix unit has more phases, and each phase has a pipeline edge and a state change: after loading A there is a cycle to drain the last read and switch state, the same after loading B, there is the PRE cycle that clears the array, the transition from streaming to storing, and the FIN state that raises done. Any two of these (read off the state machine, not separately measured): the idle cycle between LA and LB, the PRE (clear) cycle, the FIN cycle.

---

## Chapter 8

**1. Why does a decode step need no causal mask?**

Worked answer: the cache holds only the tokens generated so far, including the new one. Attention to "later" tokens is impossible because they do not exist yet. A mask is needed when many positions are computed at once (prefill, training), where the rows for later tokens are present in the same matrices and must be hidden.

**2. A model has 24 layers, 16 heads of width 64 and an fp16 cache. How many bytes per token and for a 2,048-token context?**

Worked answer: per token = 2 (K and V) x 24 x 16 x 64 x 2 bytes = 98,304 bytes (96 KiB). For 2,048 tokens: 98,304 x 2,048 = 201,326,592 bytes = 192 MiB per sequence.

**3. In the cached program at L = 32, 12 of the 24 `MM` instructions do not depend on the context length. Which ones, and what is their total cycle cost?**

Worked answer: the projections of the new token: 4 each for q, k and v (output width 16 in blocks of 4, `M = 1, K = 16, N = 4`). Each costs `16 + 64 + 19 + 4 + 7 = 110` busy cycles plus 2 for fetch and issue = 112, so 12 x 112 = 1,344 cycles, about 26% of the 5,251-cycle step. (The other 12 MMs, the score tiles and the output tiles, do depend on L.)

**4. Why does the exact integer reference miss "q requantization scale is 3% too large" while the stage-wise check catches it?**

Worked answer: the reference calls `rq_params` for its scales, just as the builder does, so a bug in `rq_params` changes both outputs identically and they remain equal. The stage-wise check never calls `rq_params`: it recomputes each stage from the real-valued meaning of the scales (q should equal the product of the quantized inputs times scale_x times scale_w divided by scale_q), so a 3% error moves some values by several levels, and it reports them.

**5. Why can no single-step test catch "the new key is never appended to the cache"?**

Worked answer: the append writes the new key to external memory for use by the *following* step; within the step itself the new key is already in the scratchpad, so the output of this step is the same with or without the append. Only a test that runs a second step reading the cache the first step wrote can see it. That is why `sequence()` runs whole decode loops with the external memory persisting from step to step.
