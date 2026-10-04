#!/usr/bin/env python3
"""Chapter 51: the INDEPENDENT reference: a second matching engine written differently (price levels as dictionaries of queues, best price by min/max over the levels), its own ITCH 5.0 encoder and decoder, its own book rebuilder.
It shares no code with the kernel and prints exactly what native/book_cli.c prints. Usage: book_ref.py run CMDFILE | feed FILE"""
import hashlib, struct, sys
from collections import deque
MAXQ = 1_000_000; MAXTS = (1 << 48) - 1; CAP = 1024
class Book:
    def __init__(s): s.lv = {0: {}, 1: {}}; s.ord = {}; s.seq = 1; s.match = 1; s.last_ts = 0; s.trades = 0; s.vol = 0; s.notional = 0; s.out = bytearray(); s.tr = []
    # ---- encoder
    def msg(s, t, ts, body): m = bytes([t]) + struct.pack(">HH", 1, 0) + ts.to_bytes(6, "big") + body; s.out += struct.pack(">H", len(m)) + m
    def rest(s, id, side, price, qty):
        s.ord[id] = [side, price, qty, s.seq]; s.lv[side].setdefault(price, deque()).append(id); s.seq += 1
    def drop(s, id):
        side, price, qty, _ = s.ord.pop(id); d = s.lv[side][price]; d.remove(id)
        if not d: del s.lv[side][price]
    def best(s, side):
        if not s.lv[side]: return None
        p = max(s.lv[side]) if side == 0 else min(s.lv[side]); return s.lv[side][p][0]
    def crosses(s, side, price):
        b = s.best(1 - side)
        if b is None: return False
        return price == 0 or (s.ord[b][1] <= price if side == 0 else s.ord[b][1] >= price)
    def check_ts(s, ts): return ts > MAXTS or ts < s.last_ts
    def go(s, id, side, price, qty, ts):
        left = qty
        while left > 0:
            b = s.best(1 - side)
            if b is None: break
            bp = s.ord[b][1]
            if price != 0 and (bp > price if side == 0 else bp < price): break
            q = min(left, s.ord[b][2]); mn = s.match; s.match += 1; s.tr.append((b, id, bp, q, mn)); s.msg(ord("E"), ts, struct.pack(">QIQ", b, q, mn))
            s.ord[b][2] -= q; left -= q; s.trades += 1; s.vol += q; s.notional += bp * q
            if s.ord[b][2] == 0: s.drop(b)
        if left > 0 and price != 0:
            s.rest(id, side, price, left); s.msg(ord("A"), ts, struct.pack(">QcI8sI", id, b"B" if side == 0 else b"S", left, b"ACME    ", price))
        return "ok"
    def new(s, id, side, price, qty, ts):
        s.tr = []
        if qty == 0 or qty > MAXQ: return "qty"
        if id == 0 or id in s.ord: return "dup-id"
        if s.check_ts(ts): return "time"
        if price != 0 and len(s.ord) >= CAP:
            if not s.crosses(side, price): return "full"
            avail = 0
            for i, (sd, p, q, _) in s.ord.items():
                if sd != side and (p <= price if side == 0 else p >= price): avail += min(q, qty)
                if avail >= qty: break
            if avail < qty: return "full"
        s.last_ts = ts; return s.go(id, side, price, qty, ts)
    def cancel(s, id, ts):
        s.tr = []
        if id not in s.ord: return "unknown-id"
        if s.check_ts(ts): return "time"
        s.last_ts = ts; s.drop(id); s.msg(ord("D"), ts, struct.pack(">Q", id)); return "ok"
    def reduce(s, id, cq, ts):
        s.tr = []
        if id not in s.ord: return "unknown-id"
        if cq == 0 or cq >= s.ord[id][2]: return "reduce"
        if s.check_ts(ts): return "time"
        s.last_ts = ts; s.ord[id][2] -= cq; s.msg(ord("X"), ts, struct.pack(">QI", id, cq)); return "ok"
    def replace(s, old, new, price, qty, ts):
        s.tr = []
        if old not in s.ord: return "unknown-id"
        if qty == 0 or qty > MAXQ: return "qty"
        if price == 0: return "price"
        if new == 0 or (new != old and new in s.ord): return "dup-id"
        if s.check_ts(ts): return "time"
        side = s.ord[old][0]; s.last_ts = ts
        if s.crosses(side, price): s.drop(old); s.msg(ord("D"), ts, struct.pack(">Q", old)); return s.go(new, side, price, qty, ts)
        sd, p, q, _ = s.ord[old]; s.drop(old); s.rest(new, side, price, qty); s.msg(ord("U"), ts, struct.pack(">QQII", old, new, qty, price)); return "ok"
    # ---- state
    def priority(s):
        bids = sorted(((-p, sq, i) for i, (sd, p, q, sq) in s.ord.items() if sd == 0)); asks = sorted(((p, sq, i) for i, (sd, p, q, sq) in s.ord.items() if sd == 1))
        return [i for _, _, i in bids] + [i for _, _, i in asks]
    def hash(s):
        b = struct.pack(">I", len(s.ord))
        for i in s.priority(): sd, p, q, _ = s.ord[i]; b += struct.pack(">QIIB", i, p, q, sd)
        return hashlib.sha256(b).hexdigest()
    def dump(s):
        o = []
        for sd, nm in ((0, "bid"), (1, "ask")):
            agg = {}
            for i in s.priority():
                x = s.ord[i]
                if x[0] == sd: agg[x[1]] = agg.get(x[1], 0) + x[2]
            o += [f"{nm} {p} {q}" for p, q in agg.items()]
        o.append("hash " + s.hash()); o.append(f"stats trades {s.trades} volume {s.vol} notional {s.notional} live {len(s.ord)}"); return o
    # ---- decoder
    def apply_feed(s, buf):
        pos = 0; n = 0
        while pos < len(buf):
            n += 1
            if len(buf) - pos < 3: return n, "truncated length prefix"
            ml = struct.unpack_from(">H", buf, pos)[0]; m = buf[pos + 2:pos + 2 + ml]
            if ml > len(buf) - pos - 2: return n, "message runs past the end of the feed"
            t = chr(m[0]); want = {"A": 36, "E": 31, "X": 23, "D": 19, "U": 35, "S": 12, "R": 39, "H": 25}.get(t)
            if want is None: return n, "unknown message type"
            if ml != want: return n, "wrong length for the message type"
            pos += 2 + ml
            if t in "SRH": continue
            if m[1] != 0 or m[2] != 1: return n, "stock locate is not 1"
            ts = int.from_bytes(m[5:11], "big")
            if ts < s.last_ts: return n, "time goes backwards"
            s.last_ts = ts
            if t == "A":
                id, = struct.unpack_from(">Q", m, 11); sd = chr(m[19]); q, = struct.unpack_from(">I", m, 20); pr, = struct.unpack_from(">I", m, 32)
                if sd not in "BS": return n, "side is not B or S"
                if q == 0 or q > MAXQ: return n, "add with a bad quantity"
                if pr == 0: return n, "add with price 0"
                if id == 0 or id in s.ord: return n, "add of an order id that is already live"
                if m[24:32] != b"ACME    ": return n, "unexpected stock symbol"
                if len(s.ord) >= CAP: return n, "order table full"
                s.rest(id, 0 if sd == "B" else 1, pr, q)
            elif t == "E":
                id, = struct.unpack_from(">Q", m, 11); q, = struct.unpack_from(">I", m, 19); mn, = struct.unpack_from(">Q", m, 23)
                if id not in s.ord: return n, "execution of an unknown order"
                if q == 0 or q > s.ord[id][2]: return n, "execution larger than the remaining quantity"
                if mn != s.match: return n, "match number out of sequence"
                s.match += 1; s.ord[id][2] -= q; s.trades += 1; s.vol += q; s.notional += s.ord[id][1] * q
                if s.ord[id][2] == 0: s.drop(id)
            elif t == "X":
                id, = struct.unpack_from(">Q", m, 11); q, = struct.unpack_from(">I", m, 19)
                if id not in s.ord: return n, "reduce of an unknown order"
                if q == 0 or q >= s.ord[id][2]: return n, "reduce by zero or by the whole quantity"
                s.ord[id][2] -= q
            elif t == "D":
                id, = struct.unpack_from(">Q", m, 11)
                if id not in s.ord: return n, "delete of an unknown order"
                s.drop(id)
            else:
                old, = struct.unpack_from(">Q", m, 11); new, = struct.unpack_from(">Q", m, 19); q, = struct.unpack_from(">I", m, 27); pr, = struct.unpack_from(">I", m, 31)
                if old not in s.ord: return n, "replace of an unknown order"
                if q == 0 or q > MAXQ or pr == 0: return n, "replace with a bad quantity or price"
                if new == 0 or (new != old and new in s.ord): return n, "replace to an id that is already live"
                side = s.ord[old][0]; s.drop(old); s.rest(new, side, pr, q)
            bb, ba = s.best(0), s.best(1)
            if bb is not None and ba is not None and s.ord[bb][1] >= s.ord[ba][1]: return n, "the book is crossed or inconsistent after this message"
        return None
