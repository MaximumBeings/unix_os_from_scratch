#!/usr/bin/env python3
"""Chapter 50: verification FROM OUTSIDE THE KERNEL. It reads the serial capture and the FAT16 disk image, plus the original files, and shares no code with the kernel: btc_ref.py uses Python's hashlib for SHA-256 and Python's
unbounded integers for the 256-bit target arithmetic, with its own parser, Merkle builder and checks.
  1. reads the FAT16 volume with its own reader and takes the 24 reports V01.RPT ... V24.RPT the kernel wrote;
  2. recomputes the canonical report of each of the 24 embedded blocks and compares it with (a) the report the kernel printed and (b) the file on the disk;
  3. checks the ten testnet hashes against the hashes PUBLISHED in Bitcoin Core's own test data (blockfilters.json, if the Core checkout is present) and the genesis block against the hash everyone knows;
  4. recomputes Bitcoin Core's 214 transaction vectors and compares the counts and the nine refusals the kernel printed;
  5. reproduces each of the seven attacks in Python and compares the verdicts;
  6. re-renders the kernel's human-readable lines (hash, target, zero bits, BIP 34 height) from Python's numbers.
Usage: verify_050.py SERIAL_TXT DISK_IMG [BITCOIN_CORE_TEST_DATA_DIR]   (default /tmp/btc/src/test/data)"""
import json, os, re, struct, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import btc_ref as R
serial = open(sys.argv[1], errors="replace").read(); disk = sys.argv[2]; core = sys.argv[3] if len(sys.argv) > 3 else "/tmp/btc/src/test/data"; B = os.path.join(here, "data", "btc"); fails = 0
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
man = [l.split() for l in open(os.path.join(B, "MANIFEST.txt")) if l.split()[0] in ("testnet", "mainnet", "synthetic", "synthetic-dup")]
order = [(net, int(h), hs, fn[:-4], fn) for net, h, hs, fn in man]
print("-- 1. the FAT16 volume the kernel wrote")
files = read_volume(disk); want = ["V%02d.RPT" % i for i in range(1, 25)]
report(sorted(files) == want, f"24 report files on the disk: {want[0]} ... {want[-1]}")
print("-- 2. all 24 blocks recomputed by the independent reference (Python hashlib and big integers)")
canon = {}
for m in re.finditer(r"@@BLK (\S+) (\S+) BEGIN\n(.*?)@@BLK \1 END", serial, re.S): canon[m.group(1)] = (m.group(2), m.group(3))
report(len(canon) == 24, f"canonical blocks printed on the serial port: {len(canon)}")
refs = {}
for i, (net, h, hs, n, fn) in enumerate(order):
    lim = 0x207fffff if net.startswith("synthetic") else 0x1d00ffff; ref = R.report(n, open(os.path.join(B, fn), "rb").read(), lim); refs[n] = ref
    name, text = canon.get(n, ("", ""))
    report(text == ref and name == "V%02d.RPT" % (i + 1) and files.get(name, b"").decode() == ref, f"{n}: kernel's printed report == reference == {name} on the disk ({ref.count(chr(10))} lines, {ref.strip().splitlines()[-1].strip()})")
print("-- 3. the published hashes")
ok = True; pub = {}
if os.path.isdir(core):
    for r in json.load(open(os.path.join(core, "blockfilters.json")))[1:]: pub[r[0]] = r[1]
real = [(net, h, hs, n) for net, h, hs, n, fn in order if net in ("testnet", "mainnet")]
for net, h, hs, n in real:
    kernel_hash = re.search(r"header hash (\w+)", refs[n]).group(1); src = pub.get(h) if net == "testnet" and pub else hs
    good = kernel_hash == src and f"hash       {kernel_hash}\n    published  {src} -> MATCH" in serial.replace("\r", ""); ok = ok and good
report(ok and len(real) == 11, f"all 11 real blocks: the hash in the kernel's printout equals the hash {'published in Bitcoin Core test data (blockfilters.json)' if pub else 'in MANIFEST.txt'}, and the kernel printed MATCH")
report(re.search(r"header hash (\w+)", refs["mn_0"]).group(1) == "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f", "the mainnet genesis block's hash is 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f, the one everybody knows")
print("-- 4. Bitcoin Core's transaction vectors")
tv = open(os.path.join(B, "txvec.bin"), "rb").read(); i = 0; nv = ni = pv = cf = scr = 0; reasons = []
while i < len(tv):
    kind, exp, ln = tv[i], tv[i + 1], struct.unpack_from("<H", tv, i + 2)[0]; raw = tv[i + 4:i + 4 + ln]; i += 4 + ln; t, p = R.parse_tx(raw, 0); c = R.check_tx(t)
    if kind == 0: nv += 1; pv += c == "ok"
    else:
        ni += 1
        if c != "ok": cf += 1; reasons.append(c)
        else: scr += 1
