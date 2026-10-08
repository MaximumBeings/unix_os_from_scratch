# 7. GA-2: An Instruction Set, a Sequencer, and the Whole Chip

**What you will understand:** how the pieces of Chapters 3-6 become one machine that runs *programs*. You will meet the instruction set (eight instructions, 128 bits each), write an assembler for it, build the execution units and the sequencer that drives them, check the whole chip against a Python reference simulator on 78 programs (memory contents **and** cycle counts, to the cycle), break it 49 ways, and learn what a test generator must never produce.

**What you need to know first:** Chapters 1-6. This chapter is mostly wiring and control, so the arithmetic is already trusted: the matrix unit *is* the Chapter 4 array, the requantizer *is* Chapter 3's circuit, the softmax *is* Chapter 6's algorithm.

## The machine

```text
                    program ROM  (128-bit instructions)
                         |
                  +------+-------+            external memory (DRAM model)
                  |  sequencer   |            read port: latency LAT, pipelined
                  | fetch/issue/ |<---------- write port
                  |    wait      |                   ^   |
                  +--+--+--+--+--+                   |   |
          one unit runs at a time                    |   v
   +-----+-----+------+------+------+------+---------+---------+
   | LD  | ST  |  MM  |  RQ  |  SM  | VADD |  AMAX             |
   | DMA | out | 4x4  |requan| soft | sat. | arg-              |
   |     |     |array |tize  | max  | add  | max               |
   +--+--+--+--+--+---+--+---+--+---+--+---+--+----------------+
      |     |     |      |      |      |      |
      +-----+-----+------+------+------+------+----->  scratchpad: 4096 x 32-bit words
                                                       (1 read port, 1 write port, 1-cycle read)
```

The design is deliberately **simple**: instructions run one at a time, in order, never overlapped; every value occupies one 32-bit word (an int8 is stored sign-extended). That makes the cycle count of a program the sum of the cycle counts of its instructions, which is what lets this chapter *predict* it exactly. Chapter 9 asks what overlap would buy.

## The instruction set

```python
--8<-- "model/ga2_isa.py"
```

The top of that file is the specification; the rest is the assembler, a reference simulator, and the cycle model. Eight instructions:

| op | name | does |
|---|---|---|
| 0 | `HALT` | stop (an all-zero word is a HALT) |
| 1 | `LD dst src len` | external memory -> scratchpad (the DMA of Chapter 5) |
| 2 | `ST src dst len` | scratchpad -> external memory |
| 3 | `MM dst A B M K N tb lda ldb ldc` | `C = A x B`, int8 in, int32 out; M, N <= 4, K <= 64; `tb` reads B transposed; the `ld*` are row strides |
| 4 | `RQ dst src len m s relu` | requantize int32 -> int8 (Chapter 3) |
| 5 | `SM dst src len` | softmax of int8 scores -> Q0.16 probabilities (Chapter 6) |
| 6 | `VADD dst src1 src2 len` | saturating int8 add (residual connections) |
| 7 | `AMAX dst src len` | index of the first maximum (greedy decoding picks the next token with it) |

Two design choices to notice. `MM` has **strides** (`lda`, `ldb`, `ldc`) so that one instruction can multiply a tile of a bigger matrix without copying it out: Chapter 8's attention reads the keys of the cache straight from where they sit. And `MM` is limited to a 4 x 4 output tile; it is the compiler's job (Chapter 11) to cut a big product into such tiles. Hardware that does less makes the compiler's job real, which is the point of this book.

An example, in the assembler's syntax (comments after `;`):

```text
--8<-- "out/ch07_run_out.txt:2:9"
```

## The execution units

Each unit takes the 128-bit instruction when `start` pulses, owns the scratchpad ports while it is busy, and pulses `done` after its last write. Because the scratchpad read is synchronous (address now, data next cycle), every unit is a small pipeline: issue read `i` in one cycle, process word `i` in the next.

### Store, requantize, add, argmax: `rtl/ga2_vec.v`

```verilog
--8<-- "rtl/ga2_vec.v"
```

`ga2_rq` instantiates Chapter 3's `requant` combinationally between the read and the write. `ga2_vadd` has only one read port, so it takes two cycles per element (read one operand, read the other) and clamps to -127..127 as Chapter 3 requires. `ga2_amax` keeps the best value so far with a *strict* greater-than, so ties give the first index.

### Softmax: `rtl/ga2_sm.v`

