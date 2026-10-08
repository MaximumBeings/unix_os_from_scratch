#!/usr/bin/env python3
"""Figures for the Background page -> docs/assets/fig/bg-*.svg"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "assets", "fig")
# tool map
f = Fig(920, 470, "The tools and what flows between them"); f.text(460, 22, "The book's toolchain: one description of the circuit, two simulators, one synthesizer, one test-of-the-test", 14, bold=True)
f.box(360, 60, 200, 70, ["Verilog", "rtl/*.v  (the circuit)"], C["blue2"], C["blue"], 14, True)
f.box(20, 60, 230, 70, ["Python golden model", "model/*.py  (independent answer key)"], C["orange2"], C["orange"], 12.5, True)
f.box(670, 60, 230, 70, ["Testbench", "tb/*.v  (self-checking, PASS/FAIL)"], C["purple2"], C["purple"], 12.5, True)
f.path("M135,132 L135,160 L785,160 L785,134", "none", C["orange"], 1.6, "5 4"); f.poly([(785, 132), (780, 142), (790, 142)], C["orange"]); f.text(655, 153, "expected answers (hex files)", 11, C["orange"], italic=True)
f.arrow(562, 95, 668, 95, C["ink"], 2); f.text(615, 86, "drives", 10.5, C["line"], italic=True)
f.box(120, 200, 230, 70, ["Icarus Verilog", "interpreter, 4-state (0 1 x z)"], C["green2"], C["green"], 13, True); f.box(380, 200, 230, 70, ["Verilator", "compiles to C++, 2-state, fast"], C["green2"], C["green"], 13, True)
f.arrow(430, 132, 300, 198, C["ink"], 2); f.arrow(460, 132, 480, 198, C["ink"], 2)
f.box(160, 320, 380, 56, ["PASS / FAIL  (exit status 0 or 1)", "two independent simulators must agree"], C["green2"], C["green"], 13, True); f.arrow(235, 272, 300, 318, C["ink"], 2); f.arrow(495, 272, 440, 318, C["ink"], 2)
f.box(670, 200, 230, 70, ["Yosys + ABC", "synthesis: Verilog -> gates"], C["red2"], C["red"], 13, True); f.arrow(530, 132, 740, 198, C["ink"], 2)
f.box(670, 320, 230, 56, ["netlist + cell counts", "(and timing, Chapter 10)"], C["red2"], C["red"], 12.5, True); f.arrow(785, 272, 785, 318, C["ink"], 2)
f.box(20, 410, 880, 44, ["mutation script (Python): break the circuit one line at a time and check that the testbench fails  -  the test of the test"], C["yellow2"], C["gray"], 12.5, True)
f.path("M250,376 L250,408", "none", C["gray"], 1.6, "4 3"); f.save(f"{OUT}/bg-toolmap.svg")
# three descriptions
f = Fig(900, 330, "One function, three descriptions"); f.text(450, 22, "The majority-of-three function written three ways: Yosys turns all of them into the same kind of gates", 14, bold=True)
codes = [("assign (dataflow)", ["assign y = (a & b)", "         | (a & c) | (b & c);"], C["blue2"], C["blue"]), ("always + case (behaviour)", ["always @* case ({a,b,c})", "  3'b011,3'b101,", "  3'b110,3'b111: y = 1;", "  default: y = 0;", "endcase"], C["green2"], C["green"]), ("gates (structure)", ["and g1(ab,a,b);", "and g2(ac,a,c);", "and g3(bc,b,c);", "or  g4(y,ab,ac,bc);"], C["orange2"], C["orange"])]
for k, (t, ls, fl, st) in enumerate(codes):
    x = 20 + k * 292; f.box(x, 50, 276, 150, [], fl, st); f.text(x + 138, 72, t, 13, st, bold=True); f.lines(x + 16, 98, ls, 11.5, C["ink"], "start", lh=17, mono=True)
f.arrow(450, 204, 450, 238, C["ink"], 2.2); f.box(300, 240, 300, 40, ["Yosys: 5 cells (3 AND, 2 OR)  or  4 with NAND"], C["red2"], C["red"], 12.5, True)
f.text(450, 312, "same truth table, same circuit: the style is a matter of readability, not of what hardware you get", 12, C["line"], italic=True); f.save(f"{OUT}/bg-three.svg")
# software vs hardware
f = Fig(900, 300, "Software versus hardware description"); f.text(450, 22, "A program runs one statement after another; a hardware description is many things happening at once", 14, bold=True)
f.box(30, 50, 400, 200, [], C["gray2"], C["gray"]); f.text(230, 74, "software (C, Python)", 14, C["ink"], bold=True); f.lines(50, 104, ["x = a + b;", "y = x * 2;", "z = y - 1;"], 13, C["ink"], "start", lh=22, mono=True); f.text(230, 190, "one after the other: z needs y needs x", 12, C["line"], italic=True); f.text(230, 214, "time is the order of statements", 12, C["line"], italic=True)
f.box(470, 50, 400, 200, [], C["blue2"], C["blue"]); f.text(670, 74, "hardware (Verilog)", 14, C["blue"], bold=True); f.lines(490, 104, ["assign x = a + b;", "assign y = x * 2;", "assign z = y - 1;"], 13, C["ink"], "start", lh=22, mono=True); f.text(670, 190, "three circuits that all exist and all run, always", 12, C["line"], italic=True); f.text(670, 214, "the order of lines does not matter; time is the clock", 12, C["line"], italic=True)
f.save(f"{OUT}/bg-concurrency.svg")
print("3 figures written")
