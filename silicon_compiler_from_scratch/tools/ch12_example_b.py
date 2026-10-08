#!/usr/bin/env python3
"""Chapter 12, running example B: retarget the model, and find where the designed answer stops being designed. (1) Build the same constructed model with a DIFFERENT next-token rule, f(t) = (3t + 7) mod 16, compile it with Capra unchanged and decode on the chip: the chip follows the new rule. (2) Increase the weights of attention and the feed-forward block (the `noise` knob) until they stop being small, and measure, for each level, how often floating point still follows the rule and how often the int8 chip still agrees with floating point (teacher-forced: both see the same tokens). Writes out/ch12_example_b.json. Usage: ch12_example_b.py"""
import json, math, os, random, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import tiny_lm as T
def make_custom(f, noise, seed=1):
    """Like T.make_model('structured') but with an arbitrary permutation f and an adjustable size (`noise`) of the attention and feed-forward weights."""
    R = random.Random(seed); g = lambda r, c, sd: [[R.gauss(0, sd) for _ in range(c)] for _ in range(r)]
    E = [[x / 4 for x in row] for row in T.hadamard(16)]; finv = {f(t): t for t in range(T.V)}; Wout = [[8.0 * E[finv[j]][d] for j in range(T.V)] for d in range(T.D)]; s = noise
    return {"kind": "custom", "E": E, "Wq": g(T.D, T.D, s / math.sqrt(T.D)), "Wk": g(T.D, T.D, s / math.sqrt(T.D)), "Wv": g(T.D, T.D, s / math.sqrt(T.D)), "Wo": g(T.D, T.D, s / math.sqrt(T.D)), "W1": g(T.D, T.HID, s / math.sqrt(T.D)), "W2": g(T.HID, T.D, s / math.sqrt(T.HID)), "Wout": Wout}
f2 = lambda t: (3 * t + 7) % 16
assert sorted(f2(t) for t in range(16)) == list(range(16))
print("== 1. a different rule: f(t) = (3t + 7) mod 16 (a permutation: 3 is odd, so it is invertible mod 16). Same compiler, same chip, new weights.")
m = make_custom(f2, 0.45); chip = T.Chip(m, 16); toks, _ = chip.generate(0, 16); exp = [0]
for _ in range(16): exp.append(f2(exp[-1]))
print("closed form  :", exp); print("floating     :", T.float_generate(m, 0, 16)); print("chip (ref)   :", toks, "  all equal:", exp == T.float_generate(m, 0, 16) == toks)
print("\n== 2. how big can the attention and feed-forward weights get before they decide the token? (16 start tokens x 12 steps, teacher-forced on floating point's own tokens; 192 decisions per row)")
print(" noise  float follows the rule  chip agrees with float  mean logit error  worst logit error")
res = {"noise": [], "float_rule": [], "chip_float": [], "err": []}
for noise in (0.45, 1.0, 2.0, 4.0):
    m = make_custom(lambda t: (5 * t + 3) % 16, noise); ch = T.Chip(m, 12); fr = ca = tot = 0; errs = []
    for s in range(T.V):
        ft = T.float_generate(m, s, 12); fr += sum(ft[i + 1] == (5 * ft[i] + 3) % 16 for i in range(12)); ctoks, crec = ch.generate(s, 12, force=ft); kc, vc = [], []
        for L in range(1, 13):
            lg, k, v, _ = T.float_step(m, ft[L - 1], kc, vc); kc.append(k); vc.append(v); ca += crec[L - 1]["tok"] == lg.index(max(lg)); tot += 1
            errs.append(math.sqrt(sum((a - b) ** 2 for a, b in zip(crec[L - 1]["logits"], lg))) / math.sqrt(sum(b * b for b in lg)))
    res["noise"].append(noise); res["float_rule"].append(fr / tot); res["chip_float"].append(ca / tot); res["err"].append(sum(errs) / len(errs))
    print(f"{noise:6.2f}  {100*fr/tot:15.1f}%  {100*ca/tot:21.1f}%  {100*sum(errs)/len(errs):14.1f}%  {100*max(errs):14.1f}%")
print("\nreading it: at noise 0.45 (the book's model) the embeddings decide everything: floating point follows the rule on every step and the chip agrees. As the random attention/feed-forward weights grow,")
print("they start to decide the token, floating point no longer follows the designed rule (the model is no longer 'designed'), and the chip's int8 arithmetic agrees with floating point less often because the logit margins shrink.")
json.dump(res, open(os.path.join(hw.ROOT, "out", "ch12_example_b.json"), "w"))
