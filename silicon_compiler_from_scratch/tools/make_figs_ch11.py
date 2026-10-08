#!/usr/bin/env python3
"""Chapter 11 figures -> docs/assets/fig/ch11-*.svg. Data-driven ones read out/ch11_example_a.json and out/ch11_example_b.json."""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch11_example_a.json")); B = json.load(open(f"{ROOT}/out/ch11_example_b.json"))
# 11.1 pipeline
f = Fig(940, 330, "Capra's stages"); f.text(470, 22, "Capra: a graph of tensor operations in, a GA-2 program out", 14, bold=True)
st = [("graph", "(floats)", C["blue2"], C["blue"]), ("calibrate", "run on samples,\nrecord ranges", C["green2"], C["green"]), ("plan scales", "int8 scale per tensor,\nmantissa + shift", C["green2"], C["green"]), ("tile", "matmul -> 4x4 MM\ninstructions", C["orange2"], C["orange"]), ("allocate", "scratchpad address,\nreuse dead space", C["orange2"], C["orange"]), ("emit", "LD MM RQ SM ... HALT", C["red2"], C["red"])]
for k, (a, b, fl, sk) in enumerate(st):
    x = 14 + k * 154; f.box(x, 56, 138, 54, [a], fl, sk, 14, True); f.lines(x + 69, 134, b.split("\n"), 11, C["line"], lh=14)
    if k < 5: f.arrow(x + 140, 83, x + 152, 83, C["ink"], 2)
f.box(14, 190, 450, 110, [], "#ffffff", C["gray"]); f.text(240, 212, "decisions the user never makes", 12.5, C["ink"], bold=True); f.lines(30, 236, ["- the int8 scale of every tensor", "- every requantizer's multiplier and shift", "- how a big product is cut into 4x4 tiles", "- where each tensor lives, and when its space is reused"], 11.5, C["ink"], "start", lh=17)
f.box(490, 190, 436, 110, [], "#ffffff", C["gray"]); f.text(708, 212, "limits the compiler enforces", 12.5, C["red"], bold=True); f.lines(506, 236, ["- inner dimension K <= 64 (no int32-accumulate instruction)", "- 4,096 words of scratchpad, int8 tensors", "- a relu must fuse into the matmul before it", "- a tensor cannot feed two concatenations"], 11.5, C["ink"], "start", lh=17); f.save(f"{OUT}/ch11-stages.svg")
# 11.2 the graph of example A
f = Fig(940, 400, "The attention graph"); f.text(470, 22, "Running example A's graph: the attention step as Capra sees it (boxes are tensors, with their shapes)", 14, bold=True)
pos = {0: (20, 60), 1: (20, 140), 2: (20, 210), 3: (230, 170), 4: (430, 100), 5: (620, 100), 6: (20, 290), 7: (20, 350), 8: (230, 320), 9: (780, 200), 10: (780, 290)}
nm = {n[0]: n for n in A["nodes"]}; shp = {0: "1x8", 1: "5x8", 2: "1x8", 3: "6x8", 4: "1x6", 5: "1x6", 6: "5x8", 7: "1x8", 8: "6x8", 9: "1x8", 10: "1x8"}
col = {"input": (C["gray2"], C["gray"]), "concat": (C["yellow2"], C["gray"]), "matmul": (C["blue2"], C["blue"]), "softmax": (C["purple2"], C["purple"]), "output": (C["green2"], C["green"])}
lab = {0: "q", 1: "kc (key cache)", 2: "kn (new key)", 3: "concat -> K", 4: "matmul K^T x 0.354", 5: "softmax", 6: "vc (value cache)", 7: "vn (new value)", 8: "concat -> V", 9: "matmul", 10: "output"}
for i, (x, y) in pos.items():
    fl, st_ = col[nm[i][1]]; f.box(x, y, 150, 42, [lab[i]], fl, st_, 11.5, True, sub=shp[i])
for a_, b_ in ((0, 4), (1, 3), (2, 3), (3, 4), (4, 5), (6, 8), (7, 8), (8, 9), (5, 9), (9, 10)):
    xa, ya = pos[a_]; xb, yb = pos[b_]
    if (a_, b_) == (9, 10): f.arrow(xa + 75, ya + 44, xb + 75, yb - 2, C["ink"], 1.6)
    else: f.arrow(xa + 152, ya + 21, xb - 2, yb + 21, C["ink"], 1.6)
