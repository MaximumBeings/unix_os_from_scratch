#!/usr/bin/env python3
"""Chapter 3 figures -> docs/assets/fig/ch03-*.svg. The charts read out/ch03_example_b.json (written by ch03_example_b.py)."""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig"); D = json.load(open(os.path.join(ROOT, "out", "ch03_example_b.json")))
# 3.1 number line: reals -> int8 levels
f = Fig(820, 330, "Real numbers mapped onto 255 int8 levels"); f.text(410, 22, "Symmetric int8: the real line is cut into steps of size scale; the largest |x| lands on +-127", 14, bold=True)
x0, x1, y = 70, 760, 120; f.line(x0, y, x1, y, C["ink"], 2.5)
sc = 2.75 / 127
def px(v): return (x0 + x1) / 2 + v / 2.75 * (x1 - x0) / 2
for q in range(-127, 128):
    v = q * sc; major = q % 32 == 0 or abs(q) == 127; f.line(px(v), y - (14 if major else 5), px(v), y + (14 if major else 5), C["blue"] if major else C["gray"], 1.6 if major else 0.8)
for q in (-127, -64, 0, 64, 127): f.text(px(q * sc), y + 34, str(q), 13, C["blue"], bold=True, mono=True)
f.text(px(-127 * sc), y + 56, f"x = {-127*sc:.2f}", 11, C["line"]); f.text(px(0), y + 56, "x = 0", 11, C["line"]); f.text(px(127 * sc), y + 56, f"x = {127*sc:.2f}", 11, C["line"]); f.text(30, y + 36, "q =", 12, C["line"], "end", italic=True)
for v, col, dx in ((-1.30, C["orange"], 0), (0.42, C["green"], 50), (-0.08, C["purple"], -50)):
    q = round(v / sc); f.circ(px(v), y, 6, col, "#fff", 1.5); f.line(px(v), y - 8, px(v), y - 52, col, 1.6, "4 3"); f.text(px(v) + dx, y - 60, f"x = {v:+.2f}", 12, col, bold=True); f.text(px(v) + dx, y - 76, f"-> q = {q}", 12, col, mono=True)
f.box(40, 214, 740, 90, [], "#ffffff", C["gray"]); f.lines(410, 236, [f"scale = max|x| / 127 = 2.75 / 127 = {sc:.5f}.   There are 255 levels (-127 .. +127); -128 is deliberately unused.", "q = clamp(round(x / scale), -127, 127);   the value represented is q x scale.", "A value is never wrong by more than half a step (here 0.0108), except where it is clamped.", "The step is the same everywhere on the line: small numbers get no extra precision."], 12.5, C["ink"], lh=20); f.save(f"{OUT}/ch03-numberline.svg")
# 3.2 int8 layer pipeline with real example numbers
f = Fig(860, 330, "The int8 layer pipeline"); f.text(430, 22, "One layer: int8 x int8 -> exact int32 -> requantizer -> int8 (numbers from Running example A)", 14, bold=True)
bx = [(20, "real X, W", "floats", C["gray2"], C["gray"]), (190, "quantize", "q = round(x/scale)", C["blue2"], C["blue"]), (360, "int8 matmul", "acc = sum Xq * Wq", C["green2"], C["green"]), (530, "requantizer", "q = round(acc * m / 2^s)", C["orange2"], C["orange"]), (700, "int8 output", "scale_out", C["purple2"], C["purple"])]
for x, a, b, fl, st in bx: f.box(x, 56, 140, 70, [a], fl, st, 13, True, sub=b)
for x in (162, 332, 502, 672): f.arrow(x, 91, x + 26, 91, C["ink"], 2.2)
f.lines(260, 152, ["Xq = [[32, -64, 95], [127, 16, -48]]", "Wq = [[127, -42], [-64, 95], [32, -116]]"], 11.5, C["line"], mono=True, lh=16)
f.lines(430, 152, [""], 11); f.text(430, 200, "acc = [[11200, -18444], [13569, 1754]]   (32-bit, exact)", 12.5, C["green"], bold=True, mono=True)
f.text(430, 232, "M = scale_x x scale_w / scale_out = 0.00687  ~  m / 2^s,   m = 14757225, s = 31", 12.5, C["orange"], bold=True, mono=True)
f.text(430, 264, "q = [[77, -127], [93, 12]]   (int8)  ->  real = q x scale_out = [[1.67, -2.75], [2.01, 0.26]]", 12.5, C["purple"], bold=True, mono=True)
f.text(430, 306, "The scales are bookkeeping: the chip only ever sees the integers Xq, Wq, m, s.", 12.5, C["line"], italic=True); f.save(f"{OUT}/ch03-pipeline.svg")
# 3.3 requantizer datapath
f = Fig(860, 330, "Inside the requantizer"); f.text(430, 22, "rtl/requant.v, step by step (all combinational)", 14, bold=True)
st = [("|acc|", "negate if acc < 0", "32 bits"), ("x m", "32 x 24 multiply", "up to 56 bits"), ("+ 2^(s-1)", "half a unit of the\nlast kept bit", "57 bits"), (">> s", "shift right by s", "drops the fraction"), ("clamp", "magnitude to 127", "7 bits"), ("sign, relu", "restore sign;\nrelu zeroes negatives", "int8")]
for k, (a, b, c) in enumerate(st):
    x = 14 + k * 140; f.box(x, 70, 118, 70, [a], C["blue2"] if k not in (2, 4) else C["orange2"], C["blue"] if k not in (2, 4) else C["orange"], 15, True)
    f.lines(x + 59, 164, b.split("\n"), 11, C["line"], lh=14); f.text(x + 59, 206, c, 11, C["purple"], italic=True)
    if k < 5: f.arrow(x + 120, 105, x + 138, 105, C["ink"], 2)
