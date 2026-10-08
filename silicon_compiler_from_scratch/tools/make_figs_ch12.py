#!/usr/bin/env python3
"""Chapter 12 figures -> docs/assets/fig/ch12-*.svg. Data-driven ones read out/ch12_example_a.json, ch12_example_b.json, ch12_run_out.txt and ch12_mutation_out.txt."""
import json, math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch12_example_a.json")); B = json.load(open(f"{ROOT}/out/ch12_example_b.json")); run = open(f"{ROOT}/out/ch12_run_out.txt").read()
# 12.1 host and chip
f = Fig(940, 360, "Host and chip"); f.text(470, 22, "One generated token: what the host does and what the chip does", 14, bold=True)
f.box(20, 60, 400, 250, [], C["orange2"], C["orange"]); f.text(220, 84, "HOST (a Python program)", 13.5, C["orange"], bold=True)
f.lines(40, 112, ["1. pick the embedding row of the input token", "   (the chip has no gather instruction)", "2. keep the KV cache between steps", "3. load the compiled program for this context length", "4. read back: token, logits, new k and new v", "5. append k and v to the cache; feed the token back"], 12, C["ink"], "start", lh=22)
f.box(520, 60, 400, 250, [], C["blue2"], C["blue"]); f.text(720, 84, "CHIP (GA-2 running a Capra program)", 13.5, C["blue"], bold=True)
f.lines(540, 112, ["x -> q, k, v           (3 projections)", "append k, v to the cache views", "scores, softmax, weighted sum", "output projection + residual add", "feed-forward (two matmuls, relu) + residual add", "final projection to 16 logits -> argmax"], 12, C["ink"], "start", lh=22)
f.arrow(422, 150, 518, 150, C["ink"], 2.4); f.text(470, 140, "x, cache", 11, C["line"], italic=True); f.arrow(518, 230, 422, 230, C["ink"], 2.4); f.text(470, 250, "token, k, v", 11, C["line"], italic=True)
f.text(470, 340, "24 steps, each a separately compiled program (one per context length), each replayed on the RTL in both simulators", 12, C["line"], italic=True); f.save(f"{OUT}/ch12-host.svg")
# 12.2 model
f = Fig(940, 380, "The tiny transformer block"); f.text(470, 22, "One decode step of the tiny model (width 16, feed-forward 32, one head, vocabulary 16)", 14, bold=True)
bx = [(20, 60, "token t", C["gray2"], C["gray"]), (180, 60, "x = E[t]  (1x16)", C["orange2"], C["orange"]), (360, 20, "q = x Wq", C["blue2"], C["blue"]), (360, 70, "k = x Wk  -> cache", C["green2"], C["green"]), (360, 120, "v = x Wv  -> cache", C["purple2"], C["purple"]), (570, 60, "attention over the cache", C["blue2"], C["blue"]), (570, 160, "h = x + a Wo", C["yellow2"], C["gray"]), (570, 250, "y = h + relu(h W1) W2", C["yellow2"], C["gray"]), (570, 330, "logits = y Wout -> argmax", C["red2"], C["red"])]
for x, y, t, fl, st in bx: f.box(x, y, 170 if x < 570 else 250, 42, [t], fl, st, 11.5, True)
for a, b in (((172, 81), (180, 81)), ((350, 81), (360, 41)), ((350, 81), (360, 91)), ((350, 81), (360, 141)), ((532, 41), (570, 81)), ((532, 91), (570, 81)), ((532, 141), (570, 81))): f.arrow(a[0], a[1], b[0], b[1], C["ink"], 1.6)
f.arrow(695, 104, 695, 158, C["ink"], 1.8); f.arrow(695, 202, 695, 248, C["ink"], 1.8); f.arrow(695, 292, 695, 328, C["ink"], 1.8)
f.path("M265,104 L265,181 L568,181", "none", C["orange"], 1.6, "5 4"); f.text(400, 198, "residual (x added back)", 10.5, C["orange"], italic=True); f.lines(150, 300, ["no layer normalization, one head:", "Capra has no such operations", "(Chapter 11's list of operations)"], 11, C["line"], lh=15); f.save(f"{OUT}/ch12-model.svg")
# 12.3 permutation cycle
f = Fig(560, 520, "The rule is one cycle"); f.text(280, 24, "f(t) = (5t + 3) mod 16 visits all 16 tokens in one cycle", 14, bold=True); cx, cy, R = 280, 280, 190
order = [0]
for _ in range(15): order.append((5 * order[-1] + 3) % 16)
for k, t in enumerate(order):
    a = -math.pi / 2 + 2 * math.pi * k / 16; x, y = cx + R * math.cos(a), cy + R * math.sin(a); a2 = -math.pi / 2 + 2 * math.pi * (k + 1) / 16
    f.circ(x, y, 18, C["blue2"], C["blue"], 1.8); f.text(x, y + 5, str(t), 13, bold=True)
    x2, y2 = cx + R * math.cos(a2), cy + R * math.sin(a2); dx, dy = x2 - x, y2 - y; d = math.hypot(dx, dy); f.arrow(x + dx / d * 20, y + dy / d * 20, x2 - dx / d * 22, y2 - dy / d * 22, C["gray"], 1.6, 7)
