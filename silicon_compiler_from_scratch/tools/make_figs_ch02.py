#!/usr/bin/env python3
"""Chapter 2 figures -> docs/assets/fig/ch02-*.svg. Charts are computed from the same arithmetic the chapter's scripts use."""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart, line_chart
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "assets", "fig")
def tc(bits):                                                         # value of a two's-complement bit string
    n = len(bits); v = int(bits, 2); return v - (1 << n) if bits[0] == "1" else v
# 2.1 weights strip
f = Fig(900, 400, "Weights of the eight bits of a signed byte"); f.text(450, 22, "An 8-bit signed number: add up the weights of the 1 bits, where the top weight is NEGATIVE", 14, bold=True)
W = [-128, 64, 32, 16, 8, 4, 2, 1]; x0 = 110
for i, w in enumerate(W): f.box(x0 + i * 70, 44, 66, 40, [str(w)], C["red2"] if i == 0 else C["blue2"], C["red"] if i == 0 else C["blue"], 15, True)
f.text(60, 70, "weight", 13, C["line"], "end", italic=True)
for r, bits in enumerate(("01010011", "10000000", "11111111", "10110101", "01111111")):
    y = 100 + r * 52; f.text(60, y + 25, "bits", 13, C["line"], "end", italic=True)
    terms = []
    for i, b in enumerate(bits):
        f.box(x0 + i * 70, y, 66, 36, [b], C["yellow2"] if b == "1" else "#ffffff", C["gray"], 16, b == "1", mono=True)
        if b == "1": terms.append(W[i])
    v = tc(bits); assert v == sum(terms)
    f.text(x0 + 8 * 70 + 8, y + 15, " + ".join(f"({t})" if t < 0 else str(t) for t in terms) if len(terms) < 6 else "(-128) + the 1 bits", 11, C["line"], "start", mono=True)
    f.text(x0 + 8 * 70 + 8, y + 31, f"= {v}", 13, C["red"] if v < 0 else C["green"], "start", True, True)
f.lines(450, 376, ["-128 = 10000000 is the only value with no positive twin. 127 = 01111111 is the largest. Chapter 3 will avoid -128 on purpose."], 12, C["line"]); f.save(f"{OUT}/ch02-weights.svg")
# 2.2 4-bit ring
f = Fig(560, 460, "The two's-complement ring (4 bits)"); cx, cy, R = 280, 235, 150; f.text(280, 24, "A 4-bit number is a position on a ring of 16: going past +7 lands on -8", 14, bold=True)
f.circ(cx, cy, R, "none", C["gray"], 2)
for k in range(16):
    a = -math.pi / 2 + 2 * math.pi * k / 16; v = k if k < 8 else k - 16; x, y = cx + R * math.cos(a), cy + R * math.sin(a); col = C["green2"] if v >= 0 else C["red2"]; st = C["green"] if v >= 0 else C["red"]
    f.circ(x, y, 19, col, st, 1.8); f.text(x, y + 5, str(v), 14, bold=True); xl, yl = cx + (R - 46) * math.cos(a), cy + (R - 46) * math.sin(a); f.text(xl, yl + 4, format(k, "04b"), 11, C["line"], mono=True)
f.text(cx, cy - 8, "go clockwise", 13, C["blue"], bold=True); f.text(cx, cy + 12, "to ADD", 13, C["blue"], bold=True)
a7, a8 = -math.pi / 2 + 2 * math.pi * 7 / 16, -math.pi / 2 + 2 * math.pi * 8 / 16
[f.circ(cx + R * math.cos(a), cy + R * math.sin(a), 25, "none", C["orange"], 2.6) for a in (a7, a8)]; f.text(cx, cy + 42, "7 + 1  ->  -8", 13, C["orange"], bold=True)
f.lines(280, 440, ["7 + 1 wraps to -8 (0111 + 0001 = 1000). A saturating adder would stop at 7 instead."], 12, C["orange"]); f.save(f"{OUT}/ch02-ring.svg")
# 2.3 shift-and-add rows for 5 * (-3)
f = Fig(780, 400, "Shift-and-add for 5 x (-3)"); f.text(390, 22, "How mul8_sa works, on 5 x (-3):  b = 11111101 has weights (-128)+64+32+16+8+4+0+1", 14, bold=True)
a = 5; b = "11111101"; Wb = [-128, 64, 32, 16, 8, 4, 2, 1]; rows = []
f.text(120, 56, "bit of b", 12, C["line"], "end", italic=True); f.text(250, 56, "weight", 12, C["line"], "middle", italic=True); f.text(520, 56, "row = a x weight  (a = 5)", 12, C["line"], "middle", italic=True); f.text(700, 56, "added?", 12, C["line"], "middle", italic=True)
tot = 0
for i in range(8):
    y = 66 + i * 34; bit = b[i]; w = Wb[i]; on = bit == "1"; val = a * w if on else 0; tot += val
    f.box(80, y, 40, 28, [bit], C["yellow2"] if on else "#ffffff", C["gray"], 14, on, mono=True); f.text(250, y + 19, f"{w:+d}", 14, C["red"] if w < 0 else C["ink"], bold=True, mono=True)
    f.box(380, y, 280, 28, [f"{val:+d}" if on else "0  (left out)"], (C["red2"] if w < 0 else C["blue2"]) if on else "#ffffff", (C["red"] if w < 0 else C["blue"]) if on else C["gray"], 13, on, mono=True)
    f.text(700, y + 19, ("subtract" if w < 0 else "add") if on else "no", 12, C["red"] if (w < 0 and on) else C["line"], mono=True)
