#!/usr/bin/env python3
"""Chapter 17: the checks of the mixture-of-experts block. Returns a list of problems; empty means all pass.
 (1) the STRUCTURED model: the router sends every token to its group's expert, the float model follows the rule, and forcing ANY other expert breaks the rule for every token (the experts are not interchangeable: routing matters);
 (2) the chip's decode follows the rule through the two-program step; its route equals the float route at every step; compiled programs equal the integer interpreter;
 (3) the chip's h, router logits and final logits are close to float;
 (4) on a RANDOM model (teacher-forced): the chip's route equals the float route except where the float router's top two are nearly tied, and the logits stay close when the routes agree;
 (5) the no-routing dense model (every expert evaluated) decodes the same tokens as the routed model on the structured model;
 (6) the bookkeeping: the expert that ran is the expert the router chose."""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capra as C, tiny_lm as T, moe_lm as M
def rel(a, b): return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / (sum(y * y for y in b) or 1.0))
def check_all():
    P = []; m = M.make_moe("structured"); exp = [0]
    for _ in range(20): exp.append(T.f_next(exp[-1]))
    for t in range(16):                                                                      # (1)
        lg, e, *_ = M.float_step(m, t, [], [])
        if e != M.group(t): P.append(f"(1) token {t}: the router chose expert {e}, not {M.group(t)}")
        if lg.index(max(lg)) != T.f_next(t): P.append(f"(1) token {t}: the routed float step does not follow the rule")
        for bad in range(M.E):
            if bad != M.group(t) and M.float_step(m, t, [], [], route=bad)[0].index(max(M.float_step(m, t, [], [], route=bad)[0])) == T.f_next(t): P.append(f"(1) token {t}: expert {bad} (the wrong one) also gives the right token")
    if M.float_generate(m, 0, 20) != exp: P.append("(1) the float model does not follow the rule")
    chip = M.MoEChip(m, 20); toks, rec = chip.generate(0, 20); kc, vc = [], []
    if toks != exp: P.append("(2) the chip does not follow the rule")
    for r, t_in in zip(rec, toks):                                                           # (2), (3), (6)
        lg, e, k, v, h, rr = M.float_step(m, t_in, kc, vc); kc.append(k); vc.append(v)
        if r["route"] != e: P.append(f"(2) step {r['L']}: chip route {r['route']}, float route {e}")
        if r["progs"][1]["code"] is not chip.progB(r["route"]).code: P.append(f"(6) step {r['L']}: the program that ran is not the program of the expert the router chose")
        if r["progs"][1]["ext"] != C.ext_image(chip.progB(r["route"]), {"h": [r["h"]]}): P.append(f"(6) step {r['L']}: the memory image given to program B does not hold the h that program A produced")
        if r["expert"] != r["route"]: P.append(f"(6) step {r['L']}: expert {r['expert']} ran, the router chose {r['route']}")
        if rel(r["logits"], lg) > 0.15: P.append(f"(3) step {r['L']}: chip logits differ from float by {rel(r['logits'], lg):.2f}")
        if rel(r["router"], rr) > 0.15: P.append(f"(3) step {r['L']}: router logits differ from float by {rel(r['router'], rr):.2f}")
    kc2 = []
    for L in (1, 6, 14):                                                                     # compiled == interpreted
        PA = chip.progA(L); inp = {"x": [m["E"][toks[L - 1]]]}
        if L > 1: kc3, vc3 = [], []; [ (lambda o: (kc3.append(o[2]), vc3.append(o[3])))(M.float_step(m, toks[i], kc3, vc3)) for i in range(L - 1)]; inp["kc"] = kc3; inp["vc"] = vc3
        out = C.run(PA, inp); ref = C.interpret(PA, inp)
        if any(out[nm][0] != ref[e["node"]] for nm, e in PA.ext.items() if e["kind"] == "output"): P.append(f"(2) program A (context {L}) differs from the interpreter")
    for e in range(M.E):
        PB = chip.progB(e); inp = {"h": [[0.5 * math.sin(i + e) for i in range(16)]]}; out = C.run(PB, inp); ref = C.interpret(PB, inp)
        if any(out[nm][0] != ref[ex["node"]] for nm, ex in PB.ext.items() if ex["kind"] == "output"): P.append(f"(2) program B (expert {e}) differs from the interpreter")
    mr = M.make_moe("random", 2); ch = M.MoEChip(mr, 12); ft = M.float_generate(mr, 3, 12); tk, rc = ch.generate(3, 12, force=ft); kc, vc = [], []           # (4)
    for L in range(1, 13):
        lg, e, k, v, h, rr = M.float_step(mr, ft[L - 1], kc, vc); kc.append(k); vc.append(v); s = sorted(rr, reverse=True); tie = (s[0] - s[1]) / (s[0] - s[-1]) < 0.06
        if rc[L - 1]["route"] != e and not tie: P.append(f"(4) step {L}: route {rc[L - 1]['route']} differs from float route {e} with a clear margin")
        if rc[L - 1]["route"] == e and rel(rc[L - 1]["logits"], lg) > 0.25: P.append(f"(4) step {L}: logits differ from float by {rel(rc[L - 1]['logits'], lg):.2f} on the same route")
    dt, _ = M.DenseChip(chip).generate(0, 20)                                                # (5)
    if dt != exp: P.append("(5) the dense (no routing) model does not follow the rule")
    return P
if __name__ == "__main__":
    p = check_all(); print("problems:", p if p else "none")
