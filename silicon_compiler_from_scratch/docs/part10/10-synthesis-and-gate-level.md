# 10. Synthesis, Timing and Gate-Level Verification

**What you will understand:** what happens between "the Verilog passes its tests" and "there is a netlist of gates you could manufacture": **synthesis** to a standard-cell library, **area and timing** read off the result, and the three ways a chip team checks that the netlist still does what the RTL did: **gate-level simulation**, **fault grading**, and **equivalence checking**. Along the way the flow hits a famous trap (X-pessimism) and the tests find a bug in the *testbench*.

**What you need to know first:** Chapters 1-9. Every number below is for a **toy cell library** invented for this book (areas in NAND2 equivalents, one made-up delay per cell). It is a scale for comparing designs and a way to exercise a real flow, not a process: no number here predicts a real chip's area, speed or power.

## The flow

Synthesis turns the RTL into a **netlist**: a graph of library cells and flip-flops.

```text
RTL (Verilog)  --elaborate-->  logic + registers  --optimize-->  --map to cells-->  netlist
                                                                    (Yosys + ABC, toy library)
```

Memories are not synthesized into flip-flops: a chip uses a **memory compiler's macro**. Here the scratchpad and the operand memories stay as black boxes in the netlist and are simulated by their behavioural models.

### The toy library: `tools/gen_toylib.py`

```python
--8<-- "tools/gen_toylib.py"
```

