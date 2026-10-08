# 6. The Vector Unit: A Lookup-Table Exponential, a Divider, and Fixed-Point Softmax

**What you will understand:** the part of a transformer that is *not* a matrix product. Attention needs **softmax**, which needs an exponential and a division, and a chip with only integer multipliers must do both in integers. You will build the three pieces (an exp lookup table, a bit-serial divider, and the softmax unit that uses them), check each one against Python **bit for bit**, measure how far the result is from the real softmax, and meet a test that would have passed a bug until a new check was added.

**What you need to know first:** Chapter 3 (fixed-point numbers, scales) and Chapter 5 (cycle-level thinking).

## Softmax, and why it is awkward in hardware

For scores `x_1 ... x_N`, softmax turns them into probabilities:

```text
p_i = exp(x_i) / sum_j exp(x_j)
```

Three things are hard for a small integer chip: `exp` is not a multiply or add; the exponent of a big score overflows any fixed-width number; and the sum needs a division, which is slow and large in hardware. The standard answers:

1. **Subtract the maximum first.** `softmax(x) = softmax(x - max x)`. Now every exponent is `<= 0`, so every `exp` is in `(0, 1]` and fits a fixed-width fraction. This is the same trick floating-point libraries use to avoid overflow.
2. **Look the exponential up in a table.** The scores are int8 (a real number times 16, i.e. Q4.4), so the distance below the maximum `d = max - x` is an integer 0..255. A 256-entry table gives `exp(-d/16)` in 16 bits. No arithmetic.
3. **Divide once, multiply many times.** Compute one reciprocal `r = 2^38 / sum` with a divider, then `p_i = e_i * r` with a multiplier.

## The table: `tools/gen_exp_lut.py` and `rtl/exp_lut.v`

The table is *generated*, not typed in: entry `d` is `round(65535 * exp(-d/16))`. The generator uses Python's `decimal` module at 50 digits, whereas the golden model uses floating-point `math.exp`, so the circuit's table and the answer key's table come from two different computations and the test fails if they ever disagree.

```python
--8<-- "tools/gen_exp_lut.py"
```

The first lines of the generated Verilog (the file has 256 entries; 67 of them are 0, from `d = 189` on, where `exp(-189/16) = 7.4e-6` is below half a unit of 65535):

```text
module exp_lut(input [7:0] d, output reg [15:0] e);
    always @* begin
        case (d)
        8'd0: e = 16'd65535;
        8'd1: e = 16'd61564;
        8'd2: e = 16'd57834;
        ...
```

## The divider: `rtl/divu.v`

```verilog
--8<-- "rtl/divu.v"
```

**Restoring division**, one quotient bit per clock: shift the remainder left, bring in the next bit of the numerator, and if the divisor fits, subtract it and write a 1. It is small, slow (40 cycles for 40 bits) and exactly what a school long division does in binary. Division by zero is *defined* (quotient all ones, remainder = numerator), because hardware cannot raise an exception and an undefined case is a hole in the test.

## The softmax unit: `rtl/softmax.v`

```verilog
--8<-- "rtl/softmax.v"
```

A state machine in five steps: find the maximum (N cycles), look up and sum the exponentials (N cycles), start the divider and wait (40+ cycles), then scale each element (N cycles). The output is `N` probabilities in Q0.16, where 65536 means 1.0; `(e_i * r + 2^21) >> 22` includes the rounding constant `2^21`, half of the final unit.

*Why `2^38`?* The sum `s` of the table entries is at least 65535 (the maximum element contributes exactly `exp(0)`) and below `64 * 65535` for up to 64 elements, i.e. under `2^22`. Then `r = 2^38 / s` is between 2^16 and 2^22.01 (23 bits), the product `e_i * r` is under 2^39, and `>> 22` leaves a number up to `2^16 = 65536`: 17 bits. The chapter's study prints these ranges.

## The answer keys: `model/softmax_gold.py`

```python
--8<-- "model/softmax_gold.py"
```

