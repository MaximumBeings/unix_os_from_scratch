#!/usr/bin/env python3
"""Chapter 7 figures -> docs/assets/fig/ch07-*.svg. Data-driven ones read out/ch07_example_a.json, ch07_example_b.json and ch07_run_out.txt."""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT = os.path.join(ROOT, "docs", "assets", "fig")
A = json.load(open(f"{ROOT}/out/ch07_example_a.json")); B = json.load(open(f"{ROOT}/out/ch07_example_b.json"))
UC = {"LD": (C["orange2"], C["orange"]), "ST": (C["orange2"], C["orange"]), "MM": (C["blue2"], C["blue"]), "RQ": (C["green2"], C["green"]), "SM": (C["purple2"], C["purple"]), "VADD": (C["teal2"], C["teal"]), "AMAX": (C["yellow2"], C["gray"])}
# 7.1 block diagram
f = Fig(900, 470, "GA-2"); f.text(450, 22, "GA-2: a sequencer runs one instruction at a time on one of seven units, all sharing one scratchpad", 14, bold=True)
f.box(20, 50, 160, 70, ["program ROM", "128-bit instructions"], C["gray2"], C["gray"], 12.5, True)
f.box(240, 50, 190, 70, ["sequencer", "FETCH - ISSUE - WAIT"], C["red2"], C["red"], 13, True)
f.arrow(182, 85, 238, 85, C["ink"], 2.2); f.text(210, 76, "ins", 11, C["line"], italic=True)
f.box(680, 50, 200, 70, ["external memory", "(DRAM model)"], C["gray2"], C["gray"], 12.5, True, sub="latency 8 cycles")
units = [("LD", "DMA"), ("ST", "store"), ("MM", "4x4 array"), ("RQ", "requantize"), ("SM", "softmax"), ("VADD", "sat. add"), ("AMAX", "argmax")]
for k, (n, s) in enumerate(units):
    x = 20 + k * 125; fl, st = UC[n]; f.box(x, 190, 112, 64, [n], fl, st, 15, True, sub=s); f.line(x + 56, 256, x + 56, 300, st, 2.4)
    f.arrow(334, 122, x + 56, 188, C["red"], 1.4, 6, "4 3") if False else None
f.path("M335,120 L335,160 L76,160 L76,188", "none", C["red"], 1.8, "5 4"); f.path("M335,160 L825,160 L825,188", "none", C["red"], 1.8, "5 4"); f.text(560, 154, "start / done (the sequencer starts exactly one unit and waits for its 'done')", 11.5, C["red"], italic=True)
f.box(20, 300, 862, 70, ["scratchpad: 4096 x 32-bit words  (one read port, one write port, 1-cycle read)"], C["green2"], C["green"], 14, True)
f.path("M143,254 L143,300", "none", C["orange"], 2.4); f.arrow(790, 124, 790, 160, C["orange"], 2.2); f.path("M790,160 L143,160", "none", C["orange"], 0.01)
f.arrow(780, 125, 780, 186, C["orange"], 2); f.text(860, 150, "LD / ST", 11, C["orange"], "end", True)
f.lines(450, 410, ["The unit named by the instruction in flight owns the scratchpad's ports. Nothing runs in parallel:", "that is what makes the cycle count of a program the SUM of its instructions' cycle counts, and so predictable."], 12.5, C["ink"], lh=19); f.save(f"{OUT}/ch07-chip.svg")
# 7.2 instruction word layout
f = Fig(900, 330, "The 128-bit instruction"); f.text(450, 22, "One 128-bit instruction word: an opcode and ten fields, reused differently by each operation", 14, bold=True)
FIELDS = [("op", 4), ("a", 16), ("b", 16), ("c", 16), ("d", 24), ("e", 6), ("f", 8), ("g", 8), ("h", 8), ("fl", 4), ("x", 18)]; x = 20; sc = 860 / 128
cols = [C["red2"], C["blue2"], C["green2"], C["orange2"], C["purple2"], C["teal2"], C["yellow2"], C["gray2"], C["blue2"], C["green2"], C["orange2"]]
for (n, w), col in zip(FIELDS, cols): f.box(x, 50, w * sc, 44, [n], col, C["gray"], 13, True, rx=3); f.text(x + w * sc / 2, 108, f"{w}", 10.5, C["line"]); x += w * sc
f.text(20, 128, "bits (127 on the left):", 11, C["line"], "start", italic=True)
use = {"LD": {"a": "spad dst", "b": "ext src", "c": "len"}, "MM": {"a": "dst", "b": "A", "c": "B", "d": "lda, ldb", "f": "M", "g": "K", "h": "N", "fl": "tb", "x": "ldc"}, "RQ": {"a": "dst", "b": "src", "c": "len", "d": "mantissa", "e": "shift", "fl": "relu"}}
for r, (opn, m) in enumerate(use.items()):
    y = 150 + r * 52; f.text(16, y + 28, opn, 13, UC[opn][1], "start", True, True); x = 20
    for (n, w), col in zip(FIELDS, cols):
        if n in m: f.box(x, y + 6, w * sc, 34, [m[n]], UC[opn][0], UC[opn][1], 9.5 if w * sc < 60 else 11, False, rx=3)
        x += w * sc
