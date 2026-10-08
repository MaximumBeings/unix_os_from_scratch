#!/usr/bin/env python3
"""Chapter 9, running example A: a capacity planner for decode serving. Given a model (weight bytes, KV-cache bytes per token), a context length, and a chip's memory capacity and bandwidth, it answers: how many requests fit, how fast can the chip go at each batch size, and which resource is the limit? All figures are DERIVED arithmetic for ASSUMED chips (no real product is meant); change the numbers at the top to plan for your own. Writes out/ch09_example_a.json. Usage: ch09_example_a.py"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
GiB = 2 ** 30
MODEL = {"name": "7B-class, int8 weights, int8 KV cache", "weight_bytes": 7e9, "kv_per_token": 2 * 32 * 32 * 128 * 1}        # 256 KiB per token
CHIPS = [("chip A: 24 GB, 0.5 TB/s", 24e9, 0.5e12), ("chip B: 80 GB, 2 TB/s", 80e9, 2.0e12), ("chip C: 192 GB, 5 TB/s", 192e9, 5.0e12)]
def plan(chip_cap, bw, ctx, model=MODEL):
    kv = model["kv_per_token"] * ctx; free = chip_cap - model["weight_bytes"]; maxb = max(0, int(free // kv))
    def tps(b): return b * bw / (model["weight_bytes"] + b * kv)
    return kv, maxb, tps
res = {"chips": [], "ctx": [], "tps": {}, "maxb": {}}
print(f"model: {MODEL['name']}: weights {MODEL['weight_bytes']/1e9:.1f} GB, KV cache {MODEL['kv_per_token']/1024:.0f} KiB per token\n")
print("tokens per second = B x BW / (weight bytes + B x KV bytes per request)   [memory-bound, one full read of the weights and every cache per step]")
print("batch size is limited by capacity: weights + B x KV <= memory\n")
for ctx in (2048, 8192, 32768):
    kv_ = MODEL["kv_per_token"] * ctx
    print(f"-- context {ctx:,} tokens: KV cache {kv_/GiB:.2f} GiB per request --")
    print(f"{'chip':26s} {'max batch (capacity)':>21s} " + " ".join(f"{'B='+str(b):>7s}" for b in (1, 4, 16, 64)) + f" {'best tokens/s':>14s} {'limit as B grows':>17s}")
    for name, cap, bw in CHIPS:
        kv, maxb, tps = plan(cap, bw, ctx); row = [(f"{tps(b):7.0f}" if b <= maxb else f"{'--':>7s}") for b in (1, 4, 16, 64)]
        best = tps(maxb) if maxb else 0; lim = bw / kv
        print(f"{name:26s} {maxb:21d} " + " ".join(row) + f" {best:14.0f} {lim:17.0f}")
        res["tps"][f"{name}|{ctx}"] = [tps(b) if b <= maxb else None for b in range(1, 129)]; res["maxb"][f"{name}|{ctx}"] = maxb
    print()
res["chips"] = [c[0] for c in CHIPS]
print("reading it:")
print(" * at batch 1 the speed is bandwidth / weight bytes: 0.5e12 / 7e9 = 71 tokens/s on chip A, 286 on chip B, 714 on chip C (before the cache read).")
print(" * chip A holds exactly one 32,768-token request next to the weights (7 GB + 8.6 GB = 15.6 GB; a second request would need 24.2 GB): look at the 'max batch' column.")
print(" * past a point more batch adds almost nothing: the curve flattens at bandwidth / KV bytes per request, the 'limit' column. Long contexts lower that limit and the max batch together.")
print(" * on these assumed chips capacity ends the curve before it has flattened (chip A at 2,048 tokens reaches 656 of its 931 limit): a bigger memory buys more batch, and more batch buys throughput only until the curve flattens.")
json.dump(res, open(os.path.join(hw.ROOT, "out", "ch09_example_a.json"), "w"))
