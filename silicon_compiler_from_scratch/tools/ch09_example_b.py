#!/usr/bin/env python3
"""Chapter 9, running example B: static against continuous batching. Eight requests with different output lengths are served on GA-2 with a batch capacity of four. STATIC batching takes four requests and runs until the longest finishes; CONTINUOUS batching refills a slot the moment its request ends. The cost of every decode step is the cycle count of the REAL batched program (model/batch.py) for the sequences present, from the cycle model (equal to the circuit's counters, Chapter 9's run). Writes out/ch09_example_b.json. Usage: ch09_example_b.py"""
import json, os, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import ga2_isa as I, batch as Bt
P = Bt.rq_params(Bt.make_batch(4, 16, 1)); LENS = [14, 3, 9, 5, 12, 2, 7, 10]; CAP = 4; cache = {}
def step_cost(ts):
    key = tuple(ts)
    if key not in cache: cache[key] = I.cycle_counts(Bt.program(list(range(len(ts))), list(ts), P))[0]
    return cache[key]
def static():
    t = 0; done = {}; log = []
    for g in range(0, len(LENS), CAP):
        grp = list(range(g, min(g + CAP, len(LENS)))); steps = max(LENS[i] for i in grp)
        for s in range(1, steps + 1):
            ts = [s for _ in grp]                                       # finished requests keep riding (padding): the batch is rigid until the longest ends
            c = step_cost(ts); log.append((t, c, len(grp), [i for i in grp if s <= LENS[i]])); t += c
            for i in grp:
                if s == LENS[i]: done[i] = t
    return t, done, log
def continuous():
    t = 0; waiting = list(range(len(LENS))); slots = []; prog = {}; done = {}; log = []
    while waiting or slots:
        while waiting and len(slots) < CAP: i = waiting.pop(0); slots.append(i); prog[i] = 0
        for i in slots: prog[i] += 1
        c = step_cost([prog[i] for i in slots]); log.append((t, c, len(slots), list(slots))); t += c
        for i in list(slots):
            if prog[i] == LENS[i]: done[i] = t; slots.remove(i)
    return t, done, log
ts_, ds, ls = static(); tc, dc, lc = continuous(); tokens = sum(LENS)
print(f"requests (output tokens each): {LENS}   batch capacity {CAP}   total tokens {tokens}\n")
print("cost of one decode step on GA-2 (cycles), from the batched program, for a few batch shapes (every sequence at the same token t):")
for n in (1, 2, 3, 4): print(f"  {n} sequence(s) at token 8: {step_cost([8]*n):6d} cycles ({step_cost([8]*n)/n:6.0f} per token)")
print()
for name, tot, d in (("static", ts_, ds), ("continuous", tc, dc)):
    lat = [d[i] for i in range(len(LENS))]
    print(f"{name:11s} total {tot:7d} cycles for {tokens} tokens = {tot/tokens:7.0f} cycles per token;  request completion time: mean {sum(lat)/len(lat):8.0f}, worst {max(lat):7d}")
print(f"\ncontinuous batching finishes the same work {ts_/tc:.2f}x faster and the mean request completes {sum(ds.values())/sum(dc.values()):.2f}x sooner.")
print("steps with how many sequences really generating a token (static, including the idle padded ones in brackets):")
print("  static    :", [f"{len(x[3])}/{x[2]}" for x in ls])
print("  continuous:", [f"{len(x[3])}" for x in lc])
useful_s = sum(len(x[3]) for x in ls); slots_s = sum(x[2] for x in ls)
print(f"\nstatic batching spent {slots_s - useful_s} of {slots_s} sequence-slots on requests that had already finished ({100*(slots_s-useful_s)/slots_s:.0f}% waste); continuous batching spent none (every slot is a live request).")
print("The ragged batch of Chapter 9's circuit run (tokens 3, 9, 1, 14 at the same step) is exactly what continuous batching creates all the time.")
json.dump({"lens": LENS, "static": {"total": ts_, "done": ds, "log": ls}, "continuous": {"total": tc, "done": dc, "log": lc}}, open(os.path.join(hw.ROOT, "out", "ch09_example_b.json"), "w"))
