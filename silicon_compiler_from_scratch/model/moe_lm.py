#!/usr/bin/env python3
"""Chapter 17: a mixture-of-experts feed-forward block on the tiny language model.
THE MODEL. Chapter 12's decode step with its feed-forward block replaced by E = 4 EXPERTS (each a feed-forward block of hidden width 32, like the original) and a ROUTER: r = h Wr (one number per expert), e = argmax r, y = h + expert_e(h). Top-1 routing, no gate weight (Capra has no element-wise multiply; the router is not trained here, so no gradient needs the gate).
WHY IT IS DONE. A model with E experts has E times the feed-forward parameters, but each token uses one expert: compute per token and, at batch 1, WEIGHT TRAFFIC per token stay those of one expert.
THE CHIP HAS NO BRANCHES, so a decode step is TWO programs and the host in between: program A (attention, residual, router) outputs h and the chosen expert; the host picks the program B_e for that expert; B_e (the expert's two matmuls, the residual and the output projection) outputs the logits. B_e loads only expert e's weights. Dispatch is the host's job (as paging was in Chapter 15), and a wrong dispatch is a wrong answer: the structured model below is built so that only the right expert knows the rule.
"""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GA2_EXT", "8192")
import capra as C, tiny_lm as T, ga2_isa as I
V = D = 16; HID = 32; E = 4
def group(t): return t % E
def make_moe(kind="structured", seed=1, noise=0.45):
    """Structured: token t belongs to group t % 4; the router sends h ~ embedding[t] to expert t % 4; expert e knows the rule f(t) = 5t+3 mod 16 for its four tokens and outputs (embedding[f(t)] - embedding[t]), so that y = h + expert(h) ~ embedding[f(t)]; the output matrix reads embedding[j] for logit j. Only the right expert knows the answer. Random: all weights random."""
    R = random.Random(seed); g = lambda r, c, sd: [[R.gauss(0, sd) for _ in range(c)] for _ in range(r)]
    if kind == "structured":
        Em = [[x / 4 for x in row] for row in T.hadamard(16)]; s = noise; cc = 4.0
        Wout = [[8.0 * Em[j][d] for j in range(V)] for d in range(D)]
        Wr = [[2.0 * sum(Em[t][d] for t in range(V) if group(t) == e) for e in range(E)] for d in range(D)]
        experts = []
        for e in range(E):
            W1 = g(D, HID, 0.02); W2 = g(HID, D, 0.02); members = [t for t in range(V) if group(t) == e]
            for j, t in enumerate(members):
                for d in range(D): W1[d][j] = cc * Em[t][d]; W2[j][d] = (Em[T.f_next(t)][d] - Em[t][d]) / cc
            experts.append((W1, W2))
    else:
        Em = [[x / 4 for x in r] for r in g(V, D, 1.0)]; s = 1.0; Wout = g(D, V, 1 / math.sqrt(D)); Wr = g(D, E, 1.0)
        experts = [(g(D, HID, 1 / math.sqrt(D)), g(HID, D, 1 / math.sqrt(HID))) for _ in range(E)]
    return {"kind": kind, "E": Em, "Wq": g(D, D, s / math.sqrt(D)), "Wk": g(D, D, s / math.sqrt(D)), "Wv": g(D, D, s / math.sqrt(D)), "Wo": g(D, D, s / math.sqrt(D)), "Wr": Wr, "experts": experts, "Wout": Wout}
mv = T.mv
def float_step(m, tok, kc, vc, route=None):
    """One decode step. route: force the expert (to measure what a wrong route costs); default is the router's argmax. Returns logits, route, k, v, h, router logits."""
    x = m["E"][tok]; q, k, v = mv(x, m["Wq"]), mv(x, m["Wk"]), mv(x, m["Wv"]); K = kc + [k]; Vv = vc + [v]
    sc = [sum(q[i] * Kr[i] for i in range(D)) / math.sqrt(D) for Kr in K]; mx = max(sc); e_ = [math.exp(s - mx) for s in sc]; z = sum(e_)
    a = [sum(e_[p] / z * Vv[p][i] for p in range(len(K))) for i in range(D)]; h = [xi + oi for xi, oi in zip(x, mv(a, m["Wo"]))]
    r = mv(h, m["Wr"]); e = r.index(max(r)) if route is None else route; W1, W2 = m["experts"][e]; y = [hi + fi for hi, fi in zip(h, mv([max(0.0, t) for t in mv(h, W1)], W2))]
    return mv(y, m["Wout"]), e, k, v, h, r
