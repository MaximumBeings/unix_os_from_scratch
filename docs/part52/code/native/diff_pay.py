#!/usr/bin/env python3
"""Chapter 52: differential test. N random companies (gen_pay.py: every filing status and pay frequency, the Step 2 box, credits, extra withholding, invalid employees, pay that crosses the Social Security wage base and the Additional Medicare threshold, deductions larger than gross, unknown employees) run through the C engine (pay_cli, the SAME 052_payroll.c the kernel links, with AddressSanitizer + UBSan) and the independent Python reference (pay_ref.py, exact Fractions, a different method for the bracket tax): the whole text output, every cent of every line, must be identical. Usage: diff_pay.py N [cli]"""
import collections, os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import gen_pay
N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/kcli"; tmp = tempfile.mkdtemp(); stats = collections.Counter(); diffs = 0; ref = os.path.join(here, "..", "pay_ref.py")
for seed in range(N):
    if diffs and os.environ.get("STOP_AT_FIRST"): break
    p = os.path.join(tmp, "f.txt"); open(p, "w").write(gen_pay.gen(seed, 10 + seed % 40))
    c = subprocess.run([cli, p], capture_output=True, text=True); r = subprocess.run([sys.executable, ref, p], capture_output=True, text=True).stdout
    if c.returncode != 0 or c.stderr: print("CRASH", seed, c.stderr[:200]); diffs += 1; continue
    if c.stdout != r: diffs += 1; print("DIFFERENT seed", seed); print(c.stdout[-400:], "--- python\n", r[-400:]) if diffs <= 2 else None; continue
    stats["companies"] += 1
    for l in r.splitlines():
        w = l.split(); stats["pay lines"] += 1; stats["... accepted" if w[3] == "ok" else "... refused (" + w[3] + ")"] += 1
        if w[3] == "ok" and int(w[9]) > 0: stats["... with Additional Medicare"] += 1
        if w[3] == "ok" and int(w[7]) == 0 and int(w[5]) > 0: stats["... past the Social Security wage base"] += 1
for k in sorted(stats): print(f"  {k}: {stats[k]}")
print(f"{N} companies, {diffs} differences"); sys.exit(1 if diffs else 0)
