#!/usr/bin/env python3
"""Chapter 50: the INDEPENDENT reference. Python's hashlib for SHA-256, Python's arbitrary-precision integers for the 256-bit target arithmetic, and its own parser, Merkle-tree builder and checks, written from Bitcoin's published rules
(the block and transaction formats, BIP 34, BIP 141/143 for the witness serialisation and the witness commitment, and the context-free checks of Bitcoin Core's CheckBlock/CheckTransaction), sharing no code with the kernel.
Usage:  btc_ref.py block FILE [pow_limit_bits_hex]   -> the canonical report text 050_btc.c's btc_report() prints
        btc_ref.py tx FILE                            -> 'check ok' or the reason, for one raw transaction in a file"""
import hashlib, struct, sys
MAX_MONEY = 21_000_000 * 100_000_000; MAX_BLOCK_WEIGHT = 4_000_000
def dsha(b): return hashlib.sha256(hashlib.sha256(b).digest()).digest()
def varint(b, p):
    n = b[p]
    if n < 0xfd: return n, p + 1
    if n == 0xfd: return struct.unpack_from("<H", b, p + 1)[0], p + 3
    if n == 0xfe: return struct.unpack_from("<I", b, p + 1)[0], p + 5
    return struct.unpack_from("<Q", b, p + 1)[0], p + 9
def parse_tx(b, p):
    """returns dict, new position. Raises ValueError on malformed data."""
    start = p
    if p + 10 > len(b): raise ValueError("short")
    ver = struct.unpack_from("<i", b, p)[0]; p += 4; wit = False
    if b[p] == 0 and p + 1 < len(b) and b[p + 1] != 0:
        if b[p + 1] != 1: raise ValueError("unknown flag")
        wit = True; p += 2
    vin_start = p; n, p = varint(b, p); ins = []
    for _ in range(n):
        if p + 36 > len(b): raise ValueError("short")
        h = b[p:p + 32]; i = struct.unpack_from("<I", b, p + 32)[0]; sl, q = varint(b, p + 36)
        if q + sl + 4 > len(b): raise ValueError("short")
        s = b[q:q + sl]; seq = struct.unpack_from("<I", b, q + sl)[0]; p = q + sl + 4; ins.append((h, i, s, seq))
    m, p = varint(b, p); outs = []
    for _ in range(m):
        if p + 8 > len(b): raise ValueError("short")
        v = struct.unpack_from("<q", b, p)[0]; sl, q = varint(b, p + 8)
        if q + sl > len(b): raise ValueError("short")
        outs.append((v, b[q:q + sl])); p = q + sl
    vout_end = p; wstacks = []
    if wit:
        for _ in range(n):
            c, p = varint(b, p); items = []
            for _ in range(c):
                l, p = varint(b, p)
                if p + l > len(b): raise ValueError("short")
                items.append(b[p:p + l]); p += l
            wstacks.append(items)
    if wit and not any(wstacks): raise ValueError("superfluous witness")
    if p + 4 > len(b): raise ValueError("short")
    lt = struct.unpack_from("<I", b, p)[0]; p += 4
    raw = b[start:p]; stripped = b[start:start + 4] + b[vin_start:vout_end] + b[p - 4:p] if wit else raw
    return dict(ver=ver, ins=ins, outs=outs, lt=lt, wit=wit, wstacks=wstacks, raw=raw, stripped=stripped, txid=dsha(stripped), wtxid=dsha(raw), size=len(raw), weight=3 * len(stripped) + len(raw)), p
def check_tx(t):
    if not t["ins"]: return "bad-txns-vin-empty"
    if not t["outs"]: return "bad-txns-vout-empty"
    if t["weight"] > MAX_BLOCK_WEIGHT: return "bad-txns-oversize"
    tot = 0
    for v, s in t["outs"]:
        if v < 0: return "bad-txns-vout-negative"
        if v > MAX_MONEY: return "bad-txns-vout-toolarge"
        tot += v
        if tot > MAX_MONEY: return "bad-txns-txouttotal-toolarge"
    if len({(h, i) for h, i, s, q in t["ins"]}) != len(t["ins"]): return "bad-txns-inputs-duplicate"
    if is_cb(t):
        if not 2 <= len(t["ins"][0][2]) <= 100: return "bad-cb-length"
    else:
        for h, i, s, q in t["ins"]:
            if h == b"\0" * 32 and i == 0xffffffff: return "bad-txns-prevout-null"
    return "ok"
def is_cb(t): return len(t["ins"]) == 1 and t["ins"][0][0] == b"\0" * 32 and t["ins"][0][1] == 0xffffffff
def compact(bits, limit_bits):
    """-> (target int or None, reason)"""
    size = bits >> 24; word = bits & 0x007fffff
    if size <= 3: t = word >> (8 * (3 - size))
    else: t = word << (8 * (size - 3))
    neg = word != 0 and bool(bits & 0x00800000); over = word != 0 and (size > 34 or (word > 0xff and size > 33) or (word > 0xffff and size > 32))
    if neg: return None, "negative"
    if over: return None, "overflow"
    if t == 0: return None, "zero"
    lt, _ = compact_nocheck(limit_bits)
    if t > lt: return None, "above the limit"
    return t, ""
def compact_nocheck(bits):
    size = bits >> 24; word = bits & 0x007fffff
    return (word >> (8 * (3 - size)) if size <= 3 else word << (8 * (size - 3))), ""