f.text(700, 60, "scores pinned to 1/16 (Q4.4)", 11, C["purple"], "middle", True); f.text(700, 172, "weights pinned to 1/127", 11, C["purple"], "middle", True); f.save(f"{OUT}/ch11-graph.svg")
# 11.3 ranges and scales
ids = [n[0] for n in A["nodes"] if n[1] != "argmax"]; sc = [A["scales"][str(i)] for i in ids]
ch = bar_chart(900, 340, [f"%{i} {nm[i][1][:6]}" for i in ids], [sc], "The int8 scale Capra chose for each tensor of Running example A (real value per step)", "scale", colors=[C["blue"]], fmt="{:.4f}"); ch.save(f"{OUT}/ch11-scales.svg")
# 11.4 tiling picture
f = Fig(920, 360, "Tiling"); f.text(460, 22, "A 5 x 12 times 12 x 10 product is cut into 3 x 3 output tiles; edge tiles are smaller", 14, bold=True)
x0, y0, cw = 80, 70, 40
for j in range(10):
    for i in range(5):
        tile_r, tile_c = i // 4, j // 4; shade = [C["blue2"], C["green2"], C["orange2"]][(tile_r * 3 + tile_c) % 3]
        f.rect(x0 + j * cw, y0 + i * cw, cw - 2, cw - 2, shade, C["gray"], 1, 3)
for tr, rows in enumerate((range(0, 4), range(4, 5))):
    for tc, cols in enumerate((range(0, 4), range(4, 8), range(8, 10))):
        xa = x0 + cols[0] * cw - 2; ya = y0 + rows[0] * cw - 2; f.rect(xa, ya, len(cols) * cw + 1, len(rows) * cw + 1, "none", C["ink"], 2.2, 4)
        f.text(xa + len(cols) * cw / 2, ya + len(rows) * cw / 2 + 5, f"{len(rows)}x{len(cols)}", 12, C["ink"], bold=True, mono=True)
f.text(x0 + 5 * cw - 1, y0 - 12, "C (5 x 10): output columns", 12, C["line"], "middle", italic=True); f.text(50, y0 + 100, "rows", 12, C["line"], rot=-90)
f.lines(560, 110, ["6 tiles: 4x4, 4x4, 4x2 (top row of tiles)", "           1x4, 1x4, 1x2 (the last row)", "", "each tile is ONE  MM  instruction:", "  M = tile rows, N = tile columns, K = 12", "  A advances by tile-rows x lda,", "  B advances by tile-columns,", "  C by the same offsets with ldc = 10", "", "the strides let one instruction work", "on a piece of a bigger matrix in place,", "with no copying"], 12, C["ink"], "start", lh=17, mono=True); f.save(f"{OUT}/ch11-tiles.svg")
# 11.5 allocation timeline
ev = A["events"]; blocks = A["blocks"]; f = Fig(940, 380, "Scratchpad over time"); f.text(470, 22, "Scratchpad allocation of Running example A: each block is a buffer (address up, allocator event number across); peak 70 words", 14, bold=True)
x0, y0, W_, H_ = 90, 50, 780, 270; amax = 80; ne = len(ev)
f.line(x0, y0, x0, y0 + H_, C["ink"], 1.5); f.line(x0, y0 + H_, x0 + W_, y0 + H_, C["ink"], 1.5)
for av in (0, 20, 40, 60, 70): f.text(x0 - 8, y0 + H_ - av / amax * H_ + 4, str(av), 11, C["line"], "end"); f.line(x0, y0 + H_ - av / amax * H_, x0 + W_, y0 + H_ - av / amax * H_, C["gray2"], 1)
colors = [C["blue2"], C["green2"], C["orange2"], C["purple2"], C["teal2"], C["yellow2"], C["red2"], C["gray2"], C["blue2"]]; strokes = [C["blue"], C["green"], C["orange"], C["purple"], C["teal"], C["gray"], C["red"], C["line"], C["blue"]]
for k, (a, n, t0, t1) in enumerate(sorted(blocks, key=lambda b: b[2])):
    f.rect(x0 + t0 / ne * W_, y0 + H_ - (a + n) / amax * H_, max((t1 - t0) / ne * W_ - 1, 4), n / amax * H_ - 1, colors[k % 9], strokes[k % 9], 1.4, 3); f.text(x0 + (t0 + t1) / 2 / ne * W_, y0 + H_ - (a + n / 2) / amax * H_ + 4, f"{n}", 10.5, C["ink"], bold=True)
