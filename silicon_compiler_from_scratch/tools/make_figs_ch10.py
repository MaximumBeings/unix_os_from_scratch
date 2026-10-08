#!/usr/bin/env python3
"""Chapter 10 figures -> docs/assets/fig/ch10-*.svg. Data-driven ones read lib/toy.lib, out/ch10_run_out.txt, out/ch10_faults_out.txt, out/ch10_example_a.json and out/ch10_example_b.json."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch10_example_a.json")); B = json.load(open(f"{ROOT}/out/ch10_example_b.json")); run = open(f"{ROOT}/out/ch10_run_out.txt").read(); flt = open(f"{ROOT}/out/ch10_faults_out.txt").read(); lib = open(f"{ROOT}/lib/toy.lib").read()
# 10.1 the flow
f = Fig(940, 300, "The synthesis flow"); f.text(470, 22, "Verilog to a netlist of library cells: the steps Yosys and ABC run", 14, bold=True)
st = [("RTL", "rtl/*.v", C["blue2"], C["blue"]), ("elaborate", "registers + logic", C["green2"], C["green"]), ("optimize", "simplify, share, flatten", C["green2"], C["green"]), ("map", "ABC picks cells from toy.lib", C["orange2"], C["orange"]), ("netlist", "out/*_net.v, .json", C["red2"], C["red"]), ("analyze", "area, timing, simulate", C["purple2"], C["purple"])]
for k, (a, b, fl, sk) in enumerate(st):
    x = 14 + k * 154; f.box(x, 60, 138, 64, [a], fl, sk, 14, True, sub=b)
    if k < 5: f.arrow(x + 140, 92, x + 152, 92, C["ink"], 2)
f.box(404, 160, 140, 44, ["toy.lib: 14 cells", "area + delay each"], C["yellow2"], C["gray"], 11.5, True); f.arrow(474, 160, 474, 126, C["gray"], 1.8, 8, "4 3")
f.box(14, 160, 250, 52, ["memories stay black boxes", "(a chip uses a memory macro)"], C["gray2"], C["gray"], 11.5)
f.lines(470, 250, ["Every step preserves the function. The next sections check that it did, three ways:", "gate-level simulation, fault grading, and equivalence checking."], 12.5, C["ink"], lh=19); f.save(f"{OUT}/ch10-flow.svg")
# 10.2 toy library
cells = re.findall(r"cell\((\w+)\) \{\n    area : ([\d.]+);", lib); dl = [re.search(r'cell_rise\(scalar\) \{ values\("([\d.]+)"\)', lib[lib.index(f"cell({n})"):]).group(1) for n, _ in cells]
ch = bar_chart(900, 340, [n for n, _ in cells], [[float(a) for _, a in cells], [float(d) * 10 for d in dl]], "The toy cell library: area (NAND2 equivalents) and delay (tenths of a ns)", "", colors=[C["blue"], C["orange"]], fmt="{:.2f}", legend=["area (GE)", "delay (x 0.1 ns)"]); ch.save(f"{OUT}/ch10-lib.svg")
# 10.3 hand STA of the chain check
f = Fig(900, 280, "Timing by hand"); f.text(450, 22, "Static timing on the small check circuit: the longest path is the sum of cell delays (plus clock-to-q and setup)", 14, bold=True)
f.box(30, 60, 100, 40, ["DFF q"], C["blue2"], C["blue"], 13, True); f.text(80, 118, "clock-to-q 0.30", 11, C["blue"])
f.arrow(132, 80, 192, 80, C["ink"], 2); f.box(194, 60, 100, 40, ["INV"], C["green2"], C["green"], 13, True); f.text(244, 118, "+ 0.04", 11, C["green"])
f.arrow(296, 80, 356, 80, C["ink"], 2); f.box(358, 60, 130, 40, ["output z"], C["red2"], C["red"], 13, True); f.text(423, 118, "+ setup 0.10", 11, C["red"])
f.text(600, 84, "= 0.30 + 0.04 + 0.10 = 0.44 ns", 14, C["red"], "start", True, True)
f.box(30, 160, 100, 40, ["a"], C["gray2"], C["gray"], 13, True); f.arrow(132, 180, 192, 180, C["ink"], 2); f.box(194, 160, 90, 40, ["INV"], C["green2"], C["green"], 12, True); f.arrow(286, 180, 326, 180, C["ink"], 2); f.box(328, 160, 90, 40, ["NAND2"], C["green2"], C["green"], 12, True); f.arrow(420, 180, 460, 180, C["ink"], 2); f.box(462, 160, 90, 40, ["INV"], C["green2"], C["green"], 12, True); f.arrow(554, 180, 594, 180, C["ink"], 2); f.box(596, 160, 80, 40, ["y"], C["red2"], C["red"], 13, True)
f.text(690, 178, "0.04 + 0.07 + 0.04 = 0.15", 12, C["line"], "start", False, True); f.text(690, 196, "(+ 0.10 setup = 0.25)", 12, C["line"], "start", False, True)
f.text(450, 250, "the analyzer prints 0.44 ns at output z: the longer of the paths, as computed here by hand", 12.5, C["ink"], "middle", True); f.save(f"{OUT}/ch10-sta.svg")
# 10.4 adder delay vs width (example A)
ws = A["W"]; ch = line_chart(860, 380, ws, [[r[0] for r in A["ripple"]], [r[0] for r in A["plus"]], [r[0] for r in A["kogge"]]], "Adder delay against width in the toy library (measured)", "adder width in bits (log scale)", "critical path (ns)", [C["red"], C["gray"], C["green"]], ["ripple chain", "'+' (Yosys default)", "Kogge-Stone prefix"], logx=True, xfmt="{:g}", yfmt="{:.1f}", ymin=0); ch.save(f"{OUT}/ch10-adders.svg")
ch = line_chart(860, 380, ws, [[r[2] for r in A["ripple"]], [r[2] for r in A["kogge"]]], "Adder area against width (measured, NAND2 equivalents)", "adder width in bits (log scale)", "area (GE)", [C["red"], C["green"]], ["ripple chain (and '+')", "Kogge-Stone prefix"], logx=True, logy=True, xfmt="{:g}", yfmt="{:,.0f}"); ch.save(f"{OUT}/ch10-adderarea.svg")
# 10.5 per-unit timing and area (run output)
rows = re.findall(r"^(exp_lut|divu|requant|systolic|ga2_st|ga2_rq|ga2_vadd|ga2_amax|ga2_sm|ga2_mm|ga2)\s+(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+) ns\s+(\d+)", run, re.M)
ch = bar_chart(900, 360, [r[0] for r in rows], [[float(r[4]) for r in rows]], "Critical path of each unit, toy library (measured, ns)", "ns", colors=[C["blue"]], fmt="{:.1f}"); ch.save(f"{OUT}/ch10-paths.svg")
ch = bar_chart(900, 360, [r[0] for r in rows], [[int(r[2]) for r in rows]], "Area of each unit, toy library (measured, NAND2 equivalents)", "GE", colors=[C["green"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch10-areas.svg")
# 10.6 pipelining variants (example B)
names = [("combinational", "requant"), ("cut after multiply", "requant_p2"), ("cut + prefix adder", "requant_p2k")]
ch = bar_chart(780, 340, [n for n, _ in names], [[B[k]["ns"] for _, k in names]], "Requantizer critical path: three designs (measured, ns)", "ns", colors=[C["orange"]], fmt="{:.2f}"); ch.save(f"{OUT}/ch10-pipeline.svg")
# 10.7 X pessimism
f = Fig(900, 320, "X-pessimism"); f.text(450, 22, "Why a four-state simulator can fail on a netlist that is right: (a & b) | (a & ~b) is just a, but not when b is x", 14, bold=True)
f.box(30, 60, 400, 150, [], C["gray2"], C["gray"]); f.text(230, 84, "what the designer meant", 13, C["ink"], bold=True); f.lines(50, 112, ["y = (a & b) | (a & ~b)", "    = a & (b | ~b)", "    = a"], 13, C["ink"], "start", lh=22, mono=True); f.text(230, 196, "for a = 1, b = 0 or 1: y = 1", 12, C["green"], "middle", True)
f.box(470, 60, 400, 150, [], C["red2"], C["red"]); f.text(670, 84, "what a four-state simulator computes", 13, C["red"], bold=True); f.lines(490, 112, ["a = 1, b = x:", "  a & b  = 1 & x    = x", "  a & ~b = 1 & ~x   = x", "  x | x  =                x"], 12.5, C["ink"], "start", lh=21, mono=True); f.text(670, 196, "y = x   (silicon would give 1)", 12, C["red"], "middle", True)
f.lines(450, 250, ["The two-valued function is the same, so the optimizer was free to rearrange it. Unreset flip-flops hold x in simulation, and the rearranged logic", "lets an x leak where the original did not. Fixes: reset everything (costs area) or start gate-level simulation from a defined state."], 12, C["line"], lh=18); f.save(f"{OUT}/ch10-xpess.svg")
# 10.8 fault coverage
pts = [(int(m.group(1)), float(m.group(3))) for m in re.finditer(r"^\s*(\d+)\s+(\d+)\s+([\d.]+)%", flt, re.M)]
ch = line_chart(860, 380, [p[0] for p in pts], [[p[1] for p in pts]], "Stuck-at fault coverage of the requantizer as the test set grows (measured on a 300-fault sample)", "vectors applied (log scale)", "coverage of testable faults (%)", [C["blue"]], ["directed + random vectors (76% at the end; 100% after adding the 71 generated patterns)"], logx=True, xfmt="{:g}", yfmt="{:.0f}", ymin=0, ymax=100); ch.save(f"{OUT}/ch10-faults.svg")
print("9 figures written")
