#!/usr/bin/env python3
"""Appendix figures -> docs/assets/fig/appx-*.svg (data from out/appx_*_out.txt where a figure shows a measured number)."""
import math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig"); rd = lambda n: open(f"{ROOT}/out/appx_{n}_out.txt").read()
# ---- A: gates, flip-flop timing, FSM
f = Fig(940, 300, "Gates"); f.text(470, 22, "The basic gates: symbol and truth table (a, b -> y)", 14, bold=True)
for i, (k, tt) in enumerate((("AND", "0001"), ("OR", "0111"), ("XOR", "0110"), ("NAND", "1110"), ("NOR", "1000"))):
    x = 20 + i * 184; g = f.gate(k, x + 20, 70, None, "#ffffff", C["ink"]); f.line(x, g["in"][0][1], x + 20, g["in"][0][1], C["ink"], 1.6); f.line(x, g["in"][1][1], x + 20, g["in"][1][1], C["ink"], 1.6); f.line(g["out"][0], g["out"][1], g["out"][0] + 18, g["out"][1], C["ink"], 1.6)
    f.text(x + 50, 62, k, 13, bold=True); f.rect(x + 20, 140, 120, 120, C["gray2"], C["gray"], 1.2, 6)
    f.text(x + 52, 160, "a b", 11, mono=True, bold=True); f.text(x + 118, 160, "y", 11, mono=True, bold=True)
    for r, (a, b) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))): f.text(x + 52, 184 + r * 20, f"{a} {b}", 11.5, mono=True); f.text(x + 118, 184 + r * 20, tt[r], 11.5, C["blue"], mono=True, bold=True)
f.save(f"{OUT}/appx-a-gates.svg")
f = Fig(940, 330, "A flip-flop samples at the clock edge"); f.text(470, 22, "A D flip-flop: q takes the value d had just before each rising clock edge", 14, bold=True)
clk = "0101010101010101"; d = "0011110000111100"[:16]; q = "0000111100001111"
f.wave(120, 60, clk, 36, 26, C["gray"], "clk"); f.wave(120, 130, "0000111100001111"[:16], 36, 26, C["blue"], "d")
f.wave(120, 200, "0000011110000111"[:16], 36, 26, C["green"], "q")
for k in range(1, 16, 2): f.line(120 + k * 36 - 36 + 36, 50, 120 + k * 36, 250, C["orange"], 1, "4 4", 0.5)
f.text(470, 300, "d changes between edges and q ignores it; q changes only just after a rising edge (dashed), to the value d had at the edge", 12, C["line"], italic=True); f.save(f"{OUT}/appx-a-dff.svg")
f = Fig(900, 330, "Detector FSM"); f.text(450, 22, "A Moore detector for the pattern 1 0 1 (Appendix A, part 5): states and transitions", 14, bold=True)
pos = {"S0": (110, 150), "S1": (330, 150), "S2": (550, 150), "S3": (770, 150)}
for s, (x, y) in pos.items(): f.circ(x, y, 42, C["green2"] if s == "S3" else C["blue2"], C["green"] if s == "S3" else C["blue"], 2.4); f.text(x, y + 5, s, 15, bold=True); f.text(x, y + 64, "out = 1" if s == "S3" else "out = 0", 11, C["line"], italic=True)
for a, b, lab in (("S0", "S1", "1"), ("S1", "S2", "0"), ("S2", "S3", "1")): xa, xb = pos[a][0] + 44, pos[b][0] - 44; f.arrow(xa, 140, xb, 140, C["ink"], 1.8); f.text((xa + xb) / 2, 130, lab, 13, bold=True)
f.arrow(pos["S2"][0] - 40, 168, pos["S1"][0] + 40, 168, C["gray"], 1.6) if False else None
f.text(450, 300, "not drawn: S0 on 0 stays in S0; S1 on 1 stays in S1; S2 on 0 -> S0; S3 on 0 -> S2; S3 on 1 -> S1 (overlap: the last 1 may start a new pattern)", 11.5, C["line"], italic=True); f.save(f"{OUT}/appx-a-fsm.svg")
# ---- B: blocking vs non-blocking
f = Fig(940, 340, "Blocking and non-blocking"); f.text(470, 22, "a <= b; b <= a   against   a = b; b = a   (a = 1, b = 2 before the clock edge)", 14, bold=True)
for i, (title, rows, col) in enumerate((("non-blocking (<=): both right-hand sides are read first", ["read b (2) and a (1)", "then a gets 2, b gets 1", "result: a = 2, b = 1  (a swap)"], C["green"]), ("blocking (=): statements run one after the other", ["a = b  ->  a is now 2", "b = a  ->  reads the NEW a (2)", "result: a = 2, b = 2  (a bug)"], C["red"]))):
    x = 30 + i * 450; f.box(x, 60, 420, 200, [], C["green2"] if i == 0 else C["red2"], col, 14); f.text(x + 210, 88, title, 12.5, bold=True)
    for j, r in enumerate(rows): f.box(x + 40, 110 + j * 48, 340, 36, [r], "#ffffff", col, 12.5, j == 2, mono=True)
