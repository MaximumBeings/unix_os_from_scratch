#!/usr/bin/env python3
"""Chapter 9 figures -> docs/assets/fig/ch09-*.svg. Data-driven ones read out/ch09_run_out.txt, out/ch09_example_a.json and out/ch09_example_b.json."""
import json, math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch09_example_a.json")); B = json.load(open(f"{ROOT}/out/ch09_example_b.json")); txt = open(f"{ROOT}/out/ch09_run_out.txt").read()
rows = re.findall(r"^(B=\d[^\n]*?)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+[\d.]+%\s+([\d.]+)x faster", txt, re.M)
# 9.1 roofline (log-log)
f = Fig(860, 470, "Roofline of GA-2"); f.text(430, 22, "Roofline: attainable MACs per cycle = min(16, words per cycle x MACs per word)", 14, bold=True)
L, T, W_, H_ = 90, 50, 720, 320
px = lambda x: L + W_ * (math.log10(x) - math.log10(0.5)) / (math.log10(64) - math.log10(0.5)); py = lambda y: T + H_ * (1 - (math.log10(y) - math.log10(0.1)) / (math.log10(32) - math.log10(0.1)))
for xv in (0.5, 1, 2, 4, 8, 16, 32, 64): f.line(px(xv), T, px(xv), T + H_, C["gray2"], 1); f.text(px(xv), T + H_ + 16, f"{xv:g}", 11, C["line"])
for yv in (0.1, 0.3, 1, 3, 10, 30): f.line(L, py(yv), L + W_, py(yv), C["gray2"], 1); f.text(L - 8, py(yv) + 4, f"{yv:g}", 11, C["line"], "end")
f.path(f"M{px(0.5):.1f},{py(0.5):.1f} L{px(16):.1f},{py(16):.1f} L{px(64):.1f},{py(16):.1f}", "none", C["ink"], 3); f.text(px(24), py(16) - 8, "compute ceiling: 16 MACs/cycle (4 x 4 array)", 12, C["ink"], bold=True)
f.text(px(1.0), py(0.5) - 24, "memory ceiling: 1 word per cycle", 12, C["ink"], "start", True, italic=True); f.line(px(16), py(16), px(16), T + H_, C["red"], 1.8, "6 4"); f.text(px(16) - 6, T + H_ + 34, "ridge point: 16 MACs per word", 12, C["red"], "end", True)
for idx, (nm, cyc, per, macs, wds, mpw, ach, ceil, ov) in enumerate(rows):
    x, y = float(mpw), float(ach); f.circ(px(x), py(y), 6, C["blue"], "#fff", 1.5)
    if idx in (0, 3, 4): f.text(px(x) + (14 if idx == 4 else 0), py(y) + 20, ("B=1" if idx == 0 else "B=4" if idx == 3 else "B=4 ragged"), 10.5, C["blue"], bold=True)
    f.circ(px(x), py(float(ceil)), 4, "#fff", C["orange"], 2)