def float_generate(m, start, n, flip=None):
    toks = [start]; kc, vc = [], []
    for _ in range(n):
        lg, e, k, v, h, r = float_step(m, toks[-1], kc, vc); kc.append(k); vc.append(v); toks.append(lg.index(max(lg)))
    return toks
# ---------------------------------------------------------------- the programs
def build_A(m, L):
    g = C.Graph(); x = g.tag(g.input("x", (1, D)), "x"); w = lambda nm, a: g.weight(nm, a)
    q = g.matmul(x, w("Wq", m["Wq"])); k = g.tag(g.matmul(x, w("Wk", m["Wk"])), "k"); v = g.tag(g.matmul(x, w("Wv", m["Wv"])), "v")
    if L > 1: K = g.tag(g.concat_rows(g.tag(g.input("kc", (L - 1, D)), "k"), k), "k"); Vv = g.tag(g.concat_rows(g.tag(g.input("vc", (L - 1, D)), "v"), v), "v")
    else: K, Vv = k, v
    p = g.softmax(g.matmul(q, K, transpose_b=True, scale=1 / math.sqrt(D))); a = g.matmul(p, Vv); h = g.tag(g.add(x, g.matmul(a, w("Wo", m["Wo"]))), "h")
    r = g.matmul(h, w("Wr", m["Wr"])); g.output(h, "h"); g.output(g.argmax(r), "route"); g.output(r, "router"); g.output(k, "k_new"); g.output(v, "v_new"); return g
def build_B(m, e, rows=1):
    g = C.Graph(); h = g.tag(g.input("h", (rows, D)), "h"); W1, W2 = m["experts"][e]
    y = g.add(h, g.matmul(g.relu(g.matmul(h, g.weight("W1", W1))), g.weight("W2", W2))); lg = g.matmul(y, g.weight("Wout", m["Wout"]))
    g.output(g.argmax(lg), "tok"); g.output(lg, "logits"); return g
def build_dense_all(m, L):
    """The same function with NO routing: every expert is evaluated and the outputs are added (non-routed experts output ~0 on this model). It shows what the MoE saves."""
    g = C.Graph(); x = g.tag(g.input("x", (1, D)), "x"); w = lambda nm, a: g.weight(nm, a)
    q = g.matmul(x, w("Wq", m["Wq"])); k = g.tag(g.matmul(x, w("Wk", m["Wk"])), "k"); v = g.tag(g.matmul(x, w("Wv", m["Wv"])), "v")
    if L > 1: K = g.tag(g.concat_rows(g.tag(g.input("kc", (L - 1, D)), "k"), k), "k"); Vv = g.tag(g.concat_rows(g.tag(g.input("vc", (L - 1, D)), "v"), v), "v")
    else: K, Vv = k, v
    p = g.softmax(g.matmul(q, K, transpose_b=True, scale=1 / math.sqrt(D))); a = g.matmul(p, Vv); h = g.tag(g.add(x, g.matmul(a, w("Wo", m["Wo"]))), "h"); y = h
    for e in range(E): y = g.add(y, g.matmul(g.relu(g.matmul(h, w(f"W1_{e}", m["experts"][e][0]))), w(f"W2_{e}", m["experts"][e][1])))
    g.output(g.argmax(g.matmul(y, w("Wout", m["Wout"]))), "tok"); g.output(k, "k_new"); g.output(v, "v_new"); return g
