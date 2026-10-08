#!/usr/bin/env python3
"""Chapter 12: a tiny transformer language model and the host program that decodes with it on GA-2.
THE MODEL.  vocabulary V = 16 tokens, width D = 16, feed-forward width 32, one layer, one attention head, no layer normalization (the compiler of Chapter 11 has no such operation).
   x = embedding[token]
   q, k, v = x Wq, x Wk, x Wv ;  keys/values of all tokens so far = the KV cache (with the new k, v appended)
   a = softmax(q K^T / sqrt(D)) V ;   h = x + a Wo ;   y = h + relu(h W1) W2 ;   logits = y Wout ;   next token = argmax logits
THE WEIGHTS ARE CONSTRUCTED, NOT TRAINED. The embeddings are the 16 orthogonal rows of a Hadamard matrix, and Wout is arranged so that the logit of token j is large when the input token is f^-1(j): the model's next token is
   f(t) = (5 t + 3) mod 16, a permutation with one cycle of length 16. Attention and the feed-forward block are real (their weights are random and their outputs are added to the residual stream) but small, so they
perturb the logits instead of deciding them. What this tests is the whole PIPELINE (compiler, scales, KV cache, ISA, chip), not the model's quality. A second, generic model with random weights measures how often
the int8 chip picks the same token as floating point.
THE HOST.  The host picks the embedding row (the chip has no gather instruction), keeps the KV cache between steps (appending the dequantized k and v the chip returns) and feeds the chosen token back.
"""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GA2_EXT", "8192")
import compiler as C, ga2_isa as I
V = D = 16; HID = 32
def f_next(t): return (5 * t + 3) % 16
def hadamard(n):
    h = [[1]]
    while len(h) < n: h = [r + r for r in h] + [r + [-x for x in r] for r in h]
    return h
def make_model(kind="structured", seed=1, noise=0.45):
    R = random.Random(seed); g = lambda r, c, sd: [[R.gauss(0, sd) for _ in range(c)] for _ in range(r)]
    if kind == "structured":
        E = [[x / 4 for x in row] for row in hadamard(16)]                        # unit-norm, mutually orthogonal rows
        finv = {f_next(t): t for t in range(V)}; Wout = [[8.0 * E[finv[j]][d] for j in range(V)] for d in range(D)]
        s = noise
    else:
        E = [[x / 4 for x in r] for r in g(V, D, 1.0)]; Wout = g(D, V, 1 / math.sqrt(D)); s = 1.0
    return {"kind": kind, "E": E, "Wq": g(D, D, s / math.sqrt(D)), "Wk": g(D, D, s / math.sqrt(D)), "Wv": g(D, D, s / math.sqrt(D)), "Wo": g(D, D, s / math.sqrt(D)),
            "W1": g(D, HID, s / math.sqrt(D)), "W2": g(HID, D, s / math.sqrt(HID)), "Wout": Wout}
# ---------------------------------------------------------------- the floating-point reference, written without the graph IR
def mv(x, W): return [sum(x[i] * W[i][j] for i in range(len(x))) for j in range(len(W[0]))]
def float_step(m, tok, kc, vc):
    x = m["E"][tok]; q, k, v = mv(x, m["Wq"]), mv(x, m["Wk"]), mv(x, m["Wv"]); K = kc + [k]; Vv = vc + [v]
    sc = [sum(q[i] * Kr[i] for i in range(D)) / math.sqrt(D) for Kr in K]; mx = max(sc); e = [math.exp(s - mx) for s in sc]; z = sum(e)
    a = [sum(e[p] / z * Vv[p][i] for p in range(len(K))) for i in range(D)]
    h = [xi + oi for xi, oi in zip(x, mv(a, m["Wo"]))]; f2 = mv([max(0.0, t) for t in mv(h, m["W1"])], m["W2"]); y = [hi + fi for hi, fi in zip(h, f2)]
    logits = mv(y, m["Wout"]); return logits, k, v, {"x": x, "o": mv(a, m["Wo"]), "f": f2}