```verilog
--8<-- "rtl/ga2_sm.v"
```

This is Chapter 6's algorithm, rebuilt around the scratchpad: pass 1 finds the maximum, pass 2 writes the exponentials *into the destination* and sums them, one division, pass 3 rescales them in place. Because it reads and writes memory it handles any length, not only the 8 or 64 of Chapter 6 (the suite includes a 600-element softmax).

### The matrix unit: `rtl/rowmem.v`, `rtl/ga2_mm.v`

```verilog
--8<-- "rtl/rowmem.v"
```

```verilog
--8<-- "rtl/ga2_mm.v"
```

The array needs `N` operands of `A` and `N` of `B` every cycle, but the scratchpad delivers one word per cycle. So the unit first **stages** the tiles: A's row `m` into operand memory `m`, B's column `n` into operand memory `n` (that is what `rowmem` is: eight small memories, each with one read address). Then it streams them skewed (memory `i` is read at position `t - i`, exactly the schedule of Chapter 4) through the 4 x 4 array, and finally writes the `M x N` result back, one word per cycle. Its time is `M*K + K*N` (staging) `+ (K+M+N-2)` (streaming) `+ M*N` (writing) plus a few cycles of control. **Most of it is staging**, not arithmetic: a point Chapter 9 returns to.

### The sequencer: `rtl/ga2.v`

```verilog
--8<-- "rtl/ga2.v"
```

Three states do the work: FETCH (read the instruction at `pc`), ISSUE (start the unit named by the opcode), WAIT (until that unit says `done`). The unit named by the instruction in flight owns the scratchpad ports, selected by `cur`. The five counters are the profile Chapter 9 will use.

## The reference simulator and the cycle model

`model/ga2_isa.py` (above) contains `Machine`, a Python simulator of the ISA that shares no code with the Verilog, and `wait_cycles`, the cycle model: each instruction takes `2` cycles of fetch and issue plus its unit's busy time, a formula in the operands:

| instruction | busy cycles |
|---|---|
| `LD` | `len + LAT + c` |
| `ST` | `len + c` |
| `MM` | `M*K + K*N + (K+M+N-2) + M*N + c` |
| `RQ` | `len + c` |
| `SM` | `3*len + 41 + c` |
| `VADD` | `2*len + c` |
| `AMAX` | `len + c` |

The **shape** of each formula is derived from the structure of the unit (three passes and a 40-cycle division for softmax; two reads per element for the add...). The **constants** `c` are control overheads that a diagram cannot give; they were read off the circuit by running each instruction alone with several sizes and subtracting the operand-dependent part:

```python
--8<-- "tools/ch07_calibrate.py"
```

```text
--8<-- "out/ch07_calibrate_out.txt"
```

If the structural part were wrong, the "left over" column would change with the size. It does not: one constant per instruction type (`LD` 1, `ST` 2, `RQ` 2, `SM` 6, `VADD` 2, `AMAX` 3, `MM` 7). **Those are the only measured numbers in the model**; everything the next sections check is a prediction.

## The test programs: `model/ga2_progs.py`

```python
--8<-- "model/ga2_progs.py"
```

Eighteen directed programs (every instruction alone, length-one operands, the largest tile, transposed `B`, vectors of 500 to 1500 elements to exercise the upper bits of every length field, an immediate HALT) and a generator of **random programs**: each loads four arenas of data, then 10-25 random instructions with random sizes, strides and addresses, then stores the results. Everything is checked: the whole external memory afterwards and all five cycle counters.

```verilog
--8<-- "tb/ga2_tb.v"
```

```verilog
--8<-- "tb/extmem_rw.v"
```

## Running it

```python
--8<-- "tools/ch07_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch07_run_out.txt"
```

### Reading it

- **The example program runs.** Two matrices are loaded, multiplied (`C = [[4,5],[10,11]]`), requantized, the largest result's index found (3), and all stored: 93 cycles in total, split into 28 on the matrix unit, 36 on DMA (loading 12 words costs 21 of them: the memory latency is paid once per load), and 13 on the vector units. The circuit and the reference agree, and the cycle counters match the model's prediction to the cycle.
- **78 programs, then 200 more, pass in both simulators, memory and counters.** That is 710,814 cycles of random programs checked against the reference.
- **The same suite passes at a different memory latency** after changing a single constant in the model (`LAT`).
- **Cost (Yosys, iCE40 mapping):** the whole chip is 21,266 cells with 5,063 flip-flops and 34 block RAMs (32 of them are the 4096 x 32-bit scratchpad). The requantizer is the largest unit by far (5,832 generic gates, mostly the 32 x 24-bit multiplier) and the softmax the second (4,533). The matrix unit's 23,330 gates are mostly the systolic array and the operand memories (3,957 flip-flops, because Yosys mapped most of them to registers). *(Yosys counts; no timing, no area.)*