f.text(470, 305, "In clocked always blocks write <=; use = only inside combinational blocks. Appendix B shows both in the same testbench.", 12, C["line"], italic=True); f.save(f"{OUT}/appx-b-swap.svg")
f = Fig(940, 330, "Verilog map"); f.text(470, 22, "What the pieces of a Verilog module become in hardware", 14, bold=True)
for i, (a, b, fl, st) in enumerate((("module / ports", "a box with named wires in and out", C["blue2"], C["blue"]), ("assign y = a & b;", "combinational logic: y follows a and b at all times", C["green2"], C["green"]), ("always @* begin ... end", "combinational logic, written as a small program (assign EVERY output on EVERY path)", C["green2"], C["green"]), ("always @(posedge clk) q <= d;", "flip-flops: q updates at each rising clock edge", C["orange2"], C["orange"]), ("reg / wire", "a name for a signal; reg does NOT mean 'register' by itself", C["gray2"], C["gray"]), ("module instance", "a copy of a box wired into a bigger box", C["blue2"], C["blue"]))):
    y = 50 + i * 44; f.box(30, y, 320, 36, [a], fl, st, 12.5, True, mono=True); f.arrow(352, y + 18, 380, y + 18, C["ink"], 1.6, 7); f.box(384, y, 530, 36, [b], "#ffffff", st, 12)
f.save(f"{OUT}/appx-b-map.svg")
# ---- C: floating point layouts, two's complement ring
f = Fig(940, 340, "Float layouts"); f.text(470, 22, "Where the bits go: sign, exponent, mantissa", 14, bold=True)
for i, (nm, e, m) in enumerate((("fp32", 8, 23), ("fp16", 5, 10), ("bf16", 8, 7), ("fp8 e4m3", 4, 3), ("fp8 e5m2", 5, 2))):
    y = 50 + i * 56; u = 18; x = 130; f.text(x - 10, y + 22, nm, 13, anchor="end", bold=True)
    f.box(x, y, 28, 34, ["s"], C["red2"], C["red"], 11, True); x2 = x + 28
    f.box(x2, y, e * u, 34, [f"exp {e}" if e * u >= 90 else f"e{e}"], C["orange2"], C["orange"], 11, True); x3 = x2 + e * u
    f.box(x3, y, m * u, 34, [f"mantissa {m}" if m * u >= 110 else f"m{m}"], C["blue2"], C["blue"], 11, True)
f.text(470, 328, "value = (-1)^s x 1.mantissa x 2^(exponent - bias): more exponent bits give range, more mantissa bits give precision", 12, C["line"], italic=True); f.save(f"{OUT}/appx-c-float.svg")
f = Fig(560, 540, "Two's complement ring"); f.text(280, 24, "4-bit two's complement: the codes form a ring", 14, bold=True); cx, cy, Rr = 280, 290, 190
for code in range(16):
    a = -math.pi / 2 + 2 * math.pi * code / 16; x, y = cx + Rr * math.cos(a), cy + Rr * math.sin(a); val = code if code < 8 else code - 16
    f.circ(x, y, 24, C["blue2"] if val >= 0 else C["orange2"], C["blue"] if val >= 0 else C["orange"], 1.8); f.text(x, y + 4, (f"{val:+d}" if val else "0"), 12.5, bold=True); f.text(cx + (Rr - 58) * math.cos(a), cy + (Rr - 58) * math.sin(a) + 4, f"{code:04b}", 10, C["line"], mono=True)