Three answer keys in one file: the table (with `math.exp`), the divider (Python's `//` and `%`, plus the defined divide-by-zero), and the whole softmax in plain integers. The divider cases include corners (dividing by one, by itself, zero numerator, all ones, divisors that just fit and just miss) and random operands of every bit width; the softmax cases include constant vectors, extremes, single outliers, and narrow ranges where exponentials differ by little.

## The testbenches

```verilog
--8<-- "tb/exp_lut_tb.v"
```

```verilog
--8<-- "tb/divu_tb.v"
```

```verilog
--8<-- "tb/softmax_tb.v"
```

Two details worth reading. The divider testbench checks the **number of cycles** as well as the answer: done must arrive exactly 40 cycles after the start, whatever the operands (a fixed-latency unit is what lets a controller schedule around it). And on every odd case it pulses `start` again in the middle of the division, with garbage operands; a divider must ignore that. The softmax testbench checks that every input takes the same number of cycles.

## Running it

```python
--8<-- "tools/ch06_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch06_run_out.txt"
```

### Reading it

- **The table matches on all 256 entries and the divider on all 24,157 cases**, in both simulators, each division in exactly 40 cycles.
- **The softmax unit matches the integer model on every case, for six vector lengths** (1, 2, 5, 8, 16, 64), in both simulators.
- **The cycle count is `3N + 43`**, derived from the structure (three passes over `N` elements, the 40-cycle division, three cycles of control) and confirmed by measurement for all six lengths. Softmax over a 64-long row takes 235 cycles on this unit. Compare Chapter 4: a 4 x 4 matrix product of K = 64 takes 70 cycles. *Softmax is not free*, and an unpipelined one like this is slow next to the array; Chapter 8 keeps that in mind.
- **Cost (Yosys, generic gates):** the table 442 gates, the divider 769 (249 flip-flops: it carries the 40-bit numerator, quotient and remainder), the whole unit for N=8 about 5,600 gates and 474 flip-flops. The unit stores the inputs and the exponentials in registers, which is why the flip-flops dominate. *(Gate counts only; no timing, no area.)*

## How close is it to the real softmax?

The integer model is checked bit-exact against the circuit; this section asks how it compares with the **mathematical** softmax computed in floating point on `x/16`. These figures are *measured* (3000 random vectors per row, fixed seeds, `tools/ch06_study.py`).

```python
--8<-- "tools/ch06_study.py"
```

```text
--8<-- "out/ch06_study_out.txt"
```

- **The error is a few units in the last place.** For N=8 the largest probability error in the study is `3.9e-05`, 2.6 units of 2^-16. The sum of the probabilities differs from 1 by at most `3e-05` at N=8 and `2e-04` at N=64.
- **The largest error is the case with many tiny terms** (N=64, one big score, 63 small ones): `1.7e-04`, 11 units. This is consistent with the table's rounding (each small entry is rounded to the nearest 1/65535 and everything beyond d = 188 is dropped to zero, which shifts the sum by up to 63 half-units); the study does not isolate the cause further.
- **The ordering is always preserved.** Whatever the input, the elements with the largest score get the largest probability (the "argmax kept" column is 1.000 in every row), which is what a greedy language-model decoder uses.
- **Anything more than about 11.8 below the maximum gets probability exactly 0.** That is a property of a 16-bit table, and a deliberate trade: those probabilities are below `7.4e-6`.

## Testing the tests

```python
--8<-- "tools/mut_ch06.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch06_mutation_out.txt"
```

All 5 table mutants, 8 divider mutants and 14 softmax mutants are caught (the softmax ones at both N=8 and N=3). One equivalent mutant is reported: the `default` branch of the table's `case` can never run, because all 256 values of the 8-bit `d` are listed.

**One test was added because a mutant got through.** The divider mutant *"a start during a division restarts it"* was not caught by the first version of the divider testbench, which only ever pulsed `start` when the unit was idle. The author checked this by running the mutant against the old testbench, which let it pass. The fix is the garbage `start` in the middle of every odd case, after which the mutant is caught. The general rule: a **handshake** (start, busy, done) has behaviour in the cases the normal flow never visits, and those cases need their own test.

## What this chapter does and does not establish

- **Bit-exact** against the integer model for the table (all 256), the divider (24,157 cases, not exhaustive: the input space is 2^80), and the softmax (907 cases for each of six lengths).
- **Accurate to a few 2^-16** against the real softmax on random integer inputs, as measured here. Not shown: behaviour on the score distributions of a particular trained model.
- **Slow by design.** A real chip would pipeline the stages, use several lookups per cycle, and avoid the divider by using a reciprocal table. This unit is the simplest correct one.

## Chapter summary

Softmax on an integer chip is: subtract the maximum, look the exponential up in a 256-entry table, add the entries, divide once, multiply by the reciprocal. The table, the 40-cycle restoring divider and the 3N+43-cycle softmax unit all match their integer reference exactly, and the fixed-point result is within a few units of 2^-16 of the real softmax. Mutation testing found that the divider's handshake had never been tested against a mid-division `start`.

## Self-check questions

1. Why is subtracting the maximum necessary, and what would the table need to hold without it?
2. How many cycles does a 128-element softmax take on this unit, and what part of that is the division?
3. The sum of the probabilities is not exactly 65536. Why, and by how much at most in the study?
4. The divider testbench pulses `start` in the middle of a division on odd cases. What bug does that catch, and why did the original testbench not?
5. The table is zero from `d = 189`. What does that do to a vector with 200 equal low scores and one high score?
