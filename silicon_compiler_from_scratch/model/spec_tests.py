#!/usr/bin/env python3
"""Chapter 16: the checks of speculative decoding. Returns a list of problems; empty means all pass.
 (1) the block form of the float verifier (explicit causal mask) equals the one-token-at-a-time float step, for random models, cache sizes and block sizes;
 (2) EXACTNESS: with the float verifier, speculative decoding produces exactly the target's greedy tokens whatever the draft (perfect, noisy, adversarial, random) and whatever k, and the bookkeeping is consistent;
 (3) CAUSALITY on the chip: changing a later token of a block leaves the integer outputs of every earlier row bit-identical; changing an earlier token does change later rows;
 (4) the compiled verifier equals the integer interpreter, and its logits and new rows are close to float;
 (5) the structured model follows its rule through the chip's speculative loop, for several k and draft qualities; the draft's chip program picks the same token as the float draft."""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capra as C, tiny_lm as T, spec_lm as S
def rel(a, b): return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / (sum(y * y for y in b) or 1.0))
def check_all():
    P = []
    for seed in (1, 2, 3):                                                                      # (1)
        m = T.make_model("random", seed); R = random.Random(seed); toks = [R.randrange(16) for _ in range(9)]; kc, vc = [], []
        for t in toks[:4]: _, k, v, _ = T.float_step(m, t, kc, vc); kc.append(k); vc.append(v)
        for mm in (1, 2, 4, 5):
            lg, ks, vs = S.float_block(m, toks[4:4 + mm], kc, vc); kc2, vc2 = list(kc), list(vc)
            for i, t in enumerate(toks[4:4 + mm]):
                l1, k1, v1, _ = T.float_step(m, t, kc2, vc2); kc2.append(k1); vc2.append(v1)
                if max(abs(a - b) for a, b in zip(lg[i], l1)) > 1e-9 or max(abs(a - b) for a, b in zip(ks[i], k1)) > 1e-9: P.append(f"(1) seed {seed} block {mm} row {i}: block form differs from the sequential step")
    for seed in (1, 2, 3):                                                                      # (2)
        m = T.make_model("random" if seed != 3 else "structured", seed); ref = T.float_generate(m, 5, 20); R = random.Random(seed); drafts = {"random": lambda t: R.randrange(16), "constant": lambda t: 7}
        for noise in (0.0, 3.0, 100.0):
            Wd = S.make_draft(m, noise, seed); drafts[f"bigram noise {noise}"] = lambda t, Wd=Wd, m=m: S.draft_next(m, Wd, t)
        for k in (1, 2, 3, 5):
            for name, dr in drafts.items():
                toks, st, calls = S.speculate(m, None, 5, 20, k, S.float_verify(m), dr)
                if toks != ref: P.append(f"(2) seed {seed} k={k} draft {name}: speculative tokens differ from the target's greedy tokens")
                if st["accepted"] > st["proposed"] or st["blocks"] * (k + 1) < 20 or st["blocks"] > 20 or st["proposed"] != st["blocks"] * k or st["emitted"] != st["accepted"] + st["blocks"] or st["cache"] != st["emitted"]: P.append(f"(2) seed {seed} k={k} draft {name}: inconsistent counts {st}")
    m = T.make_model("random", 2); m["Wq"] = [[4 * x for x in r] for r in m["Wq"]]; m["Wk"] = [[4 * x for x in r] for r in m["Wk"]]          # (3), (4): a model with SHARP attention, so that a wrong mask or query changes the output visibly
    c = 4; R = random.Random(9); samples = []; rk = rv = 0.0
    def prefix(toks):
        kc, vc = [], []
        for t in toks: _, k, v, _ = T.float_step(m, t, kc, vc); kc.append(k); vc.append(v)
        return kc, vc
    for _ in range(40):
        pre = [R.randrange(16) for _ in range(c)]; blk = [R.randrange(16) for _ in range(4)]; kc, vc = prefix(pre); lg, ks, vs = S.float_block(m, blk, kc, vc)
        samples.append({"x": [list(m["E"][t]) for t in blk], "kc": kc, "vc": vc}); rk = max(rk, max(abs(t) for r in kc + ks for t in r)); rv = max(rv, max(abs(t) for r in vc + vs for t in r))
    rngs = {"x": max(max(abs(t) for t in r) for r in m["E"]), "k": rk, "v": rv}; Pg = C.compile_graph(S.build_verify(4, c, m), samples, ranges=rngs); pre = [3, 8, 1, 12]; kc, vc = prefix(pre)
    def run(block):
        inp = {"x": [list(m["E"][t]) for t in block], "kc": [list(r) for r in kc], "vc": [list(r) for r in vc]}; out = C.run(Pg, inp); ref = C.interpret(Pg, inp)
        if any(out[nm][0] != ref[e["node"]] for nm, e in Pg.ext.items() if e["kind"] == "output"): P.append("(4) compiled verifier differs from the interpreter")
        return out
    a = run([1, 2, 3, 4]); b = run([1, 2, 9, 4]); d = run([1, 2, 3, 12]); e2 = run([5, 2, 3, 4])
    if a["logits"][0][:2] != b["logits"][0][:2]: P.append("(3) changing row 2 changed the output of rows 0 or 1: the block is not causal")
    if a["logits"][0][:3] != d["logits"][0][:3]: P.append("(3) changing row 3 changed the output of rows 0-2: the block is not causal")
    if a["logits"][0][2:] == b["logits"][0][2:]: P.append("(3) changing row 2 did not change rows 2 or 3: rows do not see the block")
    if a["logits"][0][1:] == e2["logits"][0][1:]: P.append("(3) changing row 0 did not change later rows: rows do not see earlier block rows")
    aux = {}; lg, ks, vs = S.float_block(m, [1, 2, 3, 4], kc, vc, aux)
    for i in range(4):
        if rel(a["logits"][1][i], lg[i]) > 0.15: P.append(f"(4) block row {i}: chip logits differ from float by {rel(a['logits'][1][i], lg[i]):.2f}")
        if rel(a["attn"][1][i], aux["attn"][i]) > 0.12: P.append(f"(4) block row {i}: attention output differs from float by {rel(a['attn'][1][i], aux['attn'][i]):.2f}")
        if rel(a["k_new"][1][i], ks[i]) > 0.12 or rel(a["v_new"][1][i], vs[i]) > 0.12: P.append(f"(4) block row {i}: new key or value differs from float")
    m = T.make_model("structured")
    exp = [0]
    for _ in range(16): exp.append(T.f_next(exp[-1]))
    for k in (1, 2, 4):
        for noise in (0.0, 3.0, 12.0):
            Wd = S.make_draft(m, noise, 1); chip = S.SpecChip(m, Wd, 16, k, starts=range(16)); toks, st, log = chip.generate(0, 16)
            if toks != exp: P.append(f"(5) k={k} noise {noise}: tokens differ from the rule")
            if noise == 0.0 and st["accepted"] != st["proposed"]: P.append(f"(5) k={k}: a perfect draft must be accepted completely, got {st}")
            if k == 1 and noise == 0.0:
                for t in range(4):
                    if chip.draft(t) != S.draft_next(m, Wd, t): P.append(f"(5) the draft's chip program picks a different token from the float draft after {t}")
    return P
if __name__ == "__main__":
    p = check_all(); print("problems:", p if p else "none")
