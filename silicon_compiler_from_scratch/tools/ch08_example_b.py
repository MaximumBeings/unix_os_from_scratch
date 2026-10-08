#!/usr/bin/env python3
"""Chapter 8, running example B: what the KV cache saves over a whole generation. For every context length t = 1..32 the cycle model gives the cost of one decode step with and without the cache (the model's totals match the circuit's counters to the cycle, checked here at the five lengths the circuit was run at in ch08_run.py); summed over the steps this is the cost of generating 32 tokens. Then the memory side: how many bytes the cache holds and re-reads, for the GA-2 head and for realistic models (derived). Writes out/ch08_example_b.json. Usage: ch08_example_b.py"""
import json, os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import ga2_isa as I, attn
ex = attn.make_example(32, 1); P = attn.rq_params(ex)
cyc = {"cached": [], "recompute": []}
for t in range(1, 33):
    for v in cyc: cyc[v].append(I.cycle_counts(attn.program(t, v, P))[0])
run = open(os.path.join(hw.ROOT, "out", "ch08_run_out.txt")).read(); circ = {}
for m in re.finditer(r"^\s*(\d+) (cached|recompute)\s+\d+\s+(\d+)", run, re.M): circ[(int(m.group(1)), m.group(2))] = int(m.group(3))
print("cycles for ONE decode step at context length t (cycle model); the circuit's counter in brackets where it was run")
print("   t   cached  recompute   ratio")
ok = True
for t in (1, 2, 3, 4, 6, 8, 12, 16, 24, 32):
    c, r = cyc["cached"][t - 1], cyc["recompute"][t - 1]; note = ""
    if (t, "cached") in circ: good = circ[(t, "cached")] == c and circ[(t, "recompute")] == r; ok &= good; note = f"   circuit {circ[(t,'cached')]} / {circ[(t,'recompute')]}: {'agrees' if good else 'DIFFERS'}"
    print(f"{t:4d}  {c:7d}  {r:9d}   {r/c:5.2f}{note}")
cc = [sum(cyc["cached"][:t]) for t in range(1, 33)]; cr = [sum(cyc["recompute"][:t]) for t in range(1, 33)]
print(f"\ngenerating 32 tokens one after another: cached {cc[-1]:,} cycles in total, recompute {cr[-1]:,} cycles ({cr[-1]/cc[-1]:.2f}x)")
print(f"per token, the cached step grows from {cyc['cached'][0]} to {cyc['cached'][-1]} cycles (+{(cyc['cached'][-1]-cyc['cached'][0])/31:.0f} per extra token); recompute from {cyc['recompute'][0]} to {cyc['recompute'][-1]} (+{(cyc['recompute'][-1]-cyc['recompute'][0])/31:.0f} per extra token)")
print("\n== the memory side (derived arithmetic)")
print("GA-2 head, int8: 2 x 16 x 1 = 32 bytes of cache per token. A decode step at context t RE-READS the whole cache: LD of (t-1) rows of K and of V =", "2 x 16 x (t-1) words")
rows = []
for name, L_, H, dh, b in (("GA-2 head", 1, 1, 16, 1), ("a 1B-class model: 16 layers x 16 heads x 64, fp16", 16, 16, 64, 2), ("a 7B-class model: 32 x 32 x 128, fp16", 32, 32, 128, 2), ("a 70B-class model with 8 KV heads: 80 x 8 x 128, fp16", 80, 8, 128, 2)):
    per = 2 * L_ * H * dh * b; rows.append((name, per)); print(f"  {name:55s} {per:>10,} bytes/token   {per*2048/2**20:9.1f} MiB at 2,048 tokens   {per*32768/2**30:7.2f} GiB at 32,768")
print("(the last line anticipates Chapter 14: sharing keys and values between query heads shrinks the cache by the ratio of query heads to KV heads)")
json.dump({"cached": cyc["cached"], "recompute": cyc["recompute"], "rows": rows}, open(os.path.join(hw.ROOT, "out", "ch08_example_b.json"), "w")); sys.exit(0 if ok else 1)