def main():
    mode, path = sys.argv[1], sys.argv[2]
    if mode == "feed":
        b = Book(); r = b.apply_feed(open(path, "rb").read())
        if r: print(f"REFUSED message {r[0]}: {r[1]}"); return
        print("OK"); print("\n".join(b.dump())); return
    e = Book(); o = []; n = 0
    for line in open(path):
        t = line.split()
        if not t: continue
        c = t[0]; n += 1; before = len(e.out)
        try:
            if c == "N": rc = e.new(int(t[1]), 0 if t[2] == "B" else 1, int(t[3]), int(t[4]), int(t[5]))
            elif c == "C": rc = e.cancel(int(t[1]), int(t[2]))
            elif c == "R": rc = e.replace(int(t[1]), int(t[2]), int(t[3]), int(t[4]), int(t[5]))
            elif c == "X": rc = e.reduce(int(t[1]), int(t[2]), int(t[3]))
            else: n -= 1; continue
        except (IndexError, ValueError): n -= 1; continue
        o.append(f"cmd {n} {rc}"); o += [f"trade {a} {b} {p} {q} {m}" for a, b, p, q, m in e.tr]
        if len(e.out) > before: o.append("feed " + bytes(e.out[before:]).hex())
    o += e.dump(); f = Book(); r = f.apply_feed(bytes(e.out)); o.append("rebuilt " + ("REFUSED" if r else "ok") + " " + ("same-hash" if not r and f.hash() == e.hash() else "DIFFERENT")); print("\n".join(o))
main()
