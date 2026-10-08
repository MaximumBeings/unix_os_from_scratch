#!/usr/bin/env python3
"""Chapter 17 figures -> docs/assets/fig/ch17-*.svg (data from out/ch17_example_a.json, ch17_example_b.json, ch17_run_out.txt)."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch17_example_a.json")); B = json.load(open(f"{ROOT}/out/ch17_example_b.json")); run = open(f"{ROOT}/out/ch17_run_out.txt").read()
# 17.1 the two programs
f = Fig(940, 400, "A step is two programs"); f.text(470, 22, "One decode step of the mixture-of-experts model: two programs and a decision in between", 14, bold=True)
f.box(30, 100, 220, 120, ["program A"], C["blue2"], C["blue"], 14, True, sub="attention, residual, router"); f.box(690, 100, 220, 120, ["program B_e"], C["green2"], C["green"], 14, True, sub="expert e, residual, output")
f.box(330, 112, 280, 96, ["host: dispatch"], C["orange2"], C["orange"], 14, True, sub="reads the route, picks the program")
f.arrow(252, 160, 328, 160, C["ink"], 2.2); f.text(290, 148, "h, route", 11, C["line"], italic=True); f.arrow(612, 160, 688, 160, C["ink"], 2.2); f.text(650, 148, "h", 11, C["line"], italic=True)
for e in range(4):
    x = 330 + e * 72; used = e == 1; f.box(x, 270, 64, 56, [f"expert {e}"], C["green2"] if used else C["gray2"], C["green"] if used else C["gray"], 11, used, sub="loaded" if used else "not touched")
f.arrow(470, 210, 400 + 72 * 0 + 0, 268, C["orange"], 1.6, 7) if False else f.arrow(470, 210, 398 + 72 * 0 + 36, 266, C["orange"], 1.8, 8)
f.text(470, 360, "The chip has no branch instruction: the choice of expert is made by the host between two programs. The experts that are not chosen are never loaded.", 12, C["line"], italic=True); f.save(f"{OUT}/ch17-step.svg")
# 17.2 router heat grid
rows = A["router"]; f = Fig(900, 470, "Router logits"); f.text(450, 22, "The router's logits for each of the 16 tokens (context 1): the winner is outlined", 14, bold=True)
f.text(238, 52, "token", 12, anchor="end", bold=True)
for e in range(4): f.text(300 + e * 110, 52, f"expert {e}", 12, bold=True)
for i, (t, r, w) in enumerate(rows):
    y = 62 + i * 24; f.text(238, y + 16, f"{t}", 12, anchor="end", mono=True, bold=True)
    for e in range(4):
        v = max(0.0, min(1.0, r[e] / 2.2)); col = f"rgb({int(251 - 200 * v)},{int(252 - 120 * v)},{int(254 - 40 * v)})"
        f.rect(250 + e * 110 + 0, y, 100, 22, col, C["green"] if e == w else "#d6dee8", 2.4 if e == w else 1, 3); f.text(300 + e * 110, y + 16, f"{r[e]:+.2f}", 11.5, bold=(e == w))
f.text(450, 458, "token t belongs to group t mod 4 and is routed to expert t mod 4 (the designed rule)", 11.5, C["line"], italic=True); f.save(f"{OUT}/ch17-router.svg")
# 17.3 cost
nums = {k: int(v) for k, v in re.findall(r"^\s+(?:\w[\w\s()-]*?)\s+(?:Icarus)\s+PASS.*?, (\d+) cycles", run, re.M) and []} if False else None
tm = re.findall(r"^\s{2}(routed \(A then B_e\)|dense-all \(every expert\)|Chapter 12 model \(1 FFN\))\s+Icarus\s+PASS.*?(\d+) cycles in total", run, re.M); d = {k: int(v) for k, v in tm}
ch = bar_chart(800, 340, ["one FFN (Ch. 12)", "4 experts, routed", "4 experts, dense"], [[d["Chapter 12 model (1 FFN)"], d["routed (A then B_e)"], d["dense-all (every expert)"]]], "RTL cycles for 24 decode steps (identical in both simulators)", "cycles", colors=[C["blue"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch17-cost.svg")
# 17.4 touched experts
Bs = sorted(int(k) for k in B["touch"]); ch = line_chart(860, 360, Bs, [[B["touch"][str(b)][0] for b in Bs], [B["touch"][str(b)][2] for b in Bs]], "Distinct experts (of 4) read by one decode step of B concurrent requests", "batch size B", "experts read", colors=[C["blue"], C["orange"]], legend=["uniform router (formula)", "the skewed router measured on the random models"], xfmt="{:.0f}", yfmt="{:.1f}", ymin=0, ymax=4.2); ch.save(f"{OUT}/ch17-touch.svg")
# 17.5 grouping
one, grp = B["group"]; ch = bar_chart(760, 330, ["8 separate expert programs", "4 grouped programs (2 tokens each)"], [[one, grp]], "Cycles of the expert stage for 8 tokens (RTL-validated model)", "cycles", colors=[C["green"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch17-group.svg")
# 17.6 serving
Es = sorted(int(k) for k in B["serve"]); ch = bar_chart(860, 340, [f"E = {e}" for e in Es], [[B["serve"][str(e)][2] for e in Es], [B["serve"][str(e)][4] for e in Es]], "Derived tokens per second per request at 1 TB/s: batch 1 and batch 8", "tokens/s", colors=[C["blue"], C["orange"]], fmt="{:.0f}", legend=["B = 1", "B = 8"]); ch.save(f"{OUT}/ch17-serve.svg")
print("ch17 figures written", d)