report((nv, pv, ni, cf, scr) == (121, 121, 93, 9, 84), f"reference: {nv} valid vectors ({pv} pass), {ni} invalid ({cf} refused by CheckTransaction, {scr} script-level only)")
kr = re.findall(r"refused: (\S+)   txid", serial); report(kr == reasons, f"the nine refusals the kernel printed, in order, equal the reference's: {', '.join(reasons)}")
report(f"tx_valid.json: {nv} transactions, {pv} pass" in serial and f"{ni} transactions: {cf} refused" in serial and f"{scr} invalid only at the SCRIPT level" in serial, "the kernel's counts (121 / 121; 93: 9 refused, 84 script-level) equal the reference's")
print("-- 5. the seven attacks reproduced in Python")
sec = serial[serial.index("== Part 4. Seven attacks"):serial.index("== Part 5.")]; parts = {int(m.group(1)): m.group(0) for m in re.finditer(r"Attack (\d+):.*?(?=\n  Attack \d+:|\Z)", sec, re.S)}
rd = lambda n: open(os.path.join(B, n + ".blk"), "rb").read()
def verdict(b, lim=0x1d00ffff):
    t = R.report("x", b, lim); return t.strip().splitlines()[-1].replace("  verdict ", "").replace("INVALID ", "")
b = bytearray(rd("tn_180480")); b[76] ^= 1; v1 = verdict(bytes(b)); b[76] ^= 1; b[90] ^= 1; v2 = verdict(bytes(b))
report(v1 == "high-hash" and "verdict: high-hash" in parts[1], f"attack 1 (nonce bit): Python says {v1}, the kernel printed high-hash")
report(v2 == "bad-txnmrklroot" and "verdict: bad-txnmrklroot" in parts[2], f"attack 2 (a bit of the coinbase): Python says {v2}, the kernel printed bad-txnmrklroot")
h2 = R.dsha(rd("syn_2")[:80]); prev4 = rd("syn_4")[4:36]
report(prev4 != h2 and "REFUSED: not a child, the chain is broken" in parts[3], "attack 3: block 4's previous-hash field is not block 2's hash (Python agrees), the kernel refused")
vd = verdict(rd("syn_dup"), 0x207fffff); report(vd == "bad-txns-duplicate" and "yet the block is refused: bad-txns-duplicate" in parts[4], f"attack 4 (repeated transaction): the Merkle root matches, Python says {vd}, the kernel printed the same")
blk = bytearray(rd("tn_926485")); n, p = R.varint(bytes(blk), 80); cb, _ = R.parse_tx(bytes(blk), p); off = p + 4 + 2 + (1 + 36 + 1 + len(cb["ins"][0][2]) + 4) + 1 + sum(8 + 1 + len(s) for v, s in cb["outs"]) + 1 + 1   # start + version + marker/flag + inputs + output count + outputs + witness count + item length
assert bytes(blk[off:off + 32]) == cb["wstacks"][0][0], "offset"
blk[off] ^= 1; v5 = verdict(bytes(blk)); report(v5 == "bad-witness-merkle-match" and "verdict: bad-witness-merkle-match" in parts[5], f"attack 5 (witness reserved value): Python says {v5}, the kernel printed the same")
v6 = R.report("x", rd("tn_49291")[:500]); report("unparseable" in v6 and "verdict: the data ends inside a structure" in parts[6], "attack 6 (truncated at 500 bytes): Python refuses it as unparseable too")
v7 = verdict(rd("tn_49291"), 0x1c00ffff); report(v7 == "high-hash" and "verdict: high-hash" in parts[7], f"attack 7 (harder limit than it was mined under): Python says {v7}")
print("-- 6. human-readable lines re-rendered from Python's numbers")
bad = 0
for net, h, hs, n in real:
    ref = refs[n]; tg = re.search(r"target (\w+)", ref).group(1); zb = re.search(r"zero_bits (\d+)", ref).group(1); hh = re.search(r"height (\S+)$", re.search(r"coinbase first.*", ref).group(0)).group(1)
    for needle in (f"    target     {tg}\n", f"proof of work: PASS ({zb} leading zero bits in the hash)"):
        if needle not in serial: bad += 1; print("     missing:", needle.strip())
    if hh != "none" and f"coinbase height (BIP 34): {hh} -- the block's published height is {h}: MATCH" not in serial: bad += 1; print("     missing height", n)
report(bad == 0, "the printed target, leading-zero-bit count and BIP 34 height of all 11 real blocks equal the reference's")
m = re.search(r"bitcoin demo complete: (\d+) real blocks validated, (\d+) published hashes matched, (\d+) valid blocks in all, (\d+) transaction vectors checked, (\d+) attacks refused, (\d+) report files", serial)
report(m and tuple(map(int, m.groups())) == (11, 11, 23, 214, 7, 24), "final tally: 11 real blocks, 11 published hashes matched, 23 valid blocks, 214 transaction vectors, 7 attacks, 24 report files")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
