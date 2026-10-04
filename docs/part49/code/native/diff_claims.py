#!/usr/bin/env python3
"""Chapter 49: differential test. N random VALID 837P files (random plans, accumulators, charges, units, covered and uncovered codes, duplicate lines, several office visits on one date) go through the C engine
(claims_cli, the SAME 049_*.c files the kernel links, built with AddressSanitizer + UBSan) and through the independent Python reference (claims_ref.py). Compared, byte for byte: the adjudication text, the 835 each side builds, and
the reconciliation of that 835 -- plus the reconciliation of randomly TAMPERED 835s (a changed amount, an added PLB, a negative adjustment, a claim-level CAS). Usage: diff_claims.py N [cli]"""
import os, random, subprocess, sys, tempfile, collections
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(here, "..")); sys.path.insert(0, here)
import make_claims, claims_ref
CODES = ["99202", "99203", "99204", "99205", "99211", "99212", "99213", "99214", "99215", "85025", "80053", "36415", "71046", "93000", "45378", "29881", "27447", "A9999", "99499", "J0000"]
def gen(seed):
    R = random.Random(seed); claims = []; seen = []
    for ci in range(R.randrange(1, 7)):
        lines = []
        for _ in range(R.randrange(1, 7)):
            if seen and R.random() < 0.15: pcn, code, ch, u, dos = R.choice(seen)   # a repeated line: the duplicate rule
            else:
                code = R.choice(CODES); u = R.choice([1, 1, 1, 2, 3, 5]); ch = R.choice([100, 2500, R.randrange(100, 5000), R.randrange(100, 300000), R.randrange(100, 6000000)]); dos = R.choice(["20240105", "20240105", "20240212", "20240930"]); pcn = None
            lines.append((code, str(ch), u, dos))
        pcn = R.choice(["PCN-%d" % R.randrange(1, 4), "PCN-%d" % R.randrange(1, 4), "X%d" % ci]); claims.append(dict(pcn=pcn, dx=["J069"] + (["I10"] if R.random() < .3 else []), lines=lines))
        for code, ch, u, dos in lines: seen.append((pcn, code, ch, u, dos))
    # duplicates need the SAME pcn as the earlier line: rebuild seen entries to carry their own pcn
    ded = R.choice([0, 50000, R.randrange(0, 200000)]); oop = R.choice([0, 200000, R.randrange(0, 1000000), 100]); plan = (ded, R.randrange(0, ded + 1), R.choice([0, 2500, R.randrange(0, 10000)]), R.choice([0, 2000, 10000, R.randrange(0, 10001)]), oop, R.randrange(0, oop + 1))
    return make_claims.build_837(claims, ctl=R.randrange(1, 999999999)), plan
def run(cli, mode, path, plan=None):
    cmd = [cli, mode, path] + ([str(x) for x in plan] if plan else []); r = subprocess.run(cmd, capture_output=True, text=True, errors="replace"); return r
def tamper(R, text):
    segs = [s for s in text.split("~") if s]
    idx = [i for i, s in enumerate(segs) if s.startswith(("CLP", "SVC", "CAS", "BPR"))]
    k = R.choice(["amount", "plb", "neg", "claimcas", "none"]); i = R.choice(idx)
    if k == "amount":
        e = segs[i].split("*"); j = R.choice([x for x in range(2, len(e)) if e[x] and e[x].replace(".", "").isdigit()] or [None])
        if j is not None: e[j] = str(R.randrange(0, 100000)); segs[i] = "*".join(e)
    elif k == "plb": segs.insert(len(segs) - 3, "PLB*1234567893*20241231*WO:1*" + str(R.randrange(1, 5000)))
    elif k == "neg": segs.insert(i + 1, "CAS*OA*23*-" + str(R.randrange(1, 900)))
    elif k == "claimcas":
        j = next((x for x, s in enumerate(segs) if s.startswith("NM1*QC")), None)
        if j is not None: segs.insert(j + 1, "CAS*CO*45*" + str(R.randrange(0, 900)))
    out = "~".join(segs[:-3]) + "~"
    n = sum(1 for s in segs if True); st = next(x for x, s in enumerate(segs) if s.startswith("ST*")); se = next(x for x, s in enumerate(segs) if s.startswith("SE*"))
    segs[se] = "SE*%d*0001" % (se - st + 1)
    return "~".join(segs) + "~"
if __name__ == "__main__":
    N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/ccli"; tmp = tempfile.mkdtemp(); stats = collections.Counter(); diffs = 0
    for seed in range(N):
        if diffs and os.environ.get("STOP_AT_FIRST"): break   # the mutation suite only needs to know that a difference exists
        text, plan = gen(seed); p = os.path.join(tmp, "c.837"); open(p, "w").write(text)
        c = run(cli, "adj", p, plan); ref = subprocess.run([sys.executable, os.path.join(here, "..", "claims_ref.py"), "adj", p] + [str(x) for x in plan], capture_output=True, text=True).stdout
        if c.returncode != 0 or c.stderr: print("CRASH or error", seed, c.stdout[:200], c.stderr[:200]); diffs += 1; continue
        if c.stdout != ref: diffs += 1; print("ADJ DIFFERENT seed", seed); print(c.stdout, "---", ref) if diffs <= 2 else None; continue
        b = run(cli, "build", p, plan); rb = subprocess.run([sys.executable, os.path.join(here, "..", "claims_ref.py"), "build", p] + [str(x) for x in plan], capture_output=True, text=True).stdout
        if b.stdout != rb: diffs += 1; print("835 DIFFERENT seed", seed); print(b.stdout[:300], "\n---\n", rb[:300]) if diffs <= 2 else None; continue
        stats["claim sets"] += 1; stats["claims"] += c.stdout.count("claim "); stats["service lines"] += c.stdout.count("  line "); stats["lines denied as duplicates (CO-18)"] += c.stdout.count("CO-18="); stats["lines denied non-covered (CO-96)"] += c.stdout.count("CO-96=")
        q = os.path.join(tmp, "r.835"); R = random.Random(seed * 7 + 1)
        for variant in range(3):
            t835 = rb if variant == 0 else tamper(R, rb); open(q, "w").write(t835)
            rc = run(cli, "recon", q); rp = subprocess.run([sys.executable, os.path.join(here, "..", "claims_ref.py"), "recon", q], capture_output=True, text=True); rr = rp.stdout
            if rp.returncode != 0 and rc.stdout.startswith("835 "): stats["tampered 835s both sides REFUSE (a segment outside a claim)"] += 1; break   # the reference raises, the C reader returns a structure error
            if rc.stdout != rr: diffs += 1; print("RECON DIFFERENT seed", seed, "variant", variant); print(rc.stdout[:400], "---\n", rr[:400]); break
            stats["835 reconciliations compared" + (" (tampered)" if variant else " (untouched)")] += 1; stats["... reported UNBALANCED"] += ("all_balanced 0" in rr)
    for k in sorted(stats): print(f"  {k}: {stats[k]}")
    print(f"{N} claim sets, {diffs} differences"); sys.exit(1 if diffs else 0)