f.text(L + W_, T + H_ + 56, "arithmetic intensity (MACs per word moved)", 12, C["line"], "end"); f.text(30, T + H_ / 2, "MACs per cycle", 12, C["line"], rot=-90)
f.rect(110, 68, 12, 12, C["blue"], None, 0, 6); f.text(128, 78, "achieved on the circuit", 11, C["ink"], "start"); f.circ(116, 96, 5, "#fff", C["orange"], 2); f.text(128, 100, "memory ceiling at that intensity", 11, C["ink"], "start")
f.text(430, 452, "every decode step sits far to the left of the ridge: memory-bound, whatever the batch", 12, C["red"], "middle", True); f.save(f"{OUT}/ch09-roofline.svg")
# 9.2 weight reuse
f = Fig(900, 330, "Batching shares the weights"); f.text(450, 22, "One matrix product with M = B rows reads the weights once for B requests", 14, bold=True)
f.box(30, 70, 130, 36, ["x of request 0"], C["orange2"], C["orange"], 11.5, True); f.box(190, 60, 160, 150, ["weights W", "(read once)"], C["blue2"], C["blue"], 13, True); f.arrow(162, 88, 188, 120, C["ink"], 2); f.box(380, 70, 130, 36, ["q of request 0"], C["green2"], C["green"], 11.5, True); f.arrow(352, 120, 378, 90, C["ink"], 2)
f.text(190, 245, "B = 1: M = 1, intensity ~ 1 MAC/word", 12, C["red"], "middle", True)
for k in range(4): f.box(560, 54 + k * 38, 90, 32, [f"x of req {k}"], C["orange2"], C["orange"], 10.5, True)
f.box(660, 60, 120, 150, ["weights W", "(read ONCE)"], C["blue2"], C["blue"], 12.5, True); f.arrow(652, 135, 658, 135, C["ink"], 2); f.box(795, 70, 90, 120, ["q of", "requests", "0 to 3"], C["green2"], C["green"], 11.5, True); f.arrow(782, 130, 793, 130, C["ink"], 2)
f.text(700, 245, "B = 4: M = 4, intensity up to 4x", 12, C["green"], "middle", True)
f.lines(450, 290, ["Attention cannot be shared this way: request k's scores read request k's own cache, so its traffic adds up with B."], 12, C["line"], italic=True) if False else f.text(450, 300, "Attention cannot be shared: request k's scores read only request k's own KV cache, so that traffic adds up with B.", 12, C["line"], "middle", False, False, True); f.save(f"{OUT}/ch09-reuse.svg")
# 9.3 words moved
bs = [1, 2, 3, 4]; wsh = [768] * 4; wseq = [544 * b for b in bs]
f = Fig(820, 360, "Words moved per step"); f.text(410, 22, "Words moved per decode step at L = 16: the weights are paid once, the caches once per request (derived, matches the circuit)", 13, bold=True)
sc = 230 / 3000; 
for k, b in enumerate(bs):
    x = 120 + k * 170; h1 = 768 * sc; h2 = 544 * b * sc; f.rect(x, 300 - h1, 100, h1, C["blue"], None, 0, 2); f.rect(x, 300 - h1 - h2, 100, h2, C["orange"], None, 0, 2)
    f.text(x + 50, 296 - h1 - h2 - 6, f"{768 + 544 * b:,}", 12, C["ink"], bold=True); f.text(x + 50, 322, f"B = {b}", 12.5, bold=True); f.text(x + 50, 340, f"{(768 + 544 * b) / b:.0f} per request", 10.5, C["line"])
f.rect(560, 60, 12, 12, C["blue"], None, 0, 2); f.text(578, 71, "weights (shared)", 11.5, C["ink"], "start"); f.rect(560, 82, 12, 12, C["orange"], None, 0, 2); f.text(578, 93, "cache + I/O (per request)", 11.5, C["ink"], "start"); f.save(f"{OUT}/ch09-words.svg")
# 9.4 MAC per word vs batch
xs = [1, 2, 4, 8, 16, 32, 64, 128]; ys = [b * 1280 / (768 + 544 * b) for b in xs]
ch = line_chart(840, 360, xs, [ys, [2.35] * len(xs), [1.0] * len(xs)], "MACs per word against batch size at L = 16 (derived; the circuit's B = 1, 2, 4 agree)", "batch size B (log scale)", "MACs per word moved", [C["blue"], C["red"], C["gray"]], ["intensity of the batch", "limit 2.35 (L = 16)", "limit 1 (long context)"], logx=True, xfmt="{:g}", yfmt="{:.1f}", ymin=0, ymax=3, marks=True); ch.save(f"{OUT}/ch09-intensity.svg")
# 9.5 tokens/s for chips (example A): ctx 8192
ks = [k for k in A["tps"] if k.endswith("|8192")]; xs = [1, 2, 4, 8, 16, 32, 64, 96, 128]
ser = [[(A["tps"][k][b - 1] if A["tps"][k][b - 1] is not None else None) for b in xs] for k in ks]
ch = line_chart(860, 380, xs, ser, "Tokens per second against batch size, 8,192-token context (derived; curves stop where memory is full)", "batch size B (log scale)", "tokens per second per chip", [C["blue"], C["green"], C["orange"]], [k.split("|")[0].split(":")[0] for k in ks], logx=True, xfmt="{:g}", yfmt="{:,.0f}", ymin=0); ch.save(f"{OUT}/ch09-tps.svg")
# 9.6 timelines
f = Fig(940, 400, "Static against continuous batching"); f.text(470, 22, "The same eight requests on GA-2 (cycle model of the real batched program): idle slots in static batching, none in continuous", 13.5, bold=True)
cols = [C["blue2"], C["green2"], C["orange2"], C["purple2"], C["teal2"], C["yellow2"], C["red2"], C["gray2"]]; sts = [C["blue"], C["green"], C["orange"], C["purple"], C["teal"], C["gray"], C["red"], C["line"]]
def lanes(y0, name, log, tot, static):
    f.text(30, y0 - 6, f"{name}: {tot:,} cycles", 13, C["ink"], "start", True); sc = 840 / 170118
    for lane in range(4):
        f.text(66, y0 + 20 + lane * 30, f"slot {lane}", 10.5, C["line"], "end")
        for (t, c, n, live) in log:
            if static:
                grp0 = 0 if t < 0 else None
            members = live
            # lane assignment: position in the list of live requests for continuous; fixed for static
            pass
    return sc
