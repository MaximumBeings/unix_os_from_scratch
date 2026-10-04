"""Chapter 47: the independent Python side, shared by verify_047.py and crash_test.py: a reader for the FAT16 volume the kernel leaves on the disk image, a decoder for the write-ahead-log records
(CRC-32 by Python's zlib), and an independent marketplace model (auction by literal step-by-step simulation, ledger, orders, idempotency table, canonical state hash). It shares no code with the kernel."""
import hashlib, re, struct, zlib

def read_volume(disk_path):
    """Parse the FAT16 volume on a raw disk image. Returns (geometry dict, {filename: bytes}). Tolerates a damaged volume: unreadable entries are skipped."""
    disk = open(disk_path, "rb").read()
    bps = struct.unpack_from("<H", disk, 11)[0]; spc = disk[13]; reserved = struct.unpack_from("<H", disk, 14)[0]; nfats = disk[16]
    root_entries = struct.unpack_from("<H", disk, 17)[0]; fat_size = struct.unpack_from("<H", disk, 22)[0]
    fat_lba, root_lba = reserved, reserved + nfats * fat_size
    data_lba = root_lba + (root_entries * 32 + bps - 1) // bps
    def fat_next(c): return struct.unpack_from("<H", disk, fat_lba * bps + 2 * c)[0]
    def read_chain(first, size):
        out = b""; c = first; guard = 0
        while 2 <= c < 0xFFF8 and len(out) < size and guard < 100000:
            lo = (data_lba + (c - 2) * spc) * bps; out += disk[lo:lo + spc * bps]; c = fat_next(c); guard += 1
        return out[:size]
    files = {}
    for i in range(root_entries):
        e = disk[root_lba * bps + 32 * i: root_lba * bps + 32 * i + 32]
        if len(e) < 32 or e[0] in (0, 0xE5) or e[11] & 0x18: continue
        try: name = (e[0:8].decode().rstrip() + "." + e[8:11].decode().rstrip()).rstrip(".")
        except UnicodeDecodeError: continue
        files[name] = read_chain(struct.unpack_from("<H", e, 26)[0], struct.unpack_from("<I", e, 28)[0])
    return dict(bps=bps, spc=spc, nfats=nfats, fat_size=fat_size, root_lba=root_lba, data_lba=data_lba), files

def decode_log(files):
    """Walk W0000001.LOG, W0000002.LOG, ... in order. Returns (commands, stop_reason, n_files): commands are the decoded commands of the VALID prefix; stop_reason is None if every file present decoded,
    else a description of the first bad or missing record (replay must stop there, exactly as the kernel's does)."""
    names = sorted(n for n in files if re.fullmatch(r"W\d{7}\.LOG", n)); cmds = []; k = 1
    while True:
        n = "W%07d.LOG" % k
        if n not in files: return cmds, ("log ends" if k > len(names) else "missing record %d (later files exist)" % k), len(names)
        b = files[n]
        if len(b) != 108: return cmds, f"record {k}: wrong length {len(b)} (short or torn)", len(names)
        if b[0:4] != b"WAL1": return cmds, f"record {k}: bad magic", len(names)
        if struct.unpack_from("<I", b, 8)[0] != 92: return cmds, f"record {k}: bad payload length", len(names)
        if struct.unpack_from("<I", b, 104)[0] != (zlib.crc32(b[:104]) & 0xFFFFFFFF): return cmds, f"record {k}: CRC-32 mismatch", len(names)
        if struct.unpack_from("<I", b, 4)[0] != k: return cmds, f"record {k}: wrong sequence number", len(names)
        p = b[12:104]; t, now, idem = struct.unpack_from("<3I", p, 0); a = list(struct.unpack_from("<8I", p, 12)); title = p[44:92].split(b"\0")[0].decode(errors="replace")
        cmds.append(dict(type=t, now=now, idem=idem, a=a, title=title)); k += 1

INC = [(0, 5), (100, 25), (500, 50), (2500, 100), (10000, 250), (25000, 500), (50000, 1000), (100000, 2500), (250000, 5000), (500000, 10000)]
def inc(p):
    r = INC[0][1]
    for f, i in INC:
        if p >= f: r = i
    return r