f.text(x0 + W_ / 2, y0 + H_ + 24, "allocator events in order (alloc / free)", 12, C["line"]); f.text(30, y0 + H_ / 2, "scratchpad address", 12, C["line"], rot=-90)
f.line(x0, y0 + H_ - 70 / amax * H_, x0 + W_, y0 + H_ - 70 / amax * H_, C["red"], 1.6, "6 4"); f.text(x0 + W_ - 6, y0 + H_ - 70 / amax * H_ - 6, "high-water mark: 70 words (132 without reuse)", 11.5, C["red"], "end", True); f.save(f"{OUT}/ch11-alloc.svg")
# 11.6 concat views
f = Fig(900, 300, "Concatenation costs nothing"); f.text(450, 22, "concat_rows(kc, kn) is a view: both parts are written straight into one buffer, no copy instruction", 14, bold=True)
for r in range(6): f.box(300, 60 + r * 30, 220, 26, [f"row {r}" + ("  (kc)" if r < 5 else "  (kn)")], C["blue2"] if r < 5 else C["orange2"], C["blue"] if r < 5 else C["orange"], 12, r == 5, rx=3)
f.box(30, 120, 190, 50, ["LD kc  (5 x 8)", "writes rows 0..4"], C["gray2"], C["gray"], 12, True); f.arrow(222, 145, 298, 120, C["blue"], 2)
f.box(30, 200, 190, 50, ["LD kn  (1 x 8)", "writes row 5"], C["gray2"], C["gray"], 12, True); f.arrow(222, 225, 298, 215, C["orange"], 2)
f.box(580, 120, 280, 70, ["matmul (q K^T) reads the whole", "6-row buffer as K", "(tb = 1: rows are read as columns)"], C["blue2"], C["blue"], 11.5, True); f.arrow(522, 150, 578, 150, C["ink"], 2)
f.text(450, 280, "Chapter 12 uses exactly this pattern: the KV cache plus the new key and value", 12, C["line"], italic=True); f.save(f"{OUT}/ch11-views.svg")
# 11.7 sweep (example B)
ok = [i for i, v in enumerate(B["high"]) if v is not None]
ch = bar_chart(860, 360, [str(B["hidden"][i]) for i in ok], [[B["high"][i] for i in ok], [B["high0"][i] if B["high0"][i] is not None else 4096 for i in ok]], "Scratchpad words needed against hidden width: with reuse and without (4,096 means: does not fit)", "words", colors=[C["green"], C["red"]], fmt="{:,.0f}", legend=["with buffer reuse", "keeping every buffer"], maxv=4600); ch.save(f"{OUT}/ch11-sweep.svg")
ch = line_chart(860, 340, [B["hidden"][i] for i in ok], [[B["cycles"][i] for i in ok]], "Cycles of the compiled network against hidden width (cycle model; equal to the circuit's counters)", "hidden width", "cycles", [C["blue"]], xfmt="{:g}", yfmt="{:,.0f}", ymin=0); ch.save(f"{OUT}/ch11-cycles.svg")
# 11.8 the five checks
f = Fig(940, 330, "How the compiler is tested"); f.text(470, 22, "Five checks on every random graph, each catching what the others cannot", 14, bold=True)
chk = [("(a) allocator", "compile with and without\nbuffer reuse: same outputs", C["blue2"], C["blue"]), ("(b) meaning", "simulator = integer\ninterpreter, exactly", C["green2"], C["green"]), ("(c) every tensor", "reuse off: each tensor\nequals the interpreter's", C["orange2"], C["orange"]), ("(d) scales", "each tensor within one level\nof its real-number meaning", C["purple2"], C["purple"]), ("(e) accuracy", "within 0.16 of floating\npoint on unseen inputs", C["teal2"], C["teal"])]
for k, (a, b, fl, sk) in enumerate(chk): x = 14 + k * 184; f.box(x, 56, 170, 54, [a], fl, sk, 13.5, True); f.lines(x + 85, 132, b.split("\n"), 11, C["line"], lh=14)
f.box(14, 190, 910, 110, [], "#ffffff", C["gray"]); f.lines(30, 214, ["plus two more: check_errors (five graphs the compiler must REFUSE, so removing a safety check is detected)", "and check_cancellation (one directed graph: add(x, y) with y near -x, which random graphs almost never contain).", "", "27 deliberately broken compilers (the mutation run) are all caught by at least one check."], 12, C["ink"], "start", lh=19); f.save(f"{OUT}/ch11-checks.svg")
print("8 figures written")
