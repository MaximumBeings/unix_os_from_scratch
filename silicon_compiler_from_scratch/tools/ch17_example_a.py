#!/usr/bin/env python3
"""Chapter 17, example A: one token through the router and an expert, by hand; what a wrong dispatch does; the two programs of a step. Usage: ch17_example_a.py. Writes out/ch17_example_a.json"""
import json, os, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import capra as C, ga2_isa as I, tiny_lm as T, moe_lm as M
m = M.make_moe("structured"); N = 24; chip = M.MoEChip(m, N); res = {}
print("== 1. the router at context 1: which expert does each token go to? (float router logits; the winner is the largest)")
print("   token -> router logits for experts 0..3                          winner  group(t)=t mod 4")
rows = []
for t in range(16):
    lg, e, k, v, h, r = M.float_step(m, t, [], []); rows.append([t, r, e]); print(f"   {t:5d}    " + " ".join(f"{x:+6.2f}" for x in r) + f"        {e}       {M.group(t)}")
print("   the router is a 16 x 4 matrix: column e is twice the sum of the embeddings of group e's four tokens, so h ~ embedding[t] gives about 2.0 to its own group's expert and about 0 to the others")
print("\n== 2. token 5 (group 1), step by step in floating point")
t = 5; lg, e, k, v, h, r = M.float_step(m, t, [], []); W1, W2 = m["experts"][e]; hid = [max(0.0, x) for x in T.mv(h, W1)]
print(f"   router logits {[round(x, 2) for x in r]} -> expert {e}")
print(f"   expert {e}'s hidden layer (32 units; only the 4 units that know this group's tokens can be large): non-zero units {[(j, round(x, 2)) for j, x in enumerate(hid) if x > 0.05]}")
print(f"   the unit for token 5 fires with {max(hid):.2f}; it adds (embedding[f(5)] - embedding[5]) scaled back, so y ~ embedding[{T.f_next(5)}]")
print(f"   logits {[round(x, 1) for x in lg]}  -> token {lg.index(max(lg))}   (the rule says f(5) = {T.f_next(5)})")
print("\n== 3. what dispatching to the WRONG expert does (float, token 5 and token 6, forced route)")
for t in (5, 6):
    out = []
    for bad in range(4): lgb = M.float_step(m, t, [], [], route=bad)[0]; out.append(f"expert {bad}: token {lgb.index(max(lgb))}")
    print(f"   token {t} (right expert {M.group(t)}, rule says {T.f_next(t)}): " + ";  ".join(out)); res[f"wrong{t}"] = out
print("   only the right expert gives the rule's token; a wrong expert leaves h unchanged, so the model 'predicts' the token it was just given")
print("\n== 4. the same on the chip: a host that dispatches to the next expert instead of the router's choice")
class Bad(M.MoEChip):
    def dispatch(self, route): return (route + 1) % M.E
bad = Bad(m, N); bt, _ = bad.generate(0, N); good = chip.generate(0, N)[0]
print(f"   correct dispatch:   {good}\n   off-by-one dispatch: {bt}")
print("\n== 5. the two programs of one step (context 8, token routed to expert 1)")
PA = chip.progA(8); PB = chip.progB(1)
def hist(code):
    h = {}
    for ins in code: h[ins["op"]] = h.get(ins["op"], 0) + 1
    return h
def ldw(code): return sum(ins["c"] for ins in code if ins["op"] == "LD")
for nm, P in (("A (attention, router)", PA), ("B_1 (expert 1, output)", PB)):
    cc = I.cycle_counts(P.code); print(f"   {nm:24s} {hist(P.code)}\n   {'':24s} words loaded {ldw(P.code)}; cycles {cc[0]} (matrix {cc[1]}, DMA {cc[2]}, vector {cc[3]})")
print(f"   the four B programs cost {[I.cycle_counts(chip.progB(e).code)[0] for e in range(4)]} cycles: the same, because they have the same shape; which one runs is the host's choice")
print(f"   weights per step: A loads Wq Wk Wv Wo (1024) and Wr (64); B loads one expert (1024) and Wout (256); the other three experts (3072 words) are not touched")
res["router"] = rows
json.dump(res, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch17_example_a.json"), "w"))