assert tot == -15
f.line(380, 346, 660, 346, C["ink"], 2); f.text(520, 372, f"sum of the rows = {tot}  = 5 x (-3)", 15, C["green"], bold=True, mono=True)
f.text(120, 372, "(in hardware: rows 0..6 are added,", 11, C["line"], "start"); f.text(120, 388, "row 7 is subtracted)", 11, C["line"], "start"); f.save(f"{OUT}/ch02-rows.svg")
# 2.4 MAC datapath
f = Fig(780, 390, "The MAC datapath"); f.text(390, 22, "One MAC cell: multiply, add to the accumulator, clamp or wrap, store", 14, bold=True)
f.text(40, 80, "a (int8)", 13, bold=True, mono=True, anchor="start"); f.text(40, 130, "b (int8)", 13, bold=True, mono=True, anchor="start")
f.arrow(112, 76, 178, 94, C["ink"], 2); f.arrow(112, 126, 178, 114, C["ink"], 2); f.box(180, 70, 100, 56, ["multiply", "8 x 8"], C["blue2"], C["blue"], 13, True)
f.arrow(282, 98, 348, 98, C["ink"], 2.4); f.text(315, 88, "p: 16 bits", 11, C["line"], italic=True); f.box(350, 70, 110, 56, ["sign-extend", "to 33 bits"], C["gray2"], C["gray"], 12)
f.arrow(462, 98, 528, 98, C["ink"], 2.4); f.box(530, 56, 110, 84, ["adder", "33 bits"], C["blue2"], C["blue"], 13, True)
f.arrow(642, 98, 696, 98, C["ink"], 2.4); f.text(670, 88, "wide", 11, C["line"], italic=True)
f.box(660, 150, 100, 74, ["clamp or", "wrap?", "SAT"], C["orange2"], C["orange"], 13, True); f.path("M710,100 L710,148", "none", C["ink"], 2.4); f.poly([(710, 150), (705, 140), (715, 140)], C["ink"])
f.arrow(700, 228, 700, 272, C["ink"], 2.4); f.text(716, 252, "32 bits", 11, C["line"], "start", italic=True)
f.box(540, 274, 220, 56, ["accumulator", "32 flip-flops"], C["green2"], C["green"], 13, True)
f.path("M540,302 L500,302 L500,150 L560,150 L560,142", "none", C["purple"], 2.4); f.poly([(560, 142), (555, 152), (565, 152)], C["purple"]); f.text(430, 232, "acc feeds back:", 12, C["purple"], italic=True); f.text(430, 248, "next = acc + p", 12, C["purple"], mono=True)
f.box(120, 250, 270, 100, [], "#ffffff", C["gray"]); f.lines(255, 274, ["control, every clock edge:", "rst : acc <= 0", "clr : acc <= (en ? p : 0)   (new sum)", "en  : acc <= acc + p       (accumulate)", "else: acc holds"], 11.5, C["ink"], lh=16, mono=False)
f.text(60, 376, "SAT = 0: keep the low 32 bits (wrap)     SAT = 1: clamp to -2^31 .. 2^31-1", 12, C["orange"], "start", italic=True); f.save(f"{OUT}/ch02-macpath.svg")
# 2.5 wrap vs saturate: accumulate worst-case products, N products on x
M = 1 << 32; wrap = lambda x: ((x + (1 << 31)) % M) - (1 << 31); sat = lambda x: max(-(1 << 31), min((1 << 31) - 1, x))
xs = list(range(0, 262145 + 1, 16384)); xs[-1] = 262144; tw = [wrap(n * 16384) / 2**31 for n in xs]; ts = [sat(n * 16384) / 2**31 for n in xs]; tt = [n * 16384 / 2**31 for n in xs]
ch = line_chart(780, 380, [n // 1024 for n in xs], [tt, tw, ts], "Accumulator after N worst-case products (units of 2^31)", "N, thousands of products of +16384", "accumulator / 2^31", [C["gray"], C["red"], C["green"]], ["true sum", "wrapping MAC", "saturating MAC"], xfmt="{:g}", yfmt="{:.1f}")
ch.text(560, 100, "the wrapping MAC jumps from", 12, C["red"], "start", italic=True); ch.text(560, 116, "+1.0 to -1.0 at N = 131072", 12, C["red"], "start", italic=True); ch.save(f"{OUT}/ch02-wrapsat.svg")
# 2.6 bit growth
Ks = [1, 4, 16, 64, 256, 1024, 4096, 16384, 65536, 131072]; need = [(k * 16384).bit_length() + 1 for k in Ks]
ch = bar_chart(780, 340, [str(k) for k in Ks], [need], "Bits needed for the worst-case sum of K int8 x int8 products (derived)", "bits", colors=[C["blue"]], fmt="{:.0f}", maxv=36)
ch.line(70, 40 + (340 - 100) * (1 - 32 / 36), 760, 40 + (340 - 100) * (1 - 32 / 36), C["red"], 2.2, "6 4"); ch.text(80, 40 + (340 - 100) * (1 - 32 / 36) - 6, "32-bit accumulator", 12, C["red"], "start", True); ch.text(390, 328, "K (number of products summed)", 12, C["line"]); ch.save(f"{OUT}/ch02-bitgrowth.svg")
print("6 figures written")
