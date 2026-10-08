#!/usr/bin/env python3
"""Chapter 10, running example A: how does an adder's delay grow with its width? A 4-bit adder is synthesized to the toy library and its netlist read gate by gate; then adders of width 4 to 64, written three ways ("+", a hand-built ripple chain, and a Kogge-Stone parallel-prefix adder), are synthesized and timed by the book's static timing analyzer. Everything is MEASURED with the toy library (areas in NAND2 equivalents, one made-up delay per cell): the shape of the curves is the lesson, not the nanoseconds. Writes out/ch10_example_a.json. Usage: ch10_example_a.py"""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, synth
R = hw.ROOT; res = {"W": [], "plus": [], "ripple": [], "kogge": []}
def wrap(kind, W):
    top = f"{kind}_w{W}"; open(os.path.join(R, "out", top + ".v"), "w").write(f"module {top}(input [{W-1}:0] a, input [{W-1}:0] b, output [{W}:0] s); demo_{kind} #(.W({W})) u(.*); endmodule\n"); return top
print("== 1. the 4-bit adder through the flow (written with '+')")
top = wrap("add", 4); s = synth.synth(["rtl/demo_add.v", f"out/{top}.v"], top, top); t = synth.sta(os.path.join(R, "out", top + "_net.json"), top)
print(f"cells {s['ncells']}, area {s['area']:.1f} NAND2-equivalents, cell types {dict(sorted(s['cells'].items()))}")
print(f"critical path {t['critical_ns']:.2f} ns, {t['depth']} gates deep, ends at {t['endpoint']}")
print("\nthe netlist (out/%s_net.v), reduced to its gates:" % top)
net = open(os.path.join(R, "out", top + "_net.v")).read()
for m in re.finditer(r"^\s*(\w+) (\S+) \(\n((?:\s+\.\w+\([^)]*\),?\n)+)\s*\);", net, re.M): print("  ", m.group(1), " ".join(x.strip() for x in m.group(3).split("\n") if x.strip()))
print("\n== 2. delay and area against width, three styles; critical path in ns from the static timing analyzer (gate depth in brackets)")
print("   W |   '+' ns (gates) area |  ripple ns (gates) area | Kogge-Stone ns (gates) area")
for W in (4, 8, 16, 32, 64):
    row = {}
    for kind in ("add", "ripple", "kogge"):
        tp = wrap(kind, W); sy = synth.synth(["rtl/demo_add.v", f"out/{tp}.v"], tp, tp); ti = synth.sta(os.path.join(R, "out", tp + "_net.json"), tp); row[kind] = (ti["critical_ns"], ti["depth"], sy["area"])
    res["W"].append(W); res["plus"].append(row["add"]); res["ripple"].append(row["ripple"]); res["kogge"].append(row["kogge"])
    f = lambda r: f"{r[0]:6.2f} ({r[1]:2d}) {r[2]:6.0f}"
    print(f"{W:4d} | {f(row['add'])} | {f(row['ripple'])} | {f(row['kogge'])}")
p, q, k = res["plus"], res["ripple"], res["kogge"]
print(f"\nfrom 4 to 64 bits (16x wider): ripple delay x{q[-1][0]/q[0][0]:.1f}, area x{q[-1][2]/q[0][2]:.1f};  tool '+' delay x{p[-1][0]/p[0][0]:.1f}, area x{p[-1][2]/p[0][2]:.1f};  Kogge-Stone delay x{k[-1][0]/k[0][0]:.1f}, area x{k[-1][2]/k[0][2]:.1f}.")
print(f"at 64 bits Kogge-Stone is {q[-1][0]/k[-1][0]:.1f}x faster than the ripple chain and {k[-1][2]/q[-1][2]:.1f}x its area.")
print("OBSERVATION: Yosys's default flow turned '+' into a ripple chain (the '+' and ripple columns agree to within rounding). A faster adder has to be asked for or written; the tool does not pick one on its own here.")
print("a ripple chain's delay is proportional to its width (the carry passes through every stage, Chapter 1); a prefix adder's grows with log2(width), at the price of area.")
json.dump(res, open(os.path.join(R, "out", "ch10_example_a.json"), "w"))
