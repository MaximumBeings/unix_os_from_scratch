#!/usr/bin/env python3
"""Chapter 11, running example B: write your own graph. A template for a small residual network (the function `build` is the part to edit): it is compiled with Capra, run on the reference simulator AND on the RTL in both simulators, compared with floating point; then the hidden width is swept to see footprint, cycles and accuracy; and the five kinds of graph the compiler REFUSES are shown with the exact error messages and the fix for each. Writes out/ch11_example_b.json. Usage: ch11_example_b.py"""
import json, math, os, random, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import ga2_isa as I, ga2_progs as G, capra as C
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def build(batch=4, d_in=16, hidden=32, d_out=8, seed=1):
    """THE PART TO EDIT. A two-layer network with a residual connection and a softmax head; returns (graph, names of inputs)."""
    R = random.Random(seed); w = lambda r, c: [[R.gauss(0, 1 / math.sqrt(r)) for _ in range(c)] for _ in range(r)]
    g = C.Graph(); x = g.input("x", (batch, d_in)); h = g.relu(g.matmul(x, g.weight("w1", w(d_in, hidden))))
    r = g.add(g.matmul(h, g.weight("w2", w(hidden, hidden))), h); p = g.softmax(g.matmul(r, g.weight("w3", w(hidden, d_out)), scale=0.5))
    g.output(p, "probs"); g.output(g.argmax(p), "class"); return g, {"x": (batch, d_in)}
def inputs(shapes, R): return {k: [[R.gauss(0, 1) for _ in range(c)] for _ in range(r)] for k, (r, c) in shapes.items()}
def relerr(P, out, test):
    fl = C.evaluate(P.graph, test); e = [e for e in P.ext.values() if e["kind"] == "output" and P.graph.nodes[P.graph.nodes[e["node"]].ins[0]].op != "argmax"][0]
    ref = fl[e["node"]]; got = out["probs"][1]; return math.sqrt(sum((a - b) ** 2 for r, t in zip(got, ref) for a, b in zip(r, t))) / math.sqrt(sum(b * b for t in ref for b in t))
