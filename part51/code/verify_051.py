#!/usr/bin/env python3
"""Chapter 51: verification FROM OUTSIDE THE KERNEL. It reads the serial capture, the FAT16 disk image and the original command files, and shares no code with the kernel (book_ref.py is a second matching engine, ITCH encoder/decoder and book rebuilder):
  1. reads the FAT16 volume with its own reader and takes DEMO.ITC and FLOW.ITC;
  2. replays data/book/demo.txt through the Python engine and compares its complete text output (every verdict, trade and feed message in hex, the book, its hash) with the block the kernel printed;
  3. replays data/book/flow.txt and compares the final book, hash and statistics with the kernel's, and the Python-built feed with the FLOW.ITC file on the disk BYTE FOR BYTE (and the same for DEMO.ITC);
  4. decodes each feed from the DISK with the Python subscriber and compares the rebuilt book's hash with the kernel's;
  5. re-checks the six attacks on the demo feed with the Python subscriber (same reason, same message number).
Usage: verify_051.py SERIAL_TXT DISK_IMG"""
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
ref = os.path.join(here, "book_ref.py")
def py(mode, path): return subprocess.run([sys.executable, ref, mode, path], capture_output=True, text=True).stdout
def feed_of(text): return b"".join(bytes.fromhex(l[5:]) for l in text.splitlines() if l.startswith("feed "))
print("-- 1. the FAT16 volume the kernel wrote")
files = read_volume(disk); report(sorted(files) == ["DEMO.ITC", "FLOW.ITC"], f"files on the disk: {', '.join(sorted(files))} ({', '.join(str(len(v)) for k, v in sorted(files.items()))} bytes)")
print("-- 2. the hand-written scenario: the whole canonical text, kernel vs the Python engine")
dm = re.search(r"@@BOOK DEMO BEGIN\n(.*?)@@BOOK DEMO END", serial, re.S).group(1); pd = py("run", os.path.join(here, "data/book/demo.txt"))
report(dm == pd, f"the kernel's block ({dm.count(chr(10))} lines: every verdict, trade, feed message in hex, the book, its hash, and the rebuilt-book check) equals the Python engine's, line for line")
print("-- 3. 1,200 commands of synthetic flow")
fl = re.search(r"@@BOOK FLOW BEGIN\n(.*?)@@BOOK FLOW END", serial, re.S).group(1); pf = py("run", os.path.join(here, "data/book/flow.txt")); tail = "\n".join(l for l in pf.splitlines() if not l.startswith(("cmd ", "trade ", "feed "))) + "\n"
report(fl == tail, "the kernel's final book, hash, statistics and rebuilt-book verdict equal the Python engine's")
report(files["FLOW.ITC"] == feed_of(pf), f"FLOW.ITC on the disk equals the feed the Python engine publishes, byte for byte ({len(files['FLOW.ITC'])} bytes)")
report(files["DEMO.ITC"] == feed_of(pd), f"DEMO.ITC on the disk equals the Python engine's feed, byte for byte ({len(files['DEMO.ITC'])} bytes)")
print("-- 4. each feed decoded FROM THE DISK by the independent subscriber")
for name, blk in (("DEMO.ITC", dm), ("FLOW.ITC", fl)):
    p = os.path.join(here, "build", name.lower()); open(p, "wb").write(files[name]); out = py("feed", p); hk = re.search(r"^hash (\w+)", blk, re.M).group(1); hp = re.search(r"^hash (\w+)", out, re.M)
    report(out.startswith("OK") and hp and hp.group(1) == hk, f"{name}: the Python subscriber rebuilds a book with hash {hk[:16]}..., the hash the kernel printed")
print("-- 5. the six attacks on the demo feed, re-run in Python")
feed = files["DEMO.ITC"]; cases = []
b = bytearray(feed); b[2] = ord("Z"); cases.append(bytes(b)); b = bytearray(feed); b[1] = 35; cases.append(bytes(b))
b = bytearray(feed); pos = 0
while b[pos + 2] != ord("E"): pos += 2 + int.from_bytes(b[pos:pos + 2], "big")
b[pos + 2 + 19] = 0xff; cases.append(bytes(b)); cases.append(feed[38:]); cases.append(feed[:-7])
b = bytearray(feed); pos = 0
while True:
    if b[pos + 2] == ord("A") and b[pos + 2 + 19] == ord("B"): b[pos + 2 + 32:pos + 2 + 36] = bytes([0, 0x20, 0, 0]); break
    pos += 2 + int.from_bytes(b[pos:pos + 2], "big")
cases.append(bytes(b)); lines = re.findall(r"-> REFUSED at message (\d+): (.*)", serial)
for k, c in enumerate(cases):
    p = os.path.join(here, "build", "atk.feed"); open(p, "wb").write(c); out = py("feed", p); m = re.match(r"REFUSED message (\d+): (.*)", out)
    report(m is not None and k < len(lines) and (m.group(1), m.group(2)) == lines[k], f"attack {k + 1}: Python says '{out.strip()}', the kernel printed '{lines[k][1] if k < len(lines) else '?'}' at message {lines[k][0] if k < len(lines) else '?'}")
m = re.search(r"order book demo complete: (\d+) scenario commands, (\d+) flow commands, (\d+) trades in the flow, (\d+) feed files written and read back identical, (\d+) damaged", serial)
trades = pf.count("\ntrade ") + (1 if pf.startswith("trade ") else 0)
report(m and tuple(map(int, m.groups())) == (21, 1200, trades, 2, 6), f"final tally: 21 scenario commands, 1200 flow commands, {trades} trades in the flow (counted in Python), 2 files written and read back, 6 damaged feeds refused")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
