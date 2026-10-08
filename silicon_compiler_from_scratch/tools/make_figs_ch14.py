#!/usr/bin/env python3
"""Chapter 14 figures -> docs/assets/fig/ch14-*.svg (data from out/ch14_example_b.json and out/ch14_run_out.txt)."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
B = json.load(open(f"{ROOT}/out/ch14_example_b.json")); run = open(f"{ROOT}/out/ch14_run_out.txt").read()
# 14.1 mapping panels
f = Fig(940, 400, "Query heads and key/value groups"); f.text(470, 22, "Which cache does each query head read?", 14, bold=True)
for pi, (G, name) in enumerate(((4, "MHA: G = 4"), (2, "GQA: G = 2"), (1, "MQA: G = 1"))):
    x0 = 20 + pi * 310; f.text(x0 + 140, 56, name, 13.5, bold=True)
    for h in range(4): f.box(x0 + h * 70, 80, 60, 40, [f"Q{h}"], C["blue2"], C["blue"], 13, True)
    for g in range(G):
        w = 70 * (4 // G) - 10; f.box(x0 + g * 70 * (4 // G), 250, w, 60, [f"K{g}, V{g}"], C["orange2"], C["orange"], 13, True)
        for t in range(4 // G):
            h = g * (4 // G) + t; f.arrow(x0 + h * 70 + 30, 122, x0 + g * 70 * (4 // G) + w / 2, 248, C["gray"], 1.8, 7)
    f.text(x0 + 140, 345, f"cache per token: {2*G*4} words", 12, C["line"], italic=True)
f.text(470, 385, "The query heads are all kept; only the keys and values are shared, so the cache shrinks and the attention arithmetic does not.", 12, C["line"], italic=True); f.save(f"{OUT}/ch14-groups.svg")
# 14.2 cache growth
ls = list(range(0, 25, 4)); ch = line_chart(900, 360, ls, [[l * 32 for l in ls], [l * 16 for l in ls], [l * 8 for l in ls]], "Cache words against context length (tiny model, all layers = 1)", "context length (tokens)", "cache words", colors=[C["gray"], C["blue"], C["orange"]], legend=["MHA (G=4)", "GQA (G=2)", "MQA (G=1)"], xfmt="{:.0f}", yfmt="{:.0f}", ymin=0); ch.save(f"{OUT}/ch14-cachegrow.svg")
# 14.3 cycles from the run output
rows = re.findall(r"^\s+(\d)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+) / (\d+) / (\d+)", run, re.M)
ch = bar_chart(780, 340, [f"G={r[0]}" for r in rows], [[int(r[3]) for r in rows], [int(r[4]) for r in rows]], "RTL cycles for one decode step: step 1 and step 24", "cycles", colors=[C["blue"], C["orange"]], fmt="{:,.0f}", legend=["step 1", "step 24"]); ch.save(f"{OUT}/ch14-cycles.svg")
ch = bar_chart(780, 340, [f"G={r[0]}" for r in rows], [[int(r[7]) for r in rows], [int(r[8]) for r in rows], [int(r[6]) for r in rows]], "Where step 24's cycles go: matrix / DMA / vector", "cycles", colors=[C["blue"], C["green"], C["orange"]], fmt="{:,.0f}", legend=["DMA", "vector", "matrix"]) if False else None
mm = [int(r[6]) for r in rows]; dm = [int(r[7]) for r in rows]; vc = [int(r[8]) for r in rows]
ch = bar_chart(780, 340, [f"G={r[0]}" for r in rows], [mm, dm, vc], "Where step 24's cycles go", "cycles", colors=[C["blue"], C["green"], C["orange"]], fmt="{:,.0f}", legend=["matrix", "DMA", "vector"]); ch.save(f"{OUT}/ch14-where.svg")
# 14.4 pooling accuracy
eps = sorted(B["eps"], key=float); xs = [float(e) for e in eps]
ch = line_chart(900, 380, xs, [[B["eps"][e][0] for e in eps], [B["eps"][e][2] for e in eps]], "Agreement with MHA after mean-pooling, by head similarity", "eps: relative difference between the K/V heads of a group", "agreement with MHA (%)", colors=[C["blue"], C["orange"]], legend=["G = 2", "G = 1"], xfmt="{:g}", yfmt="{:.0f}", ymin=0, ymax=100); ch.save(f"{OUT}/ch14-pool.svg")
# 14.5 serving
names = list(B["serve"]); ch = bar_chart(860, 340, names, [[B["serve"][n][2] for n in names]], "Largest batch that fits in 80 GB (7B-class model, context 8192; derived)", "requests", colors=[C["blue"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch14-batch.svg")
ch = bar_chart(860, 340, names, [[B["serve"][n][4] for n in names]], "Tokens per second at that batch (2 TB/s assumed; derived)", "tokens/s", colors=[C["green"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch14-tps.svg")
print("ch14 figures written")
