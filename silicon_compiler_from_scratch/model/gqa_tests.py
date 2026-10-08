#!/usr/bin/env python3
"""Chapter 14: the software checks of the GQA model and its compiled graph (no RTL; the RTL replay is tools/ch14_run.py). Returns a list of problems; empty means all pass.
 (1) grouped float step == plain MHA step with replicated K/V heads;  (2) compiled == interpreted, and the chip's logits within 15% of float, on random models for G = 4, 2, 1;
 (3) the structured model follows f(t) = 5t+3 mod 16 for G = 4, 2, 1;  (4) the cache the chip returns has the right shape (G groups of 4 words)."""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capra as C, tiny_lm as T, gqa_lm as Q
def replicated(m, mg):
    rep = dict(m); r = Q.H // mg["G"]
    for nm in ("Wk", "Wv"):
        cols = [[mg[nm][i][(h // r) * Q.DH + c] for i in range(Q.D)] for h in range(Q.H) for c in range(Q.DH)]; rep[nm] = [[cols[c][i] for c in range(Q.D)] for i in range(Q.D)]
    return rep
def ref_step(m, tok, K, Vv):
    """An independent MHA/GQA attention step, written head by head with an explicit group list and explicit slices (no division arithmetic for the group): returns the attention output (1 x D) before the residual."""
    G = m["G"]; group_of = {4: [0, 1, 2, 3], 2: [0, 0, 1, 1], 1: [0, 0, 0, 0]}[G]; x = m["E"][tok]; out = [0.0] * Q.D
    q = [sum(x[i] * m["Wq"][i][j] for i in range(Q.D)) for j in range(Q.D)]; k = [sum(x[i] * m["Wk"][i][j] for i in range(Q.D)) for j in range(G * Q.DH)]; v = [sum(x[i] * m["Wv"][i][j] for i in range(Q.D)) for j in range(G * Q.DH)]
    Kall = K + [k]; Vall = Vv + [v]
    for h in range(4):
        g0 = group_of[h] * 4; qh = q[4 * h:4 * h + 4]; s = [sum(qh[i] * kk[g0 + i] for i in range(4)) * 0.5 for kk in Kall]; e = [math.exp(t - max(s)) for t in s]; z = sum(e)
        ah = [sum(e[p] * Vall[p][g0 + i] for p in range(len(Kall))) / z for i in range(4)]
        for j in range(Q.D): out[j] += sum(ah[i] * m["Wo"][4 * h + i][j] for i in range(4))
    return out, k, v
def check_all(steps=8):
    P = []
    for G in (4, 2, 1):                                      # (5) the grouped step against the independent head-by-head step; (6) the pooled weights against literal averages
        m0 = Q.make_mha("random", 5); m = Q.to_groups(m0, G) if G != 4 else m0; kc, vc = [], []; tok = 2
        for t in range(5):
            aux = {}; lg, k, v = Q.float_step(m, tok, kc, vc, aux); a2, k2, v2 = ref_step(m, tok, kc, vc)
            if max(abs(a - b) for a, b in zip(aux["attn"], a2)) > 1e-9: P.append(f"(5) G={G} step {t}: attention output differs from the independent reference")
            kc.append(k); vc.append(v); tok = lg.index(max(lg))
    m0 = Q.make_mha("random", 5); mg = Q.to_groups(m0, 2)
    for i in range(Q.D):
        for c in range(4):
            if abs(mg["Wk"][i][c] - (m0["Wk"][i][c] + m0["Wk"][i][4 + c]) / 2) > 1e-12 or abs(mg["Wv"][i][4 + c] - (m0["Wv"][i][8 + c] + m0["Wv"][i][12 + c]) / 2) > 1e-12: P.append("(6) pooled weights are not the mean of the heads in the group"); break
    for G in (2, 1):
        for seed in (1, 2):
            m = Q.make_mha("random", seed); mg = Q.to_groups(m, G); rep = replicated(m, mg); kc, vc, kg, vg = [], [], [], []; tok = 3
            for t in range(6):
                l1, k1, v1 = Q.float_step(rep, tok, kc, vc); l2, k2, v2 = Q.float_step(mg, tok, kg, vg)
                if max(abs(a - b) for a, b in zip(l1, l2)) > 1e-9: P.append(f"(1) G={G} seed {seed} step {t}: grouped step differs from the replicated MHA step")
                kc.append(k1); vc.append(v1); kg.append(k2); vg.append(v2); tok = l1.index(max(l1))
    for G in (4, 2, 1):
        for seed in (1, 2):
            m0 = Q.make_mha("random", seed); m = Q.to_groups(m0, G) if G != 4 else m0; chip = Q.Chip(m, steps); ft = Q.float_generate(m, 3, steps); toks, rec = chip.generate(3, steps, force=ft); kc, vc = [], []
            for L in range(1, steps + 1):
                lg, k, v = Q.float_step(m, ft[L - 1], kc, vc); kc.append(k); vc.append(v); ch = rec[L - 1]["logits"]
                e = math.sqrt(sum((a - b) ** 2 for a, b in zip(ch, lg)) / sum(b * b for b in lg))
                if e > 0.15: P.append(f"(2) G={G} seed {seed} step {L}: chip logits differ from float by {e:.2f}")
                aux = {}; Q.float_step(m, ft[L - 1], kc[:-1], vc[:-1], aux); rl = lambda a, b: math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / sum(y * y for y in b))
                for nm, ch_, fl_ in (("attention output", rec[L - 1]["attn"], aux["attn"]), ("new key", rec[L - 1]["k"], k), ("new value", rec[L - 1]["v"], v)):
                    if rl(ch_, fl_) > 0.12: P.append(f"(7) G={G} seed {seed} step {L}: chip {nm} differs from float by {rl(ch_, fl_):.2f}")
                Pg = chip.program(L); inp = {"x": [m["E"][ft[L - 1]]]}
                if L > 1:
                    for gi in range(G): inp[f"kc{gi}"] = [r[gi * Q.DH:(gi + 1) * Q.DH] for r in kc[:-1]]; inp[f"vc{gi}"] = [r[gi * Q.DH:(gi + 1) * Q.DH] for r in vc[:-1]]
                out = C.run(Pg, inp); ref = C.interpret(Pg, inp)
                if any(out[nm][0] != ref[ex["node"]] for nm, ex in Pg.ext.items() if ex["kind"] == "output"): P.append(f"(2) G={G} seed {seed} step {L}: compiled program differs from the interpreter")
    exp = [0]
    for _ in range(12): exp.append(T.f_next(exp[-1]))
    for G in (4, 2, 1):
        m0 = Q.make_mha("structured"); m = Q.to_groups(m0, G) if G != 4 else m0
        if Q.float_generate(m, 0, 12) != exp: P.append(f"(3) G={G}: float model does not follow the rule")
        t, rec = Q.Chip(m, 12).generate(0, 12)
        if t != exp: P.append(f"(3) G={G}: chip does not follow the rule")
    if Q.cache_words_per_token(4) != 32 or Q.cache_words_per_token(1) != 8: P.append("(4) cache size formula")
    return P
if __name__ == "__main__":
    p = check_all(); print("problems:", p if p else "none")
