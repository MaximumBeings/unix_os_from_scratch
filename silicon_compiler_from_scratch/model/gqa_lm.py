#!/usr/bin/env python3
"""Chapter 14: grouped-query attention (GQA) on the tiny language model.
H = 4 query heads of width d = 4 (D = 16). G key/value groups: G = 4 is multi-head attention (MHA), G = 2 is GQA, G = 1 is multi-query attention (MQA). Query head h reads the keys and values of group g = h // (H // G).
THE FLOATING-POINT REFERENCE uses fused matrices and slices them (Wq is D x D, head h is columns 4h..4h+3), the way a framework would; THE CHIP'S GRAPH uses one small weight per head and per group (Capra has no column slice), and no head sums are fused anywhere. The two are written differently on purpose.
CONVERSION: a trained MHA model is turned into GQA by mean-pooling the key and value projections inside each group (the recipe of the GQA paper); `to_groups` does that. Weights here are constructed or random, not trained, so the accuracy numbers are a stand-in.
"""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GA2_EXT", "8192")
import capra as C, tiny_lm as T
V = D = 16; H = 4; DH = 4; HID = 32
def mv(x, W): return T.mv(x, W)
def make_mha(kind="structured", seed=1, noise=0.45):
    """An MHA model: fused Wq, Wk, Wv (D x D), Wo (D x D). Built from tiny_lm's constructor so the structured model keeps its designed rule f(t) = 5t+3 mod 16."""
    m = T.make_model(kind, seed, noise); m = dict(m); m["H"] = H; m["G"] = H; return m
def to_groups(m, G):
    """Mean-pool the key/value head projections inside each group of H/G heads. Wk, Wv become D x (G*DH)."""
    r = H // G; out = dict(m); out["G"] = G
    for nm in ("Wk", "Wv"):
        W = m[nm]; cols = []
        for g in range(G):
            for c in range(DH): cols.append([sum(W[i][(g * r + t) * DH + c] for t in range(r)) / r for i in range(D)])
        out[nm] = [[cols[c][i] for c in range(G * DH)] for i in range(D)]
    return out
def float_step(m, tok, kc, vc, aux=None):
    """One decode step. kc, vc: lists (per position) of G*DH-vectors. Returns logits, k (G*DH), v (G*DH)."""
    G = m["G"]; r = H // G; x = m["E"][tok]; q = mv(x, m["Wq"]); k = mv(x, m["Wk"]); v = mv(x, m["Wv"]); K = kc + [k]; Vv = vc + [v]; heads = []
    for h in range(H):
        g = h // r; qh = q[h * DH:(h + 1) * DH]
        sc = [sum(qh[i] * Kp[g * DH + i] for i in range(DH)) / math.sqrt(DH) for Kp in K]; mx = max(sc); e = [math.exp(s - mx) for s in sc]; z = sum(e)
        heads += [sum(e[p] / z * Vv[p][g * DH + i] for p in range(len(K))) for i in range(DH)]
    att = mv(heads, m["Wo"]); hh = [xi + oi for xi, oi in zip(x, att)]; f2 = mv([max(0.0, t) for t in mv(hh, m["W1"])], m["W2"]); y = [a + b for a, b in zip(hh, f2)]
    if aux is not None: aux["attn"] = att
    return mv(y, m["Wout"]), k, v
def float_generate(m, start, n):
    toks = [start]; kc, vc = [], []
    for _ in range(n):
        lg, k, v = float_step(m, toks[-1], kc, vc); kc.append(k); vc.append(v); toks.append(lg.index(max(lg)))
    return toks
