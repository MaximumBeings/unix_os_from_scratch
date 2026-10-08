#!/usr/bin/env python3
"""Chapter 12, running example A: watch the model think. Eight decode steps of the tiny transformer on the chip (reference simulator running the compiler's programs), one at a time: which token goes in, which token comes out, the 16 logits the chip computed next to the floating-point logits, and the margin between the winner and the runner-up. Writes out/ch12_example_a.json. Usage: ch12_example_a.py"""
import json, math, os, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import tiny_lm as T, ga2_isa as I
N = 8; m = T.make_model("structured"); chip = T.Chip(m, N); toks, rec = chip.generate(0, N)
print(f"model: {T.V} tokens, width {T.D}, feed-forward {T.HID}, one layer, one head. The designed rule is f(t) = (5t + 3) mod 16.")
print("the weights are constructed so that rule holds; attention and the feed-forward block run for real but are small (Chapter 12 text).\n")
kc, vc, tok = [], [], 0; out = {"steps": []}
print("step  context  in -> out   float winner   gap (best - 2nd, as % of logit range)   logits (chip, as int8 x scale): top three")
for L in range(1, N + 1):
    lg, k, v, aux = T.float_step(m, tok, kc, vc); kc.append(k); vc.append(v); chip_lg = rec[L - 1]["logits"]
    fw = lg.index(max(lg)); cw = chip_lg.index(max(chip_lg)); s = sorted(lg, reverse=True); gap = (s[0] - s[1]) / (s[0] - s[-1])
    top = sorted(range(T.V), key=lambda j: -chip_lg[j])[:3]
    print(f"{L:4d}  {L:7d}  {tok:2d} -> {cw:2d}      {fw:2d}             {100*gap:5.1f}%                              " + ", ".join(f"tok {j}: {chip_lg[j]:+.2f}" for j in top))
    out["steps"].append({"L": L, "in": tok, "chip": chip_lg, "float": lg, "winner": cw}); tok = fw
print("\none step in detail (step 3): the 16 logits, chip against floating point (a logit is the model's score for each possible next token)")
st = out["steps"][2]
for j in range(T.V): print(f"  token {j:2d}  float {st['float'][j]:+7.3f}   chip {st['chip'][j]:+7.3f}   diff {st['chip'][j]-st['float'][j]:+6.3f}" + ("   <- winner" if j == st["winner"] else ""))
print(f"\nthe winner ({st['winner']}) is the token the rule says follows {st['in']}: f({st['in']}) = {(5*st['in']+3) % 16}.")
print(f"the largest chip-vs-float difference on any logit at this step is {max(abs(a-b) for a, b in zip(st['chip'], st['float'])):.3f}, against a winning margin of {sorted(st['float'], reverse=True)[0]-sorted(st['float'], reverse=True)[1]:.2f}: the margin is what makes int8 safe here.")
print("\nwhat the host did at each step (the chip has no instruction for these): picked the embedding row of the input token, kept the KV cache in its own memory between steps, fed the chosen token back.")
json.dump(out, open(os.path.join(hw.ROOT, "out", "ch12_example_a.json"), "w"))