## Testing the tests

```python
--8<-- "tools/mut_ch07.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch07_mutation_out.txt"
```

All 49 broken chips are caught, among them the bugs this kind of design actually has: a field read from the wrong bits, a stride swapped for a size, a unit on the wrong bus, an off-by-one in the streaming time, a missing clear between products, a transposed store, ties resolved the wrong way, a clamp one off.

Four survivors, each **equivalent** and each of a different kind:

- *VADD reads its two sources in the other order*: addition commutes. A true equivalent.
- *Rows of A beyond M are not masked*: PE(i,j) depends only on row i of A and column j of B, so the extra rows change nothing observable (they would cost power).
- *M is a 3-bit field instead of 4*: every legal value (1 to 4) fits in 3 bits. **Equivalent within the ISA**, not in general.
- *The load source address is 12 bits instead of 16*: the test memory has 2048 words, so no test can reach an address that needs the upper bits. **Equivalent within the tested range.** A bigger memory would remove this survivor, and the author reports it rather than calling it harmless.

### What the process found

1. **The first run disagreed with the model on 71 of 72 programs, by a few cycles each.** Not a bug: the constants had been guessed before they were measured. Calibrating them (the table above) fixed all of those at once. The memory contents had been right from the first run.
2. **One program differed in memory in both simulators.** The cause was in the *generator*: it produced `SM dst=2184 src=2174 len=64`, source and destination overlapping partially. The units are pipelined, so this is undefined, and the reference simulator (which reads everything before writing) and the circuit (which writes while still reading) give different answers. The ISA now says "operand regions of RQ, SM and VADD must be identical or disjoint", and the generator produces only those. The lesson is general: **undefined behaviour has to be written into the specification and kept out of the tests**, otherwise a test failure cannot tell you which side is wrong.
3. **One mutant was badly designed by the author** ("M is a 3-bit field") and survived because it was not a bug. It was reclassified as equivalent and replaced by a real one (a 2-bit field), which is caught.
4. **Before adding the 500-1500-element programs** the length-field mutants (8-bit lengths) would have survived, since the random programs only reach length 255. They are the reason the directed suite has long vectors.

## What this chapter does and does not establish

- **Verified**: the chip computes what the reference simulator computes, and takes exactly the cycles the model says, on 78 + 218 + 48 program runs, in two simulators (the later two in Icarus only). The reference simulator is itself checked only through this comparison and through the unit-level golden models of Chapters 3-6.
- **Not verified**: timing (no clock period is measured), instruction sequences longer than 255, behaviour with a different clock for memory, and anything the generator cannot produce (for instance, `MM` with `K` above 64 is illegal and is not tested).
- **Simplifications that matter later**: one element per 32-bit word (so the scratchpad is 4x bigger than the data would need), strictly serial instructions (no overlap of a load with a compute: the double buffering of Chapter 5 is not used by this design), and one set of operands per instruction.

## Chapter summary

GA-2 is eight instructions executed one at a time by a sequencer that fetches, starts the unit the instruction names, and waits. The matrix unit stages two tiles, streams them through Chapter 4's array and writes the result; the vector units are small pipelines around Chapter 3's requantizer and Chapter 6's softmax. A Python reference predicts the whole external memory and every cycle count; the chip matches on 344 program runs; 49 of 49 mutants were caught and the four survivors are explained.

## Self-check questions

1. `MM dst=100 A=0 B=64 M=2 K=5 N=3 tb=1 lda=5 ldb=5 ldc=3`: where in the scratchpad is `B[k][n]` and where is `C[m][n]`?
2. How many busy cycles does the model give for that instruction? Which part is arithmetic and which is data movement?
3. Why does the ISA forbid partially overlapping operand regions for `SM`?
4. A mutant changes the load's source-address field from 16 bits to 12. Why did no test catch it, and what would catch it?
5. Why is the cycle model's constant for `MM` (7) bigger than the ones for the vector units (2 to 6)? Name two things in `ga2_mm.v` that it contains.
