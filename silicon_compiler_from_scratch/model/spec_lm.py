#!/usr/bin/env python3
"""Chapter 16: speculative decoding on the tiny language model.
THE IDEA. Decoding one token costs a whole pass over the weights, and the chip is limited by loading them (Chapter 9). A cheap DRAFT model guesses the next k tokens; the expensive TARGET model then checks all k guesses, plus one more position, in ONE pass over its weights: the guessed tokens go through the target together, as a block of m = k+1 rows, with a causal rule (row i may look at the cache and at rows 0..i of the block, no further). The target's own greedy choice after each row is compared with the draft's guess: the longest agreeing prefix is accepted, and the target's choice at the first disagreement (or after the last guess) is a free extra token. One pass over the weights therefore yields between 1 and k+1 tokens, and the tokens are exactly those the target would have produced alone (greedy), because every accepted token is one the target itself chose.
THE DRAFT here is a bigram model: logits = embedding[token] Wd (one 16 x 16 matrix, one matmul). THE VERIFIER is a Capra graph with m query rows; the causal rule uses slice_rows (Chapter 16's new view operation): row i's query is a one-row slice of Q, and its keys and values are PREFIX slices of the cache-plus-block tensors: rows 0 .. c+i. Nothing is copied.
"""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GA2_EXT", "8192")
import capra as C, tiny_lm as T, ga2_isa as I
V = D = 16; HID = 32
# ---------------------------------------------------------------- the verifier graph: m rows, c rows of cache
def build_verify(m, c, mdl):
    g = C.Graph(); X = g.tag(g.input("x", (m, D)), "x"); w = lambda nm: g.weight(nm, mdl[nm])
    Q = g.matmul(X, w("Wq")); Kn = g.tag(g.matmul(X, w("Wk")), "k"); Vn = g.tag(g.matmul(X, w("Wv")), "v")
    if c > 0: K = g.tag(g.concat_rows(g.tag(g.input("kc", (c, D)), "k"), Kn), "k"); Vv = g.tag(g.concat_rows(g.tag(g.input("vc", (c, D)), "v"), Vn), "v")
    else: K, Vv = Kn, Vn
    A = None
    for i in range(m):
        qi = g.slice_rows(Q, i, 1); Ki = g.slice_rows(K, 0, c + i + 1); Vi = g.slice_rows(Vv, 0, c + i + 1)
        ai = g.matmul(g.softmax(g.matmul(qi, Ki, transpose_b=True, scale=1 / math.sqrt(D))), Vi); A = ai if A is None else g.concat_rows(A, ai)
    g.output(A, "attn"); h = g.add(X, g.matmul(A, w("Wo"))); y = g.add(h, g.matmul(g.relu(g.matmul(h, w("W1"))), w("W2"))); lg = g.matmul(y, w("Wout"))
    g.output(g.argmax(lg), "tok"); g.output(lg, "logits"); g.output(Kn, "k_new"); g.output(Vn, "v_new"); return g
def build_draft(Wd):
    g = C.Graph(); x = g.input("x", (1, D)); lg = g.matmul(x, g.weight("Wd", Wd)); g.output(g.argmax(lg), "tok"); g.output(lg, "logits"); return g
# ---------------------------------------------------------------- the floating-point verifier, written as a block with an explicit mask (independent of float_step)
def float_block(mdl, toks, kc, vc, aux=None):
    """Logits of every row of a block of tokens given a cache, as the matrix computation with a causal mask. Returns (logits rows, k rows, v rows)."""
    m = len(toks); c = len(kc); X = [mdl["E"][t] for t in toks]; att = []
    Q = [T.mv(x, mdl["Wq"]) for x in X]; Kn = [T.mv(x, mdl["Wk"]) for x in X]; Vn = [T.mv(x, mdl["Wv"]) for x in X]; K = kc + Kn; Vv = vc + Vn; out = []
    for i in range(m):
        s = [sum(Q[i][t] * K[j][t] for t in range(D)) / math.sqrt(D) if j <= c + i else -1e30 for j in range(c + m)]; mx = max(s); e = [math.exp(t - mx) if t > -1e29 else 0.0 for t in s]; z = sum(e)
        a = [sum(e[j] / z * Vv[j][t] for j in range(c + m)) for t in range(D)]; att.append(a); h = [x + o for x, o in zip(X[i], T.mv(a, mdl["Wo"]))]
        f = T.mv([max(0.0, t) for t in T.mv(h, mdl["W1"])], mdl["W2"]); out.append(T.mv([a_ + b_ for a_, b_ in zip(h, f)], mdl["Wout"]))
    if aux is not None: aux["attn"] = att
    return out, Kn, Vn
# ---------------------------------------------------------------- the draft model
def solve(A, B):
    n = len(A); M = [list(A[i]) + list(B[i]) for i in range(n)]
    for i in range(n):
        p = max(range(i, n), key=lambda r: abs(M[r][i])); M[i], M[p] = M[p], M[i]; d = M[i][i]; M[i] = [x / d for x in M[i]]
        for r in range(n):
            if r != i: f = M[r][i]; M[r] = [a - f * b for a, b in zip(M[r], M[i])]
    return [row[n:] for row in M]
