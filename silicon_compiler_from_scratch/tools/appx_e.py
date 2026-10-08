#!/usr/bin/env python3
"""Appendix E (transformers and LLM inference): runnable checks, tied to the book's tiny model. Usage: appx_e.py"""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import tiny_lm as T
print("== 1. the pipeline of one decode step, with the tiny model of Chapter 12 (vocabulary 16, width 16, feed-forward width 32, one layer, one head)")
m = T.make_model("structured"); tok = 5; x = m["E"][tok]; lg, k, v, aux = T.float_step(m, tok, [], [])
print(f"  token {tok} -> embedding row (16 numbers, norm {math.sqrt(sum(a * a for a in x)):.2f}) -> q, k, v (16 numbers each) -> attention over the cache (1 token so far)")
print(f"  -> attention output added to the embedding (residual) -> feed-forward block added again (residual) -> 16 logits -> argmax = token {lg.index(max(lg))} (the designed rule says f(5) = {T.f_next(5)})")
print("\n== 2. count the parameters: a transformer layer has 4 d^2 attention weights and 2 d f feed-forward weights; plus embeddings and the output matrix")
cnt = {nm: sum(len(r) for r in m[nm]) for nm in ("Wq", "Wk", "Wv", "Wo", "W1", "W2", "Wout")}; cnt["E"] = sum(len(r) for r in m["E"]); d, f_, V = 16, 32, 16
formula = 4 * d * d + 2 * d * f_ + V * d + d * V
print(f"  tiny model, counted: {cnt}; total {sum(cnt.values())}; formula 4 d^2 + 2 d f + 2 V d = {formula}: {sum(cnt.values()) == formula}")
print(f"  {'model':26s} {'layers':>6s} {'d':>6s} {'ffn':>7s} {'vocab':>7s} {'parameters':>13s}")
for nm, L, d, f, Vv in (("tiny (this book)", 1, 16, 32, 16), ("a 125M-class model", 12, 768, 3072, 50000), ("a 7B-class model", 32, 4096, 11008, 32000)):
    P = L * (4 * d * d + 3 * d * f if nm.startswith("a 7B") else 4 * d * d + 2 * d * f) + 2 * Vv * d; print(f"  {nm:26s} {L:6d} {d:6d} {f:7d} {Vv:7d} {P:13,d}")
print("  (the 7B-class row uses a gated feed-forward block with three matrices, 3 d f, as modern models do; the other rows use two)")
print("\n== 3. the KV cache: for every token, every layer stores one key and one value vector per KV head")
print("  bytes per token = 2 (K and V) x layers x kv_heads x head_width x bytes_per_number")
for nm, L, kvh, hw, nb in (("tiny model, int8", 1, 1, 16, 1), ("7B-class, 32 heads, fp16", 32, 32, 128, 2), ("7B-class, 32 heads, int8", 32, 32, 128, 1), ("7B-class, 8 KV heads, int8 (GQA)", 32, 8, 128, 1)):
    b = 2 * L * kvh * hw * nb; print(f"  {nm:34s}: {b:9,d} bytes per token = {b / 1024:7.0f} KiB; a 4,096-token context: {b * 4096 / 2**20:9.1f} MiB")
print("\n== 4. prefill against decode: processing the prompt is a matrix-matrix product (many tokens at once); generating is a matrix-vector product (one token at a time)")
d, f, L = 4096, 11008, 32; w = L * (4 * d * d + 3 * d * f)
for name, n in (("prefill, prompt of 512 tokens", 512), ("decode, one token", 1)):
    ops = n * w; by = w; print(f"  {name:32s}: {ops / 1e9:9.1f} G multiply-adds, reads the {w / 1e9:.1f} GB of weights once: {ops / by:7.1f} ops per weight byte")
print("  decode does as much memory work as prefill and 1/512 of the arithmetic; this is why decoding is limited by memory bandwidth (Chapter 9) and why batching, speculation and mixtures of experts exist (Chapters 9, 16, 17)")
print("\n== 5. the decode loop (greedy), as the host runs it in Chapter 12")
toks = T.float_generate(m, 0, 10); print(f"  start token 0; each step feeds the last token, appends its key and value to the cache, takes the argmax of the logits: {toks}")
print("  the loop is inherently sequential: token t+1 needs token t")
print("\n== 6. greedy against sampled decoding: the same logits, two rules")
import random; R = random.Random(1); lg2 = [3.0, 2.0, 0.5, 0.0]; mx = max(lg2); e = [math.exp(x - mx) for x in lg2]; p = [x / sum(e) for x in e]
print(f"  logits {lg2} -> probabilities {[round(x, 3) for x in p]}; greedy always picks index {lg2.index(mx)}; sampling picks index 0 about {100 * p[0]:.0f}% of the time:")
draws = [R.choices(range(4), weights=p)[0] for _ in range(10000)]; print(f"  10,000 samples: {[round(draws.count(i) / 100, 1) for i in range(4)]} % for indices 0..3")
print("  (this book decodes greedily: its tests need a deterministic answer)")
print("\n== 7. layer normalization, which the tiny model leaves out because Capra has no operation for it: x -> (x - mean) / sqrt(variance + eps) * gain")
xs = [2.0, -1.0, 0.5, 4.5]; mu = sum(xs) / 4; var = sum((a - mu) ** 2 for a in xs) / 4; ln = [(a - mu) / math.sqrt(var + 1e-5) for a in xs]; print(f"  {xs} -> {[round(a, 3) for a in ln]} (mean {sum(ln)/4:+.4f}, variance {sum(a * a for a in ln)/4:.4f}); it needs a reciprocal square root, which is a divider with a table (Chapter 6)")
