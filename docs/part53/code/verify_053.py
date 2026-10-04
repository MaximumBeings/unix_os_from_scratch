#!/usr/bin/env python3
"""Chapter 53: verification FROM OUTSIDE THE KERNEL. It reads the serial capture and the FAT16 disk image and shares no code with the kernel (lsm_ref.py is a second implementation of the store written from the file formats):
  1. reads the FAT16 volume with its own reader;
  2. runs data/kv/demo.txt through the Python store and compares its complete result text (every put, delete and read, the digests, the statistics) with the block the kernel printed;
  3. replays the kernel's real-disk recovery steps in Python (flush, two puts, the log cut by 5 bytes, an orphan planted, reopen) and compares EVERY FILE on the disk (name and bytes) with the Python store's file system;
  4. opens the files FROM THE DISK with the Python store and checks that its digest is the one the kernel printed;
  5. re-runs the kernel's 40-operation crash sweep in Python (same generator, same crash points) and compares every figure.
Usage: verify_053.py SERIAL_TXT DISK_IMG"""
import os, re, struct, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import lsm_ref
fails = 0
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
print("-- 1. the FAT16 volume the kernel left")
files = read_volume(disk); report(True, "files on the disk: " + ", ".join(f"{k} ({len(v)} bytes)" for k, v in sorted(files.items())))
print("-- 2. the scripted workload: the whole result text, kernel vs the Python store")
blk = re.search(r"@@KV DEMO BEGIN\n(.*?)@@KV DEMO END", serial, re.S).group(1); script = os.path.join(here, "data/kv/demo.txt"); ref = lsm_ref.run(script).split("\n", 1)[1]
report(blk == ref, f"the kernel's block ({blk.count(chr(10))} lines: every result code, every value read, both digests and the statistics) equals the Python store's, line for line")
print("-- 3. the real-disk recovery, replayed in Python, and every file compared")
fs = lsm_ref.Fs(); L = lsm_ref.Lsm(fs)
for line in open(script):
    w = line.split()
    if not w: continue
    if w[0] == "put": L.put(lsm_ref.tok(w[1]), lsm_ref.tok(w[2]))
    elif w[0] == "del": L.delete(lsm_ref.tok(w[1]))
    elif w[0] == "flush": L.flush()
    elif w[0] == "compact": L.compact()
L2, text = lsm_ref.disk_crash_steps(fs, L); m = re.search(r"recovery: open (\w+), replayed (\d+) log records, cut (\d+) torn bytes, deleted (\d+) orphan files", serial)
rm = re.search(r"recovery: open (\w+), replayed (\d+) log records, cut (\d+) torn bytes, deleted (\d+) orphan files", text)
report(m and m.groups() == rm.groups(), f"recovery on the disk: kernel '{m.group(0) if m else '?'}' = Python '{rm.group(0)}'")
want = {k: v for k, v in fs.f.items() if v}; got = {k: v for k, v in files.items()}
report(sorted(want) == sorted(got), f"the same files exist on the disk and in the Python store's file system: {', '.join(sorted(want))}")
for k in sorted(want): report(want[k] == got.get(k), f"{k}: {len(want[k])} bytes, identical to the Python store's file byte for byte")
print("-- 4. the files opened FROM THE DISK by the Python store")
dfs = lsm_ref.Fs(); dfs.f = dict(files); L3 = lsm_ref.Lsm(dfs); d, n = L3.digest(); kf = re.search(r"@@KV FINAL (\w+) (\d+)", serial)
report(L3.rc == 0 and kf and (kf.group(1), int(kf.group(2))) == (d, n), f"opened by Python straight from the disk image: digest {d[:16]}... over {n} keys = the kernel's")
print("-- 5. the crash sweep, re-run in Python")
sw = lsm_ref.sweep(); km = re.search(r"workload: 40 operations \((\d+) puts and deletes\), total cost (\d+) units \(.*?\), (\d+) flushes, (\d+) compactions", serial)
kp = re.search(r"(\d+) crash points: (\d+) recovered to the acknowledged state, (\d+) to the acknowledged state plus the operation in flight, (\d+) failures; (\d+) had a torn log tail cut, (\d+) had orphan files deleted", serial)
total, fl, cp, logical, points, at_k, at_k1, nf, torn, orph = sw
report(km and tuple(map(int, km.groups())) == (logical, total, fl, cp), f"the workload: {logical} puts and deletes, cost {total}, {fl} flushes, {cp} compactions: same in the kernel and in Python")
report(kp and tuple(map(int, kp.groups())) == (points, at_k, at_k1, nf, torn, orph), f"{points} crash points: {at_k} recovered to the acknowledged state, {at_k1} to the state including the operation in flight, {nf} failures, {torn} torn log tails, {orph} orphan deletions: every figure equal")
t = re.search(r"LSM demo complete: (\d+) scripted commands, (\d+) disk recoveries, (\d+) crash points swept, (\d+) failures", serial)
report(t and tuple(map(int, t.groups())) == (133, 1, points, 0), f"final tally: 133 scripted commands, 1 disk recovery, {points} crash points swept, 0 failures")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
