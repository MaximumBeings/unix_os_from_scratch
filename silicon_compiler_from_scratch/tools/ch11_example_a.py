#!/usr/bin/env python3
"""Chapter 11, running example A: compile one attention step pass by pass. The graph is a single decode-step attention (scores of a query against a key cache plus the new key, softmax, weighted sum of the value cache plus the new value). The script prints what every stage of Capra decided: the graph, the calibrated ranges, the int8 scales (and which are pinned), the requantizer parameters, the tiles, the scratchpad allocation over time (recorded from the allocator itself), the emitted program, and then checks the compiled program against the integer interpreter and floating point. Writes out/ch11_example_a.json. Usage: ch11_example_a.py"""
import json, math, os, random, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import ga2_isa as I, capra as C
D, L = 8, 5; R = random.Random(3)
g = C.Graph(); q = g.input("q", (1, D)); kc = g.input("kc", (L, D)); kn = g.input("kn", (1, D)); K = g.concat_rows(kc, kn)
s = g.matmul(q, K, transpose_b=True, scale=1 / math.sqrt(D)); p = g.softmax(s); vc = g.input("vc", (L, D)); vn = g.input("vn", (1, D)); V = g.concat_rows(vc, vn)
o = g.matmul(p, V); g.output(o, "out")
def sample(): return {"q": [[R.gauss(0, 1) for _ in range(D)]], "kc": [[R.gauss(0, 1) for _ in range(D)] for _ in range(L)], "kn": [[R.gauss(0, 1) for _ in range(D)]], "vc": [[R.gauss(0, 1) for _ in range(D)] for _ in range(L)], "vn": [[R.gauss(0, 1) for _ in range(D)]]}
calib = [sample() for _ in range(20)]; test = sample()
print("== PASS 0: the graph (what the user writes)")
names = {n.id: n.attrs.get("name", "") for n in g.nodes}
for n in g.nodes: print(f"  %{n.id:<2d} = {n.op:8s} {', '.join('%' + str(i) for i in n.ins):8s} -> {n.shape}  {names[n.id]}" + (" ^T" if n.attrs.get("transpose_b") else "") + (f" x{n.attrs['scale']:.4f}" if n.op == "matmul" and n.attrs["scale"] != 1 else ""))
print("\n== PASS 1: calibration: run the graph in floating point on 20 sample sets; each tensor's range is the largest |value| seen")
rng = {}
for c in calib:
    for nid, m in C.evaluate(g, c).items(): rng[nid] = max(rng.get(nid, 0.0), max(abs(x) for r in m for x in r))
for n in g.nodes: print(f"  %{n.id:<2d} {n.op:8s} range {rng[n.id]:8.4f}")
print("\n== PASS 2: scale planning: one int8 scale per tensor (range / 127), with groups and pinned scales")
P = C.compile_graph(g, calib)
for n in g.nodes:
    if n.op == "argmax": continue
    pin = "  <- pinned: softmax input must be Q4.4 (1/16)" if n.id == s else "  <- pinned: softmax output is a probability, 1/127" if n.id == p else "  <- shares a scale with its concat result (same buffer)" if n.id in (kc, kn, vc, vn) else ""
    print(f"  %{n.id:<2d} {n.op:8s} scale {P.scale[n.id]:.6f}  (range/127 = {rng[n.id]/127:.6f}){pin}")
print("\n== PASS 3: requantizer parameters: M = factor x scale_a x scale_b / scale_out, written as mantissa / 2^shift (Chapter 3)")
for k, (m_, s_) in P.params.items(): print(f"  node {k}: M ~ {m_} / 2^{s_} = {m_ / 2**s_:.8f}")
print("\n== PASS 4: tiling: each matmul is cut into 4x4 output tiles, one MM instruction each")
for n in g.nodes:
    if n.op != "matmul": continue
    (m, k) = g.nodes[n.ins[0]].shape; nn = n.shape[1]; tiles = [(i, j) for i in range(0, m, 4) for j in range(0, nn, 4)]
    print(f"  %{n.id}: ({m} x {k}) x ({k} x {nn}) -> {math.ceil(m/4)} x {math.ceil(nn/4)} = {len(tiles)} tile(s):", ", ".join(f"[rows {i}-{min(i+3,m-1)}, cols {j}-{min(j+3,nn-1)}]" for i, j in tiles))
print("\n== PASS 5: allocation: scratchpad words over time, recorded from the allocator itself, numbered in the order the events happen")
events = []; code_len = lambda: None
orig_alloc, orig_rel = C.Allocator.alloc, C.Allocator.release
cur = {"code": None}
def alloc(self, n): a = orig_alloc(self, n); events.append(("alloc", a, n, len(cur["code"]) if cur["code"] is not None else 0)); return a
def rel(self, s_, n): orig_rel(self, s_, n); events.append(("free", s_, n, len(cur["code"]) if cur["code"] is not None else 0))
# the compiler builds `code` as a local list; to timestamp events we count them in order instead
C.Allocator.alloc = alloc; C.Allocator.release = rel
P1 = C.compile_graph(g, calib); C.Allocator.alloc, C.Allocator.release = orig_alloc, orig_rel
print("  events in order (the allocator is first-fit with coalescing; a buffer is released after its last consumer):")
live = {}; blocks = []
for k, (kind, a, n, _) in enumerate(events):
    if kind == "alloc": live[(a, n)] = k; print(f"   {k:2d}: alloc {n:4d} words at {a:4d} .. {a+n-1:4d}")
    else: print(f"   {k:2d}: free  {n:4d} words at {a:4d}"); blocks.append((a, n, live.pop((a, n), 0), k))
for (a, n), k0 in live.items(): blocks.append((a, n, k0, len(events)))
print(f"  scratchpad high-water mark with reuse: {P1.spad_high} words; without reuse: {C.compile_graph(g, calib, reuse=False).spad_high} words (of {I.SPAD})")
print("  note: kc and kn are VIEWS of one buffer (the concat result), as are vc and vn: the cache plus the new row is assembled in place, so the concatenation costs no instruction.")
print("\n== PASS 6: emission: the program")
for k, ins in enumerate(P.code): print(f"  {k:3d}  {I.disassemble(ins)}")
print(f"\n{len(P.code)} instructions (including HALT); cycles on GA-2 (model): {I.cycle_counts(P.code)[0]}")
print("\n== checks: the compiled program against the integer interpreter, and against floating point")
out = C.run(P, test); ints = C.interpret(P, test); fl = C.evaluate(g, test); got = out["out"][1]; ref = fl[o]
num = math.sqrt(sum((a - b) ** 2 for r, t in zip(got, ref) for a, b in zip(r, t))); den = math.sqrt(sum(b * b for t in ref for b in t))
exact = out["out"][0] == ints[g.nodes[o].id] if False else out["out"][0] == ints[[n.id for n in g.nodes if n.op == "output"][0]]
print(f"  simulator output equals the interpreter's integers: {exact};  relative error against floating point: {100*num/den:.2f}%")
print("  stage-wise real-number check (every tensor within one level of the meaning of its operation):", C.stagewise_problems(P, ints) or "no problems")
print("  output (float):", [round(v, 3) for v in got[0]]); print("  reference    :", [round(v, 3) for v in ref[0]])
json.dump({"events": events, "blocks": blocks, "high": P1.spad_high, "code": [I.disassemble(i) for i in P.code], "ops": [i["op"] for i in P.code], "scales": {str(k): v for k, v in P.scale.items()}, "nodes": [(n.id, n.op, names[n.id]) for n in g.nodes]}, open(os.path.join(hw.ROOT, "out", "ch11_example_a.json"), "w"))
