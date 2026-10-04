#!/usr/bin/env python3
"""Chapter 52: verification FROM OUTSIDE THE KERNEL. It reads the serial capture, the FAT16 disk image and the original pay file, and shares no code with the kernel (pay_ref.py is a second implementation: exact Fractions, bracket tax by the table-row method):
  1. reads the FAT16 volume with its own reader and takes PAYROLL.REG;
  2. runs data/pay/year.txt through pay_ref.py and compares its complete output with the register block the kernel printed AND with the file on the disk, byte for byte;
  3. recomputes the year totals per employee from the register (net, FIT, SS, MED, ADD) and compares them with Part 2's printed figures;
  4. re-checks the Social Security cap (employee 5: exactly 1,091,820 cents) and that no one's Social Security exceeds it;
  5. re-checks the tampered line and the final tally.
Usage: verify_052.py SERIAL_TXT DISK_IMG"""
import os, re, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__)); fails = 0
serial = open(sys.argv[1], errors="replace").read().replace("\r", ""); disk = sys.argv[2]
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
def money(c): s = f"{abs(c) // 100:,}.{abs(c) % 100:02d}"; return ("-" if c < 0 else "") + "$" + s
print("-- 1. the FAT16 volume the kernel wrote")
files = read_volume(disk); report(sorted(files) == ["PAYROLL.REG"], f"files on the disk: {', '.join(sorted(files))} ({len(files.get('PAYROLL.REG', b''))} bytes)")
print("-- 2. the whole year, kernel vs the Python reference")
ref = subprocess.run([sys.executable, os.path.join(here, "pay_ref.py"), os.path.join(here, "data/pay/year.txt")], capture_output=True, text=True).stdout
blk = re.search(r"@@PAY REGISTER BEGIN\n(.*?)@@PAY REGISTER END", serial, re.S).group(1)
report(blk == ref, f"the kernel's register block ({blk.count(chr(10))} lines, every cent of every withholding and the year-to-date) equals the Python reference's, line for line")
report(files["PAYROLL.REG"] == ref.encode(), "PAYROLL.REG on the disk equals the Python reference's output, byte for byte")
print("-- 3. the year totals, re-added from the register")
tot = {}; ss_cap = 1091820
for l in ref.splitlines():
    w = l.split(); i = int(w[2]); v = list(map(int, w[4:13])); t = tot.setdefault(i, [0, 0, 0, 0, 0, 0])
    t[0] += 1; t[1] += v[2]; t[2] += v[3]; t[3] += v[4]; t[4] += v[5]; t[5] += v[6]
shown = {int(m.group(1)): m.groups() for m in re.finditer(r"employee (\d+) \((\d+) pay periods\): gross \$([\d,.]+), income tax \$([\d,.]+), Social Security \$([\d,.]+), Medicare \$([\d,.]+), Additional Medicare \$([\d,.]+), 401\(k\)\+Section 125 \$([\d,.]+), net \$([\d,.]+)", serial)}
for i in sorted(tot):
    t = tot[i]; g = shown.get(i)
    ok = g is not None and int(g[1]) == t[0] and [money(t[1]), money(t[2]), money(t[3]), money(t[4]), money(t[5])] == ["$" + g[k] for k in (3, 4, 5, 6, 8)]
    report(ok, f"employee {i}: {t[0]} periods, income tax {money(t[1])}, SS {money(t[2])}, Medicare {money(t[3])}, Additional Medicare {money(t[4])}, net {money(t[5])}")
print("-- 4. the Social Security cap")
report(tot[5][2] == ss_cap, f"employee 5's Social Security total is {tot[5][2]} cents = 6.2% of $176,100 = {ss_cap}")
report(all(t[2] <= ss_cap for t in tot.values()), "no employee's Social Security exceeds the cap")
print("-- 5. the refusals, the tampered line and the tally")
for what in ("filing status is not S, MFJ or HOH", "pay periods per year must be 52, 26, 24 or 12", "an amount is negative or above", "deductions exceed gross pay"): report(what in serial, f"refusal printed: {what}")
first = ref.splitlines()[0].split(); net = int(first[10]); expect = int(first[5]) - sum(int(first[k]) for k in (6, 7, 8, 9))
report(net == expect and f"net {money(net + 1)}, but gross minus every withholding is {money(expect)} -> TAMPERING DETECTED" in serial, f"first line: net {money(net)} equals gross minus withholdings; the kernel caught net {money(net + 1)}")
m = re.search(r"payroll demo complete: (\d+) pay periods computed, (\d+) refused, (\d+) register lines audited, (\d+) attacks detected", serial)
report(m and tuple(map(int, m.groups())) == (ref.count("\n"), 0, ref.count("\n"), 6), f"final tally: {ref.count(chr(10))} pay periods computed, 0 refused, {ref.count(chr(10))} lines audited, 6 attacks detected")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
