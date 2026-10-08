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
