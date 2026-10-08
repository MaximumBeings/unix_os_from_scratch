#!/usr/bin/env python3
"""Chapter 11: Capra's test battery. A generator of random graphs and the checks applied to each:
  (a) ALLOCATOR: the program compiled with buffer reuse and the one compiled with every buffer kept alive give identical outputs;
  (b) MEANING: the outputs of the reference simulator equal the integer interpreter's, exactly;
  (c) EVERY TENSOR: with reuse off, every tensor read back from the scratchpad equals the interpreter's;
  (d) SCALES: every tensor is within one level of the real-number meaning of its operation (stagewise_problems);
  (e) ACCURACY: the dequantized outputs stay within a bound of the floating-point result on inputs NOT used for calibration.
Usage: capra_tests.py [nseeds]"""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capra as C
def random_graph(seed):
    R = random.Random(seed); g = C.Graph(); rows = R.randrange(1, 7); cols = R.randrange(2, 20); names = {}
    def new_input(shape, sd=1.0): nm = f"in{len(names)}"; names[nm] = (shape, sd); return g.input(nm, shape)
    def std_of(t):          # typical magnitude of a tensor, measured by running the graph so far; a partner input in an add or concat is generated at the same magnitude
        v = C.evaluate(g, sample_inputs(names, random.Random(0)))[t]; flat = [x for r in v for x in r]; return max(1e-3, math.sqrt(sum(x * x for x in flat) / len(flat)))
    def weight(r, c): return g.weight(f"w{len(g.nodes)}", [[R.gauss(0, 1 / math.sqrt(r)) for _ in range(c)] for _ in range(r)])
    cur = new_input((rows, cols)); pool = [cur]; in_concat = set(); outputs = []; used_scores = False
    for _ in range(R.randrange(3, 8)):
        shape = g.nodes[cur].shape; kind = R.choice(["matmul", "matmul", "matmul_relu", "scores", "softmax", "add", "concat"])
        if kind in ("matmul", "matmul_relu"):
            cur = g.matmul(cur, weight(shape[1], R.randrange(1, 11)))
            if kind == "matmul_relu": cur = g.relu(cur)
        elif kind == "scores" and shape[1] <= 64 and not used_scores:         # at most one self-product: repeated squaring spans orders of magnitude, which no static per-tensor scale can serve
            used_scores = True; cur = g.matmul(cur, cur, transpose_b=True, scale=1 / math.sqrt(shape[1]))
        elif kind == "softmax":
            cur = g.softmax(g.matmul(cur, weight(shape[1], R.randrange(2, 9)), scale=0.5))
        elif kind == "add":
            same = [p for p in pool if g.nodes[p].shape == shape and p != cur]; other = R.choice(same) if same else new_input(shape, std_of(cur)); cur = g.add(cur, other)
        elif kind == "concat" and shape[0] <= 9 and cur not in in_concat:
            extra = new_input((R.randrange(1, 4), shape[1]), std_of(cur)); in_concat.update([cur, extra]); cur = g.concat_rows(cur, extra)
        pool.append(cur)
    if R.random() < 0.3: g.output(g.argmax(cur), "tok")
    g.output(cur, "out")
    if len(pool) > 2 and R.random() < 0.3 and pool[1] not in in_concat: g.output(pool[1], "mid")
    return g, names
def sample_inputs(names, R): return {nm: [[R.gauss(0, sd) for _ in range(c)] for _ in range(r)] for nm, ((r, c), sd) in names.items()}
def check_graph(seed, bound=0.16):
    problems = []; g, names = random_graph(seed); R = random.Random(seed * 7 + 1)
    calib = [sample_inputs(names, R) for _ in range(30)]; test = sample_inputs(names, R)
    try: P1 = C.compile_graph(g, calib, reuse=True); P0 = C.compile_graph(g, calib, reuse=False)
    except C.CompileError as e: return [f"seed {seed}: compile error: {e}"], None
    o1 = C.run(P1, test); o0, m0 = C.run(P0, test, machine=True); ints = C.interpret(P0, test)
    if {k: v[0] for k, v in o1.items()} != {k: v[0] for k, v in o0.items()}: problems.append(f"seed {seed}: (a) reuse and no-reuse programs give different outputs")
    for nm, e in P0.ext.items():
        if e["kind"] == "output":
            if o0[nm][0] != ints[P0.graph.nodes[e["node"]].id]: problems.append(f"seed {seed}: (b) output {nm} differs from the interpreter")
    for n in g.nodes:
        if n.op in ("output",): continue
        try:
            if C.read_tensor(P0, m0, n.id) != ints[n.id]: problems.append(f"seed {seed}: (c) tensor {n.id} ({n.op}) read back from the scratchpad differs from the interpreter")
        except ValueError: pass
    problems += [f"seed {seed}: (d) {p}" for p in C.stagewise_problems(P0, ints)]
    fl = C.evaluate(g, test); worst = 0.0
    for nm, e in P0.ext.items():
        if e["kind"] != "output" or g.nodes[g.nodes[e["node"]].ins[0]].op == "argmax": continue
        ref = fl[e["node"]]; got = o0[nm][1]; num = math.sqrt(sum((a - b) ** 2 for r, s in zip(got, ref) for a, b in zip(r, s))); den = math.sqrt(sum(b * b for s in ref for b in s)) or 1.0
        worst = max(worst, num / den)
    if worst > bound: problems.append(f"seed {seed}: (e) error {worst:.3f} over the bound {bound}")
    return problems, worst
