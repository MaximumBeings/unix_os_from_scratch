#!/usr/bin/env python3
"""Chapter 16 figures -> docs/assets/fig/ch16-*.svg (data from out/ch16_example_a.json, ch16_example_b.json, ch16_run_out.txt)."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch16_example_a.json")); B = json.load(open(f"{ROOT}/out/ch16_example_b.json")); run = open(f"{ROOT}/out/ch16_run_out.txt").read()
# 16.1 one block
f = Fig(940, 470, "One speculative block"); f.text(470, 22, "Two blocks of Running example A: guess with the cheap draft, check all guesses in one pass of the target", 14, bold=True)
for bi, tr in enumerate((A["trace"][0], A["trace"][1])):
    y0 = 50 + bi * 205; blk = tr["block"]; ch = tr["choice"]; acc = tr["accepted"]
    f.text(20, y0 + 16, f"block {bi + 1}", 13, C["ink"], anchor="start", bold=True)
    f.text(20, y0 + 52, "draft guesses", 11.5, C["line"], anchor="start")
    f.text(20, y0 + 120, "target's choice", 11.5, C["line"], anchor="start"); f.text(20, y0 + 94, "block fed to target", 11.5, C["line"], anchor="start")
    for i, t in enumerate(blk):
        x = 190 + i * 150; f.box(x, y0 + 34, 110, 30, [str(t) if i == 0 else f"guess {t}"], C["gray2"] if i == 0 else C["blue2"], C["gray"] if i == 0 else C["blue"], 12.5, True)
        f.box(x, y0 + 76, 110, 30, [f"row {i}: {t}"], C["gray2"], C["gray"], 12, False)
        ok = i < acc + 1; good = i < len(ch) and (i == acc or (i < len(blk) - 1 and blk[i + 1] == ch[i]))
        col = C["green"] if i < acc else (C["orange"] if i == acc else C["gray"]); fl = C["green2"] if i < acc else (C["orange2"] if i == acc else C["gray2"])
        f.box(x, y0 + 118, 110, 30, [f"-> {ch[i]}"], fl, col, 13, True); f.arrow(x + 55, y0 + 108, x + 55, y0 + 116, C["ink"], 1.4, 6)
    f.text(190 + 4 * 150 - 20, y0 + 170, f"{acc} guesses accepted + 1 token from the target = {acc + 1} tokens: {tr['new']}", 12.5, C["ink"], anchor="end", bold=True)
f.text(470, 455, "green: a guess the target agrees with; orange: the target's own token (free); grey: rows after a disagreement are discarded", 11.5, C["line"], italic=True); f.save(f"{OUT}/ch16-block.svg")
# 16.2 mask
c, m = 4, 4; f = Fig(900, 380, "Causal mask by slicing"); f.text(450, 22, "Which keys does each block row see? Prefix slices of the cache-plus-block tensor (cache 4 rows, block 4 rows)", 14, bold=True)
for j in range(c + m): f.box(210 + j * 62, 46, 54, 28, ["cache %d" % j if j < c else "blk %d" % (j - c)], C["gray2"] if j < c else C["orange2"], C["gray"] if j < c else C["orange"], 10.5, True)
for i in range(m):
    y = 92 + i * 60; f.text(180, y + 22, f"row {i}", 13, anchor="end", bold=True)
    for j in range(c + m):
        see = j <= c + i; f.rect(210 + j * 62, y, 54, 44, C["green2"] if see else C["gray2"], C["green"] if see else C["gray"], 1.4, 4)
        f.text(237 + j * 62, y + 27, "yes" if see else "-", 12, C["green"] if see else C["gray"], bold=see)
    f.text(210 + (c + m) * 62 + 8, y + 26, f"slice_rows(K, 0, {c + i + 1})", 11, C["line"], anchor="start", mono=True)
f.text(450, 350, "No mask is computed and nothing is copied: row i's keys are the first c+i+1 rows of K, a view of the same buffer.", 12, C["line"], italic=True); f.save(f"{OUT}/ch16-mask.svg")
# 16.3 cost of a block
cv = A["verify"][0]; cs = A["single"]
ch = bar_chart(780, 340, ["one verifier block (m = 4)", "four single-token steps"], [[cv, sum(cs)]], "RTL-validated cycles: verify four positions at once or one at a time (cache 4 to 8 rows)", "cycles", colors=[C["blue"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch16-cost.svg")
# 16.4 speedup grid
ks = [1, 2, 3, 4, 5]; nzs = [0, 3, 4, 5, 6]
ch = line_chart(900, 380, ks, [[B["grid"][f"{n}/{k}"][1] for k in ks] for n in nzs], "Measured speedup over plain decoding (RTL-validated cycles) against k, for drafts of decreasing quality", "k (guesses per block)", "speedup", colors=[C["green"], C["blue"], C["orange"], C["red"], C["gray"]], legend=[f"draft noise {n}" for n in nzs], xfmt="{:.0f}", yfmt="{:.1f}", ymin=0.3); ch.save(f"{OUT}/ch16-grid.svg")
# 16.5 serving: speedup against k for a in .6 .8 .9 at c = 0.05
ks2 = list(range(1, 13))
def sp(a, k, c): return (1 - a ** (k + 1)) / (1 - a) / (1 + k * c)
ch = line_chart(900, 380, ks2, [[sp(a, k, 0.05) for k in ks2] for a in (0.6, 0.8, 0.9)], "Derived speedup against k for a draft that costs 5% of the target (acceptance a = 0.6, 0.8, 0.9)", "k (guesses per block)", "speedup", colors=[C["gray"], C["blue"], C["orange"]], legend=["a = 0.6", "a = 0.8", "a = 0.9"], xfmt="{:.0f}", yfmt="{:.1f}", ymin=1); ch.save(f"{OUT}/ch16-best-k.svg")
# 16.6 RTL totals
rows = re.findall(r"^(.{44}) +(True|False) +(\S+) +(\S+) +(\d+) +(\d+) +([\d.]+)x", run, re.M)
names = ["plain", "k=3 perfect", "k=3 noise 3", "k=3 noise 4", "random: plain", "random: k=1", "random: k=3"]
ch = bar_chart(900, 360, names, [[int(r[5]) for r in rows]], "RTL cycles for 32 tokens (both simulators agree to the cycle)", "cycles", colors=[C["blue"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch16-rtl.svg")
print("ch16 figures written", len(rows))
