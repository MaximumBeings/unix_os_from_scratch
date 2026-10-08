#!/usr/bin/env python3
"""Chapter 15: the software checks of the page table, the window rule and the paged host. Returns a list of problems; empty means all pass.
 (1) the page table against a shadow model (plain Python lists), under 400 random operations (new, append, fork, trim, release), with the invariants checked after EVERY operation;
 (2) exhaustion raises OutOfPages and nothing leaks;  (3) the pages in use stay within the window bound;
 (4) the paged host and a plain-list host produce identical integers on the chip, for several (window, page) pairs;
 (5) the window really is a window: with W >= L it equals full attention; with a small W it differs; the chip follows the windowed float reference closely;
 (6) the structured model follows its rule under every window."""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capra as C, tiny_lm as T, paged_lm as PL
def invariants(pt, shadow, tag):
    P = []; cnt = [0] * len(pt.ref)
    for rid, tb in pt.table.items():
        for p in tb: cnt[p] += 1
    if cnt != pt.ref: P.append(f"{tag}: reference counts differ from the number of block-table entries")
    if len(set(pt.free)) != len(pt.free): P.append(f"{tag}: a page is on the free list twice")
    if any(pt.ref[p] != 0 for p in pt.free): P.append(f"{tag}: a referenced page is on the free list")
    if len(pt.free) + sum(1 for r in pt.ref if r > 0) != len(pt.ref): P.append(f"{tag}: a page is neither free nor referenced (leak)")
    for rid, rows in shadow.items():
        lo = pt.dropped[rid] * pt.page
        if pt.length[rid] != len(rows): P.append(f"{tag}: length of request {rid}")
        elif pt.rows(rid, lo, len(rows)) != rows[lo:]: P.append(f"{tag}: rows of request {rid} differ from the shadow")
    return P
def check_pagetable(seed):
    R = random.Random(seed); pt = PL.PageTable(24, 4); shadow = {}; P = []; nxt = 0; ctr = 0
    for step in range(400):
        op = R.choice(["append"] * 6 + ["new", "fork", "trim", "release"]); ids = list(shadow)
        try:
            if op == "new" and len(ids) < 5: pt.new(nxt); shadow[nxt] = []; nxt += 1
            elif op == "append" and ids:
                rid = R.choice(ids); ctr += 1; row = (ctr, -ctr); pt.append(rid, row); shadow[rid].append(row)
            elif op == "fork" and ids and len(ids) < 5: s = R.choice(ids); pt.fork(s, nxt); shadow[nxt] = list(shadow[s]); nxt += 1
            elif op == "trim" and ids: rid = R.choice(ids); k = R.randrange(0, len(shadow[rid]) + 1); pt.trim(rid, k)
            elif op == "release" and ids: rid = R.choice(ids); pt.release(rid); del shadow[rid]
        except PL.OutOfPages: pass
        P += invariants(pt, shadow, f"seed {seed} op {step} ({op})")
        if P: return P[:3]
    for rid in list(shadow): pt.release(rid)
    if len(pt.free) != 24 or any(pt.ref): P.append(f"seed {seed}: pages leaked after releasing every request")
    return P