One table generates both the file synthesis reads (`lib/toy.lib`) and the simulation models (`lib/toy_cells.v`), so the two cannot disagree. (Chapter 10's mutation run deliberately breaks that guarantee to see if the tests notice.)

### The flow and a small timing analyzer: `tools/synth.py`

```python
--8<-- "tools/synth.py"
```

`synth` runs Yosys: elaborate, optimize, flatten, legalize the flip-flops, map them and the logic to the toy library, write a Verilog netlist and a JSON netlist. `sta` is a **static timing analyzer** in 60 lines: it adds up cell delays along the longest path between flip-flops (or macros, or ports). It ignores wires, fan-out and clock skew. Is it right? It is checked on a circuit small enough to do by hand:

```text
--8<-- "out/ch10_run_out.txt:17:19"
```

## Results

```python
--8<-- "tools/ch10_run.py"
```

**Output (cloud sandbox -- live-executed; Yosys 0.33, Icarus Verilog 12.0, Verilator 5.020)**

```text
--8<-- "out/ch10_run_out.txt"
```

### Reading it

- **Area.** The whole chip, apart from its memories, is 30,997 cells: 52,334 NAND2 equivalents and 1,899 flip-flops. The biggest pieces are the matrix unit (21,150 GE, containing the 4 x 4 array), the softmax unit (8,065) and the requantizer (7,881, and again inside `RQ`). The array synthesized on its own comes out larger (26,062 GE) than the matrix unit that contains it; the reason was not investigated (plausibly, outputs that the matrix unit never uses are pruned there). The 4,096 x 32-bit scratchpad is not in the count: it is a macro.
- **Timing.** The longest path in the whole chip is 10.74 ns (83 gates deep), about **93 MHz in this toy library**, and it ends at the scratchpad's write data. It begins at the scratchpad's read data: the path **scratchpad read -> requantizer -> scratchpad write**, all in one cycle. The requantizer alone is 79 gates deep (9.87 ns): the 32 x 24-bit multiplier, the rounding adder, the shifter and the clamp are all in one combinational stretch. Every other unit is 2 to 6 ns. **One unit sets the clock for the whole chip.** The fix is the one every real design makes: pipeline the requantizer (a register in the middle of the multiplier) and give it two or three cycles. That would move the critical path to the next-slowest stages (5 to 6 ns in this model, an estimate from the table, not a re-synthesis) at the price of a couple of cycles of latency in `RQ`, which Chapter 7's cycle model would absorb in one constant.
- **Registers.** Only the units with state have flip-flops (the divider's 249 are its numerator, quotient and remainder).

## Gate-level simulation: the same tests on the netlist

The strongest cheap check that synthesis did not change behaviour is to run the **same testbench and the same programs** on the netlist. The gate-level run drives the netlist of toy cells with Chapter 7's 78-program suite and compares memory and all five cycle counters with the Python reference. In Verilator it **passes: the synthesized chip takes exactly the same cycles and produces exactly the same memory** (219,583 cycles). It runs slowly (about two minutes in Verilator for the whole suite, five in Icarus): a 31,000-cell netlist is a lot to evaluate.

### The trap: X-pessimism

In Icarus the same netlist **fails** from the very first program, with the external memory left unchanged. Verilator passes. The cause is not a logic error, and the evidence is that giving the flip-flops a defined power-up value (the `POWERUP_ZERO` option of the cell model) makes Icarus pass the same programs with the same counters.

Icarus is four-state (0, 1, `x`, `z`). In the RTL, registers that are not reset simply hold `x` until the first time they are loaded, and nothing reads them before that. After synthesis, the logic has been rearranged by an optimizer that only had to preserve the *two-valued* function. A rearrangement that is equivalent for 0 and 1 may not be for `x`: for example `(a & b) | (a & ~b)` is just `a`, but with `b = x` the unsimplified form evaluates to `x`. Idle units' unreset address registers therefore leak `x` into the scratchpad's write address. This is **X-pessimism**: the simulator is more pessimistic than silicon, which has a definite 0 or 1 in every flip-flop. Teams respond by resetting every flop (which costs area) or by starting gate-level simulations from a defined state. The book does the second and says so.

That raises a fair question: *does the chip work from any power-up state?* Verilator can start every register at a random value. With three different random seeds the 30-program suite passes each time.

**A bug in the testbench.** The first random power-up run (seed 1) failed. The symptom was a stray word `deadbeef` at the front of the data: the testbench's memory model returns that value for out-of-range addresses, and it had answered a request issued by the chip's random power-up state in the very first clock, *before reset had taken effect*. The answer arrived while the real first load was starting. A real memory system is reset with the chip, so the fix is in the testbench: the memory model now discards every request in flight when `rst` is high (`tb/extmem_rw.v`). The chip was right; the first test had been written for a world in which power-up state is always zero.

## Fault grading: how good are the tests at the gate level?

A manufactured chip can have defects. The simplest model of one is a **stuck-at fault**: one gate output is permanently 0 or 1. A test set is judged by the fraction of faults it **detects**, meaning it makes some output differ from a good chip. This is the gate-level answer to the question mutation testing answered for the RTL.

```python
--8<-- "tools/ch10_faults.py"
```

```verilog
--8<-- "tb/requant_fault_tb.v"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch10_faults_out.txt"
```

The experiment samples 150 of the requantizer's 5,403 gate outputs (300 stuck-at faults) and applies the golden vectors until each fault is detected.

- **Directed and random vectors alone detect 76% of the testable faults**, even with 41,808 vectors: coverage rises quickly at first (39% after 50 vectors) and then crawls (70% after 11,808, 75% after 41,808). The directed tests of Chapter 3 detect 53% by themselves.
- **Faults the vectors missed were not redundant.** An **equivalence checker** (ABC's `cec`, which proves two netlists compute the same function or produces an input on which they differ) was asked, for each of the 74 undetected faults, whether the faulty circuit is equivalent to the good one *with the mantissa normalized* (its top bit held at 1, the book's operating condition). In 3 cases it is: those faults are **redundant** (no input can ever show them, so no test can detect them and they are not defects that matter). For the other 71 it returned a distinguishing input.
- **Those 71 inputs are test vectors.** They are legal operands (e.g. `acc = 997757377`, `m = 12553790`, `s = 48`) with carry patterns in the 56-bit rounding adder that random operands almost never produce. Appending them as golden vectors raises the detection to **297 of 297 testable faults (100%)**. This is **automatic test pattern generation** (ATPG) by equivalence checking: the tool, not the engineer, finds the missing tests.
- **The lesson for a functional test suite.** Thousands of random vectors that pass the golden-model comparison say little about whether *every gate* is exercised. Real chips therefore add **design-for-test** structures (scan chains, which let a tester set and read every flip-flop) and run an ATPG tool. The mutation score of Chapter 3 (15 of 15) and the 76% fault coverage describe the same suite from two sides: it is good at catching mistakes a designer would make and weaker at exercising every gate.

*(Sample size: 150 nets, not all 5,403. The percentages describe that sample.)*

## Testing the tests of the flow

The flow itself can be wrong: the library description that synthesis reads can disagree with the simulation models. The mutation script gives synthesis a wrong function for one cell at a time and checks that gate-level simulation of the requantizer notices.

```python
--8<-- "tools/mut_ch10.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch10_mutation_out.txt"
```

Every wrong function that the mapper actually *used* is caught: XNOR2, NAND2, NOR2, MUX2, AOI21 and AND2 (the mapper used between 126 and 1,298 of each) fail the simulation, and the INV and BUF mistakes break synthesis itself. Five survive and each is an **equivalent mutant of the flow**: the mapper used **zero** XOR2, OAI21, NAND3, NOR3 and OR2 cells for this design, so a wrong description of them cannot change the netlist. (It would for another design. The check is only as broad as the circuit it runs on.)

## What this chapter does and does not establish

- **Established**: GA-2 synthesizes to 30,997 cells plus macros; the netlist runs all 78 programs with identical memory and cycle counters; it also passes from three random power-up states; a wrong cell description is caught whenever the design uses the cell; stuck-at fault coverage of the requantizer sample goes from 76% to 100% with generated patterns.
- **Not established**: anything about a real process. The areas and delays are those of a toy library; there is no place-and-route, wire delay, clock tree, power analysis or manufacturing test flow. The timing analyzer is a model. Formal equivalence was shown on the requantizer's faults, not between the RTL and the netlist of the whole chip.

## Chapter summary

Synthesis maps the RTL onto a cell library; area and a longest-path timing figure follow from the netlist (here 52,334 NAND2 equivalents and a 10.7 ns critical path through the requantizer, which is the unit to pipeline). The netlist passes the same program suite as the RTL, but a four-state simulator needs a defined power-up state to avoid X-pessimism, and random power-up testing exposed a stray-response bug in the testbench. Fault grading shows that a suite that finds every RTL mutant still detects only 76% of gate-level faults, and equivalence checking both classifies the rest and generates the missing tests.

## Self-check questions

1. Why does the toy library come with two files generated from one table?
2. The critical path of the chip is `sp.rdata -> ... -> sp.wdata`. Which unit is on it, and what would you change?
3. What is X-pessimism, and what evidence shows that the Icarus failure was not a logic error?
4. Why did the first random power-up run produce `deadbeef` in the data, and who was at fault?
5. What does it mean that 3 faults were "proven redundant", and why is detecting them not a goal?