f.text(450, 318, "every field has a fixed place: the decoder is wiring, not logic. d holds two 12-bit strides for MM; fields not used by an operation are 0.", 12, C["line"], italic=True); f.save(f"{OUT}/ch07-word.svg")
# 7.3 sequencer FSM
f = Fig(900, 300, "Sequencer states"); f.text(450, 22, "Every instruction: FETCH (1 cycle), ISSUE (1 cycle), then WAIT until the unit is done", 14, bold=True)
for k, (n, s_, fl, st) in enumerate((("IDLE", "start = 0", C["gray2"], C["gray"]), ("FETCH", "read imem[pc]\n1 cycle", C["blue2"], C["blue"]), ("ISSUE", "pulse start to the\nunit named by op\n1 cycle", C["orange2"], C["orange"]), ("WAIT", "until unit_done\nbusy cycles", C["green2"], C["green"]), ("HALT", "op = 0:\nhalted = 1", C["red2"], C["red"]))):
    x = 20 + k * 175; f.box(x, 70, 130, 56, [n], fl, st, 15, True); f.lines(x + 65, 148, s_.split("\n"), 11, C["line"], lh=14)
    if k < 4: f.arrow(x + 132, 98, x + 172, 98, C["ink"], 2.2)
f.path("M610,70 C610,20 255,20 255,68", "none", C["green"], 2.2, "6 4"); f.poly([(255, 70), (250, 60), (260, 60)], C["green"]); f.text(430, 36, "unit_done: pc <- pc + 1, fetch the next instruction", 12, C["green"], bold=True)
f.path("M610,126 C610,200 835,200 835,128", "none", C["red"], 2); f.text(722, 214, "op == HALT (issue)", 11.5, C["red"], bold=True)
f.lines(450, 260, ["total cycles of a program = sum over instructions of (2 + busy time of the unit) + 2 for the final HALT", "(the counters cyc_total, cyc_mm, cyc_dma, cyc_vec and n_inst record exactly this)"], 12, C["ink"], lh=18); f.save(f"{OUT}/ch07-fsm.svg")
# 7.4 trace timeline (example A)
rows = A["rows"]; f = Fig(900, 340, "Timeline of Running example A"); f.text(450, 22, f"What the circuit did, instruction by instruction ({A['total']} cycles in all), from the trace", 14, bold=True)
sc = 800 / A["total"]; x0 = 60
for r in rows:
    fl, st = UC[r["op"]]; y = 52 + r["pc"] * 40
    f.text(x0 - 8, y + 20, r["op"], 12, st, "end", True, True); f.box(x0 + r["fetch"] * sc, y + 4, 2 * sc, 28, [""], C["gray2"], C["gray"], 8, rx=2); f.box(x0 + r["issue"] * sc + sc, y + 4, r["busy"] * sc, 28, [str(r["busy"]) if r["busy"] * sc > 18 else ""], fl, st, 11, True, rx=3)
