#!/usr/bin/env python3
"""Chapter 15, example B: what paging and windows buy. (1) memory waste: contiguous reservation vs pages of different sizes; (2) prefix sharing and copy-on-write; (3) what a window costs in accuracy on a random model; (4) serving arithmetic (derived). Usage: ch15_example_b.py. Writes out/ch15_example_b.json"""
import json, math, os, random, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import paged_lm as PL, tiny_lm as T
res = {}
print("== 1. memory: how many requests fit?  Pool of 8,192 cache rows. Request lengths are drawn from a long-tailed distribution (mean about 260 rows, longest allowed 2,048). The CONTIGUOUS scheme must reserve the longest allowed length per request because the final length is unknown when the request starts; the PAGED scheme takes pages as it goes.")
R = random.Random(11); lens = [min(2048, max(16, int(R.lognormvariate(5.2, 0.9)))) for _ in range(400)]
print(f"   400 sample lengths: mean {sum(lens)/len(lens):.0f}, median {sorted(lens)[200]}, max {max(lens)}")
CAP = 8192
def admit_contiguous():
    n = used = 0
    for L in lens:
        if (n + 1) * 2048 > CAP: break
        n += 1; used += L
    return n, used, n * 2048
def admit_paged(page):
    pt = PL.PageTable(CAP // page, page); n = used = 0
    for i, L in enumerate(lens):
        pt.new(i)
        try:
            for r in range(L): pt.append(i, None)
        except PL.OutOfPages: pt.release(i); break
        n += 1; used += L
    return n, used, pt.used() * page
print(f"{'scheme':22s} {'requests held':>14s} {'rows used':>10s} {'rows reserved':>14s} {'wasted':>8s}")
n, u, r = admit_contiguous(); res["contig"] = [n, u, r]; print(f"{'contiguous (reserve 2048)':22s} {n:14d} {u:10d} {r:14d} {100*(r-u)/r:7.1f}%")
res["paged"] = {}
for page in (1, 4, 16, 64, 256):
    n, u, r = admit_paged(page); res["paged"][page] = [n, u, r]; print(f"{'paged, page = ' + str(page):22s} {n:14d} {u:10d} {r:14d} {100*(r-u)/r:7.1f}%")
print("   (the waste of the paged schemes is the unused tail of each request's last page, on average half a page)")
print("\n== 2. prefix sharing: 8 requests share a system prompt, each then generates 40 rows of its own")
res["share"] = {}
print(f"{'prompt rows':>12s} {'page':>5s} {'pages, copied':>14s} {'pages, shared':>14s} {'saved':>7s} {'copy-on-write copies':>21s}")
for prompt in (96, 100, 250):
    for page in (16, 40):
        pc = PL.PageTable(400, page); ps = PL.PageTable(400, page)
        for i in range(8):
            pc.new(i)
            for r in range(prompt + 40): pc.append(i, None)
        ps.new("base")
        for r in range(prompt): ps.append("base", None)
        before = ps.used(); cow = 0
        for i in range(8):
            ps.fork("base", i)
            for r in range(40):
                u0 = ps.used(); ps.append(i, None); cow += (ps.used() - u0 > 0 and (ps.length[i] - 1) % page != 0)
        ps.release("base"); res["share"][f"{prompt}/{page}"] = [pc.used(), ps.used(), cow]
        print(f"{prompt:12d} {page:5d} {pc.used():14d} {ps.used():14d} {100*(pc.used()-ps.used())/pc.used():6.0f}% {cow:21d}")
print("   a prompt that ends in the middle of a page (100 rows, page 16 or 40) costs one extra page per request: the first append copies the shared tail page")
print("\n== 3. what a window costs in accuracy: random models, teacher-forced on the full-context float tokens (3 models x 16 starts x 24 steps); window W against no window")
print(f"{'W':>4s} {'agreement':>14s} {'mean logit error':>17s}")
res["win"] = {}
for W in (2, 4, 8, 16, 24):
    ag = tot = 0; er = []
    for seed in (1, 2, 3):
        m = T.make_model("random", seed)
        for s in range(16):
            ft = PL.float_generate(m, s, 24, None); k1, v1, k2, v2 = [], [], [], []
            for L in range(1, 25):
                a = T.float_step(m, ft[L - 1], k1, v1); b = PL.float_windowed(m, ft[L - 1], k2, v2, W); k1.append(a[1]); v1.append(a[2]); k2.append(b[1]); v2.append(b[2])
                ag += a[0].index(max(a[0])) == b[0].index(max(b[0])); tot += 1; er.append(math.sqrt(sum((x - y) ** 2 for x, y in zip(a[0], b[0])) / sum(x * x for x in a[0])))
    res["win"][W] = [ag, tot, sum(er) / len(er)]; print(f"{W:4d} {ag:6d}/{tot} ({100*ag/tot:5.1f}%) {100*sum(er)/len(er):16.1f}%")
print("   W = 24 is the full context here (24 steps), so it is exact by construction")
print("\n== 4. serving arithmetic (DERIVED, ASSUMED chip: 80 GB; 7B-class model, 7 GB int8 weights, 256 KiB of cache per token, as in Chapters 9 and 14)")
print(f"{'context':>8s} {'KV GiB, full':>13s} {'max batch, full':>16s} {'KV GiB, window 4096':>20s} {'max batch, window':>18s}")
res["serve"] = {}
for ctx in (2048, 8192, 32768, 131072):
    kv = 256 * 1024; full = kv * ctx; win = kv * min(ctx, 4096); mb = lambda x: int((80e9 - 7e9) // x)
    res["serve"][ctx] = [full, win, mb(full), mb(win)]; print(f"{ctx:8d} {full/2**30:13.2f} {mb(full):16d} {win/2**30:20.2f} {mb(win):18d}")
print("   with a window the cache of a request stops growing at the window: the batch the memory allows no longer shrinks with the context. The price is what a token outside the window can no longer say to a later one: the model must not need it (or must have been trained with the window).")
json.dump({str(k): v for k, v in res.items()}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch15_example_b.json"), "w"))