def cache_words_per_token(G): return 2 * G * DH
def build_graph(m, L):
    G = m["G"]; r = H // G; g = C.Graph(); x = g.tag(g.input("x", (1, D)), "x")
    sl = lambda W, a, b: [row[a:b] for row in W]; w = lambda nm, a: g.weight(nm, a)
    Kc, Vc = [], []
    for gi in range(G):
        k = g.tag(g.matmul(x, w(f"Wk{gi}", sl(m["Wk"], gi * DH, (gi + 1) * DH))), "k"); v = g.tag(g.matmul(x, w(f"Wv{gi}", sl(m["Wv"], gi * DH, (gi + 1) * DH))), "v")
        if L > 1: Kg = g.tag(g.concat_rows(g.tag(g.input(f"kc{gi}", (L - 1, DH)), "k"), k), "k"); Vg = g.tag(g.concat_rows(g.tag(g.input(f"vc{gi}", (L - 1, DH)), "v"), v), "v")
        else: Kg, Vg = k, v
        Kc.append(Kg); Vc.append(Vg); g.output(k, f"k_new{gi}"); g.output(v, f"v_new{gi}")
    acc = None
    for h in range(H):
        gi = h // r; q = g.matmul(x, w(f"Wq{h}", sl(m["Wq"], h * DH, (h + 1) * DH)))
        p = g.softmax(g.matmul(q, Kc[gi], transpose_b=True, scale=1 / math.sqrt(DH))); a = g.matmul(p, Vc[gi]); o = g.matmul(a, w(f"Wo{h}", m["Wo"][h * DH:(h + 1) * DH]))
        acc = o if acc is None else g.add(acc, o)
    g.output(acc, "attn"); hh = g.add(x, acc); y = g.add(hh, g.matmul(g.relu(g.matmul(hh, w("W1", m["W1"]))), w("W2", m["W2"]))); lg = g.matmul(y, w("Wout", m["Wout"]))
    g.output(g.argmax(lg), "tok"); g.output(lg, "logits"); return g
def calibrate(m, steps, starts=range(V)):
    G = m["G"]; calib = {L: [] for L in range(1, steps + 1)}; rk = rv = rx = 0.0
    for s in starts:
        toks = [s]; kc, vc = [], []
        for L in range(1, steps + 1):
            lg, k, v = float_step(m, toks[-1], kc, vc); inp = {"x": [m["E"][toks[-1]]]}
            if L > 1:
                for gi in range(G): inp[f"kc{gi}"] = [list(r[gi * DH:(gi + 1) * DH]) for r in kc]; inp[f"vc{gi}"] = [list(r[gi * DH:(gi + 1) * DH]) for r in vc]
            calib[L].append(inp); rk = max(rk, max(abs(t) for t in k)); rv = max(rv, max(abs(t) for t in v)); rx = max(rx, max(abs(t) for t in m["E"][toks[-1]])); kc.append(k); vc.append(v); toks.append(lg.index(max(lg)))
    return calib, {"x": rx, "k": rk, "v": rv}
class Chip:
    def __init__(self, m, steps, starts=range(V)):
        self.m = m; self.G = m["G"]; self.calib, self.ranges = calibrate(m, steps, starts); self.progs = {}
    def program(self, L):
        if L not in self.progs: self.progs[L] = C.compile_graph(build_graph(self.m, L), self.calib[L], ranges=self.ranges)
        return self.progs[L]
    def generate(self, start, n, force=None):
        toks = [start]; kc, vc = [], []; rec = []; G = self.G
        for L in range(1, n + 1):
            P = self.program(L); inp = {"x": [self.m["E"][toks[-1]]]}
            if L > 1:
                for gi in range(G): inp[f"kc{gi}"] = [r[gi * DH:(gi + 1) * DH] for r in kc]; inp[f"vc{gi}"] = [r[gi * DH:(gi + 1) * DH] for r in vc]
            out, mach = C.run(P, inp, machine=True)
            rec.append({"L": L, "code": P.code, "ext": C.ext_image(P, inp), "logits": out["logits"][1][0], "tok": out["tok"][0][0][0], "high": P.spad_high, "attn": out["attn"][1][0], "k": [t for gi in range(G) for t in out[f"k_new{gi}"][1][0]], "v": [t for gi in range(G) for t in out[f"v_new{gi}"][1][0]]})
            kc.append([t for gi in range(G) for t in out[f"k_new{gi}"][1][0]]); vc.append([t for gi in range(G) for t in out[f"v_new{gi}"][1][0]]); toks.append(force[L] if force else out["tok"][0][0][0])
        return toks, rec
if __name__ == "__main__":
    m0 = make_mha("structured"); exp = [0]
    for _ in range(24): exp.append(T.f_next(exp[-1]))
    for G in (4, 2, 1):
        m = to_groups(m0, G) if G != 4 else m0; fl = float_generate(m, 0, 24); t, rec = Chip(m, 24).generate(0, 24)
        print(f"G={G}: float ok {fl == exp}, chip ok {t == exp}, spad high-water {max(r['high'] for r in rec)}")
