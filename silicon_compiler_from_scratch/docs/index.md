# Silicon Compiler from Scratch

![index](assets/art/index.svg)

--8<-- "docs/assets/art/index.md"


**An open-source LLM-inference accelerator, from Verilog to the compiler that targets it.**

This book builds a small neural-network accelerator the way a chip team would, in the order a chip team would, and then builds the compiler that turns a model into programs for it. Everything runs on a laptop with three open-source tools: **Icarus Verilog** and **Verilator** (two independent simulators) and **Yosys** (synthesis). No vendor tools, no FPGA board, no licences.

**What you will build**

| Part | Chapters | What you end up with |
|---|---|---|
| The flow | 1 | One circuit through four tools, and a way to test the tests |
| The arithmetic | 2, 3 | An int8 multiply-accumulate unit, and the quantization arithmetic around it |
| The datapath | 4, 5, 6 | A systolic array, a scratchpad with a DMA engine, and exp, reciprocal and softmax hardware |
| The machine | 7, 8 | An instruction set, a sequencer that runs it, and attention with a KV cache |
| The system | 9, 10 | Batching and a bandwidth (roofline) analysis; synthesis and gate-level simulation |
| The compiler | 11, 12 | A compiler from a model graph to the chip's instructions, and a transformer decode step run end to end on the simulated silicon |

**The rules of the book**

- **Every circuit has a golden model written in Python** that shares no code with the Verilog. A testbench compares the two, and the same testbench runs in two simulators.
- **Every test is tested.** A mutation script breaks the Verilog one line at a time; a test that does not notice has a gap, and the book says what it found.
- **Every number is measured or derived, and says which.** Cycle counts come from simulation, areas from Yosys, and where a figure is an assumption (a clock speed, a memory bandwidth) the page says so.
- **This is a model of a chip, not a chip.** Nothing here has been placed, routed, timed against a real process, taped out or run on silicon. Each chapter says what its numbers can and cannot show.

This book is a companion to *Unix OS from Scratch* and *IR Forge: A Compiler from Scratch*; Chapters 40 to 43 of the compiler book built GA-1, a cycle-counting software model of a matrix accelerator. This book builds a hardware description of one, and the compiler for it.

*Under development.*