class MoEChip:
    """Compiles A for each context length and B for each expert; calibrates from float runs; dispatches on the router's choice. dispatch(route) -> expert lets tests break the dispatch."""
    def __init__(self, m, steps, starts=range(V)):
        self.m = m; self.steps = steps; self.A = {}; self.B = {}; self.ca = {}; self.cb = {e: [] for e in range(E)}; rk = rv = rx = rh = 0.0
        for s in starts:
            toks = [s]; kc, vc = [], []
            for L in range(1, steps + 1):
                lg, e, k, v, h, r = float_step(m, toks[-1], kc, vc); self.ca.setdefault(L, []).append({"x": [m["E"][toks[-1]]], **({"kc": [list(r_) for r_ in kc], "vc": [list(r_) for r_ in vc]} if L > 1 else {})})
                self.cb[e].append({"h": [list(h)]}); rk = max(rk, max(abs(t) for t in k)); rv = max(rv, max(abs(t) for t in v)); rx = max(rx, max(abs(t) for t in m["E"][toks[-1]])); rh = max(rh, max(abs(t) for t in h))
                kc.append(k); vc.append(v); toks.append(lg.index(max(lg)))
        self.ranges = {"x": rx, "k": rk, "v": rv, "h": rh}; allh = [s_ for e in range(E) for s_ in self.cb[e]]
        for e in range(E):
            if not self.cb[e]: self.cb[e] = allh[:8]                  # an expert the calibration runs never used: calibrate on whatever reached the layer
    def progA(self, L):
        if L not in self.A: self.A[L] = C.compile_graph(build_A(self.m, L), self.ca[L], ranges=self.ranges)
        return self.A[L]
    def progB(self, e):
        if e not in self.B: self.B[e] = C.compile_graph(build_B(self.m, e), self.cb[e], ranges=self.ranges)
        return self.B[e]
    def dispatch(self, route): return route
    def generate(self, start, n, force=None):
        toks = [start]; kc, vc = [], []; rec = []
        for L in range(1, n + 1):
            PA = self.progA(L); inp = {"x": [self.m["E"][toks[-1]]]}
            if L > 1: inp["kc"] = [list(r) for r in kc]; inp["vc"] = [list(r) for r in vc]
            oa = C.run(PA, inp); route = oa["route"][0][0][0]; h = oa["h"][1]; e = self.dispatch(route); PB = self.progB(e); ob = C.run(PB, {"h": h})
            rec.append({"L": L, "route": route, "expert": e, "h": h[0], "router": oa["router"][1][0], "logits": ob["logits"][1][0], "tok": ob["tok"][0][0][0], "progs": [{"kind": "A", "code": PA.code, "ext": C.ext_image(PA, inp)}, {"kind": "B", "code": PB.code, "ext": C.ext_image(PB, {"h": h})}]})
            kc.append(oa["k_new"][1][0]); vc.append(oa["v_new"][1][0]); toks.append(force[L] if force else ob["tok"][0][0][0])
        return toks, rec
def cycles_of(rec): return sum(I.cycle_counts(p["code"])[0] for r in rec for p in r["progs"])
if __name__ == "__main__":
    m = make_moe("structured"); exp = [0]
    for _ in range(24): exp.append(T.f_next(exp[-1]))
    print("float rule ok:", float_generate(m, 0, 24) == exp); chip = MoEChip(m, 24); toks, rec = chip.generate(0, 24); print("chip rule ok:", toks == exp, "routes", [r["route"] for r in rec][:12], "cycles", cycles_of(rec))
class DenseChip:
    """The no-routing comparison: one program per step evaluates every expert."""
    def __init__(self, moe):
        self.m = moe.m; self.moe = moe; self.P = {}
    def prog(self, L):
        if L not in self.P: self.P[L] = C.compile_graph(build_dense_all(self.m, L), self.moe.ca[L], ranges=self.moe.ranges)
        return self.P[L]
    def generate(self, start, n):
        toks = [start]; kc, vc = [], []; rec = []
        for L in range(1, n + 1):
            P = self.prog(L); inp = {"x": [self.m["E"][toks[-1]]]}
            if L > 1: inp["kc"] = [list(r) for r in kc]; inp["vc"] = [list(r) for r in vc]
            o = C.run(P, inp); rec.append({"L": L, "progs": [{"kind": "D", "code": P.code, "ext": C.ext_image(P, inp)}]}); kc.append(o["k_new"][1][0]); vc.append(o["v_new"][1][0]); toks.append(o["tok"][0][0][0])
        return toks, rec
