# 4. The Systolic Array: N x N Multiplies Per Cycle, and the Schedule That Makes It Work

**What you will understand:** how a grid of multiply-accumulate units computes a whole matrix product without anyone ever fetching a value twice. You will build an output-stationary systolic array in Verilog (any size N), see the exact cycle on which each product happens, derive its utilization from the schedule, check the circuit against ordinary matrix multiplication for six array shapes in two simulators, and break it sixteen ways to see whether the tests notice.

**What you need to know first:** Chapter 2 (the MAC) and Chapter 3 (int8 and the int32 accumulator).

## The idea

A matrix product `C = A x B` with `A` of size N x K and `B` of size K x N needs `N * N * K` multiply-accumulates. A naive processor fetches an `A` value and a `B` value from memory for every one of them. A **systolic array** (the name is from the heart: data pulses through rhythmically) avoids that:

- There is one **processing element (PE)** per output element: PE(i,j) owns `C[i][j]` and keeps its partial sum in its own accumulator. That is why this layout is called **output-stationary**: the outputs stay still, the inputs move.
- Row `i` of `A` enters PE(i,0) from the left, and every PE hands the `a` value it just used to its right neighbour (through a register). Column `j` of `B` enters PE(0,j) from the top and moves down the same way.
- Each value is read from memory **once** at the edge and then used N times by N different PEs, each one on a different output element.

For PE(i,j) to multiply the right pair, `A[i][k]` and `B[k][j]` must arrive on the same cycle. The trick is to **skew** the inputs: delay row `i` of `A` by `i` cycles and column `j` of `B` by `j` cycles. Then `A[i][k]` reaches PE(i,j) at cycle `k + i + j`, and so does `B[k][j]`. In the circuit below that is nothing but registers; the skew is applied by whoever feeds the edges (in this chapter the testbench, in Chapter 7 the sequencer).

## The circuit: `rtl/systolic.v`

```verilog
--8<-- "rtl/systolic.v"
```

`pe` is a MAC with two pass-through registers. `systolic` is two nested `generate` loops that wire PEs into a grid: `aw[i][j]` is the `a` value entering PE(i,j) from the left (and leaving PE(i,j-1)), `bw[i][j]` the `b` value entering from above. `clr` clears the accumulators **and** the pass-through registers, because values left in the pipeline from the previous product would otherwise be multiplied into the next one. There is no enable: a zero fed to a PE adds zero.

## The answer key and the wavefront: `model/systolic_model.py`

```python
--8<-- "model/systolic_model.py"
```

The key is plain matrix multiplication in exact integers; it shares no code and no structure with the circuit. The file also prints the **wavefront**: which PEs are working in each cycle and on which `A[i][k] * B[k][j]`.

## The testbench: `tb/systolic_tb.v`

```verilog
--8<-- "tb/systolic_tb.v"
```

The testbench applies the skew itself, runs exactly `K + 2N - 2` cycles and then compares all `N * N` accumulators. Reading at exactly that cycle is part of the test: a version of the circuit with an extra pipeline register would have unfinished sums at that point, and a version that finished earlier would be producing wrong sums.

## Running it

```python
--8<-- "tools/ch04_run.py"
```

**Output (cloud sandbox -- live-executed; Icarus Verilog 12.0, Verilator 5.020, Yosys 0.33)**

```text
--8<-- "out/ch04_run_out.txt"
```

### Reading it

- **The wavefront is a diamond.** The array fills diagonally (one PE busy in cycle 0, 8 of 9 in cycles 3-4) and drains the same way. For 3 x 3 PEs and K = 4 that is 36 multiply-accumulates in 8 cycles, utilization 0.5.
- **Utilization = K / (K + 2N - 2).** This is derived from the schedule: each PE does exactly K useful MACs, and the product takes `K + 2N - 2` cycles because the last PE, PE(N-1,N-1), starts only at cycle `2N - 2`. The Python model counts the busy PEs cycle by cycle and agrees: 0.400 for N=4, K=4; 0.914 for N=4, K=64; 0.821 for N=8, K=64. **Small matrices waste an array**, and long inner dimensions amortize the fill and drain. This single formula is why Chapter 9 will batch requests.
- **All six array shapes pass in both simulators**, including non-square products (K different from N, so a transposed or swapped index shows up), K = 1 (almost all pipeline), and N = 8 with K = 33 (47 cycles).
- **Cost is linear in the number of PEs.** About 612 generic gates per PE (39,202 for N=8), 44 flip-flops per PE on average (the 32-bit accumulator plus the two 8-bit pass-through registers, less the unused ones at the edges). The single-PE case (N=1) came out larger per PE (986 gates); the author did not investigate why, and the figure is reported as measured. *(Yosys 0.33 default flow; iCE40 cells are for an FPGA and the figures say nothing about timing or about a real chip's area.)*

## Testing the tests

```python
--8<-- "tools/mut_ch04.py"
```

**Output (cloud sandbox -- live-executed)**

```text
--8<-- "out/ch04_mutation_out.txt"
```

All 16 broken arrays are caught, in both shapes (3 x 3 with K=5, and 4 x 4 with K=7). The list covers the three places an array goes wrong: the arithmetic in the PE (subtraction, unsigned, 16-bit truncation, double counting), the pass-through (a or b not passed, b replaced by a, passed to the wrong neighbour), and the plumbing (rows or columns entering in reverse, the top edge wired to the wrong bus, the result transposed), plus the three `clr` variants (accumulator not cleared, either pass-through register not cleared). The `clr` mutants are the interesting ones: they only fail on the **second** and later products, because stale values are only left behind once a product has run. A test that does one product per power-up would miss all three.

One mutant survives and is an **equivalent mutant**: writing the sum as `a_in * b_in + acc` instead of `acc + a_in * b_in`. Addition commutes; the synthesized circuit is the same.

## What this chapter does and does not establish

- **Correct**: for six shapes (N=2..8, K=1..33) the circuit computes exactly the integer matrix product, on directed corners (zeros, the largest possible sums, an identity, a patterned matrix) and random int8 matrices, back to back with `clr` between products.
- **Not covered**: accumulator overflow (K never reaches 2^16 here), timing, and non-square arrays (N x M). An array that is not square is a different design.
- **The cycle counts are the model's.** The wall-clock time of one product is the cycle count divided by a clock frequency this book never measures.

## Chapter summary

A systolic array puts a MAC at every output element, streams skewed rows of A and columns of B through it, and finishes after `K + 2N - 2` cycles with utilization `K / (K + 2N - 2)`. The Verilog is two small modules; the schedule is the design. The circuit matched the golden model on six shapes in two simulators, and all 16 mutants were caught, including the three `clr` bugs that only show on a second product.

## Self-check questions

1. At which cycle does PE(2,1) multiply `A[2][3]` by `B[3][1]`?
2. Derive the number of cycles for a K-long product on an N x N array, and the utilization for N = 4, K = 4.
3. Why does `clr` also clear the pass-through registers? Which mutants show it?
4. A tester runs one matrix product per simulation, starting from reset. Which of the 16 mutants would it miss?
5. A 128 x 128 array (as in a commercial matrix unit) multiplies matrices with K = 64. What is the utilization, and what does that suggest about small batches?
