#!/usr/bin/env python3
"""Chapter 48: differential test. N random valid XBRL filings (gen_docs.py): the C engine (ratios_cli, built with AddressSanitizer + UBSan, the SAME 048_xbrl.c / 048_ratios.c the kernel links) vs
the independent Python reference (ratios_ref.py, Python's own XML parser, exact integer arithmetic). Every line must match. Usage: diff_test.py N [cli] -- prints a summary and exits 1 on any difference."""
import os, subprocess, sys, tempfile, collections
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here); sys.path.insert(0, os.path.join(here, ".."))
import gen_docs, ratios_ref
N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/rcli"; tmp = tempfile.mkdtemp(); stats = collections.Counter(); diffs = 0
for seed in range(N):
    text, price = gen_docs.gen(seed); p = os.path.join(tmp, "d.xml"); open(p, "w", encoding="utf-8").write(text)
    try: ref = "\n".join(ratios_ref.analyse(p, price or None)) + "\n"
    except Exception as e: ref = "ANALYSE_ERROR\n"
    r = subprocess.run([cli, p, str(price)], capture_output=True, text=True); c = r.stdout
    if r.returncode not in (0, 1) or r.stderr: print("CRASH/SANITIZER", seed, r.stderr[:300]); diffs += 1; continue
    if c.startswith("ANALYSE_ERROR") or c.startswith("PARSE_ERROR"): c = "ANALYSE_ERROR\n"
    if c != ref:
        diffs += 1
        if diffs <= 3: print("DIFFERENT, seed", seed); print("--- C\n" + c + "--- Python\n" + ref)
        continue
    stats["documents"] += 1
    if ref.startswith("ANALYSE_ERROR"): stats["both refuse the document (no period end, or a duplicated id)"] += 1; continue
    stats["ACCEPTED" if "verdict ACCEPTED" in ref else "REJECTED"] += 1
    if "check_conflicting_duplicates FAIL" in ref: stats["rejected: conflicting duplicate"] += 1
    if "check_assets_eq_liab_plus_equity FAIL" in ref: stats["rejected: balance sheet does not balance"] += 1
    if "check_gross_profit FAIL" in ref: stats["gross-profit identity FAIL (reported, not rejected)"] += 1
    for line in ref.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "NA":
            stats["NA values (all reasons)"] += 1
            if parts[2] == "overflow": stats["NA because of overflow"] += 1
            if parts[2:4] == ["denominator", "not"]: stats["NA because denominator not positive"] += 1
        elif len(parts) == 3 and parts[2] in ("x", "bp", "usd", "usd4"): stats["values compared"] += 1
for k in sorted(stats): print(f"  {k}: {stats[k]}")
print(f"{N} documents, {diffs} differences"); sys.exit(1 if diffs else 0)
