# 1. The Flow: One Circuit Through Four Tools, and a Way to Test the Tests

**What you will understand:** the loop every chip team lives in, on the smallest circuit that still has all of its parts. You will write a 4-bit adder in Verilog, check it **exhaustively** against an answer key written in Python, run it in **two simulators** that share no code, have **Yosys** turn it into gates and tell you how many, and then **break the circuit on purpose** to find out whether your test would have noticed. Every later chapter is this loop with a bigger circuit.

**What you need to know first:** nothing about hardware. If you can read a few lines of C you can read the Verilog. Install the tools first (see Getting Started).

## Hardware description in two paragraphs

Verilog describes a *circuit*, not a program. `assign sum = a ^ b ^ cin;` is not a statement that runs when control reaches it: it says that the wire `sum` is *always* the XOR of three other wires, and a gate (or several) will exist for it. A **module** is a box with named input and output wires; modules contain wires and other modules, and a design is a tree of them. There is no "calling" a module: you *instantiate* it, which means you build another copy of the box.

Everything happens at once, in the physical circuit. A **simulator** is a program that *imitates* that: it keeps the value of every wire and recomputes the ones whose inputs changed, in small time steps. A **synthesis tool** goes the other way: it reads the description and decides which gates, in which arrangement, would behave the same. A **testbench** is a second module, with no inputs and no outputs, that exists only to wiggle the inputs of the circuit under test and look at what comes out.

## The circuit: `rtl/adder4.v`

A **full adder** adds three bits (two operands and a carry-in) and gives a sum bit and a carry-out. Four of them in a row, each passing its carry to the next, add two 4-bit numbers: a **ripple-carry adder**. It is slow (the carry has to ripple through all four stages) and tiny, which is why it is the first circuit.

```verilog
--8<-- "rtl/adder4.v"
```

## The answer key: `model/adder4_gold.py`

The golden model is written in **another language by design**. If the Verilog and its checker were written by the same hands in the same notation, a misunderstanding would be copied into both and the test would pass. Here the answer key is five lines of Python arithmetic (`a + b + cin`, split into a 4-bit sum and a carry), and it enumerates **every** input: 16 values of `a`, 16 of `b`, 2 of `cin`, 512 cases in all. Nothing is sampled: for a circuit this small, *exhaustive* is cheap, and "the testbench passes" then means "the circuit is correct", not "the circuit is probably correct".

```python
--8<-- "model/adder4_gold.py"
```

## The testbench: `tb/adder4_tb.v`

The testbench reads the 512 expected answers, applies each input, waits one time unit, and compares. Note `!==` rather than `!=`: it also treats an unknown value (`x`, which a simulator reports for a wire nobody drives) as a mismatch. On any difference it prints the first five and calls `$fatal`, so the **exit status** of the simulator says pass or fail, which is what lets a script (or a mutation run) decide automatically.

```verilog
--8<-- "tb/adder4_tb.v"
```

## The toolbox: `tools/hw.py`

Every chapter's test goes through four small functions around the three programs, so that a result always means the same thing. The mutation function used at the end of this chapter is in the same file.

```python
--8<-- "tools/hw.py"
```

## Running the whole flow

```python
--8<-- "tools/ch01_flow.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch01_flow_out.txt"
```

### Reading it

- **Both simulators pass all 512 cases.** Icarus interprets the design; Verilator translates it to C++ and compiles it. They are different programs written by different people, so a bug in one is unlikely to be a bug in the other. Running every testbench in both is the cheapest independent check there is.
- **Yosys, generic gates: 20 cells.** Eight XORs and twelve NANDs: each full adder is two XORs (the sum) and three NANDs (the carry: `(a AND b) OR (cin AND (a XOR b))` rewritten with NANDs), times four. That is the "number of gates" in the sense a textbook means it.
- **Yosys, iCE40: 9 lookup tables.** An FPGA does not have gates; it has small lookup tables (4 inputs, 1 output), and the tool packs the logic into them. Nine tables for the same circuit shows why "how big is it?" has no single answer: it depends on the target. The book uses both counts, labelled.

## Testing the test

A testbench that passes proves nothing about the testbench. The only way to learn whether a test can fail is to give it a circuit that is wrong. `tools/mut_ch01.py` makes ten **mutants**: copies of the adder with one line broken (a term dropped, a wire crossed, a stage missing), and runs the testbench on each. Every mutant must be **caught**, which here means the simulator's exit status is not zero.

```python
--8<-- "tools/mut_ch01.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch01_mutation_out.txt"
```

**All ten broken circuits were caught.** The last line is the other half of the lesson: a mutant can *survive* without the test being at fault. Writing the propagate term as `a | b` instead of `a ^ b` changes the text but not the behaviour (when both inputs are 1 the generate term already forces the carry), so no test, however good, can tell the two apart. Such a survivor is called an **equivalent mutant**. Each later chapter reports its survivors and says, for each, whether it is a gap in the tests (add a test) or an equivalent mutant (say so and move on). Keeping those two apart honestly is most of the discipline.

## What this chapter does and does not establish

- **Exhaustive** means exhaustive: for the adder, the testbench has seen every input. For the multiplier of the next chapter (65,536 pairs) it still can; for the systolic array (inputs of thousands of bits) it cannot, and from Chapter 4 on the book uses random tests, directed corner cases, and the mutation counts to say how much a pass means.
- **Gate counts here are for a generic library** (NAND, XOR) and for an FPGA's LUTs. They are not areas in square microns, and no timing was measured: the real figures need a process design kit and a place-and-route tool, which this book does not use.
- **Two simulators agreeing is strong evidence, not proof.** They agree on what the Verilog *means*; they cannot catch a design that does the wrong thing correctly. That is the golden model's job, and why it is written separately.

## Chapter summary

The flow is: *write the circuit, write an independent answer key, run both in two simulators, synthesize, and break the circuit to see whether the test notices.* The rest of the book is that loop, applied to a multiplier, a systolic array, a memory system, an instruction set and a compiler.

## Self-check questions

1. Why does the testbench compare with `!==` and not `!=`, and what would a plain `!=` let through?
2. The golden model enumerates all 512 inputs. Roughly how many inputs does a circuit with two 32-bit operands have, and what does that do to the idea of "exhaustive"?
3. Yosys reports 20 gates for the adder and 9 lookup tables for the same design. Why is neither number "wrong", and which would you quote for a chip?
4. A testbench that tried only `cin = 0` would miss exactly one of the ten mutants (the author ran it to find out). Which one, and why does that make exhaustiveness valuable?
5. A mutant survives. What is the first question to ask, and what are the two possible answers?