def check_many(seeds, bound=0.16):
    probs = []; worst = 0.0
    for s in seeds:
        p, w = check_graph(s, bound); probs += p; worst = max(worst, w or 0.0)
    return probs, worst
def check_cancellation():
    """Directed case: add(x, y) with y close to -x, so the operands are large and their sum is tiny. The sum's scale must still hold the operands (they are rescaled to it); a scale chosen from the sum alone clips them.
    int8 cannot represent the tiny sum itself, so no accuracy bound is applied here: only checks (a)-(d) (reuse, interpreter, every tensor, real-number meaning)."""
    R = random.Random(1); g = C.Graph(); x = g.input("x", (3, 6)); y = g.input("y", (3, 6)); sm = g.add(x, y)
    w = g.weight("w", [[R.gauss(0, 0.4) for _ in range(4)] for _ in range(6)]); g.output(g.matmul(sm, w), "o")
    def smp(): a = [[R.gauss(0, 3) for _ in range(6)] for _ in range(3)]; return {"x": a, "y": [[-v + R.gauss(0, 0.05) for v in r] for r in a]}
    calib = [smp() for _ in range(20)]; test = smp(); P1 = C.compile_graph(g, calib, reuse=True); P0 = C.compile_graph(g, calib, reuse=False)
    o1 = C.run(P1, test); o0, m0 = C.run(P0, test, machine=True); ints = C.interpret(P0, test); problems = []
    if o1["o"][0] != o0["o"][0]: problems.append("cancellation: (a) reuse and no-reuse differ")
    if o0["o"][0] != ints[g.nodes[-1].id]: problems.append("cancellation: (b) output differs from the interpreter")
    for n in g.nodes:
        if n.op != "output" and C.read_tensor(P0, m0, n.id) != ints[n.id]: problems.append(f"cancellation: (c) tensor {n.id} ({n.op}) differs from the interpreter")
    return problems + ["cancellation: (d) " + q for q in C.stagewise_problems(P0, ints)]
def check_errors():
    """Things Capra must REFUSE. A compiler that quietly accepts them produces wrong code. Returns the list of cases it wrongly accepted."""
    R = random.Random(0); wrong = []
    def W(r, c): return [[R.gauss(0, 0.1) for _ in range(c)] for _ in range(r)]
    def attempt(name, build, inputs):
        try: C.compile_graph(build(), [inputs]); wrong.append(f"accepted: {name}")
        except C.CompileError: pass
    def inner_too_big():
        g = C.Graph(); x = g.input("x", (2, 65)); g.output(g.matmul(x, g.weight("w", W(65, 3))), "o"); return g
    def relu_with_second_consumer():
        g = C.Graph(); x = g.input("x", (2, 4)); y = g.matmul(x, g.weight("w", W(4, 4))); g.output(g.add(g.relu(y), y), "o"); return g
    def relu_on_input():
        g = C.Graph(); x = g.input("x", (2, 4)); g.output(g.relu(x), "o"); return g
    def too_big_for_scratchpad():
        g = C.Graph(); x = g.input("x", (64, 64)); g.output(g.matmul(x, g.weight("w", W(64, 64))), "o"); return g
    def feeds_two_concats():
        g = C.Graph(); x = g.input("x", (1, 3)); y = g.input("y", (1, 3)); a = g.concat_rows(x, y); b = g.concat_rows(x, y); g.output(g.add(a, b), "o"); return g
    mk = lambda r, c: [[R.gauss(0, 1) for _ in range(c)] for _ in range(r)]
    attempt("inner dimension 65", inner_too_big, {"x": mk(2, 65)}); attempt("relu on a tensor with a second consumer", relu_with_second_consumer, {"x": mk(2, 4)})
    attempt("relu that cannot be fused", relu_on_input, {"x": mk(2, 4)}); attempt("buffers larger than the scratchpad", too_big_for_scratchpad, {"x": mk(64, 64)})
    attempt("a tensor feeding two concatenations", feeds_two_concats, {"x": mk(1, 3), "y": mk(1, 3)})
    return wrong
if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    p, w = check_many(range(n)); print(f"{n} random graphs: worst relative error {w:.4f}; problems: {p[:8] if p else 'none'}"); print("refusals:", check_errors() or "all five bad graphs rejected"); print("cancellation:", check_cancellation() or "ok")
