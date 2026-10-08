#!/usr/bin/env python3
"""Chapter 5 figures -> docs/assets/fig/ch05-*.svg. The timelines and charts are drawn from out/ch05_example_a.json, out/ch05_example_b.json and ch05_run_out.txt."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch05_example_a.json")); B = json.load(open(f"{ROOT}/out/ch05_example_b.json"))
# 5.1 system view
f = Fig(860, 340, "Compute, scratchpad, DMA and DRAM"); f.text(430, 22, "Where the data lives: slow and big outside, fast and small inside", 14, bold=True)
f.box(20, 100, 150, 120, ["external memory", "(DRAM)"], C["gray2"], C["gray"], 13, True, sub="big, slow: ~100 cycles")
f.box(260, 120, 120, 80, ["DMA engine"], C["orange2"], C["orange"], 14, True, sub="copies blocks")
f.box(470, 80, 150, 160, [], C["green2"], C["green"], sw=2); f.text(545, 102, "scratchpad", 14, C["green"], bold=True); f.box(484, 116, 122, 50, ["bank 0"], "#ffffff", C["green"], 13, True, mono=True); f.box(484, 176, 122, 50, ["bank 1"], "#ffffff", C["green"], 13, True, mono=True)
f.box(710, 100, 130, 120, ["compute", "(systolic array,", "vector units)"], C["blue2"], C["blue"], 12.5, True, sub="1 word / cycle")
f.arrow(172, 160, 258, 160, C["ink"], 2.4); f.text(215, 148, "requests", 11, C["line"], italic=True); f.arrow(382, 160, 468, 160, C["ink"], 2.4); f.text(425, 148, "answers", 11, C["line"], italic=True)
f.arrow(622, 160, 708, 160, C["ink"], 2.4)
f.lines(430, 276, ["The DMA fills one bank while the compute unit works on the other: that is double buffering.", "The program, not the hardware, decides what lives in the scratchpad (unlike a cache, which guesses)."], 12.5, C["ink"], lh=19); f.save(f"{OUT}/ch05-system.svg")
# 5.2 pipelined DMA timeline
f = Fig(860, 300, "A pipelined transfer"); f.text(430, 22, "len = 8 words, latency 4: one request per cycle, answers 4 cycles later", 14, bold=True)
x0, s = 130, 52; 
for t in range(13): f.text(x0 + t * s + s / 2, 54, str(t), 11, C["line"], mono=True)
f.text(80, 54, "cycle", 11, C["line"], "end", italic=True); f.text(80, 100, "requests", 12, C["orange"], "end", True); f.text(80, 160, "answers", 12, C["green"], "end", True); f.text(80, 220, "written", 12, C["blue"], "end", True)
for k in range(8):
    f.box(x0 + k * s + 2, 80, s - 4, 34, [f"w{k}"], C["orange2"], C["orange"], 12, True, mono=True); f.box(x0 + (k + 4) * s + 2, 140, s - 4, 34, [f"w{k}"], C["green2"], C["green"], 12, True, mono=True); f.box(x0 + (k + 4) * s + 2, 200, s - 4, 34, [f"s[{k}]"], C["blue2"], C["blue"], 11, False, mono=True)
f.arrow(x0 + 5 * s, 116, x0 + 5 * s + 0.1 + 3 * s, 138, C["line"], 1.4, 6, "4 3") if False else None
f.text(430, 262, "total = len + latency = 8 + 4 = 12 cycles (plus a few cycles of control), not len x latency = 32", 12.5, C["ink"], bold=True); f.text(430, 282, "this is why a long transfer is efficient: the latency is paid once", 12, C["line"], italic=True); f.save(f"{OUT}/ch05-pipeline.svg")
# 5.3 gantt
def gantt(f, y0, name, d, x0, scale, tot_max):
    f.text(x0 - 10, y0 + 24, "DMA", 12, C["orange"], "end", True); f.text(x0 - 10, y0 + 64, "compute", 12, C["blue"], "end", True)
    cols = [C["orange2"], C["green2"], C["purple2"], C["yellow2"]]; st = [C["orange"], C["green"], C["purple"], C["gray"]]
    for n, (a, b) in enumerate(d["load"]): f.box(x0 + a * scale, y0 + 8, (b - a + 1) * scale, 30, [f"load {n}"], cols[n], st[n], 11, True)
    for n, (a, b) in enumerate(d["comp"]): f.box(x0 + a * scale, y0 + 48, (b - a + 1) * scale, 30, [f"compute {n}"], cols[n], st[n], 11, True)
    f.line(x0 + d["total"] * scale, y0 - 2, x0 + d["total"] * scale, y0 + 84, C["red"], 2, "5 4"); f.text(x0 + d["total"] * scale + 6, y0 + 92, f"{d['total']} cycles", 12.5, C["red"], "start", True)
    f.text(x0, y0 - 8, name, 13, C["ink"], "start", True)
f = Fig(900, 380, "Serial against double-buffered"); f.text(450, 22, "The same four tiles, serial and double-buffered (events recorded from the real circuit)", 14, bold=True)
sc = 3.4; gantt(f, 70, "serial: load, compute, load, compute ...", A["modes"]["serial"], 100, sc, 232); gantt(f, 220, "double-buffered: the load of the next tile hides behind the compute of this one", A["modes"]["double-buffered"], 100, sc, 232)
for t in range(0, 241, 40): f.line(100 + t * sc, 336, 100 + t * sc, 342, C["line"], 1); f.text(100 + t * sc, 356, str(t), 10.5, C["line"])
f.text(450, 372, "cycles", 11, C["line"], italic=True); f.save(f"{OUT}/ch05-gantt.svg")
# 5.4 ping-pong banks
f = Fig(900, 300, "Bank occupancy"); f.text(450, 22, "What each scratchpad bank is doing over time (double-buffered run): the banks take turns", 14, bold=True)
d = A["modes"]["double-buffered"]; sc = 4.4; x0 = 100
f.text(x0 - 10, 90, "bank 0", 12, C["green"], "end", True); f.text(x0 - 10, 170, "bank 1", 12, C["green"], "end", True)
for n in range(4):
    y = 70 if n % 2 == 0 else 150; a, b = d["load"][n]; c0, c1 = d["comp"][n]
    f.box(x0 + a * sc, y, (b - a + 1) * sc, 36, [f"loading tile {n}"], C["orange2"], C["orange"], 11, True); f.box(x0 + c0 * sc, y, (c1 - c0 + 1) * sc, 36, [f"computing tile {n}"], C["blue2"], C["blue"], 11, True)
f.lines(450, 240, ["A bank is loaded, then computed on, then free again. Bank 0 holds tiles 0 and 2, bank 1 holds tiles 1 and 3.", "The controller's two rules are visible here: a load never starts in a bank that is still being computed on,", "and a compute never starts before its bank has been completely loaded."], 12, C["line"], lh=18); f.save(f"{OUT}/ch05-banks.svg")
# 5.5 speedup vs CPW (from run output)
txt = open(f"{ROOT}/out/ch05_run_out.txt").read(); rows = re.findall(r"^\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+", txt, re.M)
ch = bar_chart(780, 340, [f"CPW={r[0]}" for r in rows], [[float(r[5]) for r in rows]], "Speedup from double buffering as compute gets slower (measured, TILE=16, LAT=4)", "serial / double-buffered", colors=[C["blue"]], fmt="{:.2f}", maxv=2.0); ch.save(f"{OUT}/ch05-speedup.svg")
# 5.6 speedup vs tile for different latencies (measured, example B)
ch = line_chart(860, 390, B["tiles"], [B["speedup"][str(l)] for l in B["lats"]], "Speedup against tile size for four memory latencies (measured on the circuit, CPW = 2)", "TILE (words, log scale)", "serial / double-buffered", [C["blue"], C["green"], C["orange"], C["red"]], [f"LAT = {l}" for l in B["lats"]], logx=True, xfmt="{:g}", yfmt="{:.1f}", ymin=1.0, ymax=2.0); ch.save(f"{OUT}/ch05-latency.svg")
print("6 figures written")
