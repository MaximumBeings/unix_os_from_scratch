#!/usr/bin/env python3
"""Chapter 47: coverage statistics of the auction differential test: reads the file auction_fuzz printed and counts what the random auctions actually exercised. Usage: fuzz_stats.py FILE"""
import collections, re, sys
L = open(sys.argv[1]).read().split("\n"); ev = [l for l in L if l.startswith("b ")]; cl = [l for l in L if l.startswith("c ")]
rc = collections.Counter(int(re.search(r"rc=(\d+)", l).group(1)) for l in ev); st = collections.Counter(int(re.search(r"status=(\d)", l).group(1)) for l in cl)
names = {0: "accepted", 2: "refused: auction ended", 3: "refused: seller bidding", 4: "refused: below the minimum bid", 5: "refused: lowering own maximum"}
changed = 0; ties = 0; prev = None; lead = None
for l in L:
    if l.startswith("S"): prev = None
    elif l.startswith("b "):
        e = int(re.search(r"end=(\d+)", l).group(1)); changed += prev is not None and e != prev; prev = e
print(f"{sum(1 for l in L if l.startswith('S'))} auctions, {len(ev)} bid events")
print("  event outcomes: " + ", ".join(f"{names[k]} {v}" for k, v in sorted(rc.items())))
print(f"  closes: sold {st.get(1, 0)}, reserve not met {st.get(2, 0)}, no bids {st.get(3, 0)}; end-time extensions observed {changed}")