f.text(cx, cy - 6, "adding 1 moves one step clockwise;", 12, C["line"], italic=True); f.text(cx, cy + 14, "past +7 the ring wraps to -8", 12, C["line"], italic=True); f.save(f"{OUT}/appx-c-ring.svg")
# ---- D: matmul picture, intensity
f = Fig(900, 330, "Matrix product"); f.text(450, 22, "(A B)[i][j] = row i of A . column j of B   (shapes: m x k times k x n gives m x n)", 14, bold=True)
def grid(x, y, r, c, hr=None, hc=None, fill=C["gray2"]):
    for i in range(r):
        for j in range(c): f.rect(x + j * 34, y + i * 34, 30, 30, C["orange2"] if i == hr else (C["blue2"] if j == hc else fill), C["gray"], 1.2, 3)
grid(60, 90, 3, 4, hr=1); grid(330, 60, 4, 3, hc=1); grid(620, 90, 3, 3)
f.rect(620 + 34 + 0, 90 + 34, 30, 30, C["green2"], C["green"], 2.4, 3)
f.text(122, 76, "A (3 x 4)", 13, bold=True); f.text(380, 48, "B (4 x 3)", 13, bold=True); f.text(670, 76, "A B (3 x 3)", 13, bold=True)
f.text(255, 170, "times", 14, C["line"], italic=True); f.text(550, 170, "=", 20, C["line"]); f.text(450, 270, "the orange row of A meets the blue column of B in one dot product, which fills the green cell: k multiply-adds per cell", 12, C["line"], italic=True); f.save(f"{OUT}/appx-d-matmul.svg")
ch = bar_chart(860, 340, ["vector x matrix", "batch 8", "batch 64", "512 x 512 x 512"], [[1.0, 8.0, 62.1, 170.7]], "Operations per byte moved (int8, 4096-wide weights; arithmetic intensity)", "ops per byte", colors=[C["blue"]], fmt="{:.1f}"); ch.save(f"{OUT}/appx-d-intensity.svg")
# ---- E: transformer block, KV sizes
f = Fig(940, 420, "Transformer block"); f.text(470, 22, "One transformer layer, one decode step (the structure of the tiny model of Chapter 12)", 14, bold=True)
steps = [("token", C["gray2"], C["gray"]), ("embedding", C["orange2"], C["orange"]), ("q, k, v projections", C["blue2"], C["blue"]), ("attention over the KV cache", C["blue2"], C["blue"]), ("output projection", C["blue2"], C["blue"]), ("+ residual", C["gray2"], C["gray"]), ("feed-forward (2 matrices, relu)", C["green2"], C["green"]), ("+ residual", C["gray2"], C["gray"]), ("output matrix -> logits", C["orange2"], C["orange"]), ("argmax -> next token", C["gray2"], C["gray"])]
for i, (t, fl, st) in enumerate(steps):
    y = 44 + i * 36; f.box(300, y, 340, 28, [t], fl, st, 12.5, i in (3, 6)); 
    if i: f.arrow(470, y - 8, 470, y, C["ink"], 1.6, 6)
