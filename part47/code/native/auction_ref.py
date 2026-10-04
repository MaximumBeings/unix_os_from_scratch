#!/usr/bin/env python3
"""Chapter 47: an INDEPENDENT reference for the auction engine. It shares no code with 047_auction.c and uses a different METHOD: instead of the engine's closed-form rule
(price = one increment above the runner-up's maximum, capped at the leader's maximum), it simulates the process eBay's documentation describes -- "if someone else outbids the buyer a bid,
eBay automatically bids again for the buyer up to the amount of their maximum bid" -- one challenge at a time. It regenerates the same pseudo-random scenarios as auction_fuzz.c (same xorshift32)
and prints the same lines; `diff` is the test.   Usage: auction_ref.py N"""
import sys
M = 0xFFFFFFFF
TABLE = [(0, 5), (100, 25), (500, 50), (2500, 100), (10000, 250), (25000, 500), (50000, 1000), (100000, 2500), (250000, 5000), (500000, 10000)]
def inc(price):
    r = TABLE[0][1]
    for frm, i in TABLE:
        if price >= frm: r = i
    return r
class Rng:
    def __init__(s, x): s.x = x if x else 1
    def next(s):
        x = s.x; x ^= (x << 13) & M; x ^= x >> 17; x ^= (x << 5) & M; s.x = x & M; return s.x
class Auction:
    def __init__(s, start, reserve, dur, win, esecs, seller): s.start, s.reserve, s.end, s.win, s.esecs, s.seller = start, reserve, dur, win, esecs, seller; s.price = start; s.lead = 0; s.leadmax = 0; s.maxes = {}; s.order = {}; s.count = 0; s.tick = 0; s.status = 0
    def min_bid(s): return s.start if s.count == 0 else s.price + inc(s.price)
    def bid(s, who, m, now):
        if s.status != 0 or now >= s.end: return 2
        if who == s.seller: return 3
        own = s.maxes.get(who, 0)
        if own:
            if m <= own: return 5
            if who != s.lead and m < s.min_bid(): return 4
        else:
            if m < s.min_bid(): return 4
            if len(s.maxes) >= 8: return 6
        s.tick += 1; s.count += 1; s.maxes[who] = m; s.order[who] = s.tick
        if s.lead == 0:                                   # first bid
            s.lead, s.leadmax, s.price = who, m, s.start
        elif who == s.lead:                               # the leader raises their own maximum: eBay bids again against the runner-up, so the price may rise (it never falls)
            s.leadmax = m
            others = [x for b, x in s.maxes.items() if b != who]
            if others:
                second = max(others); s.price = max(s.price, min(m, second + inc(second)))
        elif m > s.leadmax:                               # the challenger beats the leader's maximum: eBay bids up to it for the old leader, the challenger is one increment above it
            old = s.leadmax; s.lead, s.leadmax = who, m; s.price = min(m, old + inc(old))
        elif m == s.leadmax:                              # equal maxima: the earlier bidder keeps the lead, at the shared maximum
            if s.order[who] < s.order[s.lead]: s.lead, s.leadmax = who, m
            s.price = m
        else:                                             # the leader holds: eBay bids again for them, one increment above the challenger
            s.price = min(s.leadmax, m + inc(m))
        if s.reserve and s.leadmax >= s.reserve and s.price < s.reserve: s.price = s.reserve
        if s.esecs and now + s.win >= s.end and now + s.esecs > s.end: s.end = now + s.esecs
        return 0
    def close(s, now):
        if s.status != 0: return 0
        if now < s.end: return 7
        if s.count == 0: s.status = 3
        elif s.reserve and s.leadmax < s.reserve: s.status = 2
        else: s.status = 1
        return 0
def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    out = []
    for s in range(n):
        r = Rng((2463534242 + s * 2654435761) & M); start = 1 + r.next() % 20000
        coin = r.next() % 2; reserve = start + r.next() % 60000 if coin else 0
        ext_on = (r.next() % 4) == 0; win = 1 + r.next() % 300; esecs = 1 + r.next() % 300; dur = 600 + r.next() % 2400
        if not ext_on: win = esecs = 0
        a = Auction(start, reserve, dur, win, esecs, 3)
        out.append(f"S{s} start={start} reserve={reserve} win={win} ext={esecs} dur={dur} create=0")
        t = 0; sparse = (r.next() % 6 == 0); nev = (r.next() % 3) if sparse else 12 + r.next() % 24; maxes = []
        for e in range(nev):
            t += r.next() % (dur // (nev if nev else 1) + 1); who = 3 + r.next() % 6; mode = r.next() % 10
            if mode == 0 and maxes: m = maxes[r.next() % len(maxes)]
            elif mode == 1: m = a.min_bid()
            elif mode == 3 and reserve: m = reserve
            else:
                if mode == 2 and win and a.end >= win and a.end - win >= t: t = a.end - win
                if mode == 4 and a.end >= 1 and a.end - 1 >= t: t = a.end - 1
                m = start // 2 + r.next() % 60000
            rc = a.bid(who, m, t)
            if rc == 0 and len(maxes) < 16: maxes.append(m)
            out.append(f"b t={t} who={who} max={m} rc={rc} price={a.price if rc == 0 else 0} high={1 if (rc == 0 and a.lead == who) else 0} lead={a.lead} end={a.end} cnt={a.count}")
        ct = a.end + (r.next() % 3)
        early = a.close(a.end - 1); rc2 = a.close(ct)
        out.append(f"c early={early} rc={rc2} status={a.status} price={a.price} winner={a.lead if a.status == 1 else 0} cnt={a.count}")
    print("\n".join(out))
main()