def float_generate(m, start, n):
    toks = [start]; kc, vc = [], []
    for _ in range(n):
        logits, k, v, _ = float_step(m, toks[-1], kc, vc); kc.append(k); vc.append(v); toks.append(logits.index(max(logits)))
    return toks
# ---------------------------------------------------------------- the graph for one decode step with L tokens in context
def build_graph(m, L):
    g = C.Graph(); x = g.tag(g.input("x", (1, D)), "x"); w = lambda nm: g.weight(nm, m[nm])
    q = g.matmul(x, w("Wq")); k = g.tag(g.matmul(x, w("Wk")), "k"); v = g.tag(g.matmul(x, w("Wv")), "v")
    if L > 1: K = g.tag(g.concat_rows(g.tag(g.input("kc", (L - 1, D)), "k"), k), "k"); Vv = g.tag(g.concat_rows(g.tag(g.input("vc", (L - 1, D)), "v"), v), "v")
    else: K, Vv = k, v
    p = g.softmax(g.matmul(q, K, transpose_b=True, scale=1 / math.sqrt(D))); a = g.matmul(p, Vv); h = g.add(x, g.matmul(a, w("Wo")))
    y = g.add(h, g.matmul(g.relu(g.matmul(h, w("W1"))), w("W2"))); logits = g.matmul(y, w("Wout"))
    g.output(g.argmax(logits), "tok"); g.output(logits, "logits"); g.output(k, "k_new"); g.output(v, "v_new"); return g
def calibrate(m, steps, starts=range(V)):
    """Run the float model from several starting tokens and record, for each context length, the inputs the chip will see; and the ranges of x, k and v over everything seen."""
    calib = {L: [] for L in range(1, steps + 1)}; rk = rv = rx = 0.0
    for s in starts:
        toks = [s]; kc, vc = [], []
        for L in range(1, steps + 1):
            logits, k, v, _ = float_step(m, toks[-1], kc, vc)
            calib[L].append({"x": [m["E"][toks[-1]]], **({"kc": [list(r) for r in kc], "vc": [list(r) for r in vc]} if L > 1 else {})})
            rk = max(rk, max(abs(t) for t in k)); rv = max(rv, max(abs(t) for t in v)); rx = max(rx, max(abs(t) for t in m["E"][toks[-1]])); kc.append(k); vc.append(v); toks.append(logits.index(max(logits)))
    return calib, {"x": rx, "k": rk, "v": rv}
class Chip:
    """The host's view: compiles each context length once, then decodes token by token on the reference simulator, recording each step's program and external memory so the RTL can replay them."""
    def __init__(self, m, steps, starts=range(V)):
        self.m = m; self.calib, self.ranges = calibrate(m, steps, starts); self.progs = {}
    def program(self, L):
        if L not in self.progs: self.progs[L] = C.compile_graph(build_graph(self.m, L), self.calib[L], ranges=self.ranges)
        return self.progs[L]
    def generate(self, start, n, force=None):
        """Decode n tokens from `start`. force: a list of tokens to feed instead of the chip's own choice (teacher forcing). Returns tokens, per-step records and per-step logits."""
        toks = [start]; kc, vc = [], []; rec = []
        for L in range(1, n + 1):
            P = self.program(L); inp = {"x": [self.m["E"][toks[-1]]]}
            if L > 1: inp["kc"] = [list(r) for r in kc]; inp["vc"] = [list(r) for r in vc]
            out, mach = C.run(P, inp, machine=True)
            rec.append({"L": L, "code": P.code, "ext": C.ext_image(P, inp), "logits": out["logits"][1][0], "tok": out["tok"][0][0][0]})
            kc.append(out["k_new"][1][0]); vc.append(out["v_new"][1][0]); toks.append(force[L] if force else out["tok"][0][0][0])
        return toks, rec
if __name__ == "__main__":
    m = make_model("structured"); exp = [0]
    for _ in range(24): exp.append(f_next(exp[-1]))
    print("closed form:", exp); print("float      :", float_generate(m, 0, 24)); chip = Chip(m, 24); t, _ = chip.generate(0, 24); print("chip (ref) :", t, t == exp)
