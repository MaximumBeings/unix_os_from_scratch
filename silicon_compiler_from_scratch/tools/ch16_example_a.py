#!/usr/bin/env python3
"""Chapter 16, example A: three speculative blocks traced step by step; the anatomy of the verifier program; and the causality of its rows shown in integers. Usage: ch16_example_a.py. Writes out/ch16_example_a.json"""
import json, os, random, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import capra as C, ga2_isa as I, tiny_lm as T, spec_lm as S
m = T.make_model("random", 1); N = 24; k = 3; Wd = S.make_draft(m, 0.0, 1); sc = S.SpecChip(m, Wd, N, k); ref = T.float_generate(m, 0, N)
print("== 1. three blocks of speculative decoding (random target, bigram draft, k = 3); the target's own greedy tokens are the reference")
print(f"   target greedy tokens (float): {ref[:14]} ...")
seq = [0]; kc, vc = [], []; trace = []
for b in range(3):
    t = seq[-1]; d = []
    for _ in range(k): t = S.draft_next(m, Wd, t); d.append(t)
    block = [seq[-1]] + d; choice, ks, vs = sc.verify(len(kc), block, kc, vc); a = 0
    while a < k and d[a] == choice[a]: a += 1
    new = d[:a] + [choice[a]]; trace.append({"block": block, "choice": choice, "accepted": a, "new": new})
    print(f"   block {b + 1}: last token {block[0]}, draft guesses {d}; target's choice after each of the {len(block)} rows {choice}")
    print(f"            guess 1 {'agrees' if d[0] == choice[0] else 'DISAGREES'}" + "".join(f", guess {j + 1} {'agrees' if d[j] == choice[j] else 'DISAGREES'}" for j in range(1, a + 1) if j < k) + f"  ->  {a} accepted, and the target's own token {choice[a]} comes free: tokens {new}; cache grows by {a + 1} rows")
    seq += new; kc += ks[:a + 1]; vc += vs[:a + 1]
print(f"   tokens so far {seq}  == target greedy {ref[:len(seq)]}: {seq == ref[:len(seq)]}")
print("\n== 2. the anatomy of one verifier program (cache of 4 rows, block of 4 rows) against four ordinary single-token steps over the same positions")
c = 4; Pv = sc._vprog(c, 4) if (c, 4) in sc.calib else None
if Pv is None:
    sc2 = S.SpecChip(m, Wd, 12, 3, starts=range(16)); Pv = sc2._vprog(c, 4)
def hist(code):
    h = {}
    for ins in code: h[ins["op"]] = h.get(ins["op"], 0) + 1
    return h
def ldw(code): return sum(ins["c"] for ins in code if ins["op"] == "LD")
ch = T.Chip(m, 12); single = [ch.program(L) for L in range(c + 1, c + 5)]
cv = I.cycle_counts(Pv.code); cs = [I.cycle_counts(P.code)[0] for P in single]
print(f"   verifier (m = 4, c = 4):  {hist(Pv.code)}")
print(f"      words loaded from external memory: {ldw(Pv.code)};  cycles: {cv[0]}  (matrix {cv[1]}, DMA {cv[2]}, vector {cv[3]})")
print(f"   four single-token steps (contexts 5, 6, 7, 8): cycles {cs} = {sum(cs)}; words loaded: {[ldw(P.code) for P in single]} = {sum(ldw(P.code) for P in single)}")
print(f"   so the block costs {cv[0]/sum(cs):.2f} of the four steps: the weights ({2304} words) are loaded once, not four times.")
print("\n== 3. causality in integers: change the LAST token of the block and the integer logits of all earlier rows are identical; change an earlier token and later rows change")
sm = T.make_model("random", 2); sm["Wq"] = [[4 * x for x in r] for r in sm["Wq"]]; sm["Wk"] = [[4 * x for x in r] for r in sm["Wk"]]; R = random.Random(9); smp = []; rk = rv = 0.0
def pre(toks):
    kc, vc = [], []
    for t in toks: _, kk, vv, _ = T.float_step(sm, t, kc, vc); kc.append(kk); vc.append(vv)
    return kc, vc
for _ in range(40):
    p = [R.randrange(16) for _ in range(4)]; blk = [R.randrange(16) for _ in range(4)]; kk, vv = pre(p); lg, ks_, vs_ = S.float_block(sm, blk, kk, vv); smp.append({"x": [list(sm["E"][t]) for t in blk], "kc": kk, "vc": vv}); rk = max(rk, max(abs(t) for r in kk + ks_ for t in r)); rv = max(rv, max(abs(t) for r in vv + vs_ for t in r))
Pg = C.compile_graph(S.build_verify(4, 4, sm), smp, ranges={"x": max(max(abs(t) for t in r) for r in sm["E"]), "k": rk, "v": rv}); kk, vv = pre([3, 8, 1, 12])
def run(blk): o = C.run(Pg, {"x": [list(sm["E"][t]) for t in blk], "kc": kk, "vc": vv}); return [r[:5] for r in o["logits"][0]]
for blk in ([1, 2, 3, 4], [1, 2, 3, 12], [1, 2, 9, 4], [5, 2, 3, 4]):
    print(f"   block {blk}: first five integer logits of each row:"); [print(f"       row {i}: {r}") for i, r in enumerate(run(blk))]
print("   rows 0-2 of [1,2,3,4] and [1,2,3,12] are identical (the last token is not visible to them); [1,2,9,4] differs from row 2 on; [5,2,3,4] differs from row 0 on.")
json.dump({"trace": trace, "verify": cv, "single": cs}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch16_example_a.json"), "w"))