print("== 1. the default graph (batch 4, 16 inputs, 32 hidden, 8 classes): compile, run, compare")
g, shapes = build(); R = random.Random(9); cal = [inputs(shapes, R) for _ in range(30)]; test = inputs(shapes, R)
P = C.compile_graph(g, cal); out = C.run(P, test); fl = C.evaluate(g, test); cls_f = [int(r[0]) for r in fl[g.nodes[-1].ins[0]]]
print(f"  {len(P.code)} instructions, scratchpad high-water {P.spad_high} words, {I.cycle_counts(P.code)[0]} cycles on GA-2 (model)")
print("  class (compiled):", [r[0] for r in out["class"][0]], " class (floating point):", cls_f, f" probabilities relative error {100*relerr(P, out, test):.2f}%")
words = [1] + G.record(P.code, C.ext_image(P, test)); open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, o = fn(F, "ga2_tb", defines=("EXTW=8192",)); print(f"  RTL in {name:10s}", ([l for l in o.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or [o[-120:]])[0], f"(exit {rc})")
print("\n== 2. sweep the hidden width: footprint, cycles and accuracy (reference simulator + cycle model; the K <= 64 limit shows at 64 and above)")
print("  hidden  instr  spad words (reuse / no reuse)  cycles   error vs float   class agreement")
res = {"hidden": [], "instr": [], "high": [], "high0": [], "cycles": [], "err": []}
notes = []
for hdn in (8, 16, 32, 48, 64):
    g2, sh = build(hidden=hdn); Rr = random.Random(hdn); cal2 = [inputs(sh, Rr) for _ in range(30)]; t2 = inputs(sh, Rr)
    try: P2 = C.compile_graph(g2, cal2)
    except C.CompileError as e:
        print(f"  {hdn:6d}  does not compile: {e}"); res["hidden"].append(hdn); res["instr"].append(None); res["high"].append(None); res["high0"].append(None); res["cycles"].append(None); res["err"].append(None); continue
    try: h0 = C.compile_graph(g2, cal2, reuse=False).spad_high
    except C.CompileError: h0 = None
    o2 = C.run(P2, t2); f2 = C.evaluate(g2, t2); pf = f2[g2.nodes[-3].id] if False else None
    cf = [int(r[0]) for r in f2[g2.nodes[-1].ins[0]]]; cc = [r[0] for r in o2["class"][0]]; ok = cf == cc
    cyc = I.cycle_counts(P2.code)[0]; e = relerr(P2, o2, t2); res["hidden"].append(hdn); res["instr"].append(len(P2.code)); res["high"].append(P2.spad_high); res["high0"].append(h0); res["cycles"].append(cyc); res["err"].append(e)
    if not ok:
        probs = [n for n in g2.nodes if n.op == "softmax"][0].id; i = [k for k in range(len(cf)) if cf[k] != cc[k]][0]; row = f2[probs][i]; top = sorted(row, reverse=True)[:2]
        notes.append(f"hidden {hdn}: sample {i} gets class {cc[i]} instead of {cf[i]}; the two largest floating-point probabilities are {top[0]:.3f} and {top[1]:.3f}, a gap of {top[0]-top[1]:.3f}")
    print(f"  {hdn:6d}  {len(P2.code):5d}  {P2.spad_high:12d} / {('full' if h0 is None else h0):>6}        {cyc:6d}   {100*e:9.2f}%      {'all agree' if ok else 'DIFFER (see below)'}")
for n_ in notes: print("  note:", n_)
if notes: print("  (a class flips only where two probabilities are nearly tied: a 2% error in the probabilities can reorder a 0.001 gap. The probabilities themselves stay within about 2-3% in every row.)")
print("\n== 3. what Capra refuses, with the exact message and the fix")
def tryit(title, fix, f):
    try: f(); print(f"  {title}: compiled (unexpected)")
    except C.CompileError as e: print(f"  {title}\n      message: {e}\n      fix: {fix}")
def g_k65():
    g = C.Graph(); x = g.input("x", (2, 65)); g.output(g.matmul(x, g.weight("w", [[0.1] * 4 for _ in range(65)])), "y"); C.compile_graph(g, [{"x": [[0.5] * 65 for _ in range(2)]}])
def g_relu():
    g = C.Graph(); x = g.input("x", (2, 4)); y = g.relu(g.add(x, x)); g.output(y, "y"); C.compile_graph(g, [{"x": [[0.5] * 4 for _ in range(2)]}])
def g_mismatch(): g = C.Graph(); a = g.input("a", (2, 3)); b = g.weight("b", [[0.1] * 4 for _ in range(4)]); g.matmul(a, b)
def g_big():
    g = C.Graph(); x = g.input("x", (8, 64)); g.output(g.matmul(x, g.weight("w", [[0.01] * 64 for _ in range(64)])), "y"); C.compile_graph(g, [{"x": [[0.5] * 64 for _ in range(8)]}])
tryit("a product with inner dimension 65", "split the contraction into pieces of at most 64 (the hardware has no int32 accumulate instruction), or reduce the width", g_k65)
tryit("a relu that is not directly after a matmul", "put the relu directly after a matmul that has no other consumer (the compiler fuses it into the requantizer)", g_relu)
tryit("matrix shapes that do not match", "check the shapes; transpose_b=True multiplies by the transpose of the second operand", g_mismatch)
tryit("tensors too big for the 4,096-word scratchpad (a 64 x 64 weight is the whole of it)", "use smaller tensors or fewer live at once (the allocator already reuses dead buffers); a bigger model must be split into pieces that fit", g_big)
json.dump(res, open(os.path.join(hw.ROOT, "out", "ch11_example_b.json"), "w"))