def check_all():
    P = []
    for s in range(6): P += check_pagetable(s)
    pt = PL.PageTable(3, 2); pt.new(0)
    try:
        for i in range(10): pt.append(0, (i, i))
        P.append("(2) no OutOfPages when the pool is exhausted")
    except PL.OutOfPages: pass
    if pt.length[0] != 6: P.append("(2) a failed append changed the length")
    m = T.make_model("structured"); exp = [0]
    for _ in range(24): exp.append(T.f_next(exp[-1]))
    for W, page in ((None, 4), (8, 4), (6, 2), (4, 4), (3, 1)):
        chip = PL.PagedChip(m, 24, W, page, 64); t1, r1 = chip.generate(0, 24); chip.paged = False; t2, r2 = chip.generate(0, 24)
        if [r["ints"] for r in r1] != [r["ints"] for r in r2] or t1 != t2: P.append(f"(4) W={W} page={page}: paged host and plain host give different integers")
        if t1 != exp: P.append(f"(6) W={W}: the chip does not follow the rule")
        for r in r1:                                            # exact page count at every step, from the window rule alone
            n = r["L"] - 1; kf = 0 if W is None else max(0, r["L"] - W); exp_pages = (-(-n // page) - kf // page) if n > 0 else 0
            if r["pages"] != exp_pages: P.append(f"(3) W={W} page={page} step {r['L']}: {r['pages']} pages in use, the window rule says {exp_pages}")
        if W is not None:
            bound = -(-(W - 1) // page) + 1
            if max(r["pages"] for r in r1) > bound + 1: P.append(f"(3) W={W} page={page}: {max(r['pages'] for r in r1)} pages in use, bound {bound + 1}")
            if r1[-1]["pages"] > bound: P.append(f"(3) W={W} page={page}: {r1[-1]['pages']} pages in use at the end, bound {bound}")
        else:
            if r1[-1]["pages"] != -(-24 // page): P.append("(3) without a window every page must stay")
    mr = T.make_model("random", 2); full = PL.float_generate(mr, 3, 16, None); kc = vc = None
    k1, v1, k2, v2 = [], [], [], []; diff = 0
    for L in range(1, 14):
        a = PL.float_windowed(mr, full[L - 1], k1, v1, 20); b = T.float_step(mr, full[L - 1], k2, v2)
        if max(x - y for x, y in zip(a[0], b[0])) > 1e-12 or min(x - y for x, y in zip(a[0], b[0])) < -1e-12: P.append("(5) a window larger than the context changed the result")
        k1.append(a[1]); v1.append(a[2]); k2.append(b[1]); v2.append(b[2])
    k1, v1, k2, v2 = [], [], [], []
    for L in range(1, 14):
        a = PL.float_windowed(mr, full[L - 1], k1, v1, 3); b = T.float_step(mr, full[L - 1], k2, v2); diff = max(diff, max(abs(x - y) for x, y in zip(a[0], b[0])))
        k1.append(a[1]); v1.append(a[2]); k2.append(b[1]); v2.append(b[2])
    for W in (2, 3, 5):                                      # (5b) the windowed float step against a from-scratch recomputation of exactly the last W-1 rows
        toks = full[:12]; k1, v1 = [], []
        for L in range(1, 12):
            lg, k, v, _ = T.float_step(mr, toks[L - 1], k1[-(W - 1):] if len(k1) >= W - 1 else k1, v1[-(W - 1):] if len(v1) >= W - 1 else v1)
            prev = toks[max(0, L - W):L - 1]; kk = [T.mv(mr["E"][t], mr["Wk"]) for t in prev]; vv = [T.mv(mr["E"][t], mr["Wv"]) for t in prev]
            ref = T.float_step(mr, toks[L - 1], kk, vv)[0]; got = PL.float_windowed(mr, toks[L - 1], k1, v1, W)[0]
            if max(abs(a - b) for a, b in zip(ref, got)) > 1e-9: P.append(f"(5) W={W} step {L}: windowed float step differs from a recomputation over exactly the last {W} tokens")
            k1.append(k); v1.append(v)
    if diff < 1e-3: P.append("(5) a window of 3 made no difference to a random model: the window is not applied")
    for W in (None, 4):
        chip = PL.PagedChip(mr, 16, W, 4, 64); ft = PL.float_generate(mr, 3, 16, W); toks, rec = chip.generate(3, 16, force=ft); kc, vc = [], []
        for L in range(1, 17):
            lg, k, v, _ = PL.float_windowed(mr, ft[L - 1], kc, vc, W); kc.append(k); vc.append(v); e = math.sqrt(sum((a - b) ** 2 for a, b in zip(rec[L - 1]["logits"], lg)) / sum(b * b for b in lg))
            if e > 0.15: P.append(f"(5) W={W} step {L}: chip logits differ from the windowed float by {e:.2f}")
    return P
if __name__ == "__main__":
    p = check_all(); print("problems:", p if p else "none")