f.line(x0 + A["total"] * sc, 44, x0 + A["total"] * sc, 304, C["red"], 1.8, "5 4"); f.text(x0 + A["total"] * sc, 322, f"{A['total']}", 12, C["red"], bold=True)
f.box(70, 306, 14, 14, [""], C["gray2"], C["gray"], 8, rx=2); f.text(90, 318, "fetch + issue (2 cycles)", 11, C["ink"], "start"); f.text(260, 318, "coloured bar = the unit's busy time (number = cycles)", 11, C["ink"], "start"); f.save(f"{OUT}/ch07-trace.svg")
# 7.5 MM unit anatomy for the example's 2x3x2
M, K, N = 2, 3, 2; parts = [("stage A", M * K, C["orange2"], C["orange"]), ("stage B", K * N, C["orange2"], C["orange"]), ("stream through the array", K + M + N - 2, C["blue2"], C["blue"]), ("write C", M * N, C["green2"], C["green"]), ("control", 7, C["gray2"], C["gray"])]
f = Fig(900, 300, "Inside one MM"); f.text(450, 22, f"MM for M={M}, K={K}, N={N}: {sum(p[1] for p in parts)} cycles, of which the array itself computes {K+M+N-2}", 14, bold=True)
sc = 800 / sum(p[1] for p in parts); x = 50
for nm, c, fl, st in parts: f.box(x, 60, c * sc, 56, [str(c)], fl, st, 16, True, rx=3); f.text(x + c * sc / 2, 138, nm, 11, C["line"]); x += c * sc
f.lines(450, 190, ["staging = M*K + K*N  (copy the two tiles into the operand memories, one scratchpad word per cycle)", "streaming = K+M+N-2  (the skewed schedule of Chapter 4, here for an M x N array region)", "writing = M*N  (one result word per cycle back to the scratchpad)"], 12, C["ink"], lh=19)
f.text(450, 270, "most of the time is moving data, not multiplying: for large tiles the staging dominates, and this is what Chapter 9's bandwidth analysis measures", 12, C["red"], italic=True); f.save(f"{OUT}/ch07-mm.svg")
# 7.6 the hand-written MLP: memory map and program
f = Fig(900, 420, "The hand-written network"); f.text(450, 22, "Running example B: the scratchpad as the programmer laid it out by hand", 14, bold=True)
reg = [("X", 0, 16, C["orange2"], C["orange"]), ("W1", 16, 16, C["orange2"], C["orange"]), ("W2", 32, 8, C["orange2"], C["orange"]), ("acc1", 100, 16, C["blue2"], C["blue"]), ("hidden", 120, 16, C["green2"], C["green"]), ("acc2", 140, 8, C["blue2"], C["blue"]), ("scores", 150, 8, C["green2"], C["green"]), ("class", 160, 4, C["yellow2"], C["gray"])]
x0, sc = 40, 820 / 170
f.line(x0, 120, x0 + 170 * sc, 120, C["ink"], 1.5)
for nm, a, n, fl, st in reg: f.box(x0 + a * sc, 70, max(n * sc, 14), 44, [""], fl, st, 10, rx=2); f.text(x0 + a * sc + n * sc / 2, 62, nm, 11.5, st, bold=True); f.text(x0 + a * sc, 136, str(a), 10, C["line"], "start")
f.text(450, 158, "scratchpad word address (not to scale in the gaps: 40..99 and 164..4095 are unused here)", 11.5, C["line"], italic=True)
y = 190
for line in B["prog"].splitlines():
    if line.strip() == "halt": continue
    op = line.split()[0].upper(); fl, st = UC.get(op, (C["gray2"], C["gray"])); txt = line.split(";")[0].strip(); f.box(40, y, 130, 20, [op], fl, st, 11, True, rx=3, mono=True); f.text(180, y + 15, txt[len(op):].strip()[:84], 10.5, C["line"], "start", mono=True); y += 24
f.save(f"{OUT}/ch07-mlp.svg")
# 7.7 unit costs (measured, from the run output)
txt = open(f"{ROOT}/out/ch07_run_out.txt").read(); rows_ = re.findall(r"^(ga2_\w+)\s+(\d+|n/a \(scratchpad\))\s+(\d+)\s+(\d+)\s+(\d+)\s*$", txt, re.M)
rows_ = [r for r in rows_ if r[0] != "ga2"]; ch = bar_chart(860, 360, [r[0].replace("ga2_", "") for r in rows_], [[int(r[1]) if r[1].isdigit() else 0 for r in rows_]], "Generic gates of each GA-2 unit (measured with Yosys 0.33)", "gates", colors=[C["blue"]], fmt="{:,.0f}"); ch.save(f"{OUT}/ch07-cost.svg")
print("7 figures written")
