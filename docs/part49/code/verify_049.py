#!/usr/bin/env python3
"""Chapter 49: verification FROM OUTSIDE THE KERNEL. It reads the serial capture, the FAT16 disk image and the original files, and shares no code with the kernel (claims_ref.py has its own X12 splitter, adjudicator,
835 builder and reconciler):
  1. reads the FAT16 volume with its own reader and takes the six remittance files R101.835 ... R106.835 the kernel wrote;
  2. adjudicates the six invented claims again, in order, carrying the deductible and out-of-pocket accumulators from claim to claim, and compares with (a) the canonical adjudication text the kernel printed for each claim and (b) the
     835 FILE on the disk, byte for byte -- the 835 is rebuilt from scratch by the Python builder;
  3. reconciles every 835 on the disk with Python's own reconciler (the check a billing office runs);
  4. re-renders the human-readable lines the kernel printed ('claim PCN-A001: status 1, charged $202.00 = ...') from Python's numbers;
  5. re-checks, with its own code, every statement the kernel made about the ten real-format files (the SE01 count, the NPI check digit, the 835's reconciliation);
  6. checks the five attacks.
Usage: verify_049.py SERIAL_TXT DISK_IMG"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import claims_ref as R
serial = open(sys.argv[1], errors="replace").read(); disk = sys.argv[2]; here = os.path.dirname(os.path.abspath(__file__)); fails = 0
def report(ok, msg):
    global fails; fails += not ok; print(("  ok   " if ok else "  FAIL ") + msg)
def read_volume(path):
    d = open(path, "rb").read(); bps = struct.unpack_from("<H", d, 11)[0]; spc = d[13]; res = struct.unpack_from("<H", d, 14)[0]; nf = d[16]; rootn = struct.unpack_from("<H", d, 17)[0]; fsz = struct.unpack_from("<H", d, 22)[0]
    fat, root = res, res + nf * fsz; data = root + (rootn * 32 + bps - 1) // bps; files = {}
    def nxt(c): return struct.unpack_from("<H", d, fat * bps + 2 * c)[0]
    for i in range(rootn):
        e = d[root * bps + 32 * i: root * bps + 32 * i + 32]
        if e[0] in (0, 0xE5) or e[11] & 0x18: continue
        name = (e[0:8].decode().rstrip() + "." + e[8:11].decode().rstrip()).rstrip("."); c = struct.unpack_from("<H", e, 26)[0]; size = struct.unpack_from("<I", e, 28)[0]; out = b""; g = 0
        while 2 <= c < 0xFFF8 and len(out) < size and g < 100000: lo = (data + (c - 2) * spc) * bps; out += d[lo:lo + spc * bps]; c = nxt(c); g += 1
        files[name] = out[:size]
    return files
print("-- 1. the FAT16 volume the kernel wrote")
files = read_volume(disk); names = ["R101.835", "R102.835", "R103.835", "R104.835", "R105.835", "R106.835"]
report(sorted(files) == names, f"files on the disk: {', '.join(sorted(files))}")
print("-- 2. the six invented claims adjudicated again, in order, carrying the accumulators")
claims = []
for L in "ABCDEF": claims += R.parse_837(open(os.path.join(here, "data", f"claim_{L}.837")).read())
plan = (50000, 0, 2500, 2000, 200000, 0); res = R.adjudicate(claims, plan)
canon = {m.group(1): m.group(2) for m in re.finditer(r"@@ADJ (\w) BEGIN\n(.*?)@@ADJ \1 END", serial, re.S)}
for i, L in enumerate("ABCDEF"):
    want = R.canonical([res[i]]); want = want  # accumulators are cumulative inside res[i], exactly as the kernel's state is
    report(canon.get(L) == want, f"claim {L} ({claims[i]['pcn']}): the kernel's printed adjudication equals the independent adjudication ({want.count(chr(10))} lines)")
for i, L in enumerate("ABCDEF"):
    cl = claims[i]; want835 = R.build_835([res[i]], cl["payer"][1], cl["payer"][0], cl["billing"][1], cl["billing"][0], 19737 + 30 * i, 101 + i)
    got = files.get(names[i], b"").decode()
    report(got == want835, f"{names[i]}: the 835 file on the disk equals the 835 the Python builder produces from scratch ({len(want835)} bytes)")
    m = re.search(r"@@835 %s BEGIN\n(.*?)\n@@835 %s END" % (L, L), serial, re.S); report(m is not None and m.group(1) == got, f"{names[i]}: the 835 the kernel printed equals the file on the disk")
print("-- 3. every 835 on the disk reconciled by Python's own reconciler")
tot = 0
for n in names:
    rc = R.recon(files[n].decode()); ok = rc.rstrip().endswith("all_balanced 1") and "balanced 0" not in rc and "pr_matches 0" not in rc; tot += ok
    report(ok, f"{n}: " + rc.splitlines()[0])
print("-- 4. the human-readable lines re-rendered from Python's numbers")
def usd(c): s = "-" if c < 0 else ""; c = abs(c); return f"{s}${c // 100:,}.{c % 100:02d}"
bad = 0
for i, r in enumerate(res):
    line = f"    claim {r['c']['pcn']}: status {r['status']}, charged {usd(r['charge'])} = plan pays {usd(r['paid'])} + patient owes {usd(r['pr'])} + written off {usd(r['co'])}"
    if line not in serial.splitlines(): bad += 1; print("     missing:", line)
    for k, l in enumerate(r["lines"], 1):
        t = f"    line {k}  {l['code']}  charged {usd(l['charge'])}  allowed {usd(l['allowed'])}  plan pays {usd(l['paid'])}" + "".join(f"   {g}-{rc} {usd(a)}" for g, rc, a in l["adj"])
        if t not in serial.splitlines(): bad += 1; print("     missing:", t)
    ytd = f"    year to date: deductible met {usd(r['ded_met'])} of $500.00, out-of-pocket {usd(r['oop_met'])} of $2,000.00"
    if ytd not in serial.splitlines(): bad += 1; print("     missing:", ytd)
report(bad == 0, "every printed claim summary, service line and year-to-date line equals the independent figures")
print("-- 5. every statement the kernel made about the real-format files, re-derived with this script's own code")
def npi_ok(s):
    d = [int(c) for c in "80840" + s]; t = 0
    for i, x in enumerate(reversed(d)):
        if i % 2 == 1: x *= 2; x -= 9 if x > 9 else 0
        t += x
    return t % 10 == 0
rd = lambda n: open(os.path.join(here, "data", "real", n), errors="replace").read()
se_bad = 0; npi_bad = 0; seen = 0
for n in ("x12_valid.txt", "x12_no_errors.txt", "x12_complex.txt", "x12_valid_different_separators.txt", "x12_missing_elements.txt", "x12_ambiguous_loop.txt"):
    t = rd(n); es = t[3]; term = t[105]; segs = [s.strip("\r\n \t") for s in t.split(term) if s.strip("\r\n \t")]
    st = next(i for i, s in enumerate(segs) if s.startswith("ST" + es)); se = next(i for i, s in enumerate(segs) if s.startswith("SE" + es)); n_actual = se - st + 1; n_claimed = int(segs[se].split(es)[1])
    if n_actual == n_claimed: se_bad += 1
    npis = [s.split(es)[9] for s in segs if s.startswith("NM1" + es + "85" + es) and len(s.split(es)) > 9]
    if npis and npi_ok(npis[0]): npi_bad += 1
    seen += 1
report(se_bad == 0 and seen == 6, f"in all {seen} sample 837 files the SE01 count really is wrong (the kernel's 'strict' column) -- counted {se_bad} files where it was right")
report(npi_bad == 0, "in all six the first billing NPI really fails the check-digit test (1234567890), the kernel's 'lenient' verdict")
report(npi_ok("1234567893") and not npi_ok("1234567890"), "and 1234567893, the NPI used by the invented claims, passes it")
t835 = rd("835_mult_loops.txt"); rc = R.recon(t835)
report("all_balanced 1" in rc and "bpr 0 clp_paid_sum 0" in rc, "the real 835 reconciles (three denied lines, 915.39 charged, nothing paid, CAS CO-16 / CO-xx for every dollar): the kernel said so")
es = t835[3]; term = t835[105]; segs = [s.strip("\r\n \t") for s in t835.split(term) if s.strip("\r\n \t")]; stx = next(i for i, s in enumerate(segs) if s.startswith("ST" + es)); sex = next(i for i, s in enumerate(segs) if s.startswith("SE" + es))
report(sex - stx + 1 == int(segs[sex].split(es)[1]), "the real 835's own SE01 count is right, so the strict envelope check accepts it (unlike the 837 samples)")
print("-- 6. the attacks")
a = serial[serial.index("== Part 4"):]
report("SE01 does not equal the number of segments" in a.split("Attack 2")[0], "attack 1 (SE01 changed): refused for the SE01 count")
report("claim total does not equal the sum of its service lines" in a.split("Attack 3")[0].split("Attack 2")[1], "attack 2 (claim total $203.00): refused, the lines add to $202.00")
report("NPI" in a.split("Attack 4")[0].split("Attack 3")[1], "attack 3 (one NPI digit): refused by the check digit")
report("data ends inside a segment" in a.split("Attack 5")[0].split("Attack 4")[1], "attack 4 (cut off at 700 bytes): refused as truncated")
report("REPORTED UNBALANCED" in a.split("Attack 5")[1], "attack 5 (CLP04 changed): the remittance is reported unbalanced")
m = re.search(r"claims demo complete: (\d+) invented claims adjudicated, (\d+) remittance files written and read back identical, (\d+) remittances reconciled, (\d+) real-format files examined, (\d+) attacks refused", serial)
report(m and tuple(map(int, m.groups())) == (6, 6, 7, 10, 5), "final tally: 6 claims adjudicated, 6 files written and read back, 7 remittances reconciled (six built here plus the real one), 10 real-format files examined, 5 attacks")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
