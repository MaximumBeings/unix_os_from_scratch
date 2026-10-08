#!/usr/bin/env python3
"""Chapter 6 figures -> docs/assets/fig/ch06-*.svg, from the golden model and the example JSON files."""
import json, math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig"); sys.path.insert(0, os.path.join(ROOT, "model")); import softmax_gold as g
A = json.load(open(f"{ROOT}/out/ch06_example_a.json")); B = json.load(open(f"{ROOT}/out/ch06_example_b.json"))
# 6.1 pipeline with numbers
f = Fig(900, 400, "Softmax in five steps"); f.text(450, 22, "The five steps of the integer softmax, with the numbers of Running example A", 14, bold=True)
st = [("1 maximum", f"m = {max(A['xs'])}", C["blue2"], C["blue"]), ("2 lookup", "e = T[m - x]", C["green2"], C["green"]), ("3 sum", "s = sum e", C["orange2"], C["orange"]), ("4 divide once", "r = 2^38 / s", C["purple2"], C["purple"]), ("5 scale", "p = e*r >> 22", C["teal2"], C["teal"])]
for k, (a, b, fl, sk) in enumerate(st):
    x = 14 + k * 176; f.box(x, 50, 160, 56, [a], fl, sk, 14, True, sub=b)
    if k < 4: f.arrow(x + 162, 78, x + 174, 78, C["ink"], 2)
rows = [("x (scores)", A["xs"]), ("d = m - x", A["d"]), ("e = T[d]", A["e"]), ("p (Q0.16)", A["p"])]
f.text(60, 150, "", 11)
for r, (nm, vals) in enumerate(rows):
    y = 135 + r * 50; f.text(150, y + 24, nm, 13, C["line"], "end", True, True)
    for j, v in enumerate(vals): f.box(170 + j * 120, y, 110, 38, [str(v)], "#ffffff", C["gray"], 15, True, mono=True)
f.text(710, 190, f"s = {A['s']}", 14, C["orange"], "start", True, True); f.text(710, 214, f"r = {A['r']}", 14, C["purple"], "start", True, True); f.text(710, 238, f"sum p = {sum(A['p'])}", 13, C["line"], "start", False, True)
f.lines(450, 360, ["Subtracting the maximum first (step 2) makes every table index non-negative and every entry fit 16 bits.", "Dividing ONCE (step 4) and multiplying N times (step 5) is much cheaper than dividing N times."], 12.5, C["ink"], lh=19); f.save(f"{OUT}/ch06-steps.svg")
# 6.2 exp table
ds = list(range(0, 256, 32)) + [255]; ch = line_chart(860, 380, ds, [[g.T[d] for d in ds]], "The exp table: T[d] = round(65535 exp(-d/16))", "d = max - x   (real distance below the maximum = d / 16)", "T[d]", [C["blue"]], marks=False, xfmt="{:g}", yfmt="{:,.0f}")
ch.line(70 + (860 - 94) * 189 / 255, 40, 70 + (860 - 94) * 189 / 255, 316, C["red"], 2, "6 4"); ch.text(70 + (860 - 94) * 189 / 255 - 6, 70, "from d = 189 the entry is 0", 12, C["red"], "end", True); ch.text(70 + (860 - 94) * 189 / 255 - 6, 88, "(exp(-11.8) = 7.4e-6 is below half a unit)", 11, C["red"], "end", italic=True); ch.save(f"{OUT}/ch06-table.svg")
# 6.3 restoring division trace: 22 / 5 in 5 bits
num, den, W = 22, 5, 5; rem = 0; q = 0; rowsd = []
for i in range(W - 1, -1, -1):
    bit = (num >> i) & 1; r1 = (rem << 1) | bit; fits = r1 >= den; r2 = r1 - den if fits else r1; q = (q << 1) | int(fits); rowsd.append((W - 1 - i, bit, r1, fits, r2, q)); rem = r2
assert (q, rem) == divmod(num, den)
f = Fig(860, 360, "Restoring division, one bit per clock"); f.text(430, 22, f"Restoring division of {num} ({num:05b}) by {den} ({den:03b}), one numerator bit per clock", 14, bold=True)
hd = ["clock", "bring in bit", "remainder x2 + bit", f"fits? (>= {den})", "new remainder", "quotient so far"]; xs = [20, 100, 220, 370, 560, 690]; ws = [76, 116, 146, 186, 126, 150]
for x, w, h in zip(xs, ws, hd): f.box(x, 44, w, 32, [h], C["blue2"], C["blue"], 11.5, True, rx=4)
for k, (i, bit, r1, fits, r2, qq) in enumerate(rowsd):
    y = 84 + k * 40; vals = [str(i), str(bit), str(r1), "yes: subtract, bit 1" if fits else "no: keep, bit 0", str(r2), format(qq, f"0{i+1}b")]
    for x, w, v in zip(xs, ws, vals): f.box(x, y, w, 32, [v], C["green2"] if (v.startswith("yes")) else "#ffffff", C["green"] if v.startswith("yes") else C["gray"], 11.5, False, mono=True, rx=4)
