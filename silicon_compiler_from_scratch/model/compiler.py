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
    """Assign one int8 scale to every tensor. Tensors that must share a scale (operands and result of add and concat, input and result of relu) form a group and get the group's largest calibrated range;
    the input of a softmax is pinned to 1/16 (the softmax unit's Q4.4 input format) and its output to 1/127. Returns (scale per node id, notes)."""
    parent = list(range(len(g.nodes)))
    def find(x):
        while parent[x] != x: parent[x] = parent[parent[x]]; x = parent[x]
        return x
    def union(a, b): parent[find(a)] = find(b)
    for n in g.nodes:
        if n.op in ("add", "concat"): union(n.ins[0], n.id); union(n.ins[1], n.id)
        elif n.op == "relu": union(n.ins[0], n.id)
        elif n.op == "output": union(n.ins[0], n.id)
    rng = {}
    for sample in calib:
        vals = evaluate(g, sample)
        for nid, m in vals.items(): rng[nid] = max(rng.get(nid, 0.0), max(abs(x) for r in m for x in r))
    grp_max = {}
    for n in g.nodes:
        if n.op != "argmax": grp_max[find(n.id)] = max(grp_max.get(find(n.id), 0.0), rng[n.id])
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
    return scale, notes
