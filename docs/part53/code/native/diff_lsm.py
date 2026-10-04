#!/usr/bin/env python3
"""Chapter 53: differential test. N random scripts (gen_script.py, with crashes injected at random bytes and recoveries) run through the C store (lsm_cli, the SAME 053_lsm.c the kernel links, with AddressSanitizer + UBSan) and the independent Python store (lsm_ref.py).
The WHOLE output must be identical: every result code, every value read, every digest, every statistic, and every byte of every file on the (crashed and recovered) file system. Usage: diff_lsm.py N [cli]"""
import collections, os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here); sys.path.insert(0, os.path.join(here, ".."))
import gen_script, lsm_ref
N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/kcli"; tmp = tempfile.mkdtemp(); stats = collections.Counter(); diffs = 0
for seed in range(N):
    if diffs and os.environ.get("STOP_AT_FIRST"): break
    p = os.path.join(tmp, "s.txt"); open(p, "w").write(gen_script.gen(seed, [60, 200, 600][seed % 3]))
    c = subprocess.run([cli, p], capture_output=True, text=True); r = lsm_ref.run(p)
    if c.returncode != 0 or c.stderr: print("CRASH", seed, c.stderr[:300]); diffs += 1; continue
    if c.stdout != r:
        diffs += 1; a, b = c.stdout.splitlines(), r.splitlines()
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y: print("DIFFERENT seed", seed, "line", i + 1); print("  C  :", x[:200]); print("  Py :", y[:200]); break
        else: print("DIFFERENT length", seed, len(a), len(b))
        continue
    stats["scripts"] += 1
    for l in r.splitlines():
        w = l.split(); stats["commands"] += 1
        if w[0] == "recover": stats["crash recoveries"] += 1; stats["... with a torn log tail cut"] += w[4] == "cut" and w[5] != "0"; stats["... with orphan files deleted"] += w[7] != "0"
        if w[0] in ("put", "del") and w[-1] == "io": stats["writes refused after the crash"] += 1
        if w[0] == "flush" and w[-1] == "ok": stats["flushes"] += 1
        if w[0] == "compact" and w[-1] == "ok": stats["compactions"] += 1
        if w[0] == "get" and w[2] == "found": stats["reads that found a value"] += 1
        if w[0] == "get" and w[2] == "none": stats["reads of a missing or deleted key"] += 1
        if w[0] == "open" and w[1] == "corrupt": stats["OPENS REPORTING CORRUPT"] += 1
for k in sorted(stats): print(f"  {k}: {stats[k]}")
print(f"{N} scripts, {diffs} differences"); sys.exit(1 if diffs else 0)
