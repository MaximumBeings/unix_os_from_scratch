#!/usr/bin/env python3
"""Chapter 1 figures -> docs/assets/fig/ch01-*.svg"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "assets", "fig")
def wire(f, pts, c=None, sw=1.8): f.path("M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts), "none", c or C["ink"], sw)
def dot(f, x, y): f.circ(x, y, 3.2, C["ink"])
# 1.1 the loop
f = Fig(780, 270, "The four-step loop"); xs = [20, 215, 410, 605]; names = [("1  Describe", "Verilog", C["blue2"], C["blue"]), ("2  Simulate", "Icarus + Verilator", C["green2"], C["green"]), ("3  Synthesize", "Yosys", C["purple2"], C["purple"]), ("4  Test the tests", "mutation script", C["orange2"], C["orange"])]
notes = ["write the circuit", "run it against the\nPython answer key", "turn it into gates;\ncount them", "break the circuit one line\nat a time: does the\ntest notice?"]
for x, (a, b, fl, st), n in zip(xs, names, notes):
    f.box(x, 40, 160, 78, [a], fl, st, 15, True, sub=b); f.lines(x + 80, 142, n.split("\n"), 12, C["line"])
for x in xs[:-1]: f.arrow(x + 162, 79, x + 193, 79, C["ink"], 2.2)
f.path(f"M{xs[3]+80},{176} C {xs[3]+80},{240} {xs[0]+80},{240} {xs[0]+80},{176}", "none", C["orange"], 2.2, "6 4"); f.arrow(xs[0] + 80, 178, xs[0] + 80, 170, C["orange"], 2)
f.text(390, 236, "fix the circuit or the test, extend, repeat", 13, C["orange"], italic=True); f.text(390, 24, "Every chapter of this book is this loop on a bigger circuit", 14, C["ink"], bold=True); f.save(f"{OUT}/ch01-loop.svg")
# 1.2 full adder schematic (net-label style: a short stub with a name means "connected to every other stub of that name")
f = Fig(700, 350, "A full adder as gates: 2 XORs and 3 NANDs"); f.text(350, 22, "Full adder as gates: sum = a XOR b XOR cin;  cout = (a AND b) OR (cin AND (a XOR b))", 14, bold=True)
def stub(f, pt, name, left=True):
    x, y = pt; wire(f, [(x - 34, y), (x, y)]); f.text(x - 40, y + 5, name, 14, bold=True, anchor="end", mono=True, fill=C["orange"] if name == "p" else None)
X1 = f.gate("XOR", 170, 50, "XOR"); stub(f, X1["in"][0], "a"); stub(f, X1["in"][1], "b")
X2 = f.gate("XOR", 400, 70, "XOR"); wire(f, [X1["out"], (320, X1["out"][1]), (320, X2["in"][0][1]), X2["in"][0]], C["orange"], 2.4); f.text(300, X1["out"][1] - 8, "p", 14, C["orange"], "middle", True, True); stub(f, X2["in"][1], "cin")
wire(f, [X2["out"], (560, X2["out"][1])]); f.text(570, X2["out"][1] + 5, "sum", 15, bold=True, anchor="start", mono=True)
N1 = f.gate("NAND", 170, 200, "NAND"); stub(f, N1["in"][0], "a"); stub(f, N1["in"][1], "b")
N2 = f.gate("NAND", 400, 250, "NAND"); stub(f, N2["in"][0], "p"); stub(f, N2["in"][1], "cin")
N3 = f.gate("NAND", 560, 205, "NAND"); wire(f, [N1["out"], (520, N1["out"][1]), (520, N3["in"][0][1]), N3["in"][0]]); wire(f, [N2["out"], (520, N2["out"][1]), (520, N3["in"][1][1]), N3["in"][1]]); dot(f, 520, N2["out"][1]) if False else None
wire(f, [N3["out"], (660, N3["out"][1])]); f.text(668, N3["out"][1] + 5, "cout", 15, bold=True, anchor="start", mono=True)
f.text(240, 262, "NAND(a, b)", 12, C["line"], italic=True); f.text(422, 300, "NAND(p, cin)", 12, C["line"], italic=True); f.text(590, 262, "NAND of the two", 12, C["line"], italic=True)
f.lines(350, 322, ["Three NANDs make an OR of two ANDs: NAND(NAND(x,y), NAND(z,w)) = (x AND y) OR (z AND w).", "That is Yosys's result for each stage: 2 XORs + 3 NANDs."], 12, C["line"], lh=15); f.save(f"{OUT}/ch01-fulladder.svg")
# 1.3 ripple chain
f = Fig(780, 330, "Four full adders in a row: the carry ripples"); f.text(390, 22, "The 4-bit ripple-carry adder: each stage waits for the carry of the stage before it", 14, bold=True)
for i in range(4):
    x = 120 + i * 160; f.box(x, 100, 100, 70, [f"stage {i}", "full adder"], C["blue2"], C["blue"], 13, True)
    f.arrow(x + 30, 54, x + 30, 98, C["ink"], 1.8); f.text(x + 30, 48, f"a{i}", 13, bold=True, mono=True); f.arrow(x + 70, 54, x + 70, 98, C["ink"], 1.8); f.text(x + 70, 48, f"b{i}", 13, bold=True, mono=True)
    f.arrow(x + 50, 172, x + 50, 214, C["ink"], 1.8); f.text(x + 50, 232, f"sum{i}", 13, bold=True, mono=True)
    f.text(x + 50, 262, f"settled at t = {i+1} ns" if i < 3 else "settled at t = 4 ns", 12, C["orange"], bold=True); 
f.text(60, 140, "c0", 14, bold=True, mono=True); f.arrow(76, 135, 118, 135, C["orange"], 3)
for i in range(1, 5):
    x0 = 120 + (i - 1) * 160 + 100; x1 = x0 + 60 if i < 4 else x0 + 52; f.arrow(x0 + 1, 135, x1 - 2, 135, C["orange"], 3); f.text((x0 + x1) / 2, 126, f"c{i}", 13, C["orange"], bold=True, mono=True)
f.text(390, 306, "orange = the carry path: 4 stages in series, so the answer is valid only after 4 gate delays", 13, C["orange"], italic=True); f.save(f"{OUT}/ch01-ripple.svg")
# 1.4 flip-flop timing
f = Fig(780, 330, "A flip-flop samples d at the rising edge of clk"); f.text(390, 22, "A D flip-flop: q copies d only at a rising clock edge, and holds it in between", 14, bold=True)
step = 22; x0 = 110; clk = "0011" * 8; d = "0000111100001110111000110011001100"[:len(clk)]; q = []; cur = "0"
for k in range(len(clk)):
    if k > 0 and clk[k - 1] == "0" and clk[k] == "1": cur = d[k - 1]
    q.append(cur)
f.wave(x0, 60, clk, step, 28, C["ink"], "clk"); f.wave(x0, 130, d, step, 28, C["blue"], "d"); f.wave(x0, 200, "".join(q), step, 28, C["green"], "q")
for k in range(1, len(clk)):
    if clk[k - 1] == "0" and clk[k] == "1": xx = x0 + k * step; f.line(xx, 48, xx, 240, C["orange"], 1.4, "4 4"); f.poly([(xx, 56), (xx - 5, 66), (xx + 5, 66)], C["orange"])
f.text(x0 + 2 * step, 262, "rising edges (orange): the only moments q can change", 12, C["orange"], "start", True)
f.lines(390, 292, ["q at an edge = the value d had JUST BEFORE that edge.  Between edges, changes of d are ignored.", "That is what makes a clocked design predictable: everything updates together, on the edge."], 12, C["line"]); f.save(f"{OUT}/ch01-flipflop.svg")
# 1.5 mutation concept
f = Fig(780, 360, "Mutation testing"); f.text(390, 22, "Mutation testing: break the circuit on purpose and see whether the test notices", 14, bold=True)
f.box(20, 150, 130, 60, ["circuit.v", "(correct)"], C["green2"], C["green"], 13, True)
for k, (lab, y) in enumerate((("mutant A: carry-in tied to 0", 50), ("mutant B: a stage left out", 150), ("mutant C: a | b instead of a ^ b", 250))):
    f.arrow(152, 180, 194, y + 28, C["red"], 1.8); f.box(196, y, 246, 56, [lab], C["red2"], C["red"], 12)
    f.arrow(444, y + 28, 490, 180, C["ink"], 1.8)
f.box(492, 150, 110, 60, ["same", "testbench"], C["blue2"], C["blue"], 13, True)
f.arrow(604, 165, 650, 105, C["ink"], 2); f.pill(650, 80, 118, 40, "CAUGHT", C["green2"], C["green"], 14); f.text(709, 140, "exit status != 0", 11, C["line"], italic=True)
f.arrow(604, 195, 650, 255, C["ink"], 2); f.pill(650, 240, 118, 40, "SURVIVED", C["orange2"], C["orange"], 14)
f.lines(709, 300, ["is it equivalent?", "yes: report it, move on", "no: the test has a gap:", "add the missing input"], 11, C["line"], lh=15); f.save(f"{OUT}/ch01-mutation.svg")
# 1.6 sequential structure
f = Fig(780, 300, "A counter: state register plus next-state logic"); f.text(390, 22, "A sequential circuit = a register (the state) + combinational logic that computes the next state", 14, bold=True)
f.box(230, 80, 250, 120, [], C["blue2"], C["blue"]); f.text(355, 104, "next-state logic (combinational)", 12, C["blue"], bold=True)
f.lines(355, 132, ["if rst:   next = 0", "elif en:  next = q + 1", "else:     next = q"], 13, C["ink"], lh=22, mono=True)
f.text(120, 120, "en", 15, bold=True, mono=True); f.arrow(140, 116, 238, 116, C["ink"], 2); f.text(120, 170, "rst", 15, bold=True, mono=True); f.arrow(152, 166, 238, 166, C["ink"], 2)
f.arrow(472, 140, 566, 140, C["ink"], 2.4); f.text(519, 130, "next q", 12, C["line"], italic=True)
f.box(568, 95, 120, 90, ["register", "4 flip-flops"], C["green2"], C["green"], 14, True)
f.arrow(628, 236, 628, 188, C["orange"], 2.4); f.text(662, 226, "clk", 13, C["orange"], bold=True, mono=True, anchor="start"); f.text(662, 242, "(rising edge)", 11, C["orange"], italic=True, anchor="start")
wire(f, [(690, 140), (750, 140)]); f.text(758, 145, "q", 16, bold=True, mono=True, anchor="start"); wire(f, [(720, 140), (720, 250), (355, 250), (355, 204)], C["purple"], 2.2); f.arrow(355, 220, 355, 202, C["purple"], 2.2); dot(f, 720, 140)
f.text(540, 272, "q goes back into the logic: the feedback that gives the circuit a past", 12, C["purple"], italic=True, anchor="middle")
f.save(f"{OUT}/ch01-sequential.svg")
print("6 figures written")
