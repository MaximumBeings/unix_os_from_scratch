# 2. The MAC and Number Formats: Two's Complement, Overflow, and the Tests That Found Three Holes

![ch-02](../assets/art/ch-02.svg)

--8<-- "docs/assets/art/ch-02.md"


**What you will understand:** how an 8-bit signed number is stored, why the multiply-accumulate (MAC) unit is the atom of every matrix engine, what happens when a sum does not fit (wrap or saturate), and what the same multiplier costs when it is written two different ways. You will also see a test suite that *looked* complete have three holes, found by breaking the circuit on purpose.

**What you need to know first:** Chapter 1 (the flow: golden model, two simulators, Yosys, mutation).

## Two's complement in one paragraph

An 8-bit signed integer has 256 values, so the range is -128 to +127: one more negative value than positive. The top bit has weight -128 instead of +128; all other bits keep their usual weights. So `1000_0000` is -128, `1111_1111` is -1, `0111_1111` is +127. The asymmetry matters twice in this book: the product -128 x -128 = +16384 is the *largest* product magnitude (127 x 127 = 16129 is smaller), and Chapter 3 will choose the quantized range -127..127 on purpose, to keep the format symmetric and avoid ever producing -128.

An 8 x 8 signed product fits in 16 bits. Summing k products needs about 16 + log2(k) bits, so a 32-bit accumulator holds 2^16 worst-case products before it can overflow. That sounds like a lot; the tests below overflow it deliberately, because "rare" is not "impossible" and the hardware has to do *something* defined.

## Two multipliers: `rtl/mul8.v`

```verilog
--8<-- "rtl/mul8.v"
```

`mul8_beh` writes `a * b` and lets the synthesis tool choose the structure. `mul8_sa` builds it the way you would on paper: eight shifted copies of `a`, one per bit of `b`, added together, with the row for the sign bit of `b` *subtracted* (that bit has weight -128). Getting that one sign wrong is the classic mistake, and it is one of the mutants below.

## The MAC: `rtl/mac.v`

```verilog
--8<-- "rtl/mac.v"
```

Each clock with `en = 1` the accumulator gets `acc + a*b`. The sum is computed in **33 bits**, so the overflow test can look at the true result before it is cut down to 32. Parameter `SAT` selects the policy: `0` wraps (what plain 32-bit adders do), `1` clamps at the 32-bit limits. `clr` starts a new sum (the accumulator becomes the first product, or zero if `en` is low), and `rst` clears everything.

## The answer keys

```python
--8<-- "model/mul8_gold.py"
```

```python
--8<-- "model/mac_gold.py"
```

The multiplier is checked **exhaustively**: 256 x 256 = 65,536 pairs. The MAC has state, so its key is a *sequence*: each row is a run of cycles with the expected wrapped and saturated accumulator after it. The sequence contains directed corner cases, an initial post-reset check, upward overflow (4 x 32,768 cycles of -128 x -128, i.e. 2^31 total), downward overflow (5 x 32,768 cycles of -128 x 127), and 3,000 random rows. Word 0 of the vector file is the **row count**: Verilator is a two-state simulator and cannot see an "unknown" end marker, which hung the first version of this testbench (the lesson is in the notes below).

## The testbenches

```verilog
--8<-- "tb/mul8_tb.v"
```

```verilog
--8<-- "tb/mac_tb.v"
```

## Running it

```python
--8<-- "tools/ch02_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch02_run_out.txt"
```

### Reading it

- **Both simulators pass** both testbenches: all 65,536 multiplier pairs and all 3,039 MAC checkpoints.
- **The two multipliers cost the same.** 401 vs 398 generic gates; 191 vs 192 iCE40 cells. Writing the shift-and-add by hand bought nothing: the synthesis tool already finds a good structure for `*`. *(Measured by Yosys 0.33 with its default flow; a different tool or a hard multiplier block would change this. That is a result about this flow, not a law.)*
- **The MAC is dominated by the 32 flip-flops and the adder.** 725 gates of which 32 are registers; saturation adds 28 gates (753) and, on the FPGA mapping, 113 cells (322 -> 435) because the comparators and the mux sit on the 33-bit sum.
- **Nothing here is timing.** Gate and cell counts say nothing about speed.

## Testing the tests

```python
--8<-- "tools/mut_ch02.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch02_mutation_out.txt"
```

All 8 multiplier mutants and all 11 MAC mutants are caught. Two survive, and both are **equivalent mutants**: the clamp tests `wide > 2^31-1`; changing it to `>=` makes the circuit clamp when the sum equals 2^31-1 exactly, but clamping that value returns 2^31-1, the same number. The lower clamp is symmetric. Neither is a gap.

### The three holes that the first version had

The tests above are the *second* version. The first one passed all its own checks and still let mutants through, each teaching something:

1. **The downward overflow run was too short.** The first version drove the accumulator down for fewer cycles than it takes to reach -2^31, so the lower clamp was never exercised and "the lower clamp is one too high" survived. The fix is arithmetic, not luck: -128 x 127 = -16256 per cycle needs 2^31 / 16256 ~ 132,100 cycles, so the test runs 5 x 32,768 = 163,840.
2. **No check straight after reset.** "Reset loads 1" survived until a row was added that looks at the accumulator before any `en`.
3. **One mutant was not a mutant.** A "priority" mutant had been written as a rewrite that, on inspection, behaved identically. It was replaced by a true priority swap (`en` beats `clr`), which the tests do catch. Reading a survivor before trying to kill it is the discipline from Chapter 1.

## Two simulators, two kinds of unknown

Icarus is four-state (0, 1, `x`, `z`); Verilator is two-state. A testbench that ends its loop on an `x` sentinel works in Icarus and **hangs in Verilator** (the author watched it use 98% CPU for six minutes before finding out). The book's rule since this chapter: no sentinel values, put the count in word 0. This is also the reason to run every test in both tools: each one hides a different kind of mistake.

## What this chapter does and does not establish

- **The MAC is correct against the golden model** on the vectors used: exhaustive for the multiplier, directed plus 3,000 random sequences for the MAC. Not exhaustive for the MAC (the state space is 2^32 x inputs).
- **The gate counts compare flows, not chips.** "Same cost" is a finding about Yosys's default synthesis.
- **Overflow policy is a decision, not a fact.** Wrapping is cheapest and silently wrong; saturating costs gates and is wrong more gracefully. Chapter 3 will make the larger decision, which is to keep the accumulator wide enough that a real model should never need either.

## Chapter summary

Signed 8-bit numbers range -128..127. A MAC multiplies two of them and adds to a 32-bit accumulator that can overflow after 2^16 worst-case products; the unit can wrap or saturate. The behavioural and hand-built multipliers cost the same in Yosys. The tests were exhaustive for the multiplier, sequence-based for the MAC, and mutation found three holes in the first version.

## Self-check questions

1. Why is the product -128 x -128 the worst case for the accumulator, and how many such products does it take to overflow 32 bits signed?
2. In `mul8_sa`, why must the row for bit 7 of `b` be subtracted?
3. Why does the MAC compute its sum in 33 bits instead of 32?
4. The two multiplier styles cost 401 and 398 gates. What can you conclude, and what can you not?
5. A testbench ends its loop when the expected value is `x`. Why does it work in Icarus and hang in Verilator?
6. The mutant "the upper clamp triggers at the limit itself (`>=`)" survives. Is the test weak?
