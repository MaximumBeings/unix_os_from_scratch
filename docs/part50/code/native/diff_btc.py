#!/usr/bin/env python3
"""Chapter 50: differential test. N random blocks (gen_blocks.py: valid, SegWit, and 17 kinds of defect, some damaged byte by byte after mining) go through the C validator (btc_cli, the SAME 050_btc.c the kernel links, built with
AddressSanitizer + UBSan) and through the independent Python reference (btc_ref.py: hashlib, big integers). The canonical reports must be IDENTICAL. Also run, on every file in data/btc, with each file's proper limit, and every
raw transaction of Bitcoin Core's test vectors. Usage: diff_btc.py N [cli]"""
import collections, os, struct, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(here, "..")); sys.path.insert(0, here)
import btc_ref as R, gen_blocks
N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/bcli"; tmp = tempfile.mkdtemp(); stats = collections.Counter(); diffs = 0
def ref_block(b, lim):
    p = os.path.join(tmp, "r.blk"); open(p, "wb").write(b); return R.report("r.blk", b, lim)
for seed in range(N):
    if diffs and os.environ.get("STOP_AT_FIRST"): break
    b = gen_blocks.gen(seed); p = os.path.join(tmp, "r.blk"); open(p, "wb").write(b); lim = 0x207fffff
    c = subprocess.run([cli, "block", p, "%08x" % lim], capture_output=True, text=True, errors="replace")
    ref = R.report("r.blk", b, lim)
    if c.returncode != 0 or c.stderr: print("CRASH/SANITIZER seed", seed, c.stderr[:300]); diffs += 1; continue
    if "unparseable" in c.stdout and "unparseable" in ref: c_s = c.stdout.split("unparseable")[0]; r_s = ref.split("unparseable")[0]; ok = c_s == r_s
    else: ok = c.stdout == ref
    if not ok:
        diffs += 1; print("DIFFERENT seed", seed, gen_blocks.KINDS[seed % len(gen_blocks.KINDS)]); print(c.stdout[:700], "--- python\n", ref[:700]) if diffs <= 2 else None; continue
    stats["blocks"] += 1; v = ref.strip().splitlines()[-1]; stats[("verdict: " + v.replace("  verdict ", "")) if "unparseable" not in ref else "refused as unparseable"] += 1
base = os.path.join(here, "..", "data", "btc"); files = 0
for name in sorted(os.listdir(base)):
    if not name.endswith(".blk"): continue
    lim = 0x207fffff if name.startswith("syn") else 0x1d00ffff; b = open(os.path.join(base, name), "rb").read(); p = os.path.join(tmp, name); open(p, "wb").write(b)
    c = subprocess.run([cli, "block", p, "%08x" % lim], capture_output=True, text=True)
    if c.stdout != R.report(name, b, lim): diffs += 1; print("DIFFERENT file", name)
    else: files += 1
stats["data/btc block files compared"] = files
txv = open(os.path.join(base, "txvec.bin"), "rb").read(); i = 0; nt = 0
while i < len(txv):
    kind, exp, ln = txv[i], txv[i + 1], struct.unpack_from("<H", txv, i + 2)[0]; raw = txv[i + 4:i + 4 + ln]; i += 4 + ln
    p = os.path.join(tmp, "t.bin"); open(p, "wb").write(raw); c = subprocess.run([cli, "tx", p], capture_output=True, text=True)
    try: t, q = R.parse_tx(raw, 0); r = "check " + (R.check_tx(t) if q == len(raw) else "trailing")
    except (ValueError, IndexError, struct.error) as e: r = "unparseable"
    cs = c.stdout.strip(); ok = cs == r or (cs.startswith("unparseable") and r == "unparseable")
    if not ok: diffs += 1; print("TX DIFFERENT", kind, exp, cs, "|", r)
    nt += 1
stats["Bitcoin Core test-vector transactions compared"] = nt
for k in sorted(stats, key=str): print(f"  {k}: {stats[k]}")
print(f"{N} random blocks, {diffs} differences"); sys.exit(1 if diffs else 0)