M32 = 0xFFFFFFFF
class Market:
    def __init__(s): s.listings = [None] * 8; s.bal = [0] * 12; s.ntx = 0; s.orders = [None] * 8; s.idem = [None] * 24; s.applied = 0
    # --- ledger ---
    def transfer(s, f, t, c):
        if f >= 12 or t >= 12 or f == t: return 2
        if c == 0 or c > 100000000: return 3
        if f != 0 and s.bal[f] < c: return 1
        s.bal[f] -= c; s.bal[t] += c; s.ntx += 1; return 0
    # --- auction (literal step-by-step process, not a closed form) ---
    def L(s, i):
        for l in s.listings:
            if l and l["id"] == i: return l
    def min_bid(s, l): return l["start"] if l["bid_count"] == 0 else l["price"] + inc(l["price"])
    def reserve_met(s, l): return l["reserve"] == 0 or (l["bid_count"] > 0 and l["high_max"] >= l["reserve"])
    def create(s, a, now, title):
        id_, seller, start, reserve, dur, ew, es = a[:7]
        if start == 0 or dur == 0 or id_ == 0 or seller == 0 or (reserve and reserve < start): return 9
        if s.L(id_): return 8
        for k in range(8):
            if s.listings[k] is None:
                s.listings[k] = dict(id=id_, seller=seller, title=title, start=start, reserve=reserve, st=now, end=now + dur, ew=ew, es=es, status=0, price=start, high=0, high_max=0, bid_count=0, bid_seq=0, proxies=[])
                return 0
        return 6
    def bid(s, a, now):
        l = s.L(a[0])
        if not l: return 1, 0, 0
        who, m = a[1], a[2]
        if who == 0 or m == 0: return 9, 0, 0
        if l["status"] != 0 or now >= l["end"]: return 2, 0, 0
        if who == l["seller"]: return 3, 0, 0
        own = next((p for p in l["proxies"] if p[0] == who), None)
        if own:
            if m <= own[1]: return 5, 0, 0
            if who != l["high"] and m < s.min_bid(l): return 4, 0, 0
        else:
            if m < s.min_bid(l): return 4, 0, 0
            if len(l["proxies"]) >= 8: return 6, 0, 0
        l["bid_seq"] += 1; seq = l["bid_seq"]
        if own: l["proxies"] = [(w, m, seq) if w == who else (w, x, q) for (w, x, q) in l["proxies"]]
        else: l["proxies"].append((who, m, seq))
        l["bid_count"] += 1
        # literal process: eBay bids again for the leader up to their maximum, one increment above the challenger
        prox = {w: (x, q) for (w, x, q) in l["proxies"]}
        if l["high"] == 0:
            l["high"], l["high_max"], l["price"] = who, m, l["start"]
        elif who == l["high"]:
            l["high_max"] = m; others = [x for w, (x, q) in prox.items() if w != who]
            if others: sec = max(others); l["price"] = max(l["price"], min(m, sec + inc(sec)))
        elif m > l["high_max"]:
            old = l["high_max"]; l["high"], l["high_max"] = who, m; l["price"] = min(m, old + inc(old))
        elif m == l["high_max"]:
            if prox[who][1] < prox[l["high"]][1]: l["high"] = who
            l["price"] = m
        else:
            l["price"] = min(l["high_max"], m + inc(m))
        if l["reserve"] and l["high_max"] >= l["reserve"] and l["price"] < l["reserve"]: l["price"] = l["reserve"]
        if l["es"] and now + l["ew"] >= l["end"] and now + l["es"] > l["end"]: l["end"] = now + l["es"]
        return 0, l["price"], 1 if l["high"] == who else 0
    def close(s, a, now):
        l = s.L(a[0])
        if not l: return 1, 0
        if l["status"] != 0: return 0, l["status"]
        if now < l["end"]: return 7, l["status"]
        l["status"] = 3 if l["bid_count"] == 0 else (2 if not s.reserve_met(l) else 1)
        return 0, l["status"]
    def order(s, listing):
        return next((o for o in s.orders if o and o["listing"] == listing), None)
    # --- one command ---
    def do(s, c):
        t, a, now = c["type"], c["a"], c["now"]; code = v1 = v2 = 0
        if t == 1: code = s.create(a, now, c["title"]); v1 = a[0]
        elif t == 2: code, v1, v2 = s.bid(a, now)
        elif t == 3: code, v1 = s.close(a, now)
        elif t == 4:
            rc = s.transfer(0, a[0], a[1]) if a[0] >= 3 else 2; code = 0 if rc == 0 else (29 if rc == 1 else 10 + rc); v1 = (s.bal[a[0]] & M32) if rc == 0 else 0
        elif t == 5:
            l = s.L(a[0])
            if not l: code = 1
            elif l["status"] != 1: code = 20
            elif a[1] != l["high"]: code = 21
            elif s.order(l["id"]): code = 24
            else:
                slot = next((i for i, o in enumerate(s.orders) if o is None), None)
                if slot is None: code = 28
                else:
                    total = l["price"] + a[2]; rc = s.transfer(a[1], 1, total)
                    if rc: code = 29 if rc == 1 else 10 + rc
                    else: s.orders[slot] = dict(id=l["id"], listing=l["id"], buyer=a[1], seller=l["seller"], item=l["price"], ship=a[2], total=total, fee=0, status=1); v1 = total
        elif t == 6:
            o = s.order(a[0])
            if not o: code = 22
            elif o["status"] != 1: code = 23
            else:
                fee = min((o["total"] * 10 + 50) // 100 + 30, o["total"]); s.transfer(1, 2, fee)
                if o["total"] - fee > 0: s.transfer(1, o["seller"], o["total"] - fee)
                o["fee"], o["status"] = fee, 2; v1, v2 = o["total"] - fee, fee
        elif t == 7:
            o = s.order(a[0])
            if not o: code = 22
            elif o["status"] != 1: code = 23
            else: s.transfer(1, o["buyer"], o["total"]); o["status"] = 3; v1 = o["total"]
        return code, v1, v2
    def fp(s, c):
        h = 2166136261
        for w in [c["type"]] + c["a"]:
            for b in range(4): h ^= (w >> (8 * b)) & 0xFF; h = (h * 16777619) & M32
        return h
    def apply(s, c):
        replayed = False
        if c["type"] in (2, 4, 5) and c["idem"]:
            fp = s.fp(c); hit = next((e for e in s.idem if e and e["key"] == c["idem"]), None)
            if hit:
                s.applied += 1
                return (25, 0, 0, False) if hit["fp"] != fp else (hit["code"], hit["v1"], hit["v2"], True)
            slot = next((i for i, e in enumerate(s.idem) if e is None), None)
            if slot is None: s.applied += 1; return 26, 0, 0, False
            code, v1, v2 = s.do(c); s.idem[slot] = dict(key=c["idem"], fp=fp, code=code, v1=v1, v2=v2)
        else: code, v1, v2 = s.do(c)
        s.applied += 1; s.check(); return code, v1, v2, replayed
    def check(s):
        assert sum(s.bal) == 0, "ledger does not sum to zero"
        assert s.bal[1] == sum(o["total"] for o in s.orders if o and o["status"] == 1), "escrow != PAID orders"
        assert all(b >= 0 for b in s.bal[1:]), "negative account"
        for l in s.listings:
            if l:
                assert l["price"] >= l["start"], "price below start"
                if l["proxies"]: assert l["high_max"] == max(p[1] for p in l["proxies"]) and l["price"] <= l["high_max"], "leader rule"
                assert not (l["status"] == 1 and l["bid_count"] == 0), "sold without bids"
    def hash(s):
        h = hashlib.sha256(); w = lambda v: h.update(struct.pack("<I", v & M32))
        for l in s.listings:
            if not l: w(0); continue
            w(1); w(l["id"]); w(l["seller"]); h.update(l["title"].encode().ljust(48, b"\0"))
            for k in ("start", "reserve", "st", "end", "ew", "es", "status", "price", "high", "high_max", "bid_count", "bid_seq"): w(l[k])
            w(len(l["proxies"]))
            for (b, m, q) in l["proxies"]: w(b); w(m); w(q)
        for b in s.bal: w(b)
        w(s.ntx)
        for o in s.orders:
            if not o: w(0); continue
            w(1)
            for k in ("id", "listing", "buyer", "seller", "item", "ship", "total", "fee", "status"): w(o[k])
        for e in s.idem:
            if not e: w(0); continue
            w(1); w(e["key"]); w(e["fp"]); w(e["code"]); w(e["v1"]); w(e["v2"])
        w(s.applied); return h.digest()