f.text(430, 56, "acc (int32), m (24 bit), s (6 bit), relu  ->  q (int8)", 12.5, C["line"], italic=True)
f.lines(430, 250, ["Rounding the MAGNITUDE (steps 1-4) and putting the sign back at the end gives ties-away-from-zero and requant(-x) = -requant(x).", "Adding 2^(s-1) before the shift is how 'round to nearest' is done with a shift: the +half pushes anything at or above .5 up to the next integer."], 12, C["ink"], lh=18)
f.text(430, 306, "orange = the two steps whose mistakes the mutation table catches most often (rounding constant, clamp limit)", 11.5, C["orange"], italic=True); f.save(f"{OUT}/ch03-requant.svg")
# 3.4 rounding comparison
f = Fig(780, 300, "Rounding the half-way cases"); f.text(390, 22, "acc x M for M = 1/2: where do the ties go?", 14, bold=True)
accs = [-5, -3, -1, 1, 3, 5]; away = [-3, -2, -1, 1, 2, 3]; even = [round(a / 2) for a in accs]
f.text(40, 78, "acc / 2", 12, C["line"], "start", italic=True); f.text(40, 138, "away from zero (the chip)", 12, C["green"], "start", True); f.text(40, 198, "nearest even (Python round)", 12, C["orange"], "start", True)
for k, a in enumerate(accs):
    x = 300 + k * 78; f.box(x, 56, 64, 36, [f"{a/2:+.1f}"], C["gray2"], C["gray"], 14, True, mono=True)
    f.arrow(x + 32, 94, x + 32, 112, C["green"], 1.8); f.box(x, 114, 64, 36, [f"{away[k]:+d}"], C["green2"], C["green"], 15, True, mono=True)
    f.arrow(x + 32, 152, x + 32, 172, C["orange"], 1.8); f.box(x, 174, 64, 36, [f"{even[k]:+d}"], C["orange2"], C["orange"], 15, True, mono=True)
f.lines(390, 250, ["Away from zero is symmetric (requant(-x) = -requant(x)) and cheap: round the magnitude.", "Nearest-even removes a tiny bias but needs a sticky-bit comparison; this chip does not use it. Either works if everyone agrees."], 12, C["ink"], lh=18); f.save(f"{OUT}/ch03-rounding.svg")
# 3.5 outlier sweep (measured)
ch = line_chart(820, 380, D["f"], [[v * 100 for v in D["tensor"]], [v * 100 for v in D["column"]]], "Error on the normal columns as one column grows by a factor f (measured)", "f: how many times larger the outlier column is", "relative RMS error, %", [C["red"], C["green"]], ["one scale per tensor", "one scale per column"], logx=True, xfmt="{:g}", yfmt="{:.0f}"); ch.save(f"{OUT}/ch03-outlier.svg")
# 3.6 level usage histogram
fig = Fig(820, 360, "Levels used by a normal column"); fig.text(410, 22, "A normal weight column's int8 values: f = 1 versus f = 32 outlier in the tensor (measured)", 14, bold=True)
for row, (key, ttl, col, y0) in enumerate((("hist1", "f = 1: values spread over the range", C["green"], 50), ("hist32", "f = 32, one scale per tensor: values squeezed into the middle", C["red"], 205))):
    h = dict(D[key]); mx = max(h.values()); L, Rr = 60, 780
    fig.text(60, y0, ttl, 13, col, "start", True); base = y0 + 112; fig.line(L, base, Rr, base, C["ink"], 1.5)
    for q in range(-127, 128):
        c = h.get(q, 0)
        if c: bh = 90 * c / mx; xx = L + (q + 127) / 254 * (Rr - L); fig.rect(xx - 1.2, base - bh, 3.2, bh, col, None, 0, 0)
    for q in (-127, -64, 0, 64, 127): fig.text(L + (q + 127) / 254 * (Rr - L), base + 16, str(q), 11, C["line"], mono=True)
fig.save(f"{OUT}/ch03-levels.svg")
# 3.7 memory
ch = bar_chart(760, 330, ["fp32", "fp16", "int8", "int4"], [[28, 14, 7, 3.5]], "A 7-billion-parameter model: weight storage in GB (derived: 7e9 x bytes per weight)", "GB", colors=[C["blue"]], fmt="{:.1f}", maxv=32); ch.save(f"{OUT}/ch03-memory.svg")
print("7 figures written")
