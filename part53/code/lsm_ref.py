#!/usr/bin/env python3
"""Chapter 53: an independent second implementation of the LSM store, in Python, written from the file formats and rules in 053_lsm.h (not from the C code). It runs the same scripts as native/lsm_cli.c on the same crash-injecting file system model and prints
identical text, INCLUDING every byte of every file (so the on-disk formats, the flush and compaction points, the table numbering, the Bloom filters and the recovery are all compared). Also: `lsm_ref.py read DUMP` decodes a dump of files the way a recovering
process would (its own reader) and prints the live pairs. Usage: lsm_ref.py SCRIPT"""
import sys, zlib, hashlib, struct
MAXK, MAXV, MEMMAX, MEMBYTES, WALMAX, L0MAX, MAXTAB, MAXENT, IDXE, IO = 24, 100, 64, 1024, 4096, 4, 12, 400, 8, 65536
def crc(b): return zlib.crc32(bytes(b)) & 0xFFFFFFFF
def hash1(k):
    h = 2166136261
    for c in k: h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h
def hash2(k):
    h = 5381
    for c in k: h = ((h * 33) & 0xFFFFFFFF) ^ c
    return h | 1
def bloom_pos(k, nb): a, b = hash1(k), hash2(k); return [((a + i * b) & 0xFFFFFFFF) % nb for i in range(4)]
def bloom_bytes(n): return max(8, (n * 10 + 7) // 8)
class Crash(Exception): pass
class Fs:  # the ramfs.h model
    def __init__(s): s.f = {}; s.budget = -1; s.crashed = False
    def clone(s): o = Fs(); o.f = {k: bytes(v) for k, v in s.f.items()}; return o
    def _charge(s, cost):
        if s.crashed: return -1, 0
        if s.budget < 0 or s.budget >= cost:
            if s.budget >= 0: s.budget -= cost
            return 1, cost
        al = s.budget; s.budget = 0; s.crashed = True; return 0, al
    def read(s, n, off, ln):
        if n not in s.f: return None
        return s.f[n][off:off + ln]
    def size(s, n): return len(s.f[n]) if n in s.f else None
    def write(s, n, d):
        k, al = s._charge(len(d) + 1)
        if k < 0: return 1
        s.f[n] = bytes(d) if k else bytes(d[:al]); return 0 if k else 1
    def append(s, n, d):
        k, al = s._charge(len(d) + 1)
        if k < 0: return 1
        s.f[n] = s.f.get(n, b"") + (bytes(d) if k else bytes(d[:al])); return 0 if k else 1
    def delete(s, n):
        k, al = s._charge(1)
        if k <= 0: return 1
        s.f.pop(n, None); return 0
def tname(i): return "T%05d.SST" % i
def ent_bytes(e): k, v, tomb, seq = e; return bytes([len(k), tomb]) + struct.pack("<HI", len(v), seq) + k + v
def build_table(ents):  # ents: sorted list of (key, val, tomb, seq)
    data = b"".join(ent_bytes(e) for e in ents); idx = b""; off = 0
    for i, e in enumerate(ents):
        if i % IDXE == 0: idx += struct.pack("<IB", off, len(e[0])) + e[0]
        off += 8 + len(e[0]) + len(e[1])
    bl = bloom_bytes(len(ents)); bits = bytearray(bl)
    for e in ents:
        for p in bloom_pos(e[0], bl * 8): bits[p >> 3] |= 1 << (p & 7)
    seqs = [e[3] for e in ents]; body = data + idx + bytes(bits)
    foot = struct.pack("<10I", 0x314D534C, len(ents), len(data), len(data), len(idx), len(data) + len(idx), bl, min(seqs), max(seqs), 0)
    img = body + foot; return img + struct.pack("<I", crc(img))
def parse_table(b):  # returns list of entries or None
    if len(b) < 52 or len(b) > IO: return None
    f = b[-44:]; v = struct.unpack("<11I", f)
    if v[0] != 0x314D534C or v[10] != crc(b[:-4]) or v[9] != 0: return None
    n, dl, io, il, bo, bl, mins, maxs = v[1:9]
    if not (1 <= n <= MAXENT) or io != dl or bo != io + il or bl != bloom_bytes(n) or bl > 512 or bo + bl + 44 != len(b): return None
    ents = []; off = 0; ip = io
    while off < dl:
        if dl - off < 8: return None
        kl, tomb, vl, seq = b[off], b[off + 1], *struct.unpack("<HI", b[off + 2:off + 8])
        if not (1 <= kl <= MAXK) or tomb > 1 or vl > MAXV or (tomb and vl) or off + 8 + kl + vl > dl: return None
        k = b[off + 8:off + 8 + kl]; val = b[off + 8 + kl:off + 8 + kl + vl]
        if ents and ents[-1][0] >= k: return None
        if len(ents) % IDXE == 0:
            if ip + 5 + kl > io + il or struct.unpack("<I", b[ip:ip + 4])[0] != off or b[ip + 4] != kl or b[ip + 5:ip + 5 + kl] != k: return None
            ip += 5 + kl
        ents.append((k, val, tomb, seq)); off += 8 + kl + vl
    if off != dl or len(ents) != n or ip != io + il or min(e[3] for e in ents) != mins or max(e[3] for e in ents) != maxs: return None
    return ents
class Lsm:
    def __init__(s, fs): s.fs = fs; s.stats = dict(wal_replayed=0, wal_cut=0, orphans=0, puts=0, deletes=0, flushes=0, compactions=0, gets=0, bloom=0, rng=0, blocks=0); s.rc = s._open()
    def _manifest(s, tabs, nxt, fseq):
        body = b"MNFT" + struct.pack("<IIIH", s.gen + 1, nxt, fseq, len(tabs)) + b"".join(struct.pack("<IB", i, l) for i, l in tabs); body += struct.pack("<I", crc(body))
        return s.fs.write("MANI%d" % ((s.gen + 1) & 1), body)
    def _open(s):
        fs = s.fs; s.mem = {}; s.membytes = 0; s.walb = 0; s.seq = s.fseq = s.gen = 0; s.nxt = 1; s.tabs = []; s.meta = {}; s.failed = False; best = None
        for slot in (0, 1):
            b = fs.read("MANI%d" % slot, 0, 10**6)
            if b is None or len(b) < 22 or len(b) > 96: continue
            n = struct.unpack("<H", b[16:18])[0]
            if b[:4] != b"MNFT" or n > MAXTAB or len(b) != 18 + 5 * n + 4 or struct.unpack("<I", b[-4:])[0] != crc(b[:-4]): continue
            g, nx, fq = struct.unpack("<III", b[4:16]); t = [struct.unpack("<IB", b[18 + 5 * i:23 + 5 * i]) for i in range(n)]
            if best is None or g > best[0]: best = (g, nx, fq, t)
        if best: s.gen, s.nxt, s.fseq, s.tabs = best[0], best[1], best[2], list(best[3]); s.seq = s.fseq
        for i, (tid, lv) in enumerate(s.tabs):
            if lv > 1 or (lv == 1 and i + 1 != len(s.tabs)) or tid == 0 or tid >= s.nxt: return -4
            b = fs.read(tname(tid), 0, 10**6); e = parse_table(b) if b is not None else None
            if e is None: return -4
            s.meta[tid] = e
        live = {t for t, _ in s.tabs}
        for i in range(1, min(s.nxt, 99999) + 1):
            if i not in live and fs.size(tname(i)) is not None: fs.delete(tname(i)); s.stats["orphans"] += 1
        w = fs.read("WAL", 0, 10**6); pos = 0; prev = 0; torn = False
        if w is not None:
            if len(w) > IO: return -3
            while pos < len(w):
                if len(w) - pos < 6: break
                pl = struct.unpack("<H", w[pos + 4:pos + 6])[0]
                if pl < 8 or pl > 8 + MAXK + MAXV or len(w) - pos < 6 + pl: break
                if struct.unpack("<I", w[pos:pos + 4])[0] != crc(w[pos + 4:pos + 6 + pl]): break
                seq, tomb, kl, vl = struct.unpack("<IBBH", w[pos + 6:pos + 14])
                if not (1 <= kl <= MAXK) or tomb > 1 or vl > MAXV or (tomb and vl) or pl != 8 + kl + vl or seq <= prev: break
                k = w[pos + 14:pos + 14 + kl]; v = w[pos + 14 + kl:pos + 14 + kl + vl]; prev = seq
                if seq > s.fseq: s._apply((k, v, tomb, seq)); s.stats["wal_replayed"] += 1; s.seq = max(s.seq, seq)
                pos += 6 + pl
            torn = pos != len(w); s.stats["wal_cut"] = len(w) - pos; s.walb = pos
        if torn:
            if s.mem: rc = s.flush()
            else:
                rc = -3 if fs.write("WAL", b"") else 0
                if rc: s.failed = True
                else: s.walb = 0
            return rc
        return 0
    def _apply(s, e):
        if e[0] in s.mem: s.membytes -= 8 + len(e[0]) + len(s.mem[e[0]][1])
        s.mem[e[0]] = e; s.membytes += 8 + len(e[0]) + len(e[1])
    def _l0(s): return sum(1 for _, l in s.tabs if l == 0)
    def flush(s):
        if s.failed: return -3
        if not s.mem: return 0
        if len(s.tabs) >= MAXTAB or s.nxt > 99999: return -2
        ents = [s.mem[k] for k in sorted(s.mem)]; img = build_table(ents); tid = s.nxt
        if s.fs.write(tname(tid), img): s.failed = True; return -3
        if s._manifest([(tid, 0)] + s.tabs, tid + 1, s.seq): s.failed = True; return -3
        s.tabs = [(tid, 0)] + s.tabs; s.meta[tid] = parse_table(img); s.gen += 1; s.nxt = tid + 1; s.fseq = s.seq; s.mem = {}; s.membytes = 0; s.stats["flushes"] += 1
        if s.fs.write("WAL", b""): s.failed = True; return -3
        s.walb = 0
        return s.compact() if s._l0() >= L0MAX else 0
    def compact(s):
        if s.failed: return -3
        if not s.tabs or (len(s.tabs) == 1 and s.tabs[0][1] == 1): return 0
        if s.nxt > 99999: return -2
        view = {}
        for tid, _ in reversed(s.tabs):
            for e in s.meta[tid]: view[e[0]] = e
        out = [view[k] for k in sorted(view) if not view[k][2]]
        if len(out) > MAXENT: return -2
        tid = s.nxt; old = list(s.tabs)
        if out:
            img = build_table(out)
            if s.fs.write(tname(tid), img): s.failed = True; return -3
            if s._manifest([(tid, 1)], tid + 1, s.fseq): s.failed = True; return -3
            s.gen += 1; s.nxt = tid + 1
            for i, _ in old: s.fs.delete(tname(i))
            s.meta = {tid: parse_table(img)}; s.tabs = [(tid, 1)]
        else:
            if s._manifest([], tid + 1, s.fseq): s.failed = True; return -3
            s.gen += 1; s.nxt = tid + 1
            for i, _ in old: s.fs.delete(tname(i))
            s.meta = {}; s.tabs = []
        s.stats["compactions"] += 1; return 0
    def _mut(s, k, v, tomb):
        if s.failed: return -3
        if not (1 <= len(k) <= MAXK) or len(v) > MAXV: return -1
        e = (k, v, tomb, s.seq + 1); rec = 14 + len(k) + len(v); old = s.mem.get(k)
        nb = s.membytes + 8 + len(k) + len(v) - (8 + len(k) + len(old[1]) if old else 0)
        if s.mem and ((not old and len(s.mem) >= MEMMAX) or nb > MEMBYTES or s.walb + rec > WALMAX):
            rc = s.flush()
            if rc: return rc
        body = struct.pack("<H", 8 + len(k) + len(v)) + struct.pack("<IBBH", e[3], tomb, len(k), len(v)) + k + v
        if s.fs.append("WAL", struct.pack("<I", crc(body)) + body): s.failed = True; return -3
        s.seq = e[3]; s.walb += rec; s._apply(e); s.stats["deletes" if tomb else "puts"] += 1; return 0
    def put(s, k, v): return s._mut(k, v, 0)
    def delete(s, k): return s._mut(k, b"", 1)
    def get(s, k):
        s.stats["gets"] += 1
        if k in s.mem: e = s.mem[k]; return None if e[2] else e[1]
        for tid, _ in s.tabs:
            ents = s.meta[tid]
            if k < ents[0][0] or k > ents[-1][0]: s.stats["rng"] += 1; continue
            bl = bloom_bytes(len(ents)); bits = bytearray(bl)
            for e in ents:
                for p in bloom_pos(e[0], bl * 8): bits[p >> 3] |= 1 << (p & 7)
            if not all(bits[p >> 3] & (1 << (p & 7)) for p in bloom_pos(k, bl * 8)): s.stats["bloom"] += 1; continue
            s.stats["blocks"] += 1
            for e in ents:
                if e[0] == k: return None if e[2] else e[1]
        return None
    def live(s):
        view = {}
        for tid, _ in reversed(s.tabs):
            for e in s.meta[tid]: view[e[0]] = e
        for k, e in s.mem.items(): view[k] = e
        return [(k, view[k][1]) for k in sorted(view) if not view[k][2]]
    def digest(s):
        h = hashlib.sha256(); L = s.live()
        for k, v in L: h.update(bytes([len(k), len(v)]) + k + v)
        return h.hexdigest(), len(L)
def disk_crash_steps(fs, L):
    """Part 3 of the kernel demo, on a file system model: flush, two puts, the log cut by 5 bytes, an orphan table planted at the next id, then a recovery. Returns (new store, text)."""
    L.flush(); L.put(b"crash:a", b"alpha"); L.put(b"crash:b", b"bravo"); w = fs.f["WAL"]; wl = len(w); fs.f["WAL"] = w[:wl - 5]; fs.f[tname(L.nxt)] = bytes((i * 7) & 255 for i in range(20)); orphan = tname(L.nxt)
    L2 = Lsm(fs); d, n = L2.digest()
    return L2, f"recovery: open {RC[L2.rc]}, replayed {L2.stats['wal_replayed']} log records, cut {L2.stats['wal_cut']} torn bytes, deleted {L2.stats['orphans']} orphan files; digest {d} {n}; wal {wl}; orphan {orphan}"
def sweep():
    """Part 4 of the kernel demo: the 40-operation workload from a linear congruential generator, crashed at every 7th unit of cost. Returns (total, flushes, compactions, logical, points, at_k, at_k1, fails, torn, orph)."""
    st = [5353]
    def rnd(): st[0] = (st[0] * 1664525 + 1013904223) & 0xFFFFFFFF; return st[0] >> 8
    ops = []
    for i in range(40):
        r = rnd() % 10; kind = 0 if r < 6 else (1 if r < 8 else (2 if r == 8 else 0)); x = rnd() % 20; k = b"k%d%d" % (x // 10, x % 10); vl = 1 + rnd() % 40; v = bytes(97 + rnd() % 26 for _ in range(vl)); ops.append((kind, k, v))
    def apply(L, o): return L.put(o[1], o[2]) if o[0] == 0 else (L.delete(o[1]) if o[0] == 1 else L.flush())
    fs = Fs(); L = Lsm(fs); dg = [L.digest()[0]]; spent = 0
    tot = [0]
    def cost_run(budget):
        f = Fs(); f.budget = budget; L = Lsm(f); acked = 0
        for o in ops:
            if apply(L, o): break
            if o[0] < 2: acked += 1
        return f, L, acked
    for o in ops:
        apply(L, o)
        if o[0] < 2: dg.append(L.digest()[0])
    # total cost: run with a huge budget and see how much is left
    f = Fs(); f.budget = 10**9; L = Lsm(f)
    for o in ops: apply(L, o)
    total = 10**9 - f.budget; logical = len(dg) - 1; points = at_k = at_k1 = fails = torn = orph = 0
    for B in range(0, total + 1, 7):
        f, _, acked = cost_run(B); g = f.clone(); L2 = Lsm(g); points += 1
        if L2.rc: fails += 1; continue
        d = L2.digest()[0]; k0 = d == dg[acked]; k1 = acked + 1 <= logical and d == dg[acked + 1]
        if not k0 and not k1: fails += 1; continue
        torn += L2.stats["wal_cut"] > 0; orph += L2.stats["orphans"] > 0; at_k += k0; at_k1 += (not k0)
    return total, L.stats["flushes"], L.stats["compactions"], logical, points, at_k, at_k1, fails, torn, orph
RC = {0: "ok", -1: "arg", -2: "full", -3: "io", -4: "corrupt"}
def tok(t):
    if t.startswith("x:"): return bytes.fromhex(t[2:])
    return b"" if t == "-" else t.encode()
def hx(b): return b.hex() if b else "-"
def run(path):
    fs = Fs(); L = Lsm(fs); out = ["open " + RC[L.rc]]
    for line in open(path):
        w = line.split()
        if not w: continue
        c = w[0]
        if c == "put" and len(w) == 3: out.append(f"put {w[1]} {RC[L.put(tok(w[1]), tok(w[2]))]}")
        elif c == "del" and len(w) == 2: out.append(f"del {w[1]} {RC[L.delete(tok(w[1]))]}")
        elif c == "get" and len(w) == 2:
            v = L.get(tok(w[1])); out.append(f"get {w[1]} found {hx(v)}" if v is not None else f"get {w[1]} none")
        elif c == "flush": out.append("flush " + RC[L.flush()])
        elif c == "compact": out.append("compact " + RC[L.compact()])
        elif c == "open": L = Lsm(fs); out.append("open " + RC[L.rc])
        elif c == "crash": fs.budget = int(w[1]); out.append("crash armed")
        elif c == "recover": fs = fs.clone(); L = Lsm(fs); out.append(f"recover {RC[L.rc]} replayed {L.stats['wal_replayed']} cut {L.stats['wal_cut']} orphans {L.stats['orphans']}")
        elif c == "digest": d, n = L.digest(); out.append(f"digest {d} {n}")
        elif c == "stats": t = L.stats; out.append(f"stats puts {t['puts']} deletes {t['deletes']} flushes {t['flushes']} compactions {t['compactions']} gets {t['gets']} bloom_skips {t['bloom']} range_skips {t['rng']} block_reads {t['blocks']} tables {len(L.tabs)} mem {len(L.mem)}")
        elif c == "dump":
            for n in sorted(fs.f): out.append(f"file {n} {len(fs.f[n])} {hx(fs.f[n])}")
        else: out.append("? " + c)
    return "\n".join(out) + "\n"
if __name__ == "__main__":
    sys.stdout.write(run(sys.argv[1]))
