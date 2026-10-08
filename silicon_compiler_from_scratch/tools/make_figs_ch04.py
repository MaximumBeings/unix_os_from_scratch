#!/usr/bin/env python3
"""Chapter 4 figures -> docs/assets/fig/ch04-*.svg. Data-driven ones read out/ch04_example_a.json, ch04_example_b.json and ch04_run_out.txt."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch04_example_a.json")); B = json.load(open(f"{ROOT}/out/ch04_example_b.json"))
# 4.1 PE anatomy
f = Fig(780, 340, "One processing element"); f.text(390, 22, "A PE: a MAC plus two pass-through registers", 14, bold=True)
f.box(250, 100, 290, 140, [], "#ffffff", C["blue"], sw=2.2); f.text(395, 120, "PE(i, j)", 13, C["blue"], bold=True)
f.box(275, 138, 90, 40, ["a x b"], C["blue2"], C["blue"], 13, True, mono=True); f.box(405, 138, 110, 40, ["+ acc"], C["blue2"], C["blue"], 13, True, mono=True); f.arrow(367, 158, 403, 158, C["ink"], 2)
f.box(405, 192, 110, 36, ["acc (32 bit)"], C["green2"], C["green"], 12, True, mono=True); f.arrow(460, 180, 460, 190, C["ink"], 2); f.path("M515,210 L528,210 L528,158 L517,158", "none", C["green"], 1.6); f.text(530, 252, "acc feeds back into the +", 10.5, C["green"], "end", italic=True)
f.text(60, 154, "a_in", 14, bold=True, mono=True); f.arrow(92, 158, 273, 158, C["ink"], 2.2)
f.text(320, 44, "b_in", 14, bold=True, mono=True); f.arrow(320, 52, 320, 136, C["ink"], 2.2)
f.circ(200, 158, 3.5, C["ink"]); f.path("M200,158 L200,262", "none", C["orange"], 1.8); f.box(170, 262, 70, 30, ["a reg"], C["orange2"], C["orange"], 12, True, mono=True); f.arrow(242, 277, 740, 277, C["orange"], 2.2); f.text(430, 306, "a_out -> PE(i, j+1)  (the PE to the right)", 12, C["orange"], bold=True, mono=True)
f.circ(320, 80, 3.5, C["ink"]); f.path("M320,80 L600,80", "none", C["orange"], 1.8); f.box(600, 64, 70, 30, ["b reg"], C["orange2"], C["orange"], 12, True, mono=True); f.arrow(635, 96, 635, 150, C["orange"], 2.2); f.text(635, 168, "b_out -> PE(i+1, j)", 12, C["orange"], bold=True, mono=True); f.text(635, 184, "(the PE below)", 11, C["line"], italic=True)
f.lines(110, 220, ["every clock edge:", "acc <= acc + a_in * b_in", "a_out <= a_in", "b_out <= b_in"], 11, C["line"], mono=True, lh=15); f.save(f"{OUT}/ch04-pe.svg")
# 4.2 grid with flows (3x3)
f = Fig(780, 520, "The 3 x 3 array"); f.text(390, 22, "A 3 x 3 output-stationary array: rows of A flow right, columns of B flow down", 14, bold=True)
x0, y0, s = 200, 90, 130
for i in range(3):
    for j in range(3):
        x, y = x0 + j * s, y0 + i * s; f.box(x, y, 78, 78, [f"PE", f"({i},{j})"], C["blue2"], C["blue"], 13, True); f.text(x + 39, y + 70, f"C[{i}][{j}]", 10.5, C["green"], bold=True, mono=True)
        if j < 2: f.arrow(x + 80, y + 39, x + s - 2, y + 39, C["orange"], 2.2)
        if i < 2: f.arrow(x + 39, y + 80, x + 39, y + s - 2, C["purple"], 2.2)
for i in range(3): f.text(130, y0 + i * s + 36, f"A[{i}][k]", 13, C["orange"], "end", True, True); f.arrow(138, y0 + i * s + 39, x0 - 2, y0 + i * s + 39, C["orange"], 2.2); f.text(100, y0 + i * s + 56, f"delayed {i}", 11, C["line"], "end", italic=True)
for j in range(3): f.text(x0 + j * s + 39, 52, f"B[k][{j}]", 13, C["purple"], bold=True, mono=True); f.arrow(x0 + j * s + 39, 58, x0 + j * s + 39, y0 - 2, C["purple"], 2.2); f.text(x0 + j * s + 39, 72, f"delayed {j}", 11, C["line"], italic=True)
f.lines(660, 150, ["Each PE keeps its own", "output element C[i][j]", "in its accumulator", "(green): the OUTPUT", "stays, the INPUTS move"], 12, C["ink"], lh=18)
f.lines(660, 270, ["orange: a values move right", "purple: b values move down", "", "each value is read from", "memory ONCE and used", "by N PEs"], 11.5, C["line"], lh=16); f.save(f"{OUT}/ch04-grid.svg")
# 4.3 wavefront frames: which PEs are busy at each cycle (3x3, K=4)
N, K = 3, 4; f = Fig(900, 360, "The wavefront"); f.text(450, 22, "Which PEs are multiplying real data, cycle by cycle (3 x 3 array, K = 4): a diamond fills and drains", 14, bold=True)
for t in range(8):
    cx, cy = 20 + (t % 4) * 220, 50 + (t // 4) * 150; busy = [(i, j) for i in range(N) for j in range(N) if 0 <= t - i - j < K]
    f.text(cx + 80, cy + 12, f"cycle {t}", 13, bold=True)
    for i in range(N):
        for j in range(N):
            on = (i, j) in busy; f.box(cx + j * 54, cy + 22 + i * 36, 48, 30, [str(t - i - j) if on else ""], C["green2"] if on else C["gray2"], C["green"] if on else C["gray"], 12, on, mono=True, rx=5)
    f.text(cx + 80, cy + 138, f"{len(busy)} of 9 busy" + ("" if t not in (3, 4) else "  (peak)"), 11, C["line"], italic=True)
f.text(450, 352, "the number in a busy PE is k: the PE is computing A[i][k] x B[k][j]; the value of k = t - i - j is what the skew arranges", 11.5, C["line"], italic=True); f.save(f"{OUT}/ch04-wave.svg")
# 4.4 accumulator frames from the circuit
f = Fig(900, 420, "Accumulators over time"); f.text(450, 22, "The accumulators of the real circuit at four moments (Running example A); final C = [[1,1,9],[-3,-5,3],[7,7,8]]", 14, bold=True)
fr = {x["t"]: x for x in A["frames"]}; Cm = A["C"]
for n, t in enumerate((1, 3, 5, 7)):
    cx, cy = 20 + n * 220, 50; acc = fr[t]["acc"]; f.text(cx + 80, cy + 12, f"after edge {t}", 13, bold=True)
    for i in range(3):
        for j in range(3):
            v = acc[3 * i + j]; done = v == Cm[i][j] and t >= i + j + 3; busy = (i, j) in [tuple(b) for b in fr[t]["busy"]]
            f.box(cx + j * 54, cy + 22 + i * 40, 48, 34, [str(v)], C["green2"] if done else (C["yellow2"] if busy else C["gray2"]), C["green"] if done else (C["orange"] if busy else C["gray"]), 13, True, mono=True, rx=5)
f.lines(450, 240, ["green = this PE has finished (its accumulator holds the final C[i][j])", "yellow = still accumulating            grey = not yet started, or only partial"], 12, C["ink"], lh=18)
f.lines(450, 300, ["Read the frames left to right: the first finished element is C[0][0] (frame 3, in the top-left corner),", "the last is C[2][2] (frame 7, bottom-right). The result 'arrives' as a diagonal sweeping to the bottom-right corner.", "The circuit's numbers agree with the formula acc(i,j) = sum of A[i][k] B[k][j] for k <= t - i - j in every frame."], 12, C["line"], lh=18); f.save(f"{OUT}/ch04-frames.svg")
# 4.5 utilization curves (derived)
Ks = B["Ks"]; ut = B["util_table"]
ch = line_chart(840, 380, Ks, [ut[str(n)] for n in (4, 8, 16, 128)], "Utilization K / (K + 2N - 2) of an N x N array on one tile (derived)", "K: the inner dimension (log scale)", "utilization", [C["blue"], C["green"], C["orange"], C["red"]], ["N = 4", "N = 8", "N = 16", "N = 128"], logx=True, xfmt="{:g}", yfmt="{:.1f}", ymin=0, ymax=1); ch.save(f"{OUT}/ch04-util.svg")
# 4.6 tiling
f = Fig(860, 370, "Tiling an 8 x 8 product on a 4 x 4 array"); f.text(430, 22, "A 4 x 4 array computes an 8 x 8 result as four tiles, one after another (Running example B)", 14, bold=True)
f.text(110, 60, "C (8 x 8)", 13, bold=True)
for r in range(2):
    for c in range(2): f.box(40 + c * 70, 72 + r * 70, 66, 66, [f"tile {2*r+c}"], [C["blue2"], C["green2"], C["orange2"], C["purple2"]][2 * r + c], [C["blue"], C["green"], C["orange"], C["purple"]][2 * r + c], 13, True)
f.text(500, 60, "time on the 4 x 4 array (cycles)", 13, bold=True)
for n in range(4):
    x = 220 + n * 150; col = [C["blue2"], C["green2"], C["orange2"], C["purple2"]][n]; st = [C["blue"], C["green"], C["orange"], C["purple"]][n]
    f.box(x, 90, 146, 44, [f"tile {n}: 14 cycles"], col, st, 13, True); f.text(x, 154, str(14 * n), 11, C["line"], "start"); f.arrow(x + 73, 134, x + 73, 150, st, 1.6)
f.text(220 + 600, 154, "56", 11, C["line"], "end")
f.lines(430, 236, ["each tile: 8 + 2x4 - 2 = 14 cycles (8 of useful work per PE + 6 cycles of fill and drain)", "4 tiles x 14 = 56 cycles (measured);  ideal with no fill or drain would be 4 x 8 = 32;  utilization 32/56 = 0.571"], 12.5, C["ink"], lh=20)
f.box(40, 276, 780, 74, [], "#ffffff", C["gray"]); f.lines(430, 298, ["Tile (r, c) needs rows 4r..4r+3 of A and columns 4c..4c+3 of B: every tile reads ALL K = 8 values of its rows and columns.", "On an 8 x 8 array the same product is one tile of 22 cycles (utilization 0.364): 4x the PEs, 2.5x the speed.", "Chapter 7's matrix unit does exactly this tile loop for you; Chapter 11's compiler chooses the tiles."], 12, C["line"], lh=19); f.save(f"{OUT}/ch04-tiling.svg")
# 4.7 area scaling (measured, from the run output)
txt = open(f"{ROOT}/out/ch04_run_out.txt").read(); rows = re.findall(r"^\s*(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+(\d+)\s+(\d+)\s*$", txt, re.M)
ch = bar_chart(780, 340, [f"N={r[0]} ({r[1]} PEs)" for r in rows], [[int(r[2]) for r in rows]], "Generic gates for the array against its size (measured with Yosys 0.33)", "gates", colors=[C["blue"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch04-area.svg")
print("7 figures written")