# static: group g uses requests 4g..4g+3 in slots 0..3 ; continuous: replay slot assignment
def static_boxes():
    out = []; t = 0
    lens = B["lens"]
    for (t0, c, n, live) in B["static"]["log"]:
        grp = 0 if live and live[0] < 4 else 1
        for k in range(4):
            i = grp * 4 + k
            out.append((t0, c, k, i, i in live))
    return out
def cont_boxes():
    out = []; slots = [None] * 4; waiting = list(range(8)); prog = {}
    for (t0, c, n, live) in B["continuous"]["log"]:
        for i in live:
            if i not in slots:
                slots[slots.index(None)] = i
        for k, i in enumerate(slots):
            if i is not None and i in live: out.append((t0, c, k, i, True))
        for k, i in enumerate(slots):
            if i is not None and i in live:
                pass
        # free finished: a request finishes when it no longer appears next step; handle by comparing with done time
        for k, i in enumerate(slots):
            if i is not None and B["continuous"]["done"][str(i)] <= t0 + c: slots[k] = None
    return out
sc = 840 / 170118
f.text(30, 54, f"static batching: {B['static']['total']:,} cycles", 13, C["ink"], "start", True)
for lane in range(4): f.text(86, 78 + lane * 26, f"slot {lane}", 10.5, C["line"], "end")
for (t0, c, k, i, live) in static_boxes(): f.rect(100 + t0 * sc, 66 + k * 26, c * sc + 0.3, 22, cols[i] if live else "#ffffff", sts[i], 0.8, 1) if live else f.rect(100 + t0 * sc, 66 + k * 26, c * sc + 0.3, 22, "#f1d9d9", "#e3b4b4", 0.6, 1)
f.text(30, 200, f"continuous batching: {B['continuous']['total']:,} cycles", 13, C["ink"], "start", True)
for lane in range(4): f.text(86, 224 + lane * 26, f"slot {lane}", 10.5, C["line"], "end")
for (t0, c, k, i, live) in cont_boxes(): f.rect(100 + t0 * sc, 212 + k * 26, c * sc + 0.3, 22, cols[i], sts[i], 0.8, 1)
f.line(100 + B["continuous"]["total"] * sc, 202, 100 + B["continuous"]["total"] * sc, 320, C["red"], 2, "5 4"); f.text(100 + B["continuous"]["total"] * sc + 6, 316, "continuous ends here", 11, C["red"], "start", True)
for i in range(8): f.rect(100 + i * 100, 352, 12, 12, cols[i], sts[i], 1, 2); f.text(118 + i * 100, 363, f"req {i} ({B['lens'][i]} tok)", 10, C["ink"], "start")
f.text(470, 392, "pink = the slot rides along although its request has finished (static batching waits for the longest request of the group)", 11.5, C["line"], "middle", False, False, True); f.save(f"{OUT}/ch09-timeline.svg")
print("6 figures written")
