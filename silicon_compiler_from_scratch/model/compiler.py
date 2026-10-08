#!/usr/bin/env python3
"""Chapter 11: a compiler from a tensor graph to GA-2 programs.
PIPELINE.   graph IR  ->  float evaluation (calibration)  ->  scale planning  ->  tiling  ->  scratchpad allocation  ->  instruction emission  ->  a Program that runs on the reference simulator or the RTL.
THE IR.     Tensors are 2-D (rows, cols). Nodes: input, weight, matmul (optionally with B transposed and a constant factor), softmax (per row), add, relu, concat_rows, argmax (per row), output.
WHAT THE COMPILER DECIDES FOR THE USER: every tensor's int8 scale (from calibration data), every requantizer's multiplier and shift, how a big matrix product is cut into 4x4 output tiles, where each tensor lives in the scratchpad
(buffers are reused once their last consumer has run), and where inputs, weights and outputs live in external memory. Limits of the target: a product's inner dimension K <= 64, 4096 words of scratchpad, int8 tensors.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GA2_EXT", "8192")
import ga2_isa as I
from quant import quantize, mantissa_shift, requant, matmul_int8
from softmax_gold import softmax_fixed
B = I.build
class CompileError(Exception): pass
# ------------------------------------------------------------------ the IR
class Node:
    def __init__(self, id, op, ins, shape, **attrs): self.id, self.op, self.ins, self.shape, self.attrs = id, op, ins, shape, attrs
class Graph:
    def __init__(self): self.nodes = []
    def _add(self, op, ins, shape, **a): n = Node(len(self.nodes), op, ins, shape, **a); self.nodes.append(n); return n.id
    def input(self, name, shape): return self._add("input", [], shape, name=name)
    def weight(self, name, array): return self._add("weight", [], (len(array), len(array[0])), name=name, array=array)
    def matmul(self, a, b, transpose_b=False, scale=1.0):
        (m, k), (k2, n) = self.nodes[a].shape, self.nodes[b].shape[::-1] if transpose_b else self.nodes[b].shape
        if k != k2: raise CompileError(f"matmul shapes {self.nodes[a].shape} x {self.nodes[b].shape}{'^T' if transpose_b else ''}")
        return self._add("matmul", [a, b], (m, n), transpose_b=transpose_b, scale=scale)
    def softmax(self, a): return self._add("softmax", [a], self.nodes[a].shape)
    def add(self, a, b):
        if self.nodes[a].shape != self.nodes[b].shape: raise CompileError("add: shapes differ")
        return self._add("add", [a, b], self.nodes[a].shape)
    def relu(self, a): return self._add("relu", [a], self.nodes[a].shape)
    def concat_rows(self, a, b):
        if self.nodes[a].shape[1] != self.nodes[b].shape[1]: raise CompileError("concat_rows: widths differ")
        return self._add("concat", [a, b], (self.nodes[a].shape[0] + self.nodes[b].shape[0], self.nodes[a].shape[1]))
    def argmax(self, a): return self._add("argmax", [a], (self.nodes[a].shape[0], 1))
    def output(self, a, name): return self._add("output", [a], self.nodes[a].shape, name=name)
def evaluate(g, inputs):
    """Floating-point evaluation of the graph. inputs: {name: 2-D list}. Returns {node id: 2-D list}."""
    v = {}
    for n in g.nodes:
        if n.op == "input": v[n.id] = inputs[n.attrs["name"]]
        elif n.op == "weight": v[n.id] = n.attrs["array"]
        elif n.op == "matmul":
            a, b = v[n.ins[0]], v[n.ins[1]]; b = [list(r) for r in zip(*b)] if n.attrs["transpose_b"] else b
            v[n.id] = [[n.attrs["scale"] * sum(a[i][t] * b[t][j] for t in range(len(b))) for j in range(len(b[0]))] for i in range(len(a))]
        elif n.op == "softmax":
            out = []
            for r in v[n.ins[0]]: mx = max(r); e = [math.exp(x - mx) for x in r]; z = sum(e); out.append([x / z for x in e])
            v[n.id] = out
        elif n.op == "add": v[n.id] = [[x + y for x, y in zip(r, s)] for r, s in zip(v[n.ins[0]], v[n.ins[1]])]
        elif n.op == "relu": v[n.id] = [[max(0.0, x) for x in r] for r in v[n.ins[0]]]
        elif n.op == "concat": v[n.id] = [list(r) for r in v[n.ins[0]]] + [list(r) for r in v[n.ins[1]]]
        elif n.op == "argmax": v[n.id] = [[float(r.index(max(r)))] for r in v[n.ins[0]]]
        elif n.op == "output": v[n.id] = v[n.ins[0]]
    return v
# ------------------------------------------------------------------ scale planning
def plan_scales(g, calib):
    """Assign one int8 scale to every tensor. Tensors that must share a scale (the parts and the result of a concatenation, input and result of relu) form a group; an add does NOT force one: the compiler rescales an operand whose scale differs from the sum's and get the group's largest calibrated range;
    the input of a softmax is pinned to 1/16 (the softmax unit's Q4.4 input format) and its output to 1/127. Returns (scale per node id, notes)."""
    parent = list(range(len(g.nodes)))
    def find(x):
        while parent[x] != x: parent[x] = parent[parent[x]]; x = parent[x]
        return x
    def union(a, b): parent[find(a)] = find(b)
    for n in g.nodes:
        if n.op == "concat": union(n.ins[0], n.id); union(n.ins[1], n.id)
        elif n.op == "relu": union(n.ins[0], n.id)
        elif n.op == "output": union(n.ins[0], n.id)
    rng = {}
    for sample in calib:
        vals = evaluate(g, sample)
        for nid, m in vals.items(): rng[nid] = max(rng.get(nid, 0.0), max(abs(x) for r in m for x in r))
    grp_max = {}
    for n in g.nodes:
        if n.op != "argmax": grp_max[find(n.id)] = max(grp_max.get(find(n.id), 0.0), rng[n.id])
        if n.op == "add":                                   # the sum's scale must also hold both operands once rescaled to it (large operands may cancel)
            for i in n.ins: grp_max[find(n.id)] = max(grp_max[find(n.id)], rng[i])
    pinned = {}; notes = []
    for n in g.nodes:
        if n.op == "softmax":
            pinned[find(n.ins[0])] = 1 / 16
            if grp_max[find(n.ins[0])] > 127 / 16: notes.append(f"node {n.ins[0]}: scores reach {grp_max[find(n.ins[0])]:.2f}, above the softmax input range of {127/16:.2f}: they will saturate")
            pinned[find(n.id)] = 1 / 127
    scale = {}
    for n in g.nodes:
        if n.op == "argmax": continue
        r = find(n.id); scale[n.id] = pinned.get(r, (grp_max[r] if grp_max[r] > 0 else 1.0) / 127)
        if r in pinned and rng[n.id] > pinned[r] * 127 * 1.001 and n.op in ("concat", "relu", "input", "weight") and n.id != r:
            notes.append(f"node {n.id} ({n.op}) reaches {rng[n.id]:.3f}, above the {pinned[r]*127:.3f} that its pinned scale can hold: it will saturate")
    return scale, notes
# ------------------------------------------------------------------ memory allocation
class Allocator:
    """First-fit allocator for the scratchpad (4096 words) with coalescing free."""
    def __init__(self, size=I.SPAD): self.free_list = [(0, size)]; self.high = 0          # high: the highest scratchpad address ever handed out + 1
    def alloc(self, n):
        for i, (s, sz) in enumerate(self.free_list):
            if sz >= n:
                self.free_list[i:i + 1] = [(s + n, sz - n)] if sz > n else []; self.high = max(self.high, s + n); return s
        raise CompileError(f"scratchpad is full: cannot place a buffer of {n} words (free: {sum(sz for _, sz in self.free_list)} words in {len(self.free_list)} pieces)")
    def release(self, s, n):
        self.free_list.append((s, n)); self.free_list.sort(); merged = []
        for a, b in self.free_list:
            if merged and merged[-1][0] + merged[-1][1] == a: merged[-1] = (merged[-1][0], merged[-1][1] + b)
            else: merged.append((a, b))
        self.free_list = merged
# ------------------------------------------------------------------ the compiler
class Program:
    pass
def compile_graph(g, calib, reuse=True):
    """Compile `g` to GA-2 instructions. `calib` is a list of input dicts for calibration. reuse=False keeps every buffer alive (for debugging and for differential testing of the allocator)."""
    P = Program(); P.graph = g; P.reuse = reuse; scale, P.notes = plan_scales(g, calib); P.scale = scale; N = g.nodes
    cons = {n.id: [] for n in N}
    for n in N:
        for i in n.ins: cons[i].append(n.id)
    # relu fusion
    P.relu_fused = {}
    for n in N:
        if n.op == "relu":
            src = N[n.ins[0]]
            if src.op != "matmul" or len(cons[src.id]) != 1: raise CompileError(f"relu on node {src.id} ({src.op}) cannot be fused into a requantizer")
            P.relu_fused[src.id] = True
    # buffers: where does each tensor live?
    loc = {}                                    # node id -> (buffer id, row offset)
    bufs = []                                   # buffer id -> dict(size, members)
    def new_buf(size): bufs.append({"size": size, "members": [], "addr": None}); return len(bufs) - 1
    for n in reversed(N):                       # consumers first: a concat's output is placed before its inputs, which become views of it; a fused relu's input shares the relu's place
        if n.op in ("output", "argmax"): continue
        if n.id not in loc: loc[n.id] = (new_buf(n.shape[0] * n.shape[1]), 0)
        if n.op == "relu":
            if n.ins[0] in loc: raise CompileError(f"node {n.ins[0]} feeds two consumers that need it in different places")
            loc[n.ins[0]] = loc[n.id]
        if n.op == "concat":
            b0, off = loc[n.id]
            for k, i in enumerate(n.ins):
                if i in loc: raise CompileError(f"node {i} feeds two concatenations")
                loc[i] = (b0, off + (0 if k == 0 else N[n.ins[0]].shape[0]))
    for n in N:
        if n.op == "argmax": loc[n.id] = (new_buf(n.shape[0]), 0)
    for nid, (b, off) in loc.items(): bufs[b]["members"].append(nid)
    # lifetimes in steps (= node order)
    first = {}; last = {}
    for n in N:
        if n.op in ("input", "weight"): continue
        if n.id in loc: first.setdefault(loc[n.id][0], n.id); last[loc[n.id][0]] = max(last.get(loc[n.id][0], 0), n.id)
        for i in n.ins:
            if i in loc: b = loc[i][0]; first.setdefault(b, n.id); last[b] = max(last.get(b, 0), n.id)
    P.ext = {}; ext_top = 0
    # external layout: weights, inputs, outputs
    for n in N:
        if n.op in ("input", "weight"): P.ext[n.attrs["name"]] = {"addr": ext_top, "shape": n.shape, "node": n.id, "kind": n.op}; ext_top += n.shape[0] * n.shape[1]
    for n in N:
        if n.op == "output": P.ext[n.attrs["name"]] = {"addr": ext_top, "shape": n.shape, "node": n.id, "kind": "output"}; ext_top += n.shape[0] * n.shape[1]
    if ext_top > I.EXT: raise CompileError(f"external memory is full ({ext_top} words)")
    alloc = Allocator(); code = []; loaded = set()
    def addr_of(nid):
        b, off = loc[nid]
        if bufs[b]["addr"] is None: bufs[b]["addr"] = alloc.alloc(bufs[b]["size"])
        return bufs[b]["addr"] + off * N[nid].shape[1]
    def ensure(nid):
        n = N[nid]
        if n.op in ("input", "weight") and nid not in loaded:
            a = addr_of(nid); e = P.ext[n.attrs["name"]]["addr"]; code.append(B("LD", dst=a, src=e, len=n.shape[0] * n.shape[1])); loaded.add(nid)
        return addr_of(nid)
    P.params = {}
    for n in N:
        if n.op == "input" or n.op == "weight": continue
        if n.op == "concat" or n.op == "relu": ensure(n.ins[0]); ensure(n.ins[1]) if n.op == "concat" else None; addr_of(n.id); continue
        ins = [ensure(i) for i in n.ins]
        if n.op == "matmul":
            (m, k) = N[n.ins[0]].shape; nn = n.shape[1]; tb = n.attrs["transpose_b"]
            if k > 64: raise CompileError(f"matmul node {n.id}: inner dimension {k} exceeds the matrix unit's limit of 64")
            out = addr_of(n.id); acc = alloc.alloc(m * nn)
            for i0 in range(0, m, 4):
                for j0 in range(0, nn, 4):
                    mt, nt = min(4, m - i0), min(4, nn - j0)
                    code.append(B("MM", dst=acc + i0 * nn + j0, A=ins[0] + i0 * k, B=ins[1] + (j0 * k if tb else j0), M=mt, K=k, N=nt, tb=int(tb), lda=k, ldb=(k if tb else nn), ldc=nn))
            ms = mantissa_shift(n.attrs["scale"] * scale[n.ins[0]] * scale[n.ins[1]] / scale[n.id]); P.params[n.id] = ms
            code.append(B("RQ", dst=out, src=acc, len=m * nn, m=ms[0], s=ms[1], relu=int(P.relu_fused.get(n.id, False)))); alloc.release(acc, m * nn)
        elif n.op == "softmax":
            m, nn = n.shape; out = addr_of(n.id); tmp = alloc.alloc(m * nn)
            for r in range(m): code.append(B("SM", dst=tmp + r * nn, src=ins[0] + r * nn, len=nn))
            ms = mantissa_shift(127 / 65536); P.params[n.id] = ms; code.append(B("RQ", dst=out, src=tmp, len=m * nn, m=ms[0], s=ms[1])); alloc.release(tmp, m * nn)
        elif n.op == "add":
            size = n.shape[0] * n.shape[1]; srcs = []; temps = []
            for k, i in enumerate(n.ins):
                ratio = scale[i] / scale[n.id]
                if abs(ratio - 1) < 1e-9: srcs.append(ins[k]); continue
                t = alloc.alloc(size); temps.append(t); ms = mantissa_shift(ratio); P.params[(n.id, k)] = ms
                code.append(B("RQ", dst=t, src=ins[k], len=size, m=ms[0], s=ms[1])); srcs.append(t)       # int8 -> int8 at the sum's scale
            code.append(B("VADD", dst=addr_of(n.id), src1=srcs[0], src2=srcs[1], len=size))
            for t in temps: alloc.release(t, size)
        elif n.op == "argmax":
            out = addr_of(n.id); (m, nn) = N[n.ins[0]].shape
            for r in range(m): code.append(B("AMAX", dst=out + r, src=ins[0] + r * nn, len=nn))
        elif n.op == "output":
            e = P.ext[n.attrs["name"]]; code.append(B("ST", src=ins[0], dst=e["addr"], len=n.shape[0] * n.shape[1]))
        # free what is no longer needed after this step
        if reuse:
            for b, l in list(last.items()):
                if l == n.id and bufs[b]["addr"] is not None: alloc.release(bufs[b]["addr"], bufs[b]["size"]); bufs[b]["addr"] = None; last[b] = -1
    P.spad_high = alloc.high; P.loc = loc; P.bufs = bufs; P.code = code + [B("HALT")]; P.ext_words = ext_top
    return P
# ------------------------------------------------------------------ running a compiled program
def _q(vals, sc): return [[quantize(v, sc) for v in r] for r in vals]
def ext_image(P, inputs):
    """External memory for a run: weights and the given inputs quantized with the compiler's scales."""
    img = [0] * I.EXT; G = P.graph
    for name, e in P.ext.items():
        if e["kind"] == "output": continue
        n = G.nodes[e["node"]]; data = n.attrs["array"] if e["kind"] == "weight" else inputs[name]
        for r, row in enumerate(_q(data, P.scale[n.id])):
            for c, v in enumerate(row): img[e["addr"] + r * n.shape[1] + c] = v & 0xFFFFFFFF
    return img
def run(P, inputs, machine=False):
    """Run on the reference simulator. Returns {output name: (ints, floats)} (and the machine if asked)."""
    m = I.Machine(ext_image(P, inputs)).run(P.code); out = {}; G = P.graph
    for name, e in P.ext.items():
        if e["kind"] != "output": continue
        n = G.nodes[e["node"]]; src = G.nodes[n.ins[0]]; r, c = n.shape; w = [m.ext[e["addr"] + i] for i in range(r * c)]
        ints = [[(I.s32(w[i * c + j]) if src.op == "argmax" else I.s8(w[i * c + j])) for j in range(c)] for i in range(r)]
        sc = 1.0 if src.op == "argmax" else P.scale[n.id]; out[name] = (ints, [[v * sc for v in row] for row in ints])
    return (out, m) if machine else out
def read_tensor(P, m, nid):
    """The int values of a tensor, read from the scratchpad after a run (valid for every tensor only when compiled with reuse=False)."""
    n = P.graph.nodes[nid]; b, off = P.loc[nid]; base = P.bufs[b]["addr"]
    if base is None: raise ValueError("buffer already released")
    a = base + off * n.shape[1]; r, c = n.shape
    return [[(I.s32(m.spad[a + i * c + j]) if n.op == "argmax" else I.s8(m.spad[a + i * c + j])) for j in range(c)] for i in range(r)]
def interpret(P, inputs):
    """The compiled program's meaning in plain Python integers, node by node: no instructions, no tiles, no addresses. Shares only the arithmetic helpers and the planned scales and requantizers with the compiler."""
    G = P.graph; v = {}
    for n in G.nodes:
        if n.op == "input": v[n.id] = _q(inputs[n.attrs["name"]], P.scale[n.id])
        elif n.op == "weight": v[n.id] = _q(n.attrs["array"], P.scale[n.id])
        elif n.op == "matmul":
            b = v[n.ins[1]]; b = [list(r) for r in zip(*b)] if n.attrs["transpose_b"] else b; acc = matmul_int8(v[n.ins[0]], b); m_, s_ = P.params[n.id]
            v[n.id] = [[requant(a, m_, s_, bool(P.relu_fused.get(n.id))) for a in r] for r in acc]
        elif n.op == "softmax": m_, s_ = P.params[n.id]; v[n.id] = [[requant(p, m_, s_) for p in softmax_fixed(r)] for r in v[n.ins[0]]]
        elif n.op == "add":
            ops = [v[i] if (n.id, k) not in P.params else [[requant(x, *P.params[(n.id, k)]) for x in r] for r in v[i]] for k, i in enumerate(n.ins)]
            v[n.id] = [[max(-127, min(127, x + y)) for x, y in zip(r, s)] for r, s in zip(ops[0], ops[1])]
        elif n.op in ("relu", "output"): v[n.id] = v[n.ins[0]]
        elif n.op == "concat": v[n.id] = [list(r) for r in v[n.ins[0]]] + [list(r) for r in v[n.ins[1]]]
        elif n.op == "argmax": v[n.id] = [[r.index(max(r))] for r in v[n.ins[0]]]
    return v
def stagewise_problems(P, ints):
    """Every tensor must agree, within one level, with the REAL-NUMBER meaning of its operation applied to the dequantized values of its inputs. Uses only the planned scales, not the requantizer parameters."""
    G = P.graph; probs = []; sc = P.scale
    deq = lambda nid: [[x * sc[nid] for x in r] for r in ints[nid]]
    rnd = lambda x: max(-127, min(127, int(math.floor(abs(x) + 0.5)) * (1 if x >= 0 else -1)))
    for n in G.nodes:
        if n.op == "matmul":
            a, b = deq(n.ins[0]), deq(n.ins[1]); b = [list(r) for r in zip(*b)] if n.attrs["transpose_b"] else b
            want = [[rnd(n.attrs["scale"] * sum(a[i][t] * b[t][j] for t in range(len(b))) / sc[n.id]) for j in range(len(b[0]))] for i in range(len(a))]
            if P.relu_fused.get(n.id): want = [[max(0, x) for x in r] for r in want]
        elif n.op == "softmax":
            want = []
            for r in deq(n.ins[0]): mx = max(r); e = [math.exp(x - mx) for x in r]; z = sum(e); want.append([rnd(x / z / sc[n.id]) for x in e])
        elif n.op == "add": want = [[rnd((x + y) / sc[n.id]) for x, y in zip(r, s)] for r, s in zip(deq(n.ins[0]), deq(n.ins[1]))]
        else: continue
        tol = 2 if n.op == "add" else 1                       # an add may rescale both operands (half a level each) before it rounds
        bad = sum(1 for r, s in zip(ints[n.id], want) for x, y in zip(r, s) if abs(x - y) > tol)
        if bad: probs.append(f"node {n.id} ({n.op}): {bad} values more than {tol} level(s) from the real-number result")
    return probs