def merkle(hs):
    mutated = False; hs = list(hs)
    if not hs: return b"\0" * 32, False
    while len(hs) > 1:
        for i in range(0, len(hs) - 1, 2):
            if hs[i] == hs[i + 1]: mutated = True
        if len(hs) % 2: hs.append(hs[-1])
        hs = [dsha(hs[i] + hs[i + 1]) for i in range(0, len(hs), 2)]
    return hs[0], mutated
def bip34_height(script):
    if not script: return None
    n = script[0]
    if n == 0: return 0
    if 0x51 <= n <= 0x60: return n - 0x50
    if not 1 <= n <= 8 or len(script) < 1 + n: return None
    v = int.from_bytes(script[1:1 + n], "little")
    if script[n] & 0x80: v = -(v & ~(0x80 << (8 * (n - 1))))
    return v
def report(name, b, limit_bits=0x1d00ffff):
    o = []
    if len(b) < 81: return f"block {name} size {len(b)} unparseable too short\n"
    hdr = b[:80]; ver, prev, mr, tm, bits, nonce = struct.unpack("<I32s32sIII", hdr); h = dsha(hdr)
    try:
        n, p = varint(b, 80); txs = []
        for _ in range(n):
            t, p = parse_tx(b, p); txs.append(t)
        if p != len(b): raise ValueError("trailing bytes")
    except (ValueError, IndexError, struct.error) as e:
        return f"block {name} size {len(b)} unparseable {e}\n"
    size = len(b); bw = 3 * (80 + len(varint_bytes(len(txs))) + sum(len(t["stripped"]) for t in txs)) + size   # weight = 3 x (block size without witness data) + block size
    o.append(f"block {name} size {size} weight {bw} ntx {len(txs)}")
    o.append(f"  header hash {h[::-1].hex()} prev {prev[::-1].hex()} merkle {mr[::-1].hex()} version {ver:08x} time {tm} bits {bits:08x} nonce {nonce}")
    tgt, why = compact(bits, limit_bits)
    pow_ok = tgt is not None and int.from_bytes(h, "little") <= tgt
    o.append("  target " + (f"{tgt:064x}" if tgt is not None else f"invalid ({why})"))
    zb = 256 - int.from_bytes(h, "little").bit_length()
    o.append(f"  pow {'PASS' if pow_ok else 'FAIL'} zero_bits {zb}")
    root, mut = merkle([t["txid"] for t in txs]); mr_ok = root == mr
    o.append(f"  merkle {'PASS' if mr_ok else 'FAIL'} computed {root[::-1].hex()} mutated {'yes' if mut else 'no'}")
    first_cb = bool(txs) and is_cb(txs[0]); other_cb = any(is_cb(t) for t in txs[1:])
    height = bip34_height(txs[0]["ins"][0][2]) if first_cb and ver >= 2 else None
    o.append(f"  coinbase first {'yes' if first_cb else 'no'} others {'YES' if other_cb else 'no'} height {height if height is not None else 'none'}")
    wc = "none"
    if any(t["wit"] for t in txs) and first_cb:
        reserved = txs[0]["wstacks"][0][0] if txs[0]["wit"] and len(txs[0]["wstacks"][0]) == 1 and len(txs[0]["wstacks"][0][0]) == 32 else None
        idx = None
        for k, (v, s) in enumerate(txs[0]["outs"]):
            if len(s) >= 38 and s[:6] == bytes.fromhex("6a24aa21a9ed"): idx = k
        if idx is not None:
            wroot, _ = merkle([b"\0" * 32] + [t["wtxid"] for t in txs[1:]])
            wc = "PASS" if reserved is not None and dsha(wroot + reserved) == txs[0]["outs"][idx][1][6:38] else "FAIL"
    o.append(f"  witness {wc}")
    reason = None
    if not pow_ok: reason = "high-hash"
    elif not mr_ok: reason = "bad-txnmrklroot"
    elif mut: reason = "bad-txns-duplicate"
    elif not txs or bw > MAX_BLOCK_WEIGHT: reason = "bad-blk-length"
    elif not first_cb: reason = "bad-cb-missing"
    elif other_cb: reason = "bad-cb-multiple"
    for i, t in enumerate(txs):
        c = check_tx(t); o.append(f"  tx {i} txid {t['txid'][::-1].hex()} wtxid {t['wtxid'][::-1].hex()} in {len(t['ins'])} out {len(t['outs'])} out_sat {sum(v for v, s in t['outs'])} size {t['size']} weight {t['weight']} check {c}")
        if reason is None and c != "ok": reason = c
    if reason is None and wc == "FAIL": reason = "bad-witness-merkle-match"
    o.append("  verdict " + ("VALID" if reason is None else "INVALID " + reason))
    return "\n".join(o) + "\n"
def varint_bytes(n): return b"\x00" * (1 if n < 0xfd else 3 if n <= 0xffff else 5)   # only the LENGTH of the encoding matters here
if __name__ == "__main__":
    mode = sys.argv[1]; b = open(sys.argv[2], "rb").read()
    if mode == "block": sys.stdout.write(report(sys.argv[2].split("/")[-1], b, int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x1d00ffff))
    else:
        try: t, p = parse_tx(b, 0); print("check", check_tx(t) if p == len(b) else "trailing")
        except (ValueError, IndexError, struct.error) as e: print("unparseable", e)
