#!/usr/bin/env python3
"""Chapter 11: the compiler end to end. A readable example, twelve random graphs run on the RTL in both simulators, and a network that fits the scratchpad only because the allocator reuses buffers. Usage: ch11_run.py"""
import math, os, random, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G, compiler as C, compiler_tests as T
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
def dump(g):
    for n in g.nodes:
        extra = n.attrs.get("name", ""); extra += (" ^T" if n.attrs.get("transpose_b") else "") + (f" x{n.attrs['scale']:.3g}" if n.op == "matmul" and n.attrs.get("scale") != 1.0 else "")
        print(f"  %{n.id:<2d} = {n.op:8s} {', '.join('%' + str(i) for i in n.ins):10s} -> {n.shape}  {extra}")
def err(P, out, test):
    fl = C.evaluate(P.graph, test); w = 0.0
    for nm, e in P.ext.items():
        if e["kind"] != "output" or P.graph.nodes[P.graph.nodes[e["node"]].ins[0]].op == "argmax": continue
        ref = fl[e["node"]]; got = out[nm][1]; num = math.sqrt(sum((a - b) ** 2 for r, t in zip(got, ref) for a, b in zip(r, t))); den = math.sqrt(sum(b * b for t in ref for b in t)) or 1.0; w = max(w, num / den)
    return w
print("== 1. one graph, compiled: a two-layer network with a softmax head and a residual connection")
R = random.Random(11); g = C.Graph()
x = g.input("x", (5, 12)); w1 = g.weight("w1", [[R.gauss(0, 0.3) for _ in range(10)] for _ in range(12)]); h = g.relu(g.matmul(x, w1))
w2 = g.weight("w2", [[R.gauss(0, 0.3) for _ in range(10)] for _ in range(10)]); y = g.add(g.matmul(h, w2), h)
w3 = g.weight("w3", [[R.gauss(0, 0.3) for _ in range(3)] for _ in range(10)]); p = g.softmax(g.matmul(y, w3, scale=0.5)); g.output(p, "probs"); g.output(g.argmax(p), "class")
print("the graph:"); dump(g)
calib = [{"x": [[R.gauss(0, 1) for _ in range(12)] for _ in range(5)]} for _ in range(30)]; test = {"x": [[R.gauss(0, 1) for _ in range(12)] for _ in range(5)]}
P = C.compile_graph(g, calib); print(f"\ncompiled: {len(P.code)} instructions, scratchpad high-water mark {P.spad_high} of {I.SPAD} words, external memory {P.ext_words} words; notes: {P.notes or 'none'}")
print("scales chosen by the compiler (real value per int8 step):", {f"%{k}": round(v, 5) for k, v in P.scale.items()})
print("the program:"); [print(f"  {k:3d}  {I.disassemble(i)}") for k, i in enumerate(P.code)]
o = C.run(P, test); ints = C.interpret(P, test); fl = C.evaluate(g, test)
print("\nclass (circuit-model):", [r[0] for r in o["class"][0]], " class (floating point):", [int(r[0]) for r in fl[g.nodes[-1].ins[0]]], f"  probabilities: relative error {err(P, o, test)*100:.2f}%")
print("\n== 2. twelve random graphs (seeds 100-111, not in the test battery) on the RTL")
progs = []; meta = []
for seed in range(100, 112):
    g, names = T.random_graph(seed); Rr = random.Random(seed); cal = [T.sample_inputs(names, Rr) for _ in range(30)]; tst = T.sample_inputs(names, Rr)
    P = C.compile_graph(g, cal); P0 = C.compile_graph(g, cal, reuse=False); out = C.run(P, tst)
    ext = C.ext_image(P, tst); progs.append((P.code, ext)); meta.append((seed, len(g.nodes), P, P0.spad_high, err(P, out, tst)))
words = [len(progs)]
for prog, ext in progs: words += G.record(prog, ext)
open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
res = {}
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {name:10s}{line(out)} (exit {rc})")
    for m in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out): res[int(m.group(1))] = [int(q) for q in m.groups()[1:]]
print(f"\n{'seed':>5s} {'nodes':>6s} {'instr':>6s} {'scratchpad words (reuse)':>25s} {'(no reuse)':>11s} {'cycles':>8s} {'error vs float':>15s}")
for i, (seed, nn, P, hi0, e) in enumerate(meta): print(f"{seed:5d} {nn:6d} {len(P.code):6d} {P.spad_high:25d} {hi0:11d} {res[i][0]:8d} {e*100:14.2f}%")
print("\n== 3. a network that fits only because buffers are reused: 6 layers of (8 x 32) times (32 x 32)")
g = C.Graph(); Rr = random.Random(5); cur = g.input("x", (8, 32))
for k in range(6): cur = g.relu(g.matmul(cur, g.weight(f"w{k}", [[Rr.gauss(0, 1 / math.sqrt(32)) for _ in range(32)] for _ in range(32)])))
g.output(cur, "y"); cal = [{"x": [[Rr.gauss(0, 1) for _ in range(32)] for _ in range(8)]} for _ in range(10)]
P = C.compile_graph(g, cal); print(f"with reuse: {len(P.code)} instructions, scratchpad high-water mark {P.spad_high} words")
try: C.compile_graph(g, cal, reuse=False); print("without reuse: compiled")
except C.CompileError as e: print("without reuse:", e)
