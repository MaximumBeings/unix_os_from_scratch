#!/usr/bin/env python3
"""Appendix G (the EDA flow): run the open-source flow stage by stage on small designs. Needs Yosys. Usage: appx_g.py"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, synth
R = hw.ROOT
def ys(script): rc, out = hw._run(["yosys", "-p", script], cwd=R); return out
def cells(out, n=1):
    sec = out.split("Number of cells")[-1] if "Number of cells" in out else out.split("cells")[-1]; return {m.group(1): int(m.group(2)) for m in re.finditer(r"^\s+(\$?\w+)\s+(\d+)\s*$", sec, re.M)}
F = "rtl/demo_vote3.v"
print("== 1. the flow, one stage at a time, on a three-input majority voter (vote3_assign, written as an expression)")
for stage, cmd in (("read + elaborate (hierarchy)", "hierarchy -top vote3_assign"), ("proc: processes to logic", "hierarchy -top vote3_assign; proc"), ("opt: simplify", "hierarchy -top vote3_assign; proc; opt"), ("techmap: to simple gates", "hierarchy -top vote3_assign; proc; opt; techmap; opt"), ("abc: optimize and map to AND/OR/XOR/MUX gates", "hierarchy -top vote3_assign; proc; opt; techmap; opt; abc -g AND,OR,XOR,MUX; opt_clean")):
    out = ys(f"read_verilog -sv {F}; {cmd}; stat"); c = cells(out); print(f"  {stage:48s} {sum(c.values()):3d} cells: {c}")
print("\n== 2. three descriptions of the same circuit synthesize to the same logic: area and cells in the book's toy library (lib/toy.lib)")
res = {}
for top in ("vote3_assign", "vote3_always", "vote3_gates"):
    r = synth.synth([F], top, "appx_g_" + top); res[top] = r; print(f"  {top:14s}: {r['ncells']} cells {r['cells']}, area {r['area']}")
print("\n== 3. formal equivalence: a miter compares two circuits on ALL inputs at once (a SAT solver proves the outputs never differ)")
def equiv(a, b, extra=""):
    out = ys(f"read_verilog -sv {F}; {extra} proc; opt; miter -equiv -flatten -make_assert {a} {b} m; hierarchy -top m; sat -prove-asserts -show-ports m")
    return ("EQUIVALENT: no input makes the outputs differ" if "no model found: SUCCESS" in out else "DIFFERENT"), out
for a, b in (("vote3_assign", "vote3_always"), ("vote3_assign", "vote3_gates")): print(f"  {a} vs {b}: {equiv(a, b)[0]}")
bad = "module vote3_bad (input a, input b, input c, output y);\n    assign y = (a & b) | (a & c);\nendmodule\n"
open(os.path.join(R, "out", "appx_g_bad.v"), "w").write(bad)
out = ys(f"read_verilog -sv {F}; read_verilog -sv out/appx_g_bad.v; proc; opt; miter -equiv -flatten -make_assert vote3_assign vote3_bad m; hierarchy -top m; sat -prove-asserts -show-ports m")
print(f"  vote3_assign vs vote3_bad (the b & c term forgotten): {'EQUIVALENT' if 'no model found: SUCCESS' in out else 'DIFFERENT (the solver found an input on which the outputs differ)'}; its counterexample:")
for l in out.splitlines():
    if re.match(r"\s+\\in_[abc]\s|\s+\\gold_y|\s+\\gate_y", l) or re.match(r"\s+\\(in_a|in_b|in_c|gold_y|gate_y)", l): print("   ", " ".join(l.split()))
print("\n== 4. synthesis result: the mapped netlist of the voter (toy library cells), first lines")
net = open(os.path.join(R, "out", "appx_g_vote3_assign_net.v")).read().splitlines(); print("\n".join("  " + l for l in net[:14]))
print("\n== 5. architecture changes the result more than the tool: the same function (32-bit addition) written three ways, synthesized to the toy library, with the longest path from the book's timing analyzer (Chapter 10)")
os.makedirs(os.path.join(R, "out"), exist_ok=True); w = "".join(f"module appx_{k}32 (input [31:0] a, input [31:0] b, output [32:0] s); demo_{k} #(32) u (.a(a), .b(b), .s(s)); endmodule\n" for k in ("add", "ripple", "kogge"))
open(os.path.join(R, "out", "appx_g_add32.v"), "w").write(w)
print(f"  {'description':26s} {'cells':>6s} {'area':>8s} {'levels':>7s} {'longest path':>13s}")
for k, label in (("add", "a + b (tool's choice)"), ("ripple", "ripple-carry by hand"), ("kogge", "Kogge-Stone prefix adder")):
    top = f"appx_{k}32"; r = synth.synth(["rtl/demo_add.v", "out/appx_g_add32.v"], top, "appx_g_" + top); t = synth.sta(os.path.join(R, "out", f"appx_g_{top}_net.json"), top); print(f"  {label:26s} {r['ncells']:6d} {r['area']:8.1f} {t['depth']:7d} {t['critical_ns']:10.2f} ns")
print("  the tool turns '+' into a ripple chain (Chapter 10); a prefix adder is faster and larger. 'Synthesis' optimizes the logic you wrote; it does not invent a better algorithm")
print("\n== 6. the stages of a full flow, and which are in this book (Yosys + the book's toy library and analyzer) and which need a real PDK")
for row in (("1. RTL", "Verilog you write", "this book"), ("2. simulation", "Icarus, Verilator: does the RTL do the job", "this book"), ("3. synthesis", "RTL -> gates of a library (Yosys + ABC)", "this book (toy library)"), ("4. equivalence check", "gates still equal the RTL", "this book (miter, ABC cec)"), ("5. static timing", "longest path against the clock", "this book (a model of it)"), ("6. floorplan, place, route", "where cells go and how wires connect them (OpenROAD)", "NOT in this book"), ("7. clock tree, power grid", "distribute the clock and power", "NOT in this book"), ("8. physical verification", "design rules, layout against netlist (DRC, LVS)", "NOT in this book"), ("9. signoff, tape-out", "final timing/power with extracted parasitics; masks", "NOT in this book")): print(f"  {row[0]:28s} {row[1]:56s} {row[2]}")
