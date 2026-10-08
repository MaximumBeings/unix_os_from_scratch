#!/usr/bin/env python3
"""Chapter 15 figures -> docs/assets/fig/ch15-*.svg (data from out/ch15_example_a.json, ch15_example_b.json, ch15_run_out.txt)."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch15_example_a.json")); B = json.load(open(f"{ROOT}/out/ch15_example_b.json")); run = open(f"{ROOT}/out/ch15_run_out.txt").read()
# 15.1 logical rows -> block table -> physical pool (step 14 of example A)
st = A["log"][13]; tb = st["table"]; dr = st["dropped"]
f = Fig(940, 420, "Block table"); f.text(470, 22, "Step 14 of Running example A: logical rows, the block table and the physical pool (page = 4 rows, window = 6)", 14, bold=True)
f.text(40, 62, "logical rows", 12, C["line"], anchor="start", bold=True)
for r in range(14):
    x = 40 + r * 62; dead = (r // 4) < dr
    fill = C["gray2"] if dead else (C["orange2"] if r >= 9 else C["blue2"]); st_ = C["gray"] if dead else (C["orange"] if r >= 9 else C["blue"])
    f.box(x, 72, 54, 34, [str(r)], fill, st_, 12, True)
for p in range(4): f.text(40 + p * 248 + 112, 128, f"logical page {p}" + (" (released)" if p < dr else ""), 11, C["line"], italic=True)
f.text(40, 176, "block table", 12, C["line"], anchor="start", bold=True)
for i, ph in enumerate(tb):
    x = 250 + i * 220; f.box(x, 190, 200, 40, [f"slot {i + dr}  ->  physical page {ph}"], C["green2"], C["green"], 12, True); f.arrow(x + 100, 232, 60 + ph * 112 + 44, 296, C["green"], 1.8, 8)
f.text(40, 280, "physical pool", 12, C["line"], anchor="start", bold=True)
for p in range(8):
    x = 60 + p * 112; used = p in tb; f.box(x, 300, 88, 50, [f"page {p}"], C["green2"] if used else C["gray2"], C["green"] if used else C["gray"], 12, True, sub="in use" if used else "free")
f.text(470, 380, "Orange: rows still held (rows 9-13 are attended; row 8 shares a page with them).", 12, C["line"], italic=True); f.text(470, 402, "Page 0 held rows 0-3, was released when the window passed it, and was reused for rows 12-15.", 12, C["line"], italic=True); f.save(f"{OUT}/ch15-blocktable.svg")
# 15.2 window
f = Fig(940, 260, "Sliding window"); f.text(470, 22, "A window of 6 tokens: step 14 attends rows 9-13 (previous) and the new row", 14, bold=True)
for r in range(14):
    x = 30 + r * 58; inw = r >= 9; f.box(x, 70, 50, 44, [str(r)], C["orange2"] if inw else C["gray2"], C["orange"] if inw else C["gray"], 13, True)
f.box(30 + 14 * 58, 70, 60, 44, ["new"], C["blue2"], C["blue"], 12, True)
f.path(f"M{30 + 9 * 58 - 4},60 L{30 + 14 * 58 + 60},60", "none", C["orange"], 2.4); f.text(30 + 12 * 58, 52, "attended", 12, C["orange"], bold=True)
f.text(30 + 4.5 * 58, 140, "still stored? only until their page is released", 12, C["line"], italic=True); f.text(470, 200, "A compiled program exists for context 1, 2, ... 6; every later step uses the program for context 6, so the cost per step stops growing.", 12, C["line"], italic=True); f.save(f"{OUT}/ch15-window.svg")
# 15.3 flat cycles
rows = re.findall(r"^\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$", run, re.M); xs = [int(r[0]) for r in rows]
ch = line_chart(900, 360, xs, [[int(r[1]) for r in rows], [int(r[2]) for r in rows], [int(r[3]) for r in rows]], "RTL cycles of one decode step against the step number", "step (context length)", "cycles", colors=[C["gray"], C["blue"], C["orange"]], legend=["no window", "window 8", "window 4"], xfmt="{:.0f}", yfmt="{:,.0f}"); ch.save(f"{OUT}/ch15-flat.svg")
# 15.4 fragmentation
names = ["contiguous"] + [f"page {p}" for p in B["paged"]]; vals = [B["contig"][0]] + [B["paged"][p][0] for p in B["paged"]]
ch = bar_chart(860, 340, names, [vals], "Requests held in a pool of 8,192 cache rows", "requests", colors=[C["blue"]], fmt="{:.0f}"); ch.save(f"{OUT}/ch15-frag.svg")
# 15.5 sharing
ks = list(B["share"]); ch = bar_chart(880, 340, [k.replace("/", " rows / page ") for k in ks], [[B["share"][k][0] for k in ks], [B["share"][k][1] for k in ks]], "Pages for 8 requests that share a prompt: copied against shared", "pages", colors=[C["gray"], C["green"]], fmt="{:.0f}", legend=["each request holds its own copy", "prefix shared, copy on write"]); ch.save(f"{OUT}/ch15-share.svg")
# 15.6 accuracy
ws = list(B["win"]); ch = bar_chart(760, 340, [f"W={w}" for w in ws], [[100 * B["win"][w][0] / B["win"][w][1] for w in ws]], "Agreement with full attention, random model, by window size (%)", "agreement %", colors=[C["orange"]], fmt="{:.1f}", maxv=100); ch.save(f"{OUT}/ch15-winacc.svg")
# 15.7 serving
cs = list(B["serve"]); ch = bar_chart(860, 340, [f"{int(c):,}" for c in cs], [[B["serve"][c][2] for c in cs], [B["serve"][c][3] for c in cs]], "Largest batch against context length (80 GB, 7B-class, derived)", "requests", colors=[C["gray"], C["blue"]], fmt="{:.0f}", legend=["no window", "window 4096"]); ch.save(f"{OUT}/ch15-serve.svg")
print("ch15 figures written")
