#!/usr/bin/env python3
"""Chapter 17, example B: (1) does quantization flip routing decisions? (2) load balance and how many experts a batch touches; (3) grouped execution of a batch of tokens by expert on the chip; (4) derived serving arithmetic. Usage: ch17_example_b.py. Writes out/ch17_example_b.json"""
import json, math, os, random, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import capra as C, ga2_isa as I, tiny_lm as T, moe_lm as M
res = {}
print("== 1. does int8 quantization flip the router's decision?  Random MoE models, teacher-forced on the float tokens (3 models x 16 starts x 12 steps = 576 decisions)")
ag_route = tot = flips_clear = ag_tok_same = ag_tok_flip = n_same = n_flip = 0; margins = []; counts = [0] * M.E; flip_margin = []
for seed in (1, 2, 3):
    mr = M.make_moe("random", seed); ch = M.MoEChip(mr, 12)
    for s in range(16):
        ft = M.float_generate(mr, s, 12); tk, rc = ch.generate(s, 12, force=ft); kc, vc = [], []
        for L in range(1, 13):
            lg, e, k, v, h, r = M.float_step(mr, ft[L - 1], kc, vc); kc.append(k); vc.append(v); srt = sorted(r, reverse=True); mg = (srt[0] - srt[1]) / (srt[0] - srt[-1]); counts[e] += 1
            same = rc[L - 1]["route"] == e; tot += 1; ag_route += same; margins.append(mg); tokok = rc[L - 1]["tok"] == lg.index(max(lg))
            if same: n_same += 1; ag_tok_same += tokok
            else: n_flip += 1; ag_tok_flip += tokok; flip_margin.append(mg)
print(f"   chip route = float route on {ag_route}/{tot} decisions ({100*ag_route/tot:.1f}%); {n_flip} flips, all at a float margin (top-1 minus top-2 over the logit range) of at most {max(flip_margin) if flip_margin else 0:.3f}")
print(f"   token agreement with float: when the route agrees {ag_tok_same}/{n_same} ({100*ag_tok_same/max(1,n_same):.1f}%); when the route flipped {ag_tok_flip}/{n_flip} ({100*ag_tok_flip/max(1,n_flip):.1f}%)")
print(f"   float margins: median {sorted(margins)[len(margins)//2]:.2f}; {100*sum(1 for x in margins if x < 0.05)/len(margins):.1f}% of decisions have a margin under 0.05 and {100*sum(1 for x in margins if x < 0.15)/len(margins):.1f}% under 0.15: those are the decisions int8 noise can flip")
res["route"] = [ag_route, tot, n_flip, ag_tok_same, n_same, ag_tok_flip, n_flip]
print("\n== 2. load balance and the experts a batch touches")
print(f"   how often the float router chose each expert on the random models: {counts} of {sum(counts)} ({[round(100*c/sum(counts)) for c in counts]}%): not uniform. The structured model is perfectly balanced by construction: 6 tokens per expert per 24 steps.")
R = random.Random(5); freq = [c / sum(counts) for c in counts]
def touched(E, B, p, trials=4000):
    tt = 0
    for _ in range(trials): tt += len({R.choices(range(E), weights=p)[0] for _ in range(B)})
    return tt / trials
print(f"   expected number of DISTINCT experts one decode step of B concurrent requests touches (each reads its token's expert once per step), E = 4: uniform router, and the measured skew above")
print(f"{'B':>4s} {'uniform (formula)':>18s} {'uniform (simulated)':>20s} {'skewed (simulated)':>19s} {'of 4':>6s}"); res["touch"] = {}
for B in (1, 2, 4, 8, 16, 32):
    f_ = 4 * (1 - (1 - 1 / 4) ** B); u = touched(4, B, [1] * 4); s_ = touched(4, B, freq); res["touch"][B] = [f_, u, s_]
    print(f"{B:4d} {f_:18.2f} {u:20.2f} {s_:19.2f} {100*f_/4:5.0f}%")
print("   at B = 1 only a quarter of the expert weights are read; at B = 16 almost all of them are: the saving of an MoE shrinks with the batch, and a skewed router reads fewer experts but makes the busiest one the bottleneck")
print("\n== 3. grouped execution on the chip: 8 tokens' h vectors (taken from 8 consecutive steps of the structured decode), run expert by expert with the tokens routed to it as the rows of one program")
m = M.make_moe("structured"); chip = M.MoEChip(m, 24); toks, rec = chip.generate(0, 24); sel = rec[8:16]; groups = {}
for r in sel: groups.setdefault(r["route"], []).append(r)
one = sum(I.cycle_counts(chip.progB(r["route"]).code)[0] for r in sel); grouped = 0; ok = True; plan = []
for e, rs in sorted(groups.items()):
    mm = len(rs); P = C.compile_graph(M.build_B(m, e, mm), [{"h": [r["h"] for r in rs]}], ranges=chip.ranges); out = C.run(P, {"h": [r["h"] for r in rs]}); got = [x[0] for x in out["tok"][0]]
    ok &= got == [r["tok"] for r in rs]; c = I.cycle_counts(P.code)[0]; grouped += c; plan.append((e, mm, c))
print(f"   8 tokens routed as {[(e, len(rs)) for e, rs in sorted(groups.items())]} (expert, tokens); one B program per expert with that many rows")
print(f"   cycles: 8 separate B programs {one:,}; {len(groups)} grouped programs {grouped:,} ({grouped/one:.2f} of the separate cost); the chosen tokens are identical to the separate runs: {ok}")
print("   grouping is what makes a batch of tokens cheap: each expert's weights are loaded once for all the tokens that use it. It needs the host to reorder the batch, and an expert that gets one token still pays the full weight load")
res["group"] = [one, grouped]
print("\n== 4. serving arithmetic (DERIVED, ASSUMED): a model whose feed-forward weights are 8 GB per layer-stack as a dense model, or split into E experts with top-1 routing; everything else (attention, embeddings) 4 GB, int8; bandwidth-bound; tokens/s at 1 TB/s")
print(f"{'E':>4s} {'total weights':>14s} {'active at B=1':>14s} {'tokens/s, B=1':>14s} {'B=8: experts touched':>21s} {'tokens/s per request, B=8':>26s}"); res["serve"] = {}
for E_ in (1, 4, 16, 64):
    total = 4e9 + 8e9 * E_; act = 4e9 + 8e9; t1 = 1e12 / act; tou = E_ * (1 - (1 - 1 / E_) ** 8); bytes8 = 4e9 + 8e9 * tou; t8 = 1e12 / bytes8
    res["serve"][E_] = [total, act, t1, tou, t8]; print(f"{E_:4d} {total/1e9:12.0f} GB {act/1e9:12.0f} GB {t1:14.0f} {tou:21.2f} {t8:26.0f}")
print("   (an expert here is a full 8 GB feed-forward stack, so E experts hold E times the knowledge capacity.) At B = 1 the speed is that of the dense 12 GB model whatever E is; at B = 8 the larger E touches more distinct experts and the speed per request falls toward that of a model with all the experts loaded. The capacity to store all experts is the real cost of an MoE: the chip must hold 4 + 8E GB")
json.dump({str(k): v for k, v in res.items()}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch17_example_b.json"), "w"))