f.text(cx, cy - 6, "0 -> 3 -> 2 -> 13 -> 4 -> 7 ...", 12, C["line"], italic=True); f.text(cx, cy + 14, "a single cycle of length 16", 12, C["line"], italic=True); f.save(f"{OUT}/ch12-cycle.svg")
# 12.4 logits at step 3
st = A["steps"][2]; f = Fig(900, 420, "Logits at one step"); f.text(450, 22, "Step 3 (input token 2): the 16 logits, floating point against the chip; the winner is token 13 = f(2)", 14, bold=True)
mx = 9; yb = 330; sc = 230 / mx
f.line(60, yb, 880, yb, C["ink"], 1.5)
for j in range(16):
    x = 70 + j * 50; fv, cv = st["float"][j], st["chip"][j]
    for dx, v, col in ((0, fv, C["gray"]), (22, cv, C["blue"] if j != st["winner"] else C["orange"])):
        h = abs(v) * sc; f.rect(x + dx, (yb - h) if v >= 0 else yb, 20, max(h, 1), col, None, 0, 1)
    f.text(x + 21, yb + 32 if False else 360, str(j), 12, bold=True, mono=True)
    if j == st["winner"]: f.text(x + 21, yb - abs(cv) * sc - 8, f"{cv:+.2f}", 11, C["orange"], bold=True)
f.rect(100, 60, 12, 12, C["gray"], None, 0, 1); f.text(118, 71, "floating point", 11.5, C["ink"], "start"); f.rect(100, 82, 12, 12, C["blue"], None, 0, 1); f.text(118, 93, "chip (int8)", 11.5, C["ink"], "start"); f.rect(100, 104, 12, 12, C["orange"], None, 0, 1); f.text(118, 115, "chip winner", 11.5, C["ink"], "start")
f.text(450, 396, "token", 12, C["line"]); f.text(450, 412, "every other logit is within +-0.6 of zero: the winner stands alone, so rounding to int8 cannot change it", 11.5, C["line"], italic=True); f.save(f"{OUT}/ch12-logits.svg")
# 12.5 cycles per step
rows = re.findall(r"^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+\d+ -> \d+", run, re.M)
ch = bar_chart(900, 380, [f"step {r[0]}" for r in rows], [[int(r[4]) for r in rows], [int(r[5]) for r in rows], [int(r[6]) for r in rows]], "Cycles of a decode step by unit, against the step (context grows by one each time)", "cycles", colors=[C["blue"], C["orange"], C["green"]], fmt="{:,.0f}", legend=["matrix unit", "DMA", "vector units"]); ch.save(f"{OUT}/ch12-cycles.svg")
# 12.6 noise sweep
ch = line_chart(860, 380, B["noise"], [[100 * v for v in B["float_rule"]], [100 * v for v in B["chip_float"]]], "When do attention and feed-forward start deciding? (measured, 192 decisions per point)", "size of the random attention/feed-forward weights ('noise')", "% of decisions", [C["red"], C["blue"]], ["floating point follows the designed rule", "chip agrees with floating point"], logx=True, xfmt="{:g}", yfmt="{:.0f}", ymin=0, ymax=100); ch.save(f"{OUT}/ch12-noise.svg")
print("6 figures written")