f.lines(430, 310, [f"Result: quotient {q} (= {q:05b}), remainder {rem}.  {num} = {q} x {den} + {rem}.", "The real divider does the same with 40 bits in 40 clocks: shift, compare, subtract, write one quotient bit."], 12.5, C["ink"], lh=19); f.save(f"{OUT}/ch06-divider.svg")
# 6.4 cycle breakdown
f = Fig(900, 300, "Where the cycles go"); f.text(450, 22, "Softmax latency = 3N + 43: the 40-cycle divider is fixed, the three passes grow with N", 14, bold=True)
for row, N in enumerate((8, 64)):
    parts = [("max", N, C["blue2"], C["blue"]), ("exp+sum", N, C["green2"], C["green"]), ("divide", 40, C["purple2"], C["purple"]), ("scale", N, C["teal2"], C["teal"]), ("ctl", 3, C["gray2"], C["gray"])]
    tot = sum(p[1] for p in parts); sc = 760 / 235; x = 70; y = 60 + row * 100; f.text(40, y + 22, f"N={N}", 13, C["ink"], "end", True, True)
    for nm, c, fl, sk in parts:
        w = c * sc; f.box(x, y, w, 40, [nm if w > 38 else ""], fl, sk, 11, True, rx=3); 
        if w > 38: f.text(x + w / 2, y + 56, str(c), 10.5, C["line"])
        x += w
    f.text(x + 8, y + 26, f"= {tot}", 13, C["red"], "start", True)
f.text(450, 270, "For N = 8 the divider is 60% of the time; for N = 64 it is 17%. Pipelining the three passes (chapter 8 and beyond) is where a real chip saves cycles.", 12, C["line"], italic=True); f.save(f"{OUT}/ch06-cycles.svg")
# 6.5 fixed vs real bars (example A)
f = Fig(780, 340, "Fixed against real"); f.text(390, 22, "Probabilities of Running example A: fixed-point (circuit) next to the real softmax", 14, bold=True)
for i, (pf, px) in enumerate(zip(A["pf"], A["p"])):
    x = 90 + i * 160; hb = 220 * pf; f.rect(x, 280 - hb, 50, hb, C["gray"], None, 0, 2); hb2 = 220 * px / 65536; f.rect(x + 56, 280 - hb2, 50, hb2, C["blue"], None, 0, 2)
    f.text(x + 25, 274 - hb, f"{pf:.4f}", 10, C["line"]); f.text(x + 81, 274 - hb2, f"{px/65536:.4f}", 10, C["blue"], bold=True); f.text(x + 53, 300, f"x = {A['xs'][i]}", 12, C["ink"], bold=True, mono=True)
f.rect(500, 60, 12, 12, C["gray"], None, 0, 2); f.text(518, 71, "real softmax (float)", 11, C["ink"], "start"); f.rect(500, 82, 12, 12, C["blue"], None, 0, 2); f.text(518, 93, "circuit (Q0.16)", 11, C["ink"], "start"); f.save(f"{OUT}/ch06-compare.svg")
# 6.6 temperature (example B, circuit)
f = Fig(920, 420, "Temperature sweep"); f.text(450, 22, "The same eight logits at five sharpness settings k (circuit output): the distribution gets sharper", 14, bold=True)
for r, (k, p) in enumerate(zip(B["ks"], B["p"])):
    y0 = 50 + r * 72; f.text(84, y0 + 34, f"k = {k:g}", 13, C["ink"], "end", True, True)
    for j, v in enumerate(p): hb = 56 * v / 65536; f.rect(100 + j * 100, y0 + 60 - hb, 70, hb, C["blue"] if j else C["orange"], None, 0, 2); f.text(135 + j * 100, y0 + 56 - hb, f"{v/65536:.3f}", 10, C["ink"])
for j, v in enumerate([2.0, 1.0, 0.5, 0.0, -0.5, -1.0, -2.0, -3.0]): f.text(135 + j * 100, 412, f"logit {v:g}", 11, C["line"])
f.save(f"{OUT}/ch06-temperature.svg")
# 6.7 accuracy study (from out/ch06_study_out.txt)
txt = open(f"{ROOT}/out/ch06_study_out.txt").read(); rows = re.findall(r"^\s+(\d+) ([a-z0-9., -]+?)\s+3000\s+([\d.e+-]+)\s+([\d.]+)\s+([\d.e+-]+)", txt, re.M)
ch = bar_chart(860, 360, [f"N={r[0]} {r[1].split()[0]}" for r in rows], [[float(r[3]) for r in rows]], "Largest error of the fixed-point softmax, in units of 2^-16 (measured, 3000 vectors each)", "units of 2^-16", colors=[C["blue"]], fmt="{:.1f}"); ch.save(f"{OUT}/ch06-error.svg")
print("7 figures written")
