#!/usr/bin/env python3
"""Chapter 16: test the tests of the compiler's slice_rows. Break model/capra.py one line at a time and run the slice battery (100 random graphs with slices, checks (a)-(c), (e) with bound 0.40, and the refusals) plus the original battery on 20 graphs (a slice must not disturb the rest). 'caught' = a check reports a problem or the compiler crashes."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "capra.py")).read()
M = [
 ("slice: the view starts at the beginning of its source", "loc[n.id] = (b, off + n.attrs[\"start\"])", "loc[n.id] = (b, off)"),
 ("slice: the view's offset is counted in words, not rows", "loc[n.id] = (b, off + n.attrs[\"start\"])", "loc[n.id] = (b, off + n.attrs[\"start\"] * 0 + 1 if n.attrs[\"start\"] else off)"),
 ("slice: the shape keeps the source's row count", "(count, self.nodes[a].shape[1]), start=start", "(r, self.nodes[a].shape[1]), start=start"),
 ("slice: the interpreter takes one row too many", "v[n.id] = [list(r) for r in v[n.ins[0]][n.attrs[\"start\"]:n.attrs[\"start\"] + n.attrs[\"count\"]]]\n        elif n.op == \"concat\":", "v[n.id] = [list(r) for r in v[n.ins[0]][n.attrs[\"start\"]:n.attrs[\"start\"] + n.attrs[\"count\"] + 1]]\n        elif n.op == \"concat\":"),
 ("slice: the float evaluation takes one row too few", "v[n.id] = [list(r) for r in v[n.ins[0]][n.attrs[\"start\"]:n.attrs[\"start\"] + n.attrs[\"count\"]]]\n        elif n.op == \"argmax\":", "v[n.id] = [list(r) for r in v[n.ins[0]][n.attrs[\"start\"]:n.attrs[\"start\"] + n.attrs[\"count\"] - 1]]\n        elif n.op == \"argmax\":"),
 ("slice: its scale is not tied to its source's", "        elif n.op == \"slice\": union(n.ins[0], n.id)\n", ""),
 ("slice: a range past the end is accepted", "if count < 1 or start < 0 or start + count > r: raise", "if count < 1 or start < 0: raise"),
 ("slice: an empty slice is accepted", "if count < 1 or start < 0 or start + count > r: raise", "if start < 0 or start + count > r: raise"),
 ("slice: a slice may feed a concatenation", "if any(N[c].op in (\"concat\", \"relu\") for c in cons[n.id]): raise", "if False: raise"),
 ("slice: the source is not loaded before its view is used", "        if n.op == \"slice\": ensure(n.ins[0]); addr_of(n.id); continue", "        if n.op == \"slice\": addr_of(n.id); continue"),
]
EQUIV = [("EQUIVALENT in effect: a slice is first given a buffer of its own, which the next loop then replaces by the view (an unused buffer entry)", "        if n.op in (\"output\", \"argmax\", \"slice\"): continue\n        if n.id not in loc:", "        if n.op in (\"output\", \"argmax\"): continue\n        if n.id not in loc:")]
CODE = ("import sys; sys.path.insert(0,'model'); import capra_tests as T\n"
        "p = T.check_slices(range(100)); e = T.check_slice_errors(); q, w = T.check_many(range(20)); sys.exit(1 if (p or e or q) else 0)")
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut16c_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "capra.py"), "w").write(SRC.replace(old, new))
    try: r = subprocess.run([sys.executable, "-c", CODE], cwd=d, capture_output=True, text=True, timeout=900).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the compiler. 'caught' is EXPECTED: it shows the battery notices the mistake.")
print("unbroken compiler passes:", check("base", "def weight(self", "def weight(self")[1] is False)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M)); eq = list(pool.map(lambda m: check(*m), EQUIV))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\nslice_rows: {n} of {len(M)} broken compilers caught")
print("\nA change that looks like a bug and is not:")
for label, r in eq: print(f"{label}: {'caught' if r is True else 'NOT CAUGHT'}")
sys.exit(0 if n == len(M) else 1)
