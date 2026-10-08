#!/usr/bin/env python3
"""Chapter 8 figures -> docs/assets/fig/ch08-*.svg. Data-driven ones read out/ch08_example_a.json, ch08_example_b.json, ch08_run_out.txt and ch08_mutation_out.txt."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch08_example_a.json")); B = json.load(open(f"{ROOT}/out/ch08_example_b.json"))
# 8.1 decode step dataflow
f = Fig(900, 420, "One decode step"); f.text(450, 22, "One attention head, one decode step: what is computed, and which parts come from the cache", 14, bold=True)
f.box(20, 60, 120, 50, ["x (new token)", "1 x 16"], C["orange2"], C["orange"], 12.5, True)
for k, (nm, y, col) in enumerate((("q = x Wq", 40, C["blue"]), ("k = x Wk", 100, C["green"]), ("v = x Wv", 160, C["purple"]))):
    f.box(220, y, 120, 44, [nm], {C["blue"]: C["blue2"], C["green"]: C["green2"], C["purple"]: C["purple2"]}[col], col, 13, True, mono=True); f.arrow(142, 85, 218, y + 22, C["ink"], 1.8)
f.box(430, 100, 130, 44, ["append k to K"], C["green2"], C["green"], 12, True); f.box(430, 160, 130, 44, ["append v to V"], C["purple2"], C["purple"], 12, True)
f.arrow(342, 122, 428, 122, C["green"], 2); f.arrow(342, 182, 428, 182, C["purple"], 2)
f.box(430, 250, 130, 56, ["K cache", "L x 16 (from DRAM)"], C["gray2"], C["green"], 12, True); f.box(430, 320, 130, 56, ["V cache", "L x 16 (from DRAM)"], C["gray2"], C["purple"], 12, True)
f.text(300, 262, "the appended rows are also", 11, C["line"], italic=True); f.text(300, 277, "stored (ST) into the caches", 11, C["line"], italic=True); f.arrow(372, 270, 428, 278, C["line"], 1.4)
f.box(640, 40, 150, 44, ["scores = K q / sqrt(d)"], C["blue2"], C["blue"], 12, True); f.arrow(342, 62, 638, 62, C["blue"], 2); f.arrow(562, 278, 700, 86, C["green"], 2)
f.box(640, 120, 150, 44, ["weights = softmax"], C["teal2"], C["teal"], 12.5, True); f.arrow(715, 86, 715, 118, C["ink"], 2)
f.box(640, 200, 150, 44, ["out = weights V"], C["yellow2"], C["gray"], 12.5, True); f.arrow(715, 166, 715, 198, C["ink"], 2); f.arrow(562, 348, 700, 246, C["purple"], 2)
f.text(715, 275, "out: 1 x 16", 12, C["ink"], bold=True, mono=True); f.arrow(715, 246, 715, 262, C["ink"], 1.6)
f.lines(240, 330, ["only x, q, k, v are new work;", "K and V rows of earlier tokens", "are never recomputed"], 11.5, C["line"], lh=15); f.save(f"{OUT}/ch08-step.svg")
# 8.2 attention by hand (example A): bars + causal matrix
f = Fig(900, 400, "Attention on four tokens"); f.text(450, 22, "Running example A: how much token 3 attends to each of the four tokens (weights sum to 1)", 14, bold=True)
for i, (wf, wi) in enumerate(zip(A["w"], A["p16"])):
    x = 60 + i * 110; hb = 200 * wf; f.rect(x, 250 - hb, 44, hb, C["gray"], None, 0, 2); hb2 = 200 * wi / 65536; f.rect(x + 48, 250 - hb2, 44, hb2, C["blue"], None, 0, 2)
    f.text(x + 22, 244 - hb, f"{wf:.3f}", 10, C["line"]); f.text(x + 70, 244 - hb2, f"{wi/65536:.3f}", 10, C["blue"], bold=True); f.text(x + 46, 272, f"token {i}", 12, bold=True)
f.rect(60, 300, 12, 12, C["gray"], None, 0, 2); f.text(78, 311, "real softmax", 11, C["ink"], "start"); f.rect(180, 300, 12, 12, C["blue"], None, 0, 2); f.text(198, 311, "chip integers (Q4.4 scores, integer softmax)", 11, C["ink"], "start")
f.text(500, 60, "the same four tokens as a prefill: causal weights", 12.5, C["ink"], "start", True)
for t in range(4):
    for p in range(4):
        v = A["prefill"][t][p]; fill = "#ffffff" if p > t else f"rgb({int(255-170*v)},{int(255-110*v)},255)"
        f.box(520 + p * 62, 76 + t * 52, 58, 46, [f"{v:.2f}" if p <= t else "masked"], fill, C["gray"], 11.5 if p <= t else 10, p <= t, rx=4, mono=True)
    f.text(505, 104 + t * 52, f"t{t}", 11.5, C["line"], "end", True, True)
f.text(660, 302, "row 3 = the decode step on the left", 11.5, C["blue"], "middle", True); f.text(660, 322, "row t sees only tokens 0..t", 11.5, C["line"], "middle", italic=True)
f.lines(450, 372, ["Decoding the newest token needs only the last row. A causal model never lets a token see the future, so the earlier rows never change."], 12, C["ink"]); f.save(f"{OUT}/ch08-byhand.svg")
# 8.3 cycles per step
xs = list(range(1, 33)); ch = line_chart(860, 380, xs[::3] + [32], [[B["cached"][t - 1] for t in xs[::3] + [32]], [B["recompute"][t - 1] for t in xs[::3] + [32]]], "Cycles of one decode step against the context length t (cycle model, equal to the circuit at t = 2, 4, 8, 16, 32)", "t: tokens of context", "cycles", [C["green"], C["red"]], ["with the KV cache", "recompute K and V"], xfmt="{:g}", yfmt="{:,.0f}", ymin=0); ch.save(f"{OUT}/ch08-steps.svg")
# 8.4 cumulative
cc = [sum(B["cached"][:t]) for t in range(1, 33)]; cr = [sum(B["recompute"][:t]) for t in range(1, 33)]
ch = line_chart(860, 380, xs[::3] + [32], [[cc[t - 1] for t in xs[::3] + [32]], [cr[t - 1] for t in xs[::3] + [32]]], "Total cycles to generate t tokens one after another", "t: tokens generated", "cumulative cycles", [C["green"], C["red"]], ["with the KV cache", "recompute K and V"], xfmt="{:g}", yfmt="{:,.0f}", ymin=0); ch.save(f"{OUT}/ch08-total.svg")
# 8.5 where the cached step goes
txt = open(f"{ROOT}/out/ch08_run_out.txt").read(); rows = re.findall(r"^\s+(MM|LD|RQ|SM|ST)\s+(\d+) instructions\s+(\d+) cycles\s+([\d.]+)%", txt, re.M)
ch = bar_chart(780, 330, [f"{r[0]} x{r[1]}" for r in rows], [[float(r[3]) for r in rows]], "Where one cached decode step at L = 32 spends its cycles (% of 5,249)", "% of cycles", colors=[C["blue"]], fmt="{:.1f}", maxv=70); ch.save(f"{OUT}/ch08-where.svg")
# 8.6 cache size
ch = bar_chart(860, 340, ["GA-2 head", "1B-class", "7B-class", "70B (8 KV heads)"], [[r[1] / 1024 for r in B["rows"]]], "KV cache per token, in KiB (derived: 2 x layers x KV heads x head width x bytes)", "KiB per token", colors=[C["orange"]], fmt="{:,.2f}", maxv=620); ch.save(f"{OUT}/ch08-cache.svg")
# 8.7 mutation matrix
mut = open(f"{ROOT}/out/ch08_mutation_out.txt").read().splitlines(); rows = []
for l in mut:
    m = re.match(r"^(.{66})\s+(caught|NOT CAUGHT)\s+(caught|NOT CAUGHT)\s+(caught|NOT CAUGHT)\s*$", l)
    if m: rows.append((m.group(1).strip(), [m.group(i) == "caught" for i in (2, 3, 4)]))
f = Fig(900, 60 + 20 * len(rows) + 60, "Which check catches which mutant"); f.text(450, 22, "Each row is a broken program builder; green = the check noticed (Chapter 8's mutation table)", 14, bold=True)
for j, nm in enumerate(("(b) accuracy", "(c) exact reference", "(d) stagewise")): f.text(560 + j * 110, 50, nm, 11, C["ink"], "middle", True)
for i, (nm, v) in enumerate(rows):
    y = 58 + i * 20; f.text(20, y + 14, nm[:80], 10, C["ink"], "start")
    for j, ok in enumerate(v): f.box(520 + j * 110, y + 2, 100, 16, ["caught" if ok else "MISSED"], C["green2"] if ok else C["red2"], C["green"] if ok else C["red"], 9.5, not ok, rx=3)
f.text(450, 58 + 20 * len(rows) + 28, "no single check catches every mutant; the union catches all 23", 12.5, C["red"], "middle", True); f.save(f"{OUT}/ch08-mutants.svg")
print("7 figures written", len(rows))
