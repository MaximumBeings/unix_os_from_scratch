#!/usr/bin/env python3
"""Chapter 48: verification FROM OUTSIDE THE KERNEL. It reads three things the kernel left behind or that exist independently of it -- the serial capture, the FAT16 disk image, and the ORIGINAL full
SEC documents (not the slimmed copies baked into the kernel) -- and shares no code with the kernel:
  1. reads the FAT16 volume with its own reader and takes the six <TICKER>.RPT files the kernel wrote;
  2. recomputes every report from the full original EDGAR XBRL file with ratios_ref.py (Python's xml.etree, exact integer arithmetic, the same published rules) and compares it, line for line,
     with (a) the report the kernel printed between @@CANON markers and (b) the report file on the disk;
  3. re-renders every number the kernel printed in human form ('25.31%', '0.99x', '$99,584,000,000') from the canonical integers and compares the text;
  4. checks the accession numbers the kernel printed against the filing metadata that ships with the documents;
  5. checks the four attacks were all refused, for the reasons stated.
Usage: verify_048.py SERIAL_TXT DISK_IMG [FIXTURE_DIR]   (FIXTURE_DIR default /tmp/et/tests/fixtures/xbrl, a clone of github.com/dgunning/edgartools)"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ratios_ref
serial = open(sys.argv[1], errors="replace").read(); disk = sys.argv[2]; fx = sys.argv[3] if len(sys.argv) > 3 else "/tmp/et/tests/fixtures/xbrl"
FULL = {"AAPL": "aapl/10k_2023/aapl-20230930_htm.xml", "KO": "ko/10k_2024/ko-20240220_htm.xml", "NVDA": "nvda/10k_2026/nvda-20260125_htm.xml",
        "MSFT": "msft/10k_2024/msft-20240730_htm.xml", "XOM": "xom/10k_2023/xom-20221231_htm.xml", "JPM": "jpm/10k_2024/jpm-20240216_htm.xml"}
fails = 0
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
files = read_volume(disk); rpts = {n[:-4]: b.decode() for n, b in files.items() if n.endswith(".RPT")}
report(sorted(rpts) == sorted(FULL), f"report files on the disk: {', '.join(sorted(files))}")
canon = {m.group(1): m.group(2) for m in re.finditer(r"@@CANON (\w+) BEGIN\n(.*?)@@CANON \1 END", serial, re.S)}
report(sorted(canon) == sorted(FULL), f"canonical blocks printed on the serial port: {', '.join(sorted(canon))}")
print("-- 2. every report recomputed from the FULL original SEC document by the independent Python reference (price $100.00 for every P/E)")
total = 0
for t, rel in FULL.items():
    ref = "\n".join(ratios_ref.analyse(os.path.join(fx, rel), 10000)) + "\n"; n = ref.count("\n"); total += n
    report(canon.get(t) == ref, f"{t}: kernel's printed report equals the reference computed from {os.path.basename(rel)} ({n} lines)")
    report(rpts.get(t) == ref, f"{t}: the report FILE read from the disk image equals it too")
print(f"   ({total} lines compared in all, each twice)")
print("-- 3. the human-readable numbers re-rendered from the canonical integers")
def render(v, unit):
    sgn = "-" if v < 0 else ""; m = abs(v)
    if unit == "bp": return f"{sgn}{m // 100}.{m % 100:02d}%"
    if unit == "x": return f"{sgn}{m // 100}.{m % 100:02d}x"
    if unit == "usd": return f"{sgn}${m:,}"
    return f"{sgn}${m // 10000}.{m % 10000:04d}"
shown = 0; bad = 0
for t in FULL:
    blk = re.search(r"== %s -- .*?(?=\n== |\n== Four)" % t, serial, re.S).group(0)
    for line in canon[t].splitlines():
        p = line.split()
        if len(p) == 3 and p[2] in ("x", "bp", "usd", "usd4"):
            want = f"    {p[0]}  {render(int(p[1]), p[2])}"; shown += 1
            if want not in blk.splitlines(): bad += 1; print("     missing:", want)
        elif len(p) >= 3 and p[1] == "NA":
            want = f"    {p[0]}  n/a ({' '.join(p[2:])})"; shown += 1
            if want not in blk.splitlines(): bad += 1; print("     missing:", want)
report(bad == 0 and shown > 100, f"{shown} printed ratio lines (values and n/a reasons) match the re-rendered canonical integers exactly")
print("-- 4. accession numbers against the filing metadata shipped with the documents")
for t, rel in FULL.items():
    meta = open(os.path.join(fx, os.path.dirname(rel), "filing_metadata.txt")).read(); acc = re.search(r"Accession No: (\S+)", meta).group(1); filed = re.search(r"Filing Date: (\S+)", meta).group(1)
    report(f"Form 10-K filed {filed}, accession {acc}" in serial, f"{t}: kernel printed 'filed {filed}, accession {acc}'")
print("-- 5. the four attacks")
a = serial[serial.index("Four deliberate attacks"):]
report("check_assets_eq_liab_plus_equity: FAIL" in a.split("Attack 2")[0] and "REJECTED" in a.split("Attack 2")[0], "attack 1 (one digit of total assets): the balance-sheet identity FAILED and the filing was REJECTED")
report("check_conflicting_duplicates: FAIL" in a.split("Attack 3")[0].split("Attack 2")[1], "attack 2 (a second, different Assets fact): flagged as a conflicting duplicate and REJECTED")
report("document ends inside an element (code -2)" in a, "attack 3 (truncated download): refused, 'document ends inside an element'")
report("root element is not <xbrl> (code -10)" in a, "attack 4 (an HTML error page): refused, 'root element is not <xbrl>'")
m = re.search(r"EDGAR demo complete: (\d+) filings analysed and ACCEPTED, (\d+) REJECTED, (\d+) refused while reading, (\d+) report files", serial)
report(m and tuple(map(int, m.groups())) == (6, 2, 2, 6), "final tally: 6 accepted, 2 rejected, 2 refused while reading, 6 report files written")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
