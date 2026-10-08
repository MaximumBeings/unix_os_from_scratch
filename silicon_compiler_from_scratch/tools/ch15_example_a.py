#!/usr/bin/env python3
"""Chapter 15, example A: the page table at work, event by event. One request decodes 14 tokens with pages of 4 rows and a window of 6 tokens; then a second request is forked from it and writes into a shared page (copy on write). Usage: ch15_example_a.py. Writes out/ch15_example_a.json"""
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import paged_lm as PL
pt = PL.PageTable(8, 4); pt.new("A"); W = 6; log = []
def show(pt, rid): return f"table {pt.table[rid]}  dropped pages {pt.dropped[rid]}  free {sorted(pt.free)}"
print("== 1. one request, pages of 4 rows, window of 6 tokens (the step attends the new token and the 5 before it)")
print("   a row is written as 'token index'; the page pool has 8 physical pages (numbered 0..7, handed out from the top of the free list)")
for L in range(1, 15):
    n = min(L, W); lo = L - n; rows = pt.rows("A", lo, L - 1) if n > 1 else []
    pt.append("A", L - 1); pt.trim("A", max(0, L + 1 - W))
    print(f"  step {L:2d}: reads rows {[r for r in rows]} + the new one; wrote row {L-1} -> " + show(pt, "A")); log.append({"step": L, "gathered": rows, "table": list(pt.table["A"]), "dropped": pt.dropped["A"], "free": sorted(pt.free)})
print("   pages in use at the end:", pt.used(), "(a request with no window would hold 4)")
print("\n== 2. fork a second request B from A (shares every page), then B appends: the last page is shared, so B copies it first")
pt2 = PL.PageTable(8, 4); pt2.new("A")
for i in range(10): pt2.append("A", i)
print("   before the fork:  A", pt2.table["A"], "refcounts", {p: pt2.ref[p] for p in pt2.table["A"]})
pt2.fork("A", "B"); print("   after the fork:   A", pt2.table["A"], " B", pt2.table["B"], "refcounts", {p: pt2.ref[p] for p in pt2.table["A"]}, " (no copy: the prefix is stored once)")
pt2.append("B", 100); print("   B appends row 100 (A's last page holds rows 8, 9 and has room, but is shared): A", pt2.table["A"], " B", pt2.table["B"], "refcounts", {p: pt2.ref[p] for p in sorted(set(pt2.table["A"] + pt2.table["B"]))})
print("   A's rows are untouched:", pt2.rows("A", 0, 10), "  B's rows:", pt2.rows("B", 0, 11))
pt2.release("B"); print("   release B: A's pages keep their counts:", {p: pt2.ref[p] for p in pt2.table["A"]}, "free", sorted(pt2.free))
json.dump({"log": log}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "ch15_example_a.json"), "w"))
