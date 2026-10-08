# 5. The Scratchpad, the DMA Engine, and Double Buffering

![ch-05](../assets/art/ch-05.svg)

--8<-- "docs/assets/art/ch-05.md"


**What you will understand:** why an accelerator has its own small memory instead of a cache, how a DMA engine moves data without the compute units waiting on it, and how **double buffering** hides memory latency behind arithmetic. You will build all three, derive the cycle count of the whole stream as a formula, check the formula against the circuit to the cycle on eight shapes, and see which shapes of test are needed to catch the bugs in the controller.

**What you need to know first:** Chapters 1-4. No new arithmetic: this chapter is about *time*.

## The problem: compute is fast, memory is slow

The systolic array of Chapter 4 consumes new operands every cycle. External memory (DRAM) answers a request only many cycles later (its **latency**), though it can accept a new request every cycle (it is *pipelined*). If the array asks for data and waits, it spends most of its life waiting. The accelerator therefore has:

- a **scratchpad**: a small on-chip memory the *program* manages explicitly (unlike a cache, which guesses). Its read takes one cycle, always.
- a **DMA engine** (direct memory access): a little state machine that copies a block from external memory into the scratchpad on command, so nothing else has to be involved.
- a **controller** that overlaps the two: while the compute unit works on the tile in one half of the scratchpad, the DMA fills the other half with the next tile. That is **double buffering**.

## The scratchpad: `rtl/sram.v`

```verilog
--8<-- "rtl/sram.v"
```

One write port, one read port, synchronous read: the address goes in, the data comes out *next cycle*. If a read and a write hit the same address in the same cycle, the read returns the old value. The controller below is designed so that it never relies on that.

## The DMA engine: `rtl/dma.v`

```verilog
--8<-- "rtl/dma.v"
```

On `start` it remembers a source address, a destination and a length. Every cycle it issues the next request; every answer, which arrives `LAT` cycles later, is written to the scratchpad at the next destination address. When the last answer is written it pulses `done`. Because requests are pipelined, a transfer of `len` words takes about `len + LAT` cycles, not `len * LAT`.

## The controller: `rtl/dbuf.v`

```verilog
--8<-- "rtl/dbuf.v"
```

The controller streams `T` tiles of `TILE` words, adds up each tile (a stand-in for "compute": one word every `CPW` cycles, so `CPW` says how slow the arithmetic is relative to the memory) and reports each sum. Two rules do all the work:

- *A load of tile `n` into bank `n % 2` may start when that bank is empty* (and, in serial mode, when nothing at all is full or computing).
- *A bank becomes empty only when the compute on it has finished*, so a load can never overwrite data that is still being read.

With `dbl = 0` the second clause of the first rule makes it serial: load, compute, load, compute. With `dbl = 1` the load of tile `n+1` runs during the compute of tile `n`.

## The schedule model: `model/dbuf_gold.py`

```python
--8<-- "model/dbuf_gold.py"
```

The answer key has two parts. The sums are exact (mod 2^32; tile 0 is all ones so the sum wraps). The cycle counts come from a **schedule model**, which is derived from the picture of the pipeline:

- loading one tile takes `L = TILE + LAT + L0` cycles (one request per word, then wait for the last answer);
- computing it takes `C = TILE * CPW + C0`;
- serial: `T * (L + C)`;
- double-buffered: `L + (T - 1) * max(L, C) + C`. The first load cannot be hidden, the last compute cannot be hidden, and in between **the slower of the two sets the pace**.

*How much of this is derived and how much measured?* The two formulas are derived. The four small constants `L0, C0, S0, D0` are controller handshake overheads, which a diagram cannot give; they were read from the first shape and then fixed. Every other shape is therefore a prediction, and the testbench fails if a prediction is off by even one cycle.

## The testbench and the run

```verilog
--8<-- "tb/extmem.v"
```

```verilog
--8<-- "tb/dbuf_tb.v"
```

