#!/usr/bin/env python3
"""Chapter 51: random order flow for the tests: gen(seed, n) -> command text. A random-walk mid price near $100.00, limit orders on both sides close enough to cross often, market orders, cancels (some of orders that no longer exist), replaces (some crossing),
reduces, and a few deliberately INVALID commands (zero quantity, duplicate id, quantity over the limit, price 0 on a replace, a time stamp that goes backwards). Prices are in 1/10000 dollar, ticks of one cent."""
import random, sys
def gen(seed, n=300, hot=False):
    R = random.Random(seed); mid = 1000000; ts = 1000; nid = 1; live = []; out = []
    for _ in range(n):
        ts += R.choice([0, 1, 1, 5, 1000]); mid = max(1000, mid + R.choice([-200, -100, 0, 0, 100, 200])); k = R.random(); spread = R.choice([100, 200, 300, 500, 1000, 5000])
        if k < .45:
            side = R.choice("BS"); px = mid + (-1 if side == "B" else 1) * R.choice([-300, -100, 0, 100, 200, 400, 1000]) ; px = max(100, px - px % 100); q = R.choice([1, 5, 10, 100, 100, 250, R.randrange(1, 2000)])
            out.append(f"N {nid} {side} {px} {q} {ts}"); live.append(nid); nid += 1
        elif k < .53: out.append(f"N {nid} {R.choice('BS')} 0 {R.choice([1, 50, 500, 3000])} {ts}"); nid += 1
        elif k < .70 and live: i = R.choice(live); out.append(f"C {i} {ts}"); live.remove(i) if R.random() < .7 else None
        elif k < .74: out.append(f"C {nid + 5000} {ts}")
        elif k < .84 and live: i = R.choice(live); px = max(100, mid + R.choice([-500, -100, 0, 100, 500, 2000, -2000])); px -= px % 100; out.append(f"R {i} {nid} {px} {R.choice([1, 10, 100, 700])} {ts}"); live.append(nid); nid += 1
        elif k < .92 and live: out.append(f"X {R.choice(live)} {R.choice([1, 2, 5, 50, 100000])} {ts}")
        else:
            bad = R.choice(["qty0", "dup", "big", "price0", "back", "badreplace", "timeback_cancel"])
            if bad == "qty0": out.append(f"N {nid} B {mid} 0 {ts}"); nid += 1
            elif bad == "dup" and live: out.append(f"N {R.choice(live)} S {mid} 5 {ts}")
            elif bad == "big": out.append(f"N {nid} B {mid} 1000001 {ts}"); nid += 1
            elif bad == "price0" and live: out.append(f"R {R.choice(live)} {nid} 0 5 {ts}"); nid += 1
            elif bad == "back": out.append(f"N {nid} S {mid + 100} 5 {max(0, ts - 50)}"); nid += 1
            elif bad == "badreplace" and live and len(live) > 1: out.append(f"R {live[0]} {live[1]} {mid} 5 {ts}")
            elif live: out.append(f"C {live[0]} {max(0, ts - 7)}")
    return "\n".join(out) + "\n"
if __name__ == "__main__": sys.stdout.write(gen(int(sys.argv[1]), int(sys.argv[2]) if len(sys.argv) > 2 else 300))