def make_draft(mdl, noise, seed=0):
    """Wd such that embedding[t] Wd is a table of logits: for the structured model one-hot at f(t) (the rule), for a random model the log-frequency of the target's own next token after t; plus Gaussian noise of size `noise`."""
    R = random.Random(seed); L = [[0.0] * V for _ in range(V)]
    if mdl["kind"] == "structured":
        for t in range(V): L[t][T.f_next(t)] = 8.0
    else:
        cnt = [[0.5] * V for _ in range(V)]
        for s in range(V):
            toks = T.float_generate(mdl, s, 24)
            for a, b in zip(toks, toks[1:]): cnt[a][b] += 1
        for t in range(V): z = sum(cnt[t]); L[t] = [4 * math.log(x / z * V) for x in cnt[t]]
    L = [[x + R.gauss(0, noise) for x in r] for r in L]; return solve(mdl["E"], L)
def draft_next(mdl, Wd, tok): lg = T.mv(mdl["E"][tok], Wd); return lg.index(max(lg))
# ---------------------------------------------------------------- the host loop
def speculate(mdl, Wd, start, n, k, verify, draft=None):
    """Generate n tokens. verify(c, toks, kc, vc) -> (token choice per row, k rows, v rows); draft(tok) -> next token. Returns tokens, stats and the list of calls made."""
    seq = [start]; kc, vc = [], []; calls = []; proposed = accepted = blocks = 0
    while len(seq) - 1 < n:
        d = []; t = seq[-1]
        for _ in range(k): t = draft(t); d.append(t); calls.append(("draft",))
        block = [seq[-1]] + d; choice, ks, vs = verify(len(kc), block, kc, vc); calls.append(("verify", len(kc), len(block)))
        a = 0
        while a < k and d[a] == choice[a]: a += 1
        seq += d[:a] + [choice[a]]; kc += ks[:a + 1]; vc += vs[:a + 1]; proposed += k; accepted += a; blocks += 1
    return seq[:n + 1], {"blocks": blocks, "proposed": proposed, "accepted": accepted, "emitted": len(seq) - 1, "cache": len(kc)}, calls
def float_verify(mdl):
    def f(c, toks, kc, vc):
        lg, ks, vs = float_block(mdl, toks, kc, vc); return [r.index(max(r)) for r in lg], ks, vs
    return f
class SpecChip:
    """Compiles the verifier for each (c, m) and the draft once; calibrates from float runs of the same host loop."""
    def __init__(self, mdl, Wd, n, k, starts=range(V)):
        self.m = mdl; self.Wd = Wd; self.k = k; self.vprogs = {}; self.dprog = None; self.log = []
        self.calib = {}; rk = rv = rx = 0.0; dsamples = []
        for s in starts:
            def ver(c, toks, kc, vc, s=s):
                nonlocal rk, rv, rx
                lg, ks, vs = float_block(mdl, toks, kc, vc); inp = {"x": [list(mdl["E"][t]) for t in toks]}
                if c > 0: inp["kc"] = [list(r) for r in kc]; inp["vc"] = [list(r) for r in vc]
                self.calib.setdefault((c, len(toks)), []).append(inp)
                rk = max(rk, max(abs(t) for r in ks for t in r)); rv = max(rv, max(abs(t) for r in vs for t in r)); rx = max(rx, max(abs(t) for t in toks and mdl["E"][toks[0]]))
                return [r.index(max(r)) for r in lg], ks, vs
            def dr(t): dsamples.append({"x": [list(mdl["E"][t])]}); return draft_next(mdl, Wd, t)
            speculate(mdl, Wd, s, n, k, ver, dr)
        self.ranges = {"x": max(max(abs(t) for t in r) for r in mdl["E"]), "k": rk, "v": rv}; self.dcalib = dsamples
    def _vprog(self, c, mm):
        if (c, mm) not in self.vprogs: self.vprogs[(c, mm)] = C.compile_graph(build_verify(mm, c, self.m), self.calib[(c, mm)], ranges=self.ranges)
        return self.vprogs[(c, mm)]
    def verify(self, c, toks, kc, vc):
        P = self._vprog(c, len(toks)); inp = {"x": [list(self.m["E"][t]) for t in toks]}
        if c > 0: inp["kc"] = [list(r) for r in kc]; inp["vc"] = [list(r) for r in vc]
        out = C.run(P, inp); self.last = out; self.log.append({"kind": "verify", "c": c, "m": len(toks), "code": P.code, "ext": C.ext_image(P, inp)})
        return [r[0] for r in out["tok"][0]], [list(r) for r in out["k_new"][1]], [list(r) for r in out["v_new"][1]]
    def draft(self, t):
        if self.dprog is None: self.dprog = C.compile_graph(build_draft(self.Wd), self.dcalib)
        inp = {"x": [list(self.m["E"][t])]}; out = C.run(self.dprog, inp); self.log.append({"kind": "draft", "code": self.dprog.code, "ext": C.ext_image(self.dprog, inp)}); return out["tok"][0][0][0]
    def generate(self, start, n):
        self.log = []; toks, st, calls = speculate(self.m, self.Wd, start, n, self.k, self.verify, self.draft); return toks, st, list(self.log)
def cycles_of(log): return sum(I.cycle_counts(r["code"])[0] for r in log)
if __name__ == "__main__":
    m = T.make_model("structured"); exp = [0]
    for _ in range(24): exp.append(T.f_next(exp[-1]))
    for noise in (0.0, 4.0):
        Wd = make_draft(m, noise); chip = SpecChip(m, Wd, 24, 3); toks, st, log = chip.generate(0, 24); print(f"noise {noise}: rule ok {toks == exp}; {st}; cycles {cycles_of(log)}; calls {len(log)}")