`extmem` models DRAM: a request is accepted every cycle and the answer appears `LAT` cycles later. The testbench runs the same stream twice, serial and double-buffered, and checks four things: every tile sum, the number of sums, the number of cycles against the schedule model, and the number of words requested from external memory (exactly `T * TILE`).

```python
--8<-- "tools/ch05_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch05_run_out.txt"
```

### Reading it

- **All eight shapes pass in both simulators, to the cycle.** They include a load-bound stream (memory slower than compute), compute-bound ones, a latency of 1 and of 30, a single tile (where double buffering cannot help: 34 cycles either way, as the model says) and a wrapped sum.
- **Double buffering helps most when load and compute are balanced.** The sweep (measured in simulation) goes from 1.74x faster at `CPW=1`, where the two times are close (L=23, C=19), down to 1.08x at `CPW=16`, where the compute is eleven times the load and there is little to hide. The best possible gain is 2x: when `L = C`, the total halves. Beyond that, the stream runs at the speed of the slower side, and no amount of overlap makes the slower side faster.
- **The scratchpad costs memory, not logic.** In the generic gate count the 256 x 32-bit memory becomes thousands of flip-flop-equivalent gates (17,750 for the whole block); mapped to an FPGA, the same memory becomes **2 block RAMs** and the logic is only 762 cells. A real chip would use a memory compiler's SRAM macro. The DMA engine on its own is small (227 gates), as is the whole control. *(Measured by Yosys; area and timing of a real SRAM are not measured here.)*

## Testing the tests

```python
--8<-- "tools/mut_ch05.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch05_mutation_out.txt"
```

All 20 broken circuits are caught, but the table has a lesson in its right-hand column. Two mutants are caught by **only one of the two test shapes**:

- *"A load may overwrite a bank that is still full"* is not caught by the load-bound shape (16 words, `CPW=1`): there the load is the slow side, so by the time the controller wants to load into a bank, compute has long emptied it, and the missing guard is never needed. (This is reasoning from the schedule, which is consistent with the result, rather than something separately measured.) It is caught by the compute-bound shape, where the DMA is ready long before the bank is free and the missing guard lets it overwrite data still being read.
- *"A word is read in the first cycle of its slot"* changes nothing when `CPW = 1`: the first and last cycle of a one-cycle slot are the same cycle. At `CPW=4` they are not, and the timing check sees it.

A test suite that used only the first shape would have passed both bugs. **The shape of the test has to cover the regime in which each rule matters.** That is why this chapter's testbench is run for eight shapes and the mutation run uses one load-bound and one compute-bound shape. Nothing survived, and no equivalent mutants were found in this circuit.

## What this chapter does and does not establish

- **Correct**: the sums for all eight shapes, and the controller's rules, as far as the 20 mutants probe them.
- **A model, not a memory system.** The external memory has a fixed latency and unlimited bandwidth for one word per cycle; real DRAM has banks, row buffers, refresh, and contention. The conclusions about *overlap* hold; the cycle counts do not carry over.
- **Only one tile in flight per bank.** More general schemes (three buffers, strided or 2-D transfers) are not covered.

## Chapter summary

A scratchpad is a software-managed on-chip memory; a DMA engine fills it without the compute units waiting; double buffering overlaps the next load with the current compute. The stream's time is `L + (T-1) * max(L, C) + C`, so overlapping can at best halve the time and only when load and compute are balanced. The circuit matched this to the cycle on eight shapes, and two controller bugs were visible to only one of the two kinds of test shape.

## Self-check questions

1. For `TILE = 16`, `LAT = 4`, `CPW = 2` the model gives L = 23 and C = 35. Compute the serial and double-buffered cycles for T = 6 tiles (with S0 = D0 = 0), and check them against the run.
2. Why can double buffering never give a speedup of more than 2x?
3. Why must a bank be marked empty when the compute *finishes* and not when it starts?
4. Which of the two bugs that only one shape caught would also be missed by a testbench that runs only the serial mode?
5. A real DRAM has a latency of about 100 cycles and returns 64 bytes per request. What would you change in the DMA engine so that a tile of 1 KB is loaded efficiently?
