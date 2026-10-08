#!/usr/bin/env python3
"""Chapter 13 figures -> docs/assets/fig/ch13-*.svg. Read out/ch13_example_a.json and out/ch13_example_b_out.txt."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch13_example_a.json")); B = open(f"{ROOT}/out/ch13_example_b_out.txt").read()
# 13.1 the packed word
f = Fig(940, 330, "One packed word"); f.text(470, 22, "Eight int4 weights in one 32-bit word (Running example A)", 14, bold=True)
w = A["word"]; q = A["q4"]
for j in range(8):
    x = 40 + (7 - j) * 108; nib = (w >> (4 * j)) & 15
    f.box(x, 60, 100, 56, [f"{nib:04b}"], C["blue2"] if q[j] >= 0 else C["orange2"], C["blue"] if q[j] >= 0 else C["orange"], 15, True, mono=True)
    f.text(x + 50, 136, f"element {j}", 11.5, C["line"]); f.text(x + 50, 156, f"bits {4*j+3}..{4*j}", 11, C["line"], mono=True)
    f.arrow(x + 50, 168, x + 50, 200, C["ink"], 1.8); f.box(x + 10, 204, 80, 40, [f"{q[j]:+d}"], C["green2"], C["green"], 16, True, mono=True)
f.text(470, 272, f"word = 0x{w:08X}   (negative values are two's complement in 4 bits; blue = non-negative, orange = negative)", 12, C["line"], italic=True)
f.text(470, 300, f"real weights {A['w']}  with scale {A['sc4']:.4f}", 11.5, C["line"], italic=True, mono=True); f.save(f"{OUT}/ch13-word.svg")
# 13.2 the data path
f = Fig(940, 300, "LD then UNPACK"); f.text(470, 22, "What the compiler emits for one int4 weight: load the packed words, expand them, release the staging area", 14, bold=True)
bx = [(30, "external memory", "n/8 packed words", C["gray2"], C["gray"]), (270, "staging area", "n/8 words (scratchpad)", C["orange2"], C["orange"]), (510, "weight buffer", "n int8-valued words", C["green2"], C["green"]), (750, "MM / RQ as before", "operand of the matmul", C["blue2"], C["blue"])]
for x, t, s, fl, st in bx: f.box(x, 90, 170, 80, [t], fl, st, 13, True, sub=s)
for a, b, t in ((200, 270, "LD  (len+9 cycles)"), (440, 510, "UNPACK  (10/word + 1)"), (680, 750, "")):
    f.arrow(a + 2, 130, b - 2, 130, C["ink"], 2.2)
f.text(235, 78, "LD", 12, C["blue"], bold=True, mono=True); f.text(475, 78, "UNPACK", 12, C["orange"], bold=True, mono=True)
f.text(470, 230, "The scratchpad has ONE write port: expanding a word takes eight writes, so UNPACK costs at least 8 cycles per word (10 as built).", 12.5, C["line"], italic=True)
f.text(470, 256, "The saving is on the external link only: 8 weights cross it as one word, but all 8 are still written into the scratchpad.", 12.5, C["line"], italic=True); f.save(f"{OUT}/ch13-path.svg")
# 13.3 granularity study
rows = re.findall(r"^(per-\w+(?: \d+)?)\s+(\d+)\s+([\d.]+)%\s+([\d.]+)%", B, re.M); lab = [r[0].replace("per-", "") for r in rows]
ch = bar_chart(900, 340, lab, [[float(r[3]) for r in rows]], "int4 error of a 64x32 Gaussian matrix by scale granularity (% of output)", "error %", colors=[C["blue"]], fmt="{:.1f}"); ch.save(f"{OUT}/ch13-granularity.svg")
out_rows = re.findall(r"^(per-\w+(?: \d+)?)\s+([\d.]+)%\s+([\d.]+)%\s*$", B.split("outlier column")[1].split("== 2")[0], re.M)
ch = bar_chart(900, 340, [r[0].replace("per-", "") for r in out_rows], [[float(r[2]) for r in out_rows]], "The same with one column 8x larger (outlier): one scale per tensor collapses", "error %", colors=[C["orange"]], fmt="{:.1f}"); ch.save(f"{OUT}/ch13-outlier.svg")
# 13.4 break-even
bs = [0.1 * 1.35 ** i for i in range(16)]
def t8(b): return 1 / b
def t4(b, u): return 1 / (8 * b) + u / 8
ch = line_chart(900, 380, [round(b, 3) for b in bs], [[t8(b) for b in bs], [t4(b, 10) for b in bs], [t4(b, 4) for b in bs], [t4(b, 1) for b in bs]], "Cycles per weight against external bandwidth b (words per cycle): int8, and int4 with u = 10, 4, 1", "bandwidth b (words per cycle, log scale)", "cycles per weight", colors=[C["gray"], C["red"], C["orange"], C["green"]], legend=["int8", "int4, u = 10 (as built)", "int4, u = 4", "int4, u = 1"], logx=True, logy=True, xfmt="{:.2g}", yfmt="{:.2g}"); ch.save(f"{OUT}/ch13-breakeven.svg")
# 13.5 cycles
ch = bar_chart(760, 340, ["int8", "int4"], [[7000, 7885], [9039, 9924]], "Cycles for one decode step on the RTL (step 1 and step 24)", "cycles", colors=[C["blue"], C["orange"]], fmt="{:,.0f}", legend=["step 1", "step 24"]); ch.save(f"{OUT}/ch13-cycles.svg")
print("ch13 figures written")