f.box(700, 44 + 3 * 36 - 6, 200, 40, ["KV cache (grows with context)"], C["orange2"], C["orange"], 11, True); f.arrow(698, 44 + 3 * 36 + 14, 642, 44 + 3 * 36 + 14, C["orange"], 1.8, 7)
f.path("M300,172 C230,172 230,224 300,224", "none", C["gray"], 1.6, "5 4"); f.text(190, 202, "skip", 11, C["line"], italic=True); f.path("M300,276 C230,276 230,316 300,316", "none", C["gray"], 1.6, "5 4"); f.text(190, 296, "skip", 11, C["line"], italic=True)
f.save(f"{OUT}/appx-e-block.svg")
ch = bar_chart(860, 340, ["tiny, int8", "7B, 32 heads, fp16", "7B, 32 heads, int8", "7B, 8 KV heads, int8"], [[32 / 1024, 512, 256, 64]], "KV cache per token in KiB (4,096 tokens would be 4,096 times this)", "KiB per token", colors=[C["orange"]], fmt="{:.2f}"); ch.save(f"{OUT}/appx-e-kv.svg")
# ---- F: hierarchy, bandwidth
f = Fig(900, 340, "Memory hierarchy"); f.text(450, 22, "Memories trade size against speed (typical orders of magnitude, assumed)", 14, bold=True)
for i, (n, sz, lat, fl) in enumerate((("registers", "~1 KB", "0 ns", C["red2"]), ("on-chip SRAM scratchpad", "~1-100 MB", "1-10 ns", C["orange2"]), ("DRAM (DDR / GDDR / HBM)", "~10-100 GB", "~50-100 ns", C["blue2"]), ("flash", "~TB", "~100 us", C["gray2"]))):
    w = 200 + i * 150; x = 450 - w / 2; y = 52 + i * 62; f.box(x, y, w, 52, [n], fl, C["line"], 13, True, sub=f"{sz}, {lat}")
f.arrow(840, 70, 840, 280, C["ink"], 2); f.text(860, 70, "faster", 11, C["line"], anchor="start"); f.text(860, 290, "larger", 11, C["line"], anchor="start"); f.save(f"{OUT}/appx-f-hier.svg")
t = rd("f"); vals = [float(v) for v in re.findall(r"^\s+(?:sequential 64-byte lines|strided by 4 KiB|random lines)\s+[\d.]+ GB/s \(\s*\d+% row hits\)\s+([\d.]+) GB/s", t, re.M)]
ch = bar_chart(780, 340, ["sequential", "strided 4 KiB", "random"], [vals], "Toy DRAM, 8 requests in flight: delivered GB/s (peak of the channel 25.6)", "GB/s", colors=[C["blue"]], fmt="{:.1f}", maxv=28); ch.save(f"{OUT}/appx-f-bw.svg")
# ---- G: flow, adders
f = Fig(940, 360, "EDA flow"); f.text(470, 22, "The digital design flow: green stages are in this book, grey stages need a real process kit", 14, bold=True)
st = [("RTL", 1), ("simulate", 1), ("synthesize", 1), ("equivalence", 1), ("timing (STA)", 1), ("place + route", 0), ("clock + power", 0), ("DRC / LVS", 0), ("signoff, tape-out", 0)]
for i, (n, inb) in enumerate(st):
    x = 20 + (i % 5) * 184; y = 70 + (i // 5) * 120; f.box(x, y, 160, 60, [n], C["green2"] if inb else C["gray2"], C["green"] if inb else C["gray"], 13, True)
    if i % 5 < 4 and i < 8: f.arrow(x + 162, y + 30, x + 182, y + 30, C["ink"], 1.8, 7)
f.arrow(20 + 4 * 184 + 80, 132, 20 + 80, 188, C["ink"], 1.6, 7) if False else f.path("M920,132 C920,162 100,150 100,188", "none", C["ink"], 1.6)
f.text(470, 330, "Yosys, ABC, Icarus, Verilator, a toy library and a timing model cover the left half; OpenROAD with an open process kit covers the rest", 12, C["line"], italic=True); f.save(f"{OUT}/appx-g-flow.svg")
t = rd("g"); rows = re.findall(r"^\s+(a \+ b \(tool's choice\)|ripple-carry by hand|Kogge-Stone prefix adder)\s+(\d+)\s+([\d.]+)\s+(\d+)\s+([\d.]+) ns", t, re.M)
ch = bar_chart(860, 340, [r[0] for r in rows], [[float(r[2]) for r in rows]], "Area of three 32-bit adders in the toy library (NAND2 equivalents)", "area", colors=[C["blue"]], fmt="{:.0f}"); ch.save(f"{OUT}/appx-g-area.svg")
ch = bar_chart(860, 340, [r[0] for r in rows], [[float(r[4]) for r in rows]], "Longest path of the same adders (ns, the book's timing model)", "ns", colors=[C["orange"]], fmt="{:.2f}"); ch.save(f"{OUT}/appx-g-delay.svg")
print("appendix figures written", len(vals), len(rows))
